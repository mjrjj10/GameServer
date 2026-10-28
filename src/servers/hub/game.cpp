#include "game.h"
#include "coordinator.h" // account data
#include "instance.h"
#include <mxm/game_content.h>
#include "config.h"
#include "mailbox.h"
#include "mission_progress.h"
#include "mission_rules.h"
#include "town_rules.h"
#include <EASTL/algorithm.h>
#include <mxm/account_progress.h>
#include <time.h>
#include <EAStdC/EAString.h>
#include <common/utils.h>

static const ClassType DEFAULT_LEADER_CLASS = ClassType::LAUNCHER;


void HubGame::Init(Server* server_, const ClientLocalMapping* plidMap_)
{
	plidMap = plidMap_;
	replication.Init(server_);
	replication.plidMap = plidMap_;
	world.Init(&replication);

	matchmaker = &Matchmaker();

	playerActorUID.fill(ActorUID::INVALID);

	playerMap.fill(playerList.end());

	LoadMap();
}

void HubGame::Update(Time localTime_)
{
	ProfileFunction();

	localTime = localTime_;
	world.Update(localTime);
	replication.FrameEnd();
}

bool HubGame::JukeboxQueueSong(i32 userID, SongID songID)
{
	ASSERT(playerAccountData[userID]);
	const ClientHandle clientHd = playerMap[userID]->clientHd;

	WorldHub::ActorJukebox& jukebox = world.jukebox;
	ASSERT(jukebox.UID != ActorUID::INVALID);

	if(jukebox.queue.full()) {
		// TODO: send *actual* packet answer
		SendDbgMsg(clientHd, L"Jukebox queue is full");
		return false;
	}

	// limit reservation to 1 per player
	// @Speed
	// TODO: store the accountID (when we have it)
	const WideString& nick = playerAccountData[userID]->nickname;
	bool found = false;
	if(jukebox.currentSong.requesterNick.compare(nick) == 0) {
		found = true;
	}

	if(!found) {
		foreach(it, jukebox.queue) {
			if(it->requesterNick.compare(nick) == 0) {
				found = true;
				break;
			}
		}
	}

	if(found) {
		SendDbgMsg(clientHd, L"Song reservation is limited to 1 per player");
		return false;
	}

	const GameXmlContent::Song* xmlSong = GetGameXmlContent().FindJukeboxSongByID(songID);
	if(!xmlSong) {
		SendDbgMsg(clientHd, LFMT(L"ERROR: Jukebox song not found (%d)", songID));
		return false;
	}

	WorldHub::ActorJukebox::Song song;
	song.requesterNick = playerAccountData[userID]->nickname;
	song.songID = songID;
	song.lengthInSec = xmlSong->length;
	jukebox.queue.push_back(song);
	return true;
}

bool HubGame::LoadMap()
{
	const GameXmlContent& content = GetGameXmlContent();

	foreach(it, content.mapLobby.creatures) {
		// don't spawn "spawn points"
		if(it->IsSpawnPoint()) {
			mapSpawnPoints.push_back(SpawnPoint{ it->pos, it->rot });
			continue;
		}

		if(it->docID == CreatureIndex::Jukebox || it->docID == CreatureIndex::HalloweenJukebox) {
			WorldHub::ActorJukebox& actor = world.SpawnJukeboxActor(it->docID, it->localID, it->pos, it->rot);
			actor.spawnAnim = it->spawnAnim;
			actor.wanderDist = it->wanderDist;
			actor.tagID = it->tagID;
			actor.ownerID = it->ownerID;
			actor.dirToNearPC = it->dirToNearPC;
			actor.actionState = it->actionState;
			actor.faction = (i32)it->faction;
			actor.type = it->entityType;
			actor.seed = (i32)RandUint();
		}
		else {
			WorldHub::ActorNpc& actor = world.SpawnNpcActor(it->docID, it->localID);
			actor.pos = it->pos;
			actor.dir = it->rot;
			actor.spawnAnim = it->spawnAnim;
			actor.wanderDist = it->wanderDist;
			actor.tagID = it->tagID;
			actor.ownerID = it->ownerID;
			actor.dirToNearPC = it->dirToNearPC;
			actor.actionState = it->actionState;
			actor.faction = (i32)it->faction;
			actor.type = it->entityType;
			actor.seed = (i32)RandUint();
		}

	}

	return true;
}

void HubGame::OnPlayerConnect(ClientHandle clientHd, const Account* accountData)
{
	const i32 userID = plidMap->Get(clientHd);
	playerAccountData[userID] = accountData;
	accountClientHandleMap.emplace(accountData->UID, clientHd);

	playerList.push_back(Player(clientHd));
	playerMap[userID] = --playerList.end();

	replication.SendAccountDataLobby(clientHd, *accountData);

	replication.OnPlayerConnect(clientHd);
	replication.SetPlayerAccount(clientHd, accountData);
	SendFriendsOnConnect(userID);
	GuildOnConnect(userID);

	LOG("[client%x] HubGame :: account %llu connected (%ls)", clientHd, (u64)accountData->UID, accountData->nickname.data());
	SortieUID resultSortieUID = SortieUID::INVALID;
	if(TakeAccountReturningFromGame(accountData->nickname, &resultSortieUID)) {
		if(Config().DevShowcase || Config().DevAutoQueue) {
			LOG("[client%x] HubGame :: back from a game, loading the lobby", clientHd);
			replication.SendLoadLobby(clientHd, MapIndex::LOBBY_NORMAL, true);
		}
		else {
			LOG("[client%x] HubGame :: back from game %llu, result screen", clientHd, (u64)resultSortieUID);
			playerMap[userID]->resultSortieUID = resultSortieUID;
			replication.SendLoadLobby(clientHd, MapIndex::LOBBY_NORMAL, false);
			replication.SendResultScreenStart(clientHd);
		}
	}
}

void HubGame::OnPlayerDisconnect(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);
	LOG("[client%x] GameHub :: OnClientDisconnect :: actorUID=%u", clientHd, (u32)playerActorUID[userID]);

	if(playerAccountData[userID] && playerMap[userID] != playerList.end()) LeaveParty(userID, playerMap[userID]->sortieUID == SortieUID::INVALID);
	if(playerAccountData[userID]) BroadcastFriendState(userID, playerMap[userID] != playerList.end() && playerMap[userID]->sortieUID != SortieUID::INVALID ? MEASURED_FRIEND_STATE_IN_GAME : MEASURED_FRIEND_STATE_OFFLINE);
	if(playerAccountData[userID] && playerMap[userID] != playerList.end()) GuildOnDisconnect(userID);

	// we can disconnect before spawning, so test if we have an actor associated
	if(playerActorUID[userID] != ActorUID::INVALID) {
		world.DestroyPlayerActor(playerActorUID[userID]);
	}
	playerActorUID[userID] = ActorUID::INVALID;

	if(playerMap[userID] != playerList.end()) {
		playerList.erase(playerMap[userID]);
	}
	playerMap[userID] = playerList.end();

	accountClientHandleMap.erase(playerAccountData[userID]->UID);
	playerAccountData[userID] = nullptr;

	replication.OnClientDisconnect(clientHd);
}

void HubGame::OnPlayerGetCharacterInfo(ClientHandle clientHd, ActorUID actorUID)
{
	const i32 userID = plidMap->Get(clientHd);

	const WorldHub::ActorPlayer* actor = world.FindPlayerActor(actorUID);
	ASSERT(actor->clientHd == clientHd);
	replication.SendCharacterInfo(clientHd, actor->UID, actor->docID, actor->classType, 100, 100);

}

void HubGame::OnPlayerUpdatePosition(ClientHandle clientHd, ActorUID characterActorUID, const vec3& pos, const vec3& dir, const vec3& eye, f32 rotate, f32 speed, ActionStateID state, i32 actionID)
{
	const i32 userID = plidMap->Get(clientHd);

	// NOTE: the client is not aware that we spawned a new actor for them yet, we ignore this packet
	// LordSk (30/08/2020)
	if(playerActorUID[userID] != characterActorUID) {
		WARN("Client sent an invalid characterID (userID=0x%08x characterID=0x%08x)", userID, (u32)characterActorUID);
		return;
	}

	WorldHub::ActorPlayer* actor = world.FindPlayerActor(playerActorUID[userID]);
	ASSERT(actor);

	// TODO: check for movement hacking
	actor->pos = pos;
	actor->dir = dir;
	actor->eye = eye;
	actor->rotate = rotate;
	actor->speed = speed;
}

void HubGame::OnPlayerChatMessage(ClientHandle clientHd, i32 chatType, const wchar* msg, i32 msgLen)
{
	// command
	if(ParseChatCommand(clientHd, msg, msgLen)) {
		return; // we're done here
	}

	// TODO: chat types
	// TODO: senderStaffType
	// TODO: Actual chat system

	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	if(chatType == MEASURED_CHAT_TYPE_GUILD && GuildChat(userID, msg, msgLen)) return;
	replication.SendChatMessageToAll(playerAccountData[userID]->nickname.data(), chatType, msg, msgLen);
}

void HubGame::OnPlayerChatWhisper(ClientHandle clientHd, const wchar* destNick, const wchar* msg)
{
	const i32 userID = plidMap->Get(clientHd);

	ASSERT(playerAccountData[userID]);

	i32 destClientID = -1;
	for(int i = 0; i < playerAccountData.size(); i++) {
		if(playerAccountData[i]) {
			if(playerAccountData[i]->nickname.compare(destNick) == 0) {
				destClientID = i;
				break;
			}
		}
	}

	if(destClientID == -1) {
		replication.SendChatWhisperConfirmToClient(clientHd, destNick, msg, ErrorType::WHISPER_SEND_NOT_FOUND);
		return;
	}

	replication.SendChatWhisperConfirmToClient(clientHd, destNick, msg, ErrorType::SUCCESS);
	replication.SendChatWhisperToClient(playerMap[destClientID]->clientHd, playerAccountData[userID]->nickname.data(), msg);
}

void HubGame::OnPlayerEnterCity(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);
	if((Config().DevShowcase || Config().DevAutoQueue) && playerAccountData[userID] && playerMap[userID]->partyUID == PartyUID::INVALID) {
		LOG("[client%x] HubGame :: showcase, auto dispatch", clientHd);
		OnCreateParty(clientHd, EntrySystemID::ARENA_3v3, StageType::PVP_GAME);
	}
	if(playerActorUID[userID] != ActorUID::INVALID || !playerAccountData[userID]) return;
	LocalActorID characterID = LocalActorID((u32)LocalActorID::FIRST_SELF_MASTER + (u32)DEFAULT_LEADER_CLASS);
	SkinIndex skin = SkinIndex::DEFAULT;
	foreach_const(it, lastLeaders) {
		if(it->nickname == playerAccountData[userID]->nickname) { characterID = it->characterID; skin = it->skin; }
	}
	LOG("[client%x] HubGame :: entered the city without a leader character, spawning the last one", clientHd);
	OnPlayerSetLeaderCharacter(clientHd, characterID, skin);
}

void HubGame::OnPlayerSetLeaderCharacter(ClientHandle clientHd, LocalActorID characterID, SkinIndex skinIndex)
{
	const i32 userID = plidMap->Get(clientHd);
	if(playerAccountData[userID]) {
		bool found = false;
		foreach(it, lastLeaders) {
			if(it->nickname == playerAccountData[userID]->nickname) { it->characterID = characterID; it->skin = skinIndex; found = true; }
		}
		if(!found && !lastLeaders.full()) lastLeaders.push_back({ playerAccountData[userID]->nickname, characterID, skinIndex });
	}

	const i32 leaderMasterContentID = (u32)characterID - (u32)LocalActorID::FIRST_SELF_MASTER - 1;

	// select a spawn point at random
	const SpawnPoint& spawnPoint = mapSpawnPoints[RandUint() % mapSpawnPoints.size()];
	vec3 pos = spawnPoint.pos;
	vec3 dir = spawnPoint.dir;
	vec3 eye(0, 0, 0);

	// TODO: check if already leader character
	if((playerActorUID[userID] != ActorUID::INVALID)) {
		WorldHub::ActorCore* actor = world.FindPlayerActor(playerActorUID[userID]);
		ASSERT(actor);

		pos = actor->pos;
		dir = actor->dir;
		eye = actor->eye;

		world.DestroyPlayerActor(playerActorUID[userID]);
	}

	ASSERT(playerAccountData[userID]); // account data is not assigned
	const Account* account = playerAccountData[userID];

	// TODO: tie in account->leaderMasterID,skinIndex with class and model
	const ClassType classType = GetGameXmlContent().masters[leaderMasterContentID].classType;
	ASSERT((i32)classType == leaderMasterContentID+1);

	WorldHub::ActorPlayer& actor = world.SpawnPlayerActor(userID, classType, skinIndex, account->nickname.data(), account->guildTag.data());
	actor.pos = pos;
	actor.dir = dir;
	actor.eye = eye;
	actor.clientHd = clientHd; // TODO: this is not useful right now
	actor.level = account->level;
	actor.levelExp = account->levelExp;
	playerActorUID[userID] = actor.UID;

	replication.SendPlayerSetLeaderMaster(clientHd, playerActorUID[userID], classType, skinIndex);
}

void HubGame::OnPlayerSyncActionState(ClientHandle clientHd, ActorUID actorUID, ActionStateID state, i32 param1, i32 param2, f32 rotate, f32 upperRotate)
{
	const i32 userID = plidMap->Get(clientHd);

	WorldHub::ActorPlayer* actor = world.FindPlayerActor(playerActorUID[userID]);
	ASSERT(actor);

	// TODO: check hacking
	actor->rotate = rotate;
	actor->upperRotate = upperRotate;
	actor->actionState = state;
	actor->actionParam1 = param1;
	actor->actionParam2 = param2;
}

void HubGame::OnPlayerJukeboxQueueSong(ClientHandle clientHd, SongID songID)
{
	const i32 userID = plidMap->Get(clientHd);

	JukeboxQueueSong(userID, songID);
}

void HubGame::OnPlayerReadyToLoad(ClientHandle clientHd)
{
	replication.SendLoadLobby(clientHd, MapIndex::LOBBY_NORMAL, true);
}

void HubGame::OnReturnToCity(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const WideString& nickname = playerAccountData[userID]->nickname;
	const SortieUID resultSortieUID = playerMap[userID]->resultSortieUID;
	playerMap[userID]->resultSortieUID = SortieUID::INVALID;
	replication.SendUserReturnToCity(clientHd, nickname.data());
	if(resultSortieUID != SortieUID::INVALID) {
		foreach_const(p, playerList) {
			if(p->clientHd == clientHd || p->resultSortieUID != resultSortieUID) continue;
			replication.SendUserReturnToCity(p->clientHd, nickname.data());
		}
	}
	replication.SendReturnToCity(clientHd);
}

bool HubGame::GrantItem(i32 userID, i32 docIndex, i32 amount, u32 updateReason, u32 acquisitionReason)
{
	ASSERT(playerAccountData[userID]);
	const GameXmlContent::ItemInfo* info = GetGameXmlContent().FindItemInfo(docIndex);
	if(!info || amount <= 0) return false;
	const wchar* nickname = playerAccountData[userID]->nickname.data();
	const ClientHandle clientHd = playerMap[userID]->clientHd;
	AccountInventory inventory = AccountInventoryLoad(nickname);
	i32 left = amount;
	foreach(it, inventory.items) {
		if(left <= 0) break;
		if(it->docIndex != docIndex || it->count >= info->maxStack) continue;
		const i32 add = MIN(left, info->maxStack - it->count);
		it->count += add;
		left -= add;
		replication.SendItemUpdate(clientHd, *it, updateReason);
		replication.SendItemAcquisition(clientHd, acquisitionReason, *it, add);
	}
	while(left > 0) {
		if(inventory.items.full()) break;
		AccountItem item;
		item.uid = inventory.nextUid++;
		item.slot = MEASURED_INVENTORY_FIRST_SLOT;
		for(bool taken = true; taken; ) {
			taken = false;
			foreach_const(it, inventory.items) if(it->invenType == item.invenType && it->slot == item.slot) { taken = true; item.slot++; break; }
		}
		item.docIndex = docIndex;
		item.count = MIN(left, info->maxStack);
		left -= item.count;
		inventory.items.push_back(item);
		replication.SendItemUpdate(clientHd, item, updateReason);
		replication.SendItemAcquisition(clientHd, acquisitionReason, item, item.count);
	}
	AccountInventorySave(nickname, inventory);
	LOG("[client%x] HubGame :: %ls receives %d x item %d", clientHd, nickname, amount - left, docIndex);
	return left == 0;
}

void HubGame::OnInventoryExpand(ClientHandle clientHd, i32 tab)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	auto found = GetAccountManager().accountMap.find(playerAccountData[userID]->UID);
	ASSERT(found != GetAccountManager().accountMap.end());
	Account& account = *found->second;
	if(tab < 0 || tab > 2) {
		WARN("[client%x] HubGame :: invalid inventory tab %d", clientHd, tab);
		return;
	}
	AccountProgress progress = AccountProgressLoad(account.nickname.data());
	const auto& costs = GetGameXmlContent().inventoryExpandCosts;
	const i32 step = progress.inventoryLines[tab] - MEASURED_INVENTORY_BASE_LINES;
	if(step < 0 || step >= (i32)costs.size()) {
		replication.SendInventoryExpand(clientHd, MEASURED_INVENTORY_EXPAND_ALREADY_FULL, tab, progress.inventoryLines[tab]);
		return;
	}
	const GameXmlContent::ServiceCost& cost = costs[step];
	const bool ccoin = cost.buyType == 2;
	if((ccoin ? progress.ccoin : progress.gold) < cost.amount) {
		replication.SendInventoryExpand(clientHd, ccoin ? MEASURED_INVENTORY_EXPAND_NOT_ENOUGH_C_COIN : MEASURED_INVENTORY_EXPAND_NOT_ENOUGH_RON, tab, progress.inventoryLines[tab]);
		return;
	}
	if(ccoin) progress.ccoin -= cost.amount;
	else progress.gold -= cost.amount;
	progress.inventoryLines[tab]++;
	AccountProgressSave(account.nickname.data(), progress);
	account.gold = progress.gold;
	account.ccoin = progress.ccoin;
	LOG("[client%x] HubGame :: %ls expands inventory tab %d to %d lines for %d %s", clientHd, account.nickname.data(), tab, progress.inventoryLines[tab], cost.amount, ccoin ? "ccoin" : "gold");
	if(ccoin) replication.SendCCoin(clientHd, account.ccoin, MEASURED_CCOIN_REASON_SHOP);
	else replication.SendMoney(clientHd, account.gold, MEASURED_MISSION_REWARD_MONEY_REASON);
	replication.SendInventoryExpand(clientHd, 0, tab, progress.inventoryLines[tab]);
}

void HubGame::OnShopBuyItem(ClientHandle clientHd, u32 goodsID, i32 quantity, i32 itemIndex, i32 itemCount, bool npShop)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	auto found = GetAccountManager().accountMap.find(playerAccountData[userID]->UID);
	ASSERT(found != GetAccountManager().accountMap.end());
	Account& account = *found->second;
	const GameXmlContent::ItemInfo* info = GetGameXmlContent().FindItemInfo(itemIndex);
	const i32 units = quantity * itemCount;
	if(!info || info->buyType == 0 || info->buyValue <= 0 || quantity <= 0 || itemCount <= 0 || units > 999) {
		WARN("[client%x] HubGame :: invalid shop purchase goods=%u item=%d x%d", clientHd, goodsID, itemIndex, units);
		replication.SendShopBuyResult(clientHd, npShop, MEASURED_SHOP_INVALID_DATA, goodsID, (u32)quantity, itemIndex);
		return;
	}
	const i64 cost = (i64)info->buyValue * quantity;
	AccountProgress progress = AccountProgressLoad(account.nickname.data());
	const bool ccoin = info->buyType == 2;
	if((ccoin ? progress.ccoin : progress.gold) < cost) {
		replication.SendShopBuyResult(clientHd, npShop, ccoin ? MEASURED_SHOP_NOT_ENOUGH_CCOIN : MEASURED_SHOP_NOT_ENOUGH_GOLD, goodsID, (u32)quantity, itemIndex);
		return;
	}
	if(ccoin) progress.ccoin -= cost;
	else progress.gold -= cost;
	AccountProgressSave(account.nickname.data(), progress);
	account.gold = progress.gold;
	account.ccoin = progress.ccoin;
	LOG("[client%x] HubGame :: %ls buys %d x item %d for %lld %s", clientHd, account.nickname.data(), units, itemIndex, cost, ccoin ? "ccoin" : "gold");
	if(ccoin) replication.SendCCoin(clientHd, account.ccoin, MEASURED_CCOIN_REASON_SHOP);
	else replication.SendMoney(clientHd, account.gold, MEASURED_MISSION_REWARD_MONEY_REASON);
	GrantItem(userID, itemIndex, units, MEASURED_ITEM_UPDATE_REASON_SHOP, MEASURED_ITEM_ACQUISITION_REASON_SHOP);
	replication.SendShopBuyResult(clientHd, npShop, 0, goodsID, (u32)quantity, itemIndex);
}

void HubGame::OnHonor(ClientHandle clientHd, const WideString& nickname)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	if(nickname == playerAccountData[userID]->nickname || !AccountExists(nickname.data())) {
		replication.SendHonorResult(clientHd, MEASURED_FRIEND_REQUEST_NOT_FOUND);
		return;
	}
	AccountProgress progress = AccountProgressLoad(nickname.data());
	progress.honors++;
	AccountProgressSave(nickname.data(), progress);
	LOG("[client%x] HubGame :: %ls honors %ls (%u)", clientHd, playerAccountData[userID]->nickname.data(), nickname.data(), progress.honors);
	replication.SendHonorResult(clientHd, 0);
}

void HubGame::OnPraise(ClientHandle clientHd, const wchar* nickname, i32 nicknameLen)
{
	const i32 userID = plidMap->Get(clientHd);
	const SortieUID resultSortieUID = playerMap[userID]->resultSortieUID;
	if(resultSortieUID == SortieUID::INVALID || nicknameLen <= 0) {
		WARN("[client%x] HubGame :: praise outside of a result screen", clientHd);
		return;
	}
	WideString target;
	target.assign(nickname, nicknameLen);
	PraiseCount* entry = nullptr;
	foreach(c, praiseCounts) if(c->sortieUID == resultSortieUID && c->nickname == target) entry = &*c;
	if(!entry) {
		if(praiseCounts.full()) praiseCounts.erase(praiseCounts.begin());
		praiseCounts.push_back({ resultSortieUID, target, 0 });
		entry = &praiseCounts.back();
	}
	entry->count++;
	replication.SendPraiseResult(clientHd, 0);
	foreach_const(p, playerList) {
		if(p->resultSortieUID != resultSortieUID) continue;
		replication.SendPraise(p->clientHd, target.data(), entry->count);
	}
}

void HubGame::OnCreateParty(ClientHandle clientHd, EntrySystemID entry, StageType stageType)
{
	const i32 userID = plidMap->Get(clientHd);
	if(!GetGameXmlContent().HasEntrySystem((i32)entry)) {
		WARN("[client%x] WARNING: unknown entrySysID (%d)", clientHd, (i32)entry);
		return;
	}
	pendingPartyEntry[userID] = entry;
	pendingPartyStage[userID] = stageType;
	pendingPartyGameType[userID] = GameType::PVP_NORMAL;
	pendingPartyStageIndex[userID] = StageIndex(0);

	// TODO: validate args
	const Account& acc = *playerAccountData[userID];

	if(Config().DevMode && Config().DevQuickConnect) {
		PacketWriter<Sv::SN_DoConnectGameServer> packet;
		packet.Write<u16>(12900); // port
		eastl::array<u8,4> ip = { 127, 0, 0, 1 };
		packet.Write(ip); // ip
		packet.Write<i32>(1); // gameID
		packet.Write<u32>(0x0); // idcHash
		packet.WriteStringObj(acc.nickname.data(), acc.nickname.size());
		packet.Write<u32>(In::ProduceInstantKey(acc.UID, SortieUID(1)));
		NT_LOG("[client%x] HubGame :: %s", clientHd, PacketSerialize<Sv::SN_DoConnectGameServer>(packet.data, packet.size));
		replication.server->SendPacket(clientHd, packet);
	}
	else {
		matchmaker->QueryPartyCreate(acc.nickname, acc.UID);
	}
}

bool HubGame::SetPartyStage(Party& party, StageIndex stage)
{
	AreaIndex areaID = AreaIndex(0);
	MapIndex mapID = MapIndex(0);
	const GameXmlContent& content = GetGameXmlContent();
	if(!content.FindEntryStageArea((i32)party.entry, stage, &areaID) || !content.FindStageMap(stage, &mapID)) return false;
	party.areaIndex = areaID;
	party.stageIndex = stage;
	party.mapIndex = mapID;
	return true;
}

void HubGame::OnPartyModify(ClientHandle clientHd, StageIndex stage, GameType gameType)
{
	const i32 userID = plidMap->Get(clientHd);
	if(gameType <= GameType::INVALID || gameType >= GameType::MAX) {
		WARN("[client%x] WARNING: invalid party gameType (%d)", clientHd, (i32)gameType);
		return;
	}
	pendingPartyGameType[userID] = gameType;
	if(stage != StageIndex(0)) pendingPartyStageIndex[userID] = stage;
	const PartyUID partyUID = playerMap[userID]->partyUID;
	if(partyUID == PartyUID::INVALID) return;
	auto f = partyMap.find(partyUID);
	if(f == partyMap.end()) return;
	if(f->second->memberList.front().accountUID != playerAccountData[userID]->UID) return;
	f->second->gameType = gameType;
	if(stage != StageIndex(0) && !SetPartyStage(*f->second, stage)) WARN("[client%x] WARNING: stage %d not available for entry %d", clientHd, (i32)stage, (i32)f->second->entry);
	const AccountFriends friends = AccountFriendsLoad(playerAccountData[userID]->nickname.data());
	foreach_const(n, friends.friends) {
		const i32 friendID = FindUserByNickname(*n);
		if(friendID < 0 || playerMap[friendID] == playerList.end()) continue;
		replication.SendFriendPartyCreation(playerMap[friendID]->clientHd, playerAccountData[userID]->nickname.data(), f->second->stageIndex, gameType);
	}
}

HubGame::Party* HubGame::FindPlayerParty(i32 userID)
{
	if(playerMap[userID] == playerList.end()) return nullptr;
	const PartyUID partyUID = playerMap[userID]->partyUID;
	if(partyUID == PartyUID::INVALID) return nullptr;
	auto f = partyMap.find(partyUID);
	if(f == partyMap.end()) return nullptr;
	return &*f->second;
}

i32 HubGame::FindUserByNickname(const WideString& nickname)
{
	for(i32 i = 0; i < MAX_PLAYERS; i++) {
		if(playerAccountData[i] && playerAccountData[i]->nickname == nickname) return i;
	}
	return -1;
}

void HubGame::OnPartyInvite(ClientHandle clientHd, const wchar* nickname, i32 nicknameLen)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	WideString target;
	target.assign(nickname, nicknameLen);
	Party* party = FindPlayerParty(userID);
	if(!party || party->memberList.front().accountUID != playerAccountData[userID]->UID) {
		replication.SendPartyInviteResult(clientHd, MEASURED_PARTY_INVITE_NOT_OWNER, target.data());
		return;
	}
	const i32 targetID = FindUserByNickname(target);
	if(targetID < 0 || targetID == userID) {
		replication.SendPartyInviteResult(clientHd, MEASURED_PARTY_INVITE_NOT_FOUND_USER, target.data());
		return;
	}
	const AccountUID targetUID = playerAccountData[targetID]->UID;
	foreach_const(m, party->memberList) {
		if(m->accountUID == targetUID) {
			replication.SendPartyInviteResult(clientHd, MEASURED_PARTY_INVITE_ALREADY_JOINED, target.data());
			return;
		}
	}
	const Party* targetParty = FindPlayerParty(targetID);
	if(playerMap[targetID]->sortieUID != SortieUID::INVALID || (targetParty && targetParty->memberList.size() > 1)) {
		replication.SendPartyInviteResult(clientHd, MEASURED_PARTY_INVITE_INVALID_USER_STATE, target.data());
		return;
	}
	if(eastl::find(party->invites.begin(), party->invites.end(), targetUID) == party->invites.end()) {
		if(party->invites.full()) party->invites.erase(party->invites.begin());
		party->invites.push_back(targetUID);
	}
	LOG("[client%x] HubGame :: %ls invites %ls to party %u", clientHd, playerAccountData[userID]->nickname.data(), target.data(), (u32)party->UID);
	replication.SendPartyInvite(playerMap[targetID]->clientHd, playerAccountData[userID]->nickname.data(), party->stageIndex, party->gameType, (i32)party->UID);
}

void HubGame::OnPartyInviteAccept(ClientHandle clientHd, i32 partyID)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const AccountUID accountUID = playerAccountData[userID]->UID;
	auto f = partyMap.find(PartyUID((u32)partyID));
	if(f == partyMap.end()) {
		WARN("[client%x] HubGame :: party %d not found", clientHd, partyID);
		return;
	}
	Party& party = *f->second;
	auto invite = eastl::find(party.invites.begin(), party.invites.end(), accountUID);
	if(invite == party.invites.end()) {
		WARN("[client%x] HubGame :: no invitation to party %d", clientHd, partyID);
		return;
	}
	party.invites.erase(invite);
	const GameXmlContent::StageMaps* stage = GetGameXmlContent().FindStageMaps(party.stageIndex);
	const i32 capacity = stage && stage->joinMemberMax > 0 ? stage->joinMemberMax : (i32)party.memberList.capacity();
	if((i32)party.memberList.size() >= capacity || party.memberList.full()) {
		WARN("[client%x] HubGame :: party %d is full", clientHd, partyID);
		return;
	}
	if(FindPlayerParty(userID)) LeaveParty(userID, true);

	Party::Member member;
	member.accountUID = accountUID;
	member.userId = 2;
	for(bool taken = true; taken; ) {
		taken = false;
		foreach_const(m, party.memberList) if(m->userId == member.userId) { taken = true; member.userId++; break; }
	}
	party.memberList.push_back(member);
	playerMap[userID]->partyUID = party.UID;
	matchmaker->QueryPartyAddMember(party.UID, playerAccountData[userID]->nickname, accountUID);
	BroadcastFriendState(userID, MEASURED_FRIEND_STATE_IN_PARTY);
	LOG("[client%x] HubGame :: %ls joins party %u as %u", clientHd, playerAccountData[userID]->nickname.data(), (u32)party.UID, member.userId);

	eastl::fixed_vector<HubReplication::PartyUser,5,false> users;
	foreach_const(m, party.memberList) {
		auto h = accountClientHandleMap.find(m->accountUID);
		if(h == accountClientHandleMap.end()) continue;
		const i32 memberID = plidMap->Get(h->second);
		users.push_back({ m->userId, playerAccountData[memberID]->nickname.data() });
	}
	replication.SendPartyJoinAnswer(clientHd, partyID, party.stageType, party.gameType, users.data(), (i32)users.size());

	CreatureIndex leaderCreature = CreatureIndex::Invalid;
	if(WorldHub::ActorPlayer* actor = world.FindPlayerActor(playerActorUID[userID])) leaderCreature = CreatureIndex(100000000 + (i32)actor->classType);
	u8 slot = 0;
	foreach_const(m, party.memberList) {
		if(m->accountUID == accountUID) break;
		slot++;
	}
	foreach_const(m, party.memberList) {
		if(m->accountUID == accountUID) continue;
		auto h = accountClientHandleMap.find(m->accountUID);
		if(h == accountClientHandleMap.end()) continue;
		replication.SendPartyJoin(h->second, member.userId, playerAccountData[userID]->nickname.data(), leaderCreature, m == party.memberList.begin(), party.gameType, slot);
	}
}

void HubGame::LeaveParty(i32 userID, bool notify)
{
	Party* party = FindPlayerParty(userID);
	playerMap[userID]->partyUID = PartyUID::INVALID;
	if(!party) return;
	const AccountUID accountUID = playerAccountData[userID]->UID;
	const bool leader = party->memberList.front().accountUID == accountUID;
	if(leader) {
		matchmaker->QueryPartyCancelMatching(party->UID);
		foreach_const(m, party->memberList) {
			if(m->accountUID == accountUID) continue;
			auto h = accountClientHandleMap.find(m->accountUID);
			if(h == accountClientHandleMap.end()) continue;
			playerMap[plidMap->Get(h->second)]->partyUID = PartyUID::INVALID;
			if(notify) {
				replication.SendPartyBreakup(h->second);
				BroadcastFriendState(plidMap->Get(h->second), MEASURED_FRIEND_STATE_ONLINE);
			}
		}
		foreach_const(inv, party->invites) {
			auto h = accountClientHandleMap.find(*inv);
			if(h != accountClientHandleMap.end() && notify) replication.SendPartyCancelInvite(h->second, (i32)party->UID);
		}
		const PartyUID partyUID = party->UID;
		auto f = partyMap.find(partyUID);
		partyList.erase(f->second);
		partyMap.erase(f);
		LOG("HubGame :: party %u broken up", (u32)partyUID);
		return;
	}
	u32 leftUserId = 0;
	for(auto m = party->memberList.begin(); m != party->memberList.end(); ++m) {
		if(m->accountUID != accountUID) continue;
		leftUserId = m->userId;
		party->memberList.erase(m);
		break;
	}
	matchmaker->QueryPartyRemoveMember(party->UID, accountUID);
	if(!notify) return;
	foreach_const(m, party->memberList) {
		auto h = accountClientHandleMap.find(m->accountUID);
		if(h == accountClientHandleMap.end()) continue;
		replication.SendPartyLeave(h->second, leftUserId, playerAccountData[userID]->nickname.data(), m == party->memberList.begin());
	}
}

void HubGame::OnPartyBreakUp(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);
	Party* party = FindPlayerParty(userID);
	const i32 remain = party ? (i32)party->memberList.size() : 1;
	const bool leader = party && party->memberList.front().accountUID == playerAccountData[userID]->UID;
	LeaveParty(userID, true);
	replication.SendPartyBreakupAnswer(clientHd, leader ? remain : remain - 1);
	BroadcastFriendState(userID, MEASURED_FRIEND_STATE_ONLINE);
}

void HubGame::OnPartyLeave(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);
	LeaveParty(userID, true);
	replication.SendPartyLeaveAnswer(clientHd);
	BroadcastFriendState(userID, MEASURED_FRIEND_STATE_ONLINE);
}

void HubGame::OnCancelMatching(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);
	Party* party = FindPlayerParty(userID);
	if(!party) return;
	matchmaker->QueryPartyCancelMatching(party->UID);
	const bool solo = party->memberList.size() == 1;
	foreach_const(m, party->memberList) {
		auto h = accountClientHandleMap.find(m->accountUID);
		if(h == accountClientHandleMap.end()) continue;
		replication.SendCancelMatchingQueue(h->second);
		BroadcastPlayerStateInTown(plidMap->Get(h->second), 0, nullptr, 0);
		BroadcastFriendState(plidMap->Get(h->second), MEASURED_FRIEND_STATE_ONLINE);
	}
	if(solo) {
		LeaveParty(userID, false);
		replication.SendPartyBreakup(clientHd);
	}
}

HubReplication::FriendEntry HubGame::MakeFriendEntry(const WideString& nickname, u16* levelStorage)
{
	HubReplication::FriendEntry entry;
	entry.nickname = nickname.data();
	entry.leaderCreature = 100000000 + (i32)DEFAULT_LEADER_CLASS;
	entry.state = MEASURED_FRIEND_STATE_OFFLINE;
	entry.favorite = 0;
	const i32 friendID = FindUserByNickname(nickname);
	if(friendID >= 0) {
		*levelStorage = playerAccountData[friendID]->level;
		if(WorldHub::ActorPlayer* actor = world.FindPlayerActor(playerActorUID[friendID])) entry.leaderCreature = 100000000 + (i32)actor->classType;
	}
	else *levelStorage = AccountProgressLoad(nickname.data()).level;
	entry.level = *levelStorage;
	return entry;
}

void HubGame::SendFriendsOnConnect(i32 userID)
{
	const Account& account = *playerAccountData[userID];
	const ClientHandle clientHd = playerMap[userID]->clientHd;
	const AccountFriends friends = AccountFriendsLoad(account.nickname.data());
	eastl::fixed_vector<HubReplication::FriendEntry,64,false> list;
	eastl::fixed_vector<u16,64,false> levels;
	levels.resize(friends.friends.size());
	for(i32 i = 0; i < (i32)friends.friends.size(); i++) {
		list.push_back(MakeFriendEntry(friends.friends[i], &levels[i]));
		list.back().favorite = eastl::find(friends.favorites.begin(), friends.favorites.end(), friends.friends[i]) != friends.favorites.end() ? 1 : 0;
	}
	replication.SendFriendList(clientHd, list.data(), (i32)list.size());
	replication.SendCommunityStatusMessage(clientHd, account.nickname.data(), friends.statusMessage.data());
	list.clear();
	eastl::fixed_vector<u16,32,false> requestLevels;
	requestLevels.resize(friends.requests.size());
	for(i32 i = 0; i < (i32)friends.requests.size(); i++) list.push_back(MakeFriendEntry(friends.requests[i], &requestLevels[i]));
	replication.SendFriendRequestList(clientHd, list.data(), (i32)list.size());
	foreach_const(f, friends.friends) {
		if(FindUserByNickname(*f) < 0) continue;
		replication.SendFriendStateChanged(clientHd, f->data(), MEASURED_FRIEND_STATE_ONLINE);
		replication.SendCommunityStatusMessage(clientHd, f->data(), AccountFriendsLoad(f->data()).statusMessage.data());
	}
	replication.SendBlockList(clientHd, friends.blocked.data(), (i32)friends.blocked.size());
	BroadcastFriendState(userID, MEASURED_FRIEND_STATE_ONLINE);
}

void HubGame::BroadcastFriendState(i32 userID, u8 state)
{
	const Account& account = *playerAccountData[userID];
	if(state != MEASURED_FRIEND_STATE_OFFLINE && playerMap[userID] != playerList.end()) replication.SendFriendStateChanged(playerMap[userID]->clientHd, account.nickname.data(), state);
	if(playerMap[userID] != playerList.end()) GuildBroadcastState(userID, state);
	const AccountFriends friends = AccountFriendsLoad(account.nickname.data());
	foreach_const(f, friends.friends) {
		const i32 friendID = FindUserByNickname(*f);
		if(friendID < 0 || friendID == userID || playerMap[friendID] == playerList.end()) continue;
		replication.SendFriendStateChanged(playerMap[friendID]->clientHd, account.nickname.data(), state);
	}
}

static bool ContainsName(const eastl::fixed_vector<WideString,64,true>& list, const WideString& name)
{
	return eastl::find(list.begin(), list.end(), name) != list.end();
}

static bool ContainsName(const eastl::fixed_vector<WideString,32,true>& list, const WideString& name)
{
	return eastl::find(list.begin(), list.end(), name) != list.end();
}

void HubGame::OnFriendRequest(ClientHandle clientHd, const WideString& nickname)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const WideString& self = playerAccountData[userID]->nickname;
	if(nickname == self) {
		replication.SendFriendRequestResult(clientHd, MEASURED_FRIEND_REQUEST_YOURSELF, nickname.data());
		return;
	}
	if(!AccountExists(nickname.data())) {
		replication.SendFriendRequestResult(clientHd, MEASURED_FRIEND_REQUEST_NOT_FOUND, nickname.data());
		return;
	}
	AccountFriends mine = AccountFriendsLoad(self.data());
	if(ContainsName(mine.friends, nickname)) {
		replication.SendFriendRequestResult(clientHd, MEASURED_FRIEND_REQUEST_ALREADY_FRIEND, nickname.data());
		return;
	}
	AccountFriends theirs = AccountFriendsLoad(nickname.data());
	if(ContainsName(theirs.blocked, self) || ContainsName(mine.blocked, nickname)) {
		replication.SendFriendRequestResult(clientHd, MEASURED_FRIEND_REQUEST_ALREADY_BLOCKED, nickname.data());
		return;
	}
	if(ContainsName(theirs.requests, self)) {
		replication.SendFriendRequestResult(clientHd, MEASURED_FRIEND_REQUEST_ALREADY_SENT, nickname.data());
		return;
	}
	if(theirs.requests.full()) {
		replication.SendFriendRequestResult(clientHd, MEASURED_FRIEND_REQUEST_OVERFLOW, nickname.data());
		return;
	}
	theirs.requests.push_back(self);
	AccountFriendsSave(nickname.data(), theirs);
	LOG("[client%x] HubGame :: %ls sends a friend request to %ls", clientHd, self.data(), nickname.data());
	replication.SendFriendRequestResult(clientHd, 0, nickname.data());
	const i32 targetID = FindUserByNickname(nickname);
	if(targetID >= 0 && playerMap[targetID] != playerList.end()) {
		u16 level = 0;
		const HubReplication::FriendEntry entry = MakeFriendEntry(self, &level);
		replication.SendFriendRequested(playerMap[targetID]->clientHd, self.data(), entry.leaderCreature);
	}
}

void HubGame::OnFriendRespond(ClientHandle clientHd, const WideString& nickname, bool accept)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const WideString& self = playerAccountData[userID]->nickname;
	AccountFriends mine = AccountFriendsLoad(self.data());
	auto request = eastl::find(mine.requests.begin(), mine.requests.end(), nickname);
	if(request == mine.requests.end()) {
		replication.SendFriendRespondResult(clientHd, MEASURED_FRIEND_RESPONSE_NOT_FOUND, nickname.data(), accept ? 1 : 0);
		return;
	}
	mine.requests.erase(request);
	AccountFriends theirs = AccountFriendsLoad(nickname.data());
	if(accept) {
		if(mine.friends.full()) {
			AccountFriendsSave(self.data(), mine);
			replication.SendFriendRespondResult(clientHd, MEASURED_FRIEND_RESPONSE_MY_LIST_IS_FULL, nickname.data(), 1);
			return;
		}
		if(theirs.friends.full()) {
			AccountFriendsSave(self.data(), mine);
			replication.SendFriendRespondResult(clientHd, MEASURED_FRIEND_RESPONSE_TARGET_LIST_IS_FULL, nickname.data(), 1);
			return;
		}
		if(!ContainsName(mine.friends, nickname)) mine.friends.push_back(nickname);
		if(!ContainsName(theirs.friends, self)) theirs.friends.push_back(self);
		theirs.requests.erase(eastl::remove(theirs.requests.begin(), theirs.requests.end(), self), theirs.requests.end());
		AccountFriendsSave(nickname.data(), theirs);
	}
	AccountFriendsSave(self.data(), mine);
	LOG("[client%x] HubGame :: %ls %s the friend request of %ls", clientHd, self.data(), accept ? "accepts" : "declines", nickname.data());
	replication.SendFriendRespondResult(clientHd, 0, nickname.data(), accept ? 1 : 0);
	if(!accept) return;
	u16 level = 0;
	HubReplication::FriendEntry other = MakeFriendEntry(nickname, &level);
	const i32 otherID = FindUserByNickname(nickname);
	if(otherID >= 0) other.state = MEASURED_FRIEND_STATE_ONLINE;
	replication.SendFriendAdded(clientHd, other);
	if(otherID >= 0 && playerMap[otherID] != playerList.end()) {
		u16 selfLevel = 0;
		HubReplication::FriendEntry me = MakeFriendEntry(self, &selfLevel);
		me.state = MEASURED_FRIEND_STATE_ONLINE;
		replication.SendFriendAdded(playerMap[otherID]->clientHd, me);
	}
}

void HubGame::OnFriendRemove(ClientHandle clientHd, const WideString& nickname)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const WideString& self = playerAccountData[userID]->nickname;
	AccountFriends mine = AccountFriendsLoad(self.data());
	if(!ContainsName(mine.friends, nickname)) {
		replication.SendFriendRemoveResult(clientHd, MEASURED_FRIEND_REMOVE_NOT_FOUND, nickname.data());
		return;
	}
	mine.friends.erase(eastl::remove(mine.friends.begin(), mine.friends.end(), nickname), mine.friends.end());
	AccountFriendsSave(self.data(), mine);
	AccountFriends theirs = AccountFriendsLoad(nickname.data());
	theirs.friends.erase(eastl::remove(theirs.friends.begin(), theirs.friends.end(), self), theirs.friends.end());
	AccountFriendsSave(nickname.data(), theirs);
	replication.SendFriendRemoveResult(clientHd, 0, nickname.data());
	const i32 otherID = FindUserByNickname(nickname);
	if(otherID >= 0 && playerMap[otherID] != playerList.end()) replication.SendFriendRemoved(playerMap[otherID]->clientHd, self.data());
}

void HubGame::OnFriendFavorite(ClientHandle clientHd, const WideString& nickname, bool favorite)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const WideString& self = playerAccountData[userID]->nickname;
	const u16 netID = favorite ? Sv::SA_FriendSetFavorite::NET_ID : Sv::SA_FriendUnsetFavorite::NET_ID;
	AccountFriends mine = AccountFriendsLoad(self.data());
	if(!ContainsName(mine.friends, nickname)) {
		replication.SendNicknameResult(clientHd, netID, MEASURED_FRIEND_FAVORITE_NOT_FOUND, nickname.data());
		return;
	}
	mine.favorites.erase(eastl::remove(mine.favorites.begin(), mine.favorites.end(), nickname), mine.favorites.end());
	if(favorite && !mine.favorites.full()) mine.favorites.push_back(nickname);
	AccountFriendsSave(self.data(), mine);
	replication.SendNicknameResult(clientHd, netID, 0, nickname.data());
}

void HubGame::OnBlock(ClientHandle clientHd, const WideString& nickname, bool block)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const WideString& self = playerAccountData[userID]->nickname;
	const u16 netID = block ? Sv::SA_CommunityBlockAdd::NET_ID : Sv::SA_CommunityBlockRemove::NET_ID;
	AccountFriends mine = AccountFriendsLoad(self.data());
	if(block && (nickname == self || !AccountExists(nickname.data()))) {
		replication.SendNicknameResult(clientHd, netID, MEASURED_FRIEND_REQUEST_NOT_FOUND, nickname.data());
		return;
	}
	mine.blocked.erase(eastl::remove(mine.blocked.begin(), mine.blocked.end(), nickname), mine.blocked.end());
	if(block && !mine.blocked.full()) mine.blocked.push_back(nickname);
	AccountFriendsSave(self.data(), mine);
	LOG("[client%x] HubGame :: %ls %s %ls", clientHd, self.data(), block ? "blocks" : "unblocks", nickname.data());
	replication.SendNicknameResult(clientHd, netID, 0, nickname.data());
}

void HubGame::OnStatusMessage(ClientHandle clientHd, const WideString& message)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const WideString& self = playerAccountData[userID]->nickname;
	AccountFriends mine = AccountFriendsLoad(self.data());
	mine.statusMessage = message;
	AccountFriendsSave(self.data(), mine);
	replication.SendCommunityStatusMessageResult(clientHd, 0);
	replication.SendCommunityStatusMessage(clientHd, self.data(), message.data());
	foreach_const(f, mine.friends) {
		const i32 friendID = FindUserByNickname(*f);
		if(friendID < 0 || playerMap[friendID] == playerList.end()) continue;
		replication.SendCommunityStatusMessage(playerMap[friendID]->clientHd, self.data(), message.data());
	}
}

void HubGame::OnEnqueueGame(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);

	const PartyUID partyUID = playerMap[userID]->partyUID;
	if(partyUID == PartyUID::INVALID) return;
	auto f = partyMap.find(partyUID);
	if(f == partyMap.end()) return;
	const Party& party = *f->second;
	if(party.memberList.front().accountUID != playerAccountData[userID]->UID) {
		WARN("[client%x] HubGame :: enqueue refused, not the party leader", clientHd);
		return;
	}
	ASSERT(party.areaIndex != AreaIndex(0));
	ASSERT(party.stageIndex != StageIndex(0));
	ASSERT(party.mapIndex != MapIndex(0));
	matchmaker->QueryPartyEnqueue(partyUID, party.areaIndex, party.stageIndex, party.mapIndex, party.gameType);
}

void HubGame::OnSortieRoomFound(ClientHandle clientHd, SortieUID sortieID)
{
	// This indicates the client has received the 'match found' packet
	// When waiting time is 0 this is like accepting the match

	const i32 userID = plidMap->Get(clientHd);
	matchmaker->QueryPlayerNotifyRoomFound(playerAccountData[userID]->UID, sortieID);
	if(Config().DevShowcase || Config().DevAutoQueue) {
		playerMap[userID]->sortieUID = sortieID;
		matchmaker->QueryPlayerRoomConfirm(playerAccountData[userID]->UID, sortieID, true);
	}
}

void HubGame::OnSortieRoomConfirm(ClientHandle clientHd, bool confirm)
{
	const i32 userID = plidMap->Get(clientHd);
	matchmaker->QueryPlayerRoomConfirm(playerAccountData[userID]->UID, playerMap[userID]->sortieUID, confirm);
	foreach_const(r, sortieRosters) {
		if(r->sortieUID != playerMap[userID]->sortieUID) continue;
		foreach_const(m, r->members) {
			if(m->accountUID == playerAccountData[userID]->UID) BroadcastSortieConfirm(*r, m->row, m->teamType, confirm);
		}
	}
}

void HubGame::BroadcastSortieConfirm(const SortieRoster& roster, u8 row, u32 teamType, bool confirm)
{
	foreach_const(m, roster.members) {
		if(m->bot) continue;
		auto h = accountClientHandleMap.find(m->accountUID);
		if(h != accountClientHandleMap.end()) replication.SendMatchingPartyConfirm(h->second, row, teamType, confirm);
	}
}

void HubGame::MmOnPartyCreated(PartyUID partyUID, AccountUID leader)
{
	// TODO: find and skip if not found
	const ClientHandle clientHd = accountClientHandleMap.at(leader);
	const i32 userID = plidMap->Get(clientHd);
	playerMap[userID]->partyUID = partyUID;

	// create party
	ASSERT(partyMap.find(partyUID) == partyMap.end());
	partyList.emplace_back(partyUID);
	Party& party = *(--partyList.end());
	Party::Member member;
	member.accountUID = leader;
	party.memberList.push_back(member);

	partyMap.emplace(partyUID, --partyList.end());
	party.entry = pendingPartyEntry[userID];
	party.stageType = pendingPartyStage[userID];
	party.gameType = pendingPartyGameType[userID];
	AreaIndex areaID = AreaIndex(0);
	StageIndex stageID = StageIndex(0);
	MapIndex mapID = MapIndex(0);
	ASSERT(GetGameXmlContent().FindQueueAreaStage((i32)party.entry, &areaID, &stageID));
	ASSERT(GetGameXmlContent().FindStageMap(stageID, &mapID));
	party.areaIndex = areaID;
	party.stageIndex = stageID;
	party.mapIndex = mapID;
	if(pendingPartyStageIndex[userID] != StageIndex(0)) SetPartyStage(party, pendingPartyStageIndex[userID]);

	replication.SendPartyCreateSucess(clientHd, UserID(1), StageType::PVP_GAME);
	BroadcastFriendState(userID, MEASURED_FRIEND_STATE_IN_CREATION);
	if(Config().DevShowcase || Config().DevAutoQueue) OnEnqueueGame(clientHd);
}

void HubGame::MmOnPartyEnqueued(PartyUID partyUID)
{
	auto f = partyMap.find(partyUID);
	if(f == partyMap.end()) return;
	Party& party = *f->second;
	foreach_const(m, party.memberList) {
		// TODO: check if on this hub
		auto h = accountClientHandleMap.find(m->accountUID);
		if(h == accountClientHandleMap.end()) continue;
		const ClientHandle clientHd = h->second;
		replication.SendPartyEnqueue(clientHd, party.stageIndex, m == party.memberList.begin());
		const GameXmlContent::StageMaps* stage = GetGameXmlContent().FindStageMaps(party.stageIndex);
		const u8 mode = stage && stage->gameMode != GameModeType::GAME_MODE_INVALID ? (u8)stage->gameMode : 0;
		BroadcastPlayerStateInTown(plidMap->Get(clientHd), 1, &mode, stage ? 1 : 0);
		BroadcastFriendState(plidMap->Get(clientHd), MEASURED_FRIEND_STATE_IN_MATCHING);
	}
}

void HubGame::MmOnMatchFound(const In::MN_MatchingPartyFound& matchingParty)
{
	auto f = partyMap.find(matchingParty.partyUID);
	if(f == partyMap.end()) return;
	Party& party = *f->second;
	eastl::fixed_vector<UserID,16> rowIDs;
	for(i32 i = 0; i < matchingParty.playerCount; i++) {
		rowIDs.push_back(UserID(i + 1));
	}
	SortieRoster* roster = nullptr;
	foreach(r, sortieRosters) if(r->sortieUID == matchingParty.sortieUID) roster = &*r;
	if(!roster) {
		if(sortieRosters.size() >= MAX_PLAYERS) sortieRosters.erase(sortieRosters.begin());
		sortieRosters.push_back();
		roster = &sortieRosters.back();
		roster->sortieUID = matchingParty.sortieUID;
		const bool pve = matchingParty.gameType == GameType::PVE_NORMAL || matchingParty.gameType == GameType::PVE_RANDOM || matchingParty.gameType == GameType::PVE_TICKET;
		for(i32 i = 0; i < matchingParty.playerCount && !roster->members.full(); i++) {
			const auto& p = matchingParty.playerList[i];
			roster->members.push_back({ p.accountUID, (u8)rowIDs[i], pve || p.team > 1 ? 0u : (u32)TeamType::RED + p.team, p.isBot != 0 });
		}
	}
	foreach_const(m, party.memberList) {
		auto h = accountClientHandleMap.find(m->accountUID);
		if(h == accountClientHandleMap.end()) continue;
		const ClientHandle clientHd = h->second;
		const i32 userID = plidMap->Get(clientHd);
		if(!playerAccountData[userID]) continue;
		playerMap[userID]->sortieUID = matchingParty.sortieUID;

		u8 myTeam = 0;
		for(i32 i = 0; i < matchingParty.playerCount; i++) {
			if(matchingParty.playerList[i].accountUID == m->accountUID) myTeam = matchingParty.playerList[i].team;
		}
		replication.SendMatchingPartyFound(clientHd, matchingParty, party.stageIndex, rowIDs, myTeam);
		foreach_const(b, roster->members) if(b->bot) replication.SendMatchingPartyConfirm(clientHd, b->row, b->teamType, true);
		BroadcastPlayerStateInTown(userID, 0, nullptr, 0);
		BroadcastFriendState(userID, MEASURED_FRIEND_STATE_IN_SORTIE);
	}
}

void HubGame::BroadcastPlayerStateInTown(i32 userID, i8 state, const u8* modes, i32 modeCount)
{
	const ActorUID actorUID = playerActorUID[userID];
	if(actorUID == ActorUID::INVALID) return;
	foreach_const(p, playerList) {
		if(p->clientHd == ClientHandle::INVALID) continue;
		replication.SendPlayerStateInTown(p->clientHd, actorUID, state, modes, modeCount);
	}
}

bool HubGame::ParseChatCommand(ClientHandle clientHd, const wchar* msg, const i32 len)
{
	if(!Config().DevMode) return false; // don't allow command when dev mode is not enabled

	const i32 userID = plidMap->Get(clientHd);

	static ActorUID lastLegoActorUID = ActorUID::INVALID;

	if(msg[0] == L'!') {
		msg++;

		if(EA::StdC::Strncmp(msg, L"lego", 4) == 0) {
			WorldHub::ActorCore* playerActor = world.FindPlayerActor(playerActorUID[userID]);
			ASSERT(playerActor);

			WorldHub::ActorCore& actor = world.SpawnPlayerActor(-1, (ClassType)18, SkinIndex::DEFAULT, L"legomage15", L"MEME");
			actor.pos = playerActor->pos;
			actor.dir = playerActor->dir;
			actor.eye = playerActor->eye;

			// trigger second emote
			actor.actionState = ActionStateID::EMOTION_BEHAVIORSTATE;
			actor.actionParam1 = 2;

			lastLegoActorUID = actor.UID;

			SendDbgMsg(clientHd, LFMT(L"Actor spawned at (%g, %g, %g)", actor.pos.x, actor.pos.y, actor.pos.z));
			return true;
		}

		if (EA::StdC::Strncmp(msg, L"spawn ", 6) == 0) {
			msg += 6; // skip command
			int classTypeVal = _wtoi(msg); // convert wide string to integer

			WorldHub::ActorCore* playerActor = world.FindPlayerActor(playerActorUID[userID]);
			ASSERT(playerActor);

			WorldHub::ActorCore& actor = world.SpawnPlayerActor(-1, (ClassType)classTypeVal, SkinIndex::DEFAULT, L"spawned", L"TEST");
			actor.pos = playerActor->pos;
			actor.dir = playerActor->dir;
			actor.eye = playerActor->eye;

			lastLegoActorUID = actor.UID;

			SendDbgMsg(clientHd, LFMT(L"Actor spawned with class %d at (%g, %g, %g)",
				classTypeVal, actor.pos.x, actor.pos.y, actor.pos.z));
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"delete", 6) == 0) {
			world.DestroyPlayerActor(lastLegoActorUID);

			SendDbgMsg(clientHd, LFMT(L"Actor destroyed (%u)", lastLegoActorUID));
			return true;
		}

		if (EA::StdC::Strncmp(msg, L"change ", 7) == 0) {
			msg += 7; // skip command

			// Parse class type and optional skin
			int classTypeVal;
			int skinIndex = (int)SkinIndex::DEFAULT;

			// Check if we have two arguments (class and skin)
			const wchar* space = wcschr(msg, L' ');
			if (space) {
				// Temporarily null terminate first number
				wchar firstNum[32];
				size_t len = space - msg;
				if (len < sizeof(firstNum)) {
					wcsncpy(firstNum, msg, len);
					firstNum[len] = L'\0';
					classTypeVal = _wtoi(firstNum);
					skinIndex = _wtoi(space + 1); // Convert second number after space
				}
			}
			else {
				classTypeVal = _wtoi(msg); // Just class type provided
			}

			WorldHub::ActorPlayer* playerActor = world.FindPlayerActor(playerActorUID[userID]);
			ASSERT(playerActor);

			vec3 oldPos = playerActor->pos;
			vec3 oldDir = playerActor->dir;
			vec3 oldEye = playerActor->eye;

			// Destroy the current player actor
			world.DestroyPlayerActor(playerActorUID[userID]);

			// Create new player actor properly
			ASSERT(playerAccountData[userID]); // account data should exist
			WorldHub::ActorPlayer& actor = world.SpawnPlayerActor(
				userID,
				(ClassType)classTypeVal,
				(SkinIndex)skinIndex,
				playerAccountData[userID]->nickname.data(),
				playerAccountData[userID]->guildTag.data()
			);

			// Set position and orientation
			actor.pos = oldPos;
			actor.dir = oldDir;
			actor.eye = oldEye;

			// Store the new actor UID
			playerActorUID[userID] = actor.UID;

			// Notify clients about the new character
			replication.SendPlayerSetLeaderMaster(clientHd, actor.UID, (ClassType)classTypeVal, (SkinIndex)skinIndex);

			SendDbgMsg(clientHd, LFMT(L"Character changed to class %d with skin %d", classTypeVal, skinIndex));
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"rozark", 6) == 0) {
			WorldHub::ActorCore* playerActor = world.FindPlayerActor(playerActorUID[userID]);
			ASSERT(playerActor);

			WorldHub::ActorCore& actor = world.SpawnPlayerActor(-1, (ClassType)5001200, SkinIndex::DEFAULT, L"rozark", L"MEME");
			actor.pos = playerActor->pos;
			actor.dir = playerActor->dir;
			actor.eye = playerActor->eye;
			lastLegoActorUID = actor.UID;

			SendDbgMsg(clientHd, LFMT(L"Actor spawned at (%g, %g, %g)", actor.pos.x, actor.pos.y, actor.pos.z));
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"tanian", 6) == 0) {
			WorldHub::ActorCore* playerActor = world.FindPlayerActor(playerActorUID[userID]);
			ASSERT(playerActor);

			WorldHub::ActorCore& actor = world.SpawnPlayerActor(-1, (ClassType)5001040, SkinIndex::DEFAULT, L"rozark", L"MEME");
			actor.pos = playerActor->pos;
			actor.dir = playerActor->dir;
			actor.eye = playerActor->eye;
			lastLegoActorUID = actor.UID;

			SendDbgMsg(clientHd, LFMT(L"Actor spawned at (%g, %g, %g)", actor.pos.x, actor.pos.y, actor.pos.z));
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"fish", 4) == 0) {
			WorldHub::ActorCore* playerActor = world.FindPlayerActor(playerActorUID[userID]);
			ASSERT(playerActor);

			WorldHub::ActorCore& actor = world.SpawnPlayerActor(-1, (ClassType)5000800, SkinIndex::DEFAULT, L"rozark", L"MEME");
			actor.pos = playerActor->pos;
			actor.dir = playerActor->dir;
			actor.eye = playerActor->eye;
			lastLegoActorUID = actor.UID;

			SendDbgMsg(clientHd, LFMT(L"Actor spawned at (%g, %g, %g)", actor.pos.x, actor.pos.y, actor.pos.z));
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"smoke", 5) == 0) {
			WorldHub::ActorPlayer* playerActor = world.FindPlayerActor(playerActorUID[userID]);
			ASSERT(playerActor);

			SendDbgMsg(clientHd, LFMT(L"All you have to do was follow the damn train %s :(", playerActor->name.data()));
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"upsidedown", 10) == 0) {
			JukeboxQueueSong(userID, SongID::UpsideDown);
			return true;
		}
		
		if(EA::StdC::Strncmp(msg, L"scml", 4) == 0) {
			JukeboxQueueSong(userID, SongID::Scml);
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"poharan", 7) == 0) {
			JukeboxQueueSong(userID, SongID::Poharan);
			return true;
		}

		if(EA::StdC::Strncmp(msg, L"triangle", 8) == 0) {
			JukeboxQueueSong(userID, SongID::Triangle);
			return true;
		}
		
		if(EA::StdC::Strncmp(msg, L"arami", 5) == 0) {
			JukeboxQueueSong(userID, SongID::Arami);
			return true;
		}

	}

	return false;
}

void HubGame::SendDbgMsg(ClientHandle clientHd, const wchar* msg)
{
	replication.SendChatMessageToClient(clientHd, L"System", 1, msg);
}

void HubGame::SpawnNPC(CreatureIndex docID, i32 localID, const vec3& pos, const vec3& dir)
{
	WorldHub::ActorCore& actor = world.SpawnNpcActor(docID, localID);
	actor.pos = pos;
	actor.dir = dir;
}

static ErrorType MailErrorType(MailWriteResult r)
{
	switch(r) {
		case MailWriteResult::SUCCESS: return ErrorType::SUCCESS;
		case MailWriteResult::EMPTY_NICKNAME: return ErrorType::MAIL_EMPTY_NICKNAME;
		case MailWriteResult::EMPTY_SUBJECT: return ErrorType::MAIL_EMPTY_SUBJECT;
		case MailWriteResult::EMPTY_CONTENTS: return ErrorType::MAIL_EMPTY_CONTENTS;
		case MailWriteResult::EXCESS_SUBJECT: return ErrorType::MAIL_EXCESS_SUBJECT;
		case MailWriteResult::EXCESS_CONTENTS: return ErrorType::MAIL_EXCESS_CONTENTS;
		case MailWriteResult::UNKNOWN_RECIPIENT: return ErrorType::MAIL_WRITE_INVAILID_NICKNAME;
		case MailWriteResult::RECIPIENT_FULL: return ErrorType::MAIL_SEND_ARCHIVE_FAIL_ARCHIVE_FULL;
	}
	ASSERT(0);
	return ErrorType::SUCCESS;
}

void HubGame::OnMailShowList(ClientHandle clientHd, u8 mailbox)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	replication.SendMailList(clientHd, playerAccountData[userID]->nickname.data(), mailbox);
}

void HubGame::OnMissionReward(ClientHandle clientHd, i32 missionID)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	auto found = GetAccountManager().accountMap.find(playerAccountData[userID]->UID);
	ASSERT(found != GetAccountManager().accountMap.end());
	Account& account = *found->second;
	AccountMissions saved = AccountMissionsLoad(account.nickname.data());
	ActiveMissionList list;
	MissionBuildActive(account.nickname.data(), account.level, (i64)time(nullptr), saved, &list);
	ActiveMission* mission = MissionFind(&list, missionID);
	if(!mission || !mission->cleared || mission->rewarded) {
		WARN("[client%x] HubGame :: mission %d reward refused", clientHd, missionID);
		if(mission) replication.SendMissionReward(clientHd, (u32)ErrorType::RANDOM_MISSION_ALREADY_COMPLETED, *mission);
		return;
	}

	AccountProgress progress = AccountProgressLoad(account.nickname.data());
	const GameXmlContent::Mission& data = *mission->mission;
	bool levelUp = false;
	if(data.rewardType == GameXmlContent::Mission::RewardType::GOLD) progress.gold += data.rewardAmount;
	else if(data.rewardType == GameXmlContent::Mission::RewardType::EXP) levelUp = ProgressGainExp(&progress, (u32)data.rewardAmount);
	else if(data.rewardType == GameXmlContent::Mission::RewardType::CCOIN) progress.ccoin += data.rewardAmount;
	else if(data.rewardType != GameXmlContent::Mission::RewardType::ITEM) WARN("[client%x] HubGame :: mission %d reward type %d not granted", clientHd, missionID, (i32)data.rewardType);
	AccountProgressSave(account.nickname.data(), progress);
	account.level = progress.level;
	account.levelExp = progress.levelExp;
	account.gold = progress.gold;
	account.ccoin = progress.ccoin;
	if(data.rewardType == GameXmlContent::Mission::RewardType::ITEM && !GrantItem(userID, data.rewardItem, (i32)data.rewardAmount, MEASURED_ITEM_UPDATE_REASON_MISSION, MEASURED_ITEM_ACQUISITION_REASON_MISSION)) {
		WARN("[client%x] HubGame :: mission %d item %d could not be granted", clientHd, missionID, data.rewardItem);
	}
	if(data.rewardType == GameXmlContent::Mission::RewardType::CCOIN) replication.SendCCoin(clientHd, account.ccoin, MEASURED_MISSION_REWARD_MONEY_REASON);

	mission->rewarded = true;
	MissionStore(list, &saved);
	AccountMissionsSave(account.nickname.data(), saved);
	LOG("[client%x] HubGame :: %ls mission %d reward %lld (type %d)", clientHd, account.nickname.data(), missionID, data.rewardAmount, (i32)data.rewardType);

	if(data.rewardType == GameXmlContent::Mission::RewardType::GOLD) replication.SendMoney(clientHd, account.gold, MEASURED_MISSION_REWARD_MONEY_REASON);
	if(data.rewardType == GameXmlContent::Mission::RewardType::EXP) {
		replication.SendExpGain(clientHd, playerActorUID[userID], account.nickname.data(), levelUp, account.level, account.levelExp, (u32)data.rewardAmount);
		if(levelUp) replication.SendMoney(clientHd, account.gold, MEASURED_MISSION_REWARD_MONEY_REASON);
	}
	replication.SendMissionReward(clientHd, 0, *mission);
}

void HubGame::OnDailyMissionChange(ClientHandle clientHd, i32 step)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const Account& account = *playerAccountData[userID];
	AccountMissions saved = AccountMissionsLoad(account.nickname.data());
	ActiveMission changed;
	if(!MissionChangeDaily(account.nickname.data(), account.level, (i64)time(nullptr), step, &saved, &changed)) {
		WARN("[client%x] HubGame :: daily mission change refused (step %d)", clientHd, step);
		return;
	}
	AccountMissionsSave(account.nickname.data(), saved);
	LOG("[client%x] HubGame :: %ls daily step %d -> mission %d (change %u)", clientHd, account.nickname.data(), step, changed.mission->ID, (u32)changed.changedCount);
	replication.SendDailyMissionChange(clientHd, 0, step, changed);
}

void HubGame::OnMailRead(ClientHandle clientHd, u64 mailID)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	MailMessage mail;
	if(!MailRead(playerAccountData[userID]->nickname.data(), mailID, &mail)) {
		WARN("[client%x] HubGame :: mail %llu not found", clientHd, mailID);
		return;
	}
	replication.SendMailRead(clientHd, mail);
	replication.SendMailUnreadNotice(clientHd, playerAccountData[userID]->nickname.data());
}

void HubGame::OnMailWrite(ClientHandle clientHd, const wchar* to, const wchar* subject, i32 subjectLen, const wchar* content, i32 contentLen)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	MailMessage sent;
	const MailWriteResult r = MailSend(playerAccountData[userID]->nickname.data(), to, subject, subjectLen, content, contentLen, &sent);
	LOG("[client%x] HubGame :: mail from %ls to %ls result %d", clientHd, playerAccountData[userID]->nickname.data(), to, (i32)r);
	replication.SendMailWriteResult(clientHd, MailErrorType(r));
	if(r != MailWriteResult::SUCCESS) return;
	for(i32 i = 0; i < (i32)playerAccountData.size(); i++) {
		if(!playerAccountData[i] || playerAccountData[i]->nickname.compare(to) != 0) continue;
		replication.SendMailNewInbox(playerMap[i]->clientHd, sent);
		replication.SendMailUnreadNotice(playerMap[i]->clientHd, to);
	}
}

void HubGame::OnMailMove(ClientHandle clientHd, u8 moveType, const u64* ids, i32 count)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	const i32 moved = MailMove(playerAccountData[userID]->nickname.data(), moveType, ids, count);
	LOG("[client%x] HubGame :: mail move type %u: %d of %d", clientHd, moveType, moved, count);
	replication.SendMailMoveResult(clientHd, ErrorType::SUCCESS);
	replication.SendMailUnreadNotice(clientHd, playerAccountData[userID]->nickname.data());
}
