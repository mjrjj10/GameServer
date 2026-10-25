#include "game.h"
#include "coordinator.h" // account data
#include "instance.h"
#include <mxm/game_content.h>
#include "config.h"
#include "mailbox.h"
#include "mission_progress.h"
#include "mission_rules.h"
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

void HubGame::OnPartyModify(ClientHandle clientHd, GameType gameType)
{
	const i32 userID = plidMap->Get(clientHd);
	if(gameType <= GameType::INVALID || gameType >= GameType::MAX) {
		WARN("[client%x] WARNING: invalid party gameType (%d)", clientHd, (i32)gameType);
		return;
	}
	pendingPartyGameType[userID] = gameType;
	const PartyUID partyUID = playerMap[userID]->partyUID;
	if(partyUID == PartyUID::INVALID) return;
	auto f = partyMap.find(partyUID);
	if(f == partyMap.end()) return;
	f->second->gameType = gameType;
}

void HubGame::OnEnqueueGame(ClientHandle clientHd)
{
	const i32 userID = plidMap->Get(clientHd);

	const PartyUID partyUID = playerMap[userID]->partyUID;
	if(partyUID == PartyUID::INVALID) return;
	auto f = partyMap.find(partyUID);
	if(f == partyMap.end()) return;
	const Party& party = *f->second;
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

	replication.SendPartyCreateSucess(clientHd, UserID(userID + 1), StageType::PVP_GAME);
	if(Config().DevShowcase || Config().DevAutoQueue) OnEnqueueGame(clientHd);
}

void HubGame::MmOnPartyEnqueued(PartyUID partyUID)
{
	auto f = partyMap.find(partyUID);
	if(f == partyMap.end()) return;
	Party& party = *f->second;
	foreach_const(m, party.memberList) {
		// TODO: check if on this hub
		const ClientHandle clientHd = accountClientHandleMap.at(m->accountUID);
		replication.SendPartyEnqueue(clientHd, party.stageIndex);
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
	else WARN("[client%x] HubGame :: mission %d reward type %d not granted", clientHd, missionID, (i32)data.rewardType);
	AccountProgressSave(account.nickname.data(), progress);
	account.level = progress.level;
	account.levelExp = progress.levelExp;
	account.gold = progress.gold;

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
