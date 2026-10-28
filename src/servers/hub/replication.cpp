#include "replication.h"

#include <common/protocol.h>
#include <common/packet_serialize.h>
#include <EASTL/algorithm.h>
#include <EASTL/fixed_hash_map.h>
#include <EAStdC/EAString.h>

#include "config.h"
#include "coordinator.h" // AccountData
#include <mxm/game_content.h>
#include <EASTL/sort.h>
#include <time.h>
#include "mission_rules.h"
#include "town_rules.h"
#include "character_base_data.h"
#include <mxm/account_progress.h>

static const i32 MEASURED_JUKEBOX_HOT_TRACKS[] = { 7770015, 7770005, 7770002 };

template<typename Writer>
static void WriteInventoryItem(Writer& packet, const AccountItem& item)
{
	packet.template Write<u32>(item.uid);
	packet.template Write<u8>(item.invenType);
	packet.template Write<i32>(item.slot);
	packet.template Write<i32>(item.docIndex);
	packet.template Write<i32>(item.count);
	packet.template Write<i32>(-1);
	packet.template Write<u8>(0);
	packet.template Write<i64>(0);
	packet.template Write<u16>(0);
}

#include "mission_progress.h"
#include "mailbox.h"
#include "guild_store.h"


void HubReplication::Frame::Clear()
{
	playerList.clear();
	npcList.clear();
	playerMap.clear();
	npcMap.clear();
	actorUIDSet.clear();
	actorType.clear();
	transformMap.clear();
	actionStateMap.clear();
}

void HubReplication::PlayerLocalInfo::Reset()
{
	localActorIDMap.clear();
	actorUIDSet.clear();
	nextPlayerLocalActorID = LocalActorID::FIRST_OTHER_PLAYER;
	nextNpcLocalActorID = LocalActorID::FIRST_NPC;
	nextMonsterLocalActorID = LocalActorID::INVALID;
	isFirstLoad = true;
}

void HubReplication::Init(Server* server_)
{
	server = server_;
	memset(&playerState, 0, sizeof(playerState));

	framePrev = &frames[0];
	frameCur = &frames[1];
}

void HubReplication::FrameEnd()
{
	ProfileFunction();

	UpdatePlayersLocalState();

	FrameDifference();

	// send SN_ScanEnd if requested
	for(int clientID = 0; clientID < MAX_CLIENTS; clientID++) {
		if(playerState[clientID] != PlayerState::IN_GAME) continue;

		if(playerLocalInfo[clientID].isFirstLoad) {
			playerLocalInfo[clientID].isFirstLoad = false;

			SendInitialFrame(playerClientHd[clientID]);
		}
	}

	eastl::swap(frameCur, framePrev);
	frameCur->Clear(); // clear frame
}

void HubReplication::FramePushPlayerActor(const ActorPlayer& actor)
{
	ASSERT(frameCur->playerMap.find(actor.actorUID) == frameCur->playerMap.end());
	ASSERT(frameCur->actorUIDSet.find(actor.actorUID) == frameCur->actorUIDSet.end());

	frameCur->playerList.emplace_back(actor);
	frameCur->playerMap.emplace(actor.actorUID, --frameCur->playerList.end());
	frameCur->actorUIDSet.insert(actor.actorUID);
	frameCur->actorType.emplace(actor.actorUID, actor.Type());

	Frame::Transform tf;
	tf.pos = actor.pos;
	tf.dir = actor.dir;
	tf.eye = actor.eye;
	tf.rotate = actor.rotate;
	tf.speed = actor.speed;
	frameCur->transformMap.emplace(actor.actorUID, tf);

	Frame::ActionState at;
	at.actionState = actor.actionState;
	at.actionParam1 = actor.actionParam1;
	at.actionParam2 = actor.actionParam2;
	at.rotate = actor.rotate;
	at.upperRotate = actor.upperRotate;
	frameCur->actionStateMap.emplace(actor.actorUID, at);
}

void HubReplication::FramePushNpcActor(const HubReplication::ActorNpc& actor)
{
	ASSERT(frameCur->playerMap.find(actor.actorUID) == frameCur->playerMap.end());
	ASSERT(frameCur->actorUIDSet.find(actor.actorUID) == frameCur->actorUIDSet.end());

	frameCur->npcList.emplace_back(actor);
	frameCur->npcMap.emplace(actor.actorUID, --frameCur->npcList.end());
	frameCur->actorUIDSet.insert(actor.actorUID);
	frameCur->actorType.emplace(actor.actorUID, actor.Type());

	Frame::Transform tf;
	tf.pos = actor.pos;
	tf.dir = actor.dir;
	tf.eye = vec3(0, 0, 0);
	tf.rotate = 0;
	tf.speed = 0;
	frameCur->transformMap.emplace(actor.actorUID, tf);

	/*
	Frame::ActionState at;
	at.actionState = actor.actionState;
	at.actionParam1 = actor.actionParam1;
	at.actionParam2 = actor.actionParam2;
	at.rotate = actor.rotate;
	at.upperRotate = actor.upperRotate;
	frameCur->actionStateMap.emplace(actor.actorUID, at);
	*/
}

void HubReplication::FramePushJukebox(const HubReplication::ActorJukebox& actor)
{
	ASSERT(frameCur->actorUIDSet.find(actor.actorUID) == frameCur->actorUIDSet.end());

	frameCur->jukebox = actor;

	frameCur->actorUIDSet.insert(actor.actorUID);
	frameCur->actorType.emplace(actor.actorUID, actor.Type());
}

void HubReplication::OnPlayerConnect(ClientHandle clientHd)
{
	const i32 clientID = plidMap->Get(clientHd);

	playerClientHd[clientID] = clientHd;
	playerAccount[clientID] = nullptr;
	playerState[clientID] = PlayerState::CONNECTED;
	playerLocalInfo[clientID].Reset();
}

void HubReplication::SetPlayerAccount(ClientHandle clientHd, const Account* account)
{
	playerAccount[plidMap->Get(clientHd)] = account;
}

void HubReplication::SendLoadLobby(ClientHandle clientHd, MapIndex stageIndex, bool joinCity)
{
	// SN_LoadCharacterStart
	SendPacketData<Sv::SN_LoadCharacterStart>(clientHd, 0, nullptr);

	// SN_SetGameGvt
	{
		Sv::SN_SetGameGvt gameGvt;
		// Possible loss of information here? Not sure what the time format should be
		u32 time = (u32)TimeDiffMs(TimeRelNow());
		gameGvt.sendTime = time;
		gameGvt.virtualTime = time;
		SendPacket(clientHd, gameGvt);
	}

	// SN_SummaryInfoAll
	{
		PacketWriter<Sv::SN_SummaryInfoAll> packet;

		packet.Write<u16>(0);

		SendPacket(clientHd, packet);
	}

	// SN_AvailableSummaryRewardCountList
	{
		PacketWriter<Sv::SN_AvailableSummaryRewardCountList> packet;

		packet.Write<u16>(8);

		const i32 rewardCount[8] = {
			220004100,
			2,
			220002200,
			3,
			220005100,
			3,
			220003300,
			2
		};

		packet.WriteRaw(rewardCount, sizeof(rewardCount));

		SendPacket(clientHd, packet);
	}


	// SN_SummaryInfoLatest
	{
		PacketWriter<Sv::SN_SummaryInfoLatest> packet;

		packet.Write<u16>(10);

		const Sv::SN_SummaryInfoLatest::Info infoList[10] = {
			{ 220002200, StageIndex(200002201), 3, 220002201, 0, (i64)0xa6605c00 },
			{ 220002200, StageIndex(200002201), 3, 220002204, 0, (i64)0xa6605c00 },
			{ 220002200, StageIndex(200002201), 3, 220002205, 0, (i64)0xa6605c00 },
			{ 220005100, StageIndex(200005101), 3, 220005101, 0, (i64)0x6a2bfc00 },
			{ 220005100, StageIndex(200005101), 3, 220005102, 0, (i64)0x6a2bfc00 },
			{ 220005100, StageIndex(200005101), 3, 220005105, 0, (i64)0x6a2bfc00 },
			{ 220003300, StageIndex(200003301), 3, 220003301, 0, (i64)0xe0c7fa00 },
			{ 220003300, StageIndex(200003301), 3, 220003305, 0, (i64)0xe0c7fa00 },
			{ 220004100, StageIndex(200004101), 3, 220004101, 0, (i64)0x9eea8400 },
			{ 220004100, StageIndex(200004101), 3, 220004105, 0, (i64)0x9eea8400 },
		};

		packet.WriteRaw(infoList, sizeof(infoList));

		SendPacket(clientHd, packet);
	}

	// SN_AchieveInfo
	{
		PacketWriter<Sv::SN_AchieveInfo> packet;

		packet.Write<u8>(1); // packetNum
		packet.Write<i32>(800); // achievementScore
		packet.Write<u16>(0); // achList_count

		SendPacket(clientHd, packet);
	}

	// SN_AchieveLatest
	{
		PacketWriter<Sv::SN_AchieveLatest> packet;

		packet.Write<u16>(0); // achList_count

		SendPacket(clientHd, packet);
	}

	if(joinCity) SendJoinCity(clientHd);

	// SN_SetGameGvt
	{
		Sv::SN_SetGameGvt gameGvt;
		gameGvt.sendTime = 0;
		gameGvt.virtualTime = 0;
		SendPacket(clientHd, gameGvt);
	}
}

void HubReplication::SendJoinCity(ClientHandle clientHd)
{
	Sv::SN_CityMapInfo cityMapInfo;
	cityMapInfo.CityMapID = (MapIndex)Config().LobbyMap;
	SendPacket(clientHd, cityMapInfo);

	SendPacketData<Sv::SQ_CityLobbyJoinCity>(clientHd, 0, nullptr);
}

void HubReplication::SendResultScreenStart(ClientHandle clientHd)
{
	Sv::SN_EnterChannel channel;
	channel.chID = MEASURED_RESULT_CHANNEL_ID;
	channel.channelType = MEASURED_RESULT_CHANNEL_TYPE;
	SendPacket(clientHd, channel);

	Sv::SN_PvpAvailableReportCount report;
	report.isTroll = 0;
	report.remainingCount = MEASURED_PVP_REPORT_COUNT;
	SendPacket(clientHd, report);

	Sv::SN_PraiseStart praise;
	praise.remainDurationMs = MEASURED_PRAISE_DURATION_MS;
	SendPacket(clientHd, praise);

	Sv::SN_RestartPvpEntryStart restart;
	restart.voteTimeMS = MEASURED_RESTART_VOTE_MS;
	SendPacket(clientHd, restart);
}

void HubReplication::SendUserReturnToCity(ClientHandle clientHd, const wchar* nickname)
{
	PacketWriter<Sv::SN_UserReturnToCity> packet;
	packet.WriteStringObj(nickname);
	SendPacket(clientHd, packet);

	PacketWriter<Sv::SN_RestartPvpEntryUpdate> update;
	update.WriteStringObj(nickname);
	update.Write<u8>(0);
	SendPacket(clientHd, update);
}

void HubReplication::SendReturnToCity(ClientHandle clientHd)
{
	Sv::SA_ReturnToCity answer;
	answer.errcode = 0;
	SendPacket(clientHd, answer);
	SendJoinCity(clientHd);
}

void HubReplication::SendShopBuyResult(ClientHandle clientHd, bool npShop, u32 result, u32 goodsID, u32 quantity, i32 itemIndex)
{
	if(!npShop) {
		Sv::SA_BuyCshopItem answer;
		answer.result = result;
		answer.itemIndex = (u32)itemIndex;
		SendPacket(clientHd, answer);
		return;
	}
	Sv::SA_NpBuyCshopItem packet;
	packet.result = result;
	packet.goodsId = goodsID;
	packet.quantity = quantity;
	SendPacket(clientHd, packet);
}

void HubReplication::SendInventoryExpand(ClientHandle clientHd, u32 result, i32 tab, i32 lineCount)
{
	Sv::SA_InventoryExpand packet;
	packet.result = result;
	packet.invenTab = (u32)tab;
	packet.lineCount = (u32)lineCount;
	SendPacket(clientHd, packet);
}

void HubReplication::SendCCoin(ClientHandle clientHd, i64 amount, u32 reason)
{
	Sv::SN_CCoin packet;
	packet.amount = amount;
	packet.nReason = reason;
	SendPacket(clientHd, packet);
}

void HubReplication::SendItemUpdate(ClientHandle clientHd, const AccountItem& item, u32 reason)
{
	PacketWriter<Sv::SN_ItemUpdate> packet;
	packet.Write<u16>(1);
	WriteInventoryItem(packet, item);
	packet.Write<u32>(reason);
	SendPacket(clientHd, packet);
}

void HubReplication::SendItemAcquisition(ClientHandle clientHd, u32 reason, const AccountItem& item, i32 amount)
{
	PacketWriter<Sv::SN_ItemAcquisition> packet;
	packet.Write<u32>(reason);
	packet.Write<u32>(item.uid);
	packet.Write<i32>(item.docIndex);
	packet.Write<i32>(amount);
	SendPacket(clientHd, packet);
}

void HubReplication::SendHonorResult(ClientHandle clientHd, u32 result)
{
	Sv::SA_Honor answer;
	answer.result = result;
	SendPacket(clientHd, answer);
}

void HubReplication::SendPraiseResult(ClientHandle clientHd, u32 errorType)
{
	Sv::SA_Praise answer;
	answer.errorType = errorType;
	SendPacket(clientHd, answer);
}

void HubReplication::SendPraise(ClientHandle clientHd, const wchar* nickname, u32 count)
{
	PacketWriter<Sv::SN_Praise> packet;
	packet.WriteStringObj(nickname);
	packet.Write<u32>(count);
	SendPacket(clientHd, packet);
}

void HubReplication::SetPlayerAsInGame(ClientHandle clientHd)
{
	const i32 clientID = plidMap->Get(clientHd);
	playerState[clientID] = PlayerState::IN_GAME;
}

void HubReplication::SendCharacterInfo(ClientHandle clientHd, ActorUID actorUID, CreatureIndex docID, ClassType classType, i32 health, i32 healthMax)
{
	const i32 clientID = plidMap->Get(clientHd);

	if(playerState[clientID] != PlayerState::IN_GAME) {
		LOG("WARNING(EventPlayerRequestCharacterInfo): player not in game (clientID=%d, state=%d)", clientID, (i32)playerState[clientID]);
		return;
	}

	// SA_GetCharacterInfo
	Sv::SA_GetCharacterInfo info;
	info.characterID = GetLocalActorID(clientHd, actorUID);
	info.docIndex = docID;
	info.classType = classType;
	info.hp = health;
	info.maxHp = healthMax;
	SendPacket(clientHd, info);
}

void HubReplication::SendPlayerSetLeaderMaster(ClientHandle clientHd, ActorUID masterActorUID, ClassType classType, SkinIndex skinIndex)
{
	LocalActorID laiLeader = (LocalActorID)((u32)LocalActorID::FIRST_SELF_MASTER + (i32)classType);
	ASSERT(laiLeader >= LocalActorID::FIRST_SELF_MASTER && laiLeader < LocalActorID::LAST_SELF_MASTER);

	PlayerForceLocalActorID(clientHd, masterActorUID, laiLeader);

	const i32 clientID = plidMap->Get(clientHd);
	if(playerState[clientID] < PlayerState::IN_GAME) {
		// SN_LeaderCharacter
		Sv::SN_LeaderCharacter leader;
		leader.leaderID = laiLeader;
		leader.skinIndex = skinIndex;
		SendPacket(clientHd, leader);
	}
	else {
		// NOTE: only seems to close the master window
		// SA_LeaderCharacter
		Sv::SA_SetLeader leader;
		leader.result = 0;
		leader.leaderID = laiLeader;
		leader.skinIndex = skinIndex;
		SendPacket(clientHd, leader);
	}
}

void HubReplication::SendChatMessageToAll(const wchar* senderName, i32 chatType, const wchar* msg, i32 msgLen)
{
	// TODO: restrict message length

	PacketWriter<Sv::SN_ChatChannelMessage> packet;

	packet.Write<i32>(chatType); // chatType
	packet.WriteStringObj(senderName);
	packet.Write<u8>(0); // senderStaffType
	packet.WriteStringObj(msg, msgLen);

	for(int clientID= 0; clientID < MAX_CLIENTS; clientID++) {
		if(playerState[clientID] != PlayerState::IN_GAME) continue;

		SendPacket(playerClientHd[clientID], packet);
	}
}

void HubReplication::SendChatMessageToClient(ClientHandle toClientHd, const wchar* senderName, i32 chatType, const wchar* msg, i32 msgLen)
{
	const i32 clientID = plidMap->Get(toClientHd);
	if(playerState[clientID] != PlayerState::IN_GAME) return;

	if(msgLen == -1) msgLen = EA::StdC::Strlen(msg);

	PacketWriter<Sv::SN_ChatChannelMessage> packet;

	packet.Write<i32>(chatType); // chatType
	packet.WriteStringObj(senderName);
	packet.Write<u8>(0); // senderStaffType
	packet.WriteStringObj(msg, msgLen);

	SendPacket(toClientHd, packet);
}

void HubReplication::SendChatWhisperConfirmToClient(ClientHandle senderClientHd, const wchar* destNick, const wchar* msg, ErrorType retval)
{
	PacketWriter<Sv::SA_WhisperSend> packet;

	packet.Write<ErrorType>(retval);
	packet.WriteStringObj(destNick);
	packet.WriteStringObj(msg);

	SendPacket(senderClientHd, packet);
}

void HubReplication::SendChatWhisperToClient(ClientHandle destClientHd, const wchar* senderName, const wchar* msg)
{
	PacketWriter<Sv::SN_WhisperReceived> packet;

	packet.WriteStringObj(senderName); // senderNick
	packet.Write<u8>(0); // staffType
	packet.WriteStringObj(msg); // msg

	SendPacket(destClientHd, packet);
}

void HubReplication::SendProfileCharactersWeapons(ClientHandle clientHd)
{
	const GameXmlContent& content = GetGameXmlContent();
	// SN_ProfileCharacters
	{
		PacketWriter<Sv::SN_ProfileCharacters,2048> packet;

		packet.Write<u16>(content.masters.size()); // charaList_count

		foreach(it, content.masters) {
			Sv::SN_ProfileCharacters::Character chara;
			chara.characterID = (LocalActorID)((u32)LocalActorID::FIRST_SELF_MASTER + (i32)it->classType);
			chara.creatureIndex = it->ID;
			chara.skillSlot1 = it->skillIDs[0];
			chara.skillSlot2 = it->skillIDs[1];
			chara.classType = it->classType;
			chara.x = 0;
			chara.y = 0;
			chara.z = 0;
			chara.characterType = 1;
			chara.skinIndex = SkinIndex::DEFAULT;
			chara.weaponIndex = it->defaultWeaponIDs[0];
			chara.masterGearNo = 1;
			packet.Write(chara);
		}

		SendPacket(clientHd, packet);
	}

	// SN_ProfileWeapons
	{
		PacketWriter<Sv::SN_ProfileWeapons,4096> packet;

		u16 weaponCount = content.masters.size() * 3;
		packet.Write<u16>(weaponCount); // weaponList_count

		foreach_const(it, content.masters) {
			const GameXmlContent::Master& master = *it;

			Sv::SN_ProfileWeapons::Weapon weapon;
			weapon.characterID = (LocalActorID)((u32)LocalActorID::FIRST_SELF_MASTER + (i32)master.classType);

			for(int wi = 0; wi < 3; wi++) {
				weapon.weaponType = wi+1;
				// FIXME: because we load weapons from WEAPON.xml instead of CREATURE_CHARACTER.xml
				// we don't get the default weapons. This can be fixed later when we have a real unlock system.
				// - LordSk (10/10/2021)
				weapon.weaponIndex = master.weaponIDs[1 + wi*4];
				weapon.grade = 0;
				weapon.isUnlocked = wi == 0;
				weapon.isActivated = wi == 0;
				packet.Write(weapon);
			}
		}

		SendPacket(clientHd, packet);
	}

}

void HubReplication::SendProfileSkillsSkins(ClientHandle clientHd)
{
	const GameXmlContent& content = GetGameXmlContent();
	// SN_ProfileSkills
	{
		PacketWriter<Sv::SN_ProfileSkills,8192> packet;

		packet.Write<u8>(1); // packetNum

		i32 skillCount = 0;
		foreach_const(it, content.masters) {
			foreach_const(skill, it->skillIDs) {
				skillCount++;
			}
		}
		packet.Write<u16>(skillCount); // skills_count

		foreach_const(it, content.masters) {
			const LocalActorID characterID = (LocalActorID)((u32)LocalActorID::FIRST_SELF_MASTER + (i32)it->classType);
			for(int si = 0; si < (int)it->skillIDs.size(); si++) {
				packet.Write(characterID);
				packet.Write(it->skillIDs[si]);
				const u8 unlocked = (si < (int)it->skillUnlocked.size()) ? it->skillUnlocked[si] : (u8)1;
				packet.Write<u8>(unlocked);
				packet.Write<u8>(1);
				packet.Write<u16>(0);
			}
		}

		SendPacket(clientHd, packet);
	}

	// SN_ProfileTitles
	{
		PacketWriter<Sv::SN_ProfileTitles> packet;

		packet.Write<u16>(1); // titles_count
		packet.Write<i32>(320080004); // titles[0]

		SendPacket(clientHd, packet);
	}

	// SN_ProfileCharacterSkinList
	{
		PacketWriter<Sv::SN_ProfileCharacterSkinList,4096> packet;

		i32 skinCount = 0;
		foreach(it, content.masters) {
			skinCount += it->skinIDs.size();
		}

		packet.Write<u16>(skinCount); // skins_count

		foreach(it, content.masters) {
			const ClassType classType = it->classType;

			foreach(s, it->skinIDs) {
				packet.Write<ClassType>(classType); // classType
				packet.Write<SkinIndex>(*s); // skinIndex
				packet.Write<i32>(0); // bufCount
				packet.Write<i64>(0); // expireDateTime
			}
		}

		SendPacket(clientHd, packet);
	}

}

void HubReplication::SendAccountDataLobby(ClientHandle clientHd, const Account& account)
{
	// SN_RegionServicePolicy
	{
		PacketWriter<Sv::SN_RegionServicePolicy> packet;

		packet.Write<u16>(1); // newMasterRestrict_count
		packet.Write<u8>(1); // newMasterRestrict[0]

		packet.Write<u16>(1); // userGradePolicy_count
		packet.Write<u8>(5); // userGradePolicy[0].userGrade
		packet.Write<u16>(1); // userGradePolicy[0].benefits_count
		packet.Write<u8>(9); // userGradePolicy[0].benefits[0]

		packet.Write<u8>(2); // purchaseCCoinMethod
		packet.Write<u8>(1); // exchangeCCoinForGoldMethod
		packet.Write<u8>(0); // rewardCCoinMethod
		packet.Write<u8>(1); // pveRewardSlotOpenBuyChanceMethod

		packet.Write<u16>((u16)Config().regionBanMaster.size());
		for(auto id : Config().regionBanMaster) packet.Write<i32>(id);

		packet.Write<u16>((u16)Config().regionNewMaster.size());
		for(auto id : Config().regionNewMaster) packet.Write<i32>(id);

		packet.Write<u16>((u16)Config().eventBanMaster.size());
		for(auto id : Config().eventBanMaster) packet.Write<i32>(id);

		packet.Write<i32>(MEASURED_CHAT_FLOOD_CHECK_PERIOD_SEC);
		packet.Write<i32>(MEASURED_CHAT_FLOOD_MAX_TALK_COUNT);
		packet.Write<i32>(MEASURED_CHAT_FLOOD_BLOCK_PERIOD_SEC);

		packet.Write<u16>(0); // regionBanSkinList_count
		packet.Write<u16>(0); // pcCafeSkinList_count

		packet.Write<u8>(0); // useFatigueSystem

		SendPacket(clientHd, packet);
	}

	{
		const i32 total = (i32)ARRAY_COUNT(MEASURED_CHARACTER_BASES);
		const i32 packets = (total + MEASURED_CHARACTER_BASE_PER_PACKET - 1) / MEASURED_CHARACTER_BASE_PER_PACKET;
		for(i32 p = 0; p < packets; p++) {
			PacketWriter<Sv::SN_AllCharacterBaseData,8192> packet;
			const i32 first = p * MEASURED_CHARACTER_BASE_PER_PACKET;
			const i32 count = MIN(MEASURED_CHARACTER_BASE_PER_PACKET, total - first);
			packet.Write<u16>((u16)count);
			for(i32 i = first; i < first + count; i++) {
				const MeasuredCharacterBase& chara = MEASURED_CHARACTER_BASES[i];
				packet.Write<i32>(chara.docIndex);
				packet.Write<u16>(chara.statCount);
				for(i32 k = 0; k < chara.statCount; k++) {
					const MeasuredBaseStat& stat = MEASURED_CHARACTER_BASE_STATS[chara.statOffset + k];
					packet.Write<u8>(stat.type);
					packet.Write<f32>(stat.value);
				}
				packet.Write<u16>(chara.skillCount);
				for(i32 k = 0; k < chara.skillCount; k++) {
					const MeasuredSkillRatio& skill = MEASURED_CHARACTER_BASE_SKILLS[chara.skillOffset + k];
					packet.Write<i32>(skill.skillID);
					packet.Write<f32>(skill.baseDamage);
					packet.Write<f32>(skill.attackMultiplier);
					packet.Write<f32>(skill.healMultiplier);
					packet.Write<f32>(skill.shieldMultiplier);
					packet.Write<f32>(skill.dotMultiplier);
				}
			}
			packet.Write<i32>(p + 1);
			packet.Write<i32>(packets);
			SendPacket(clientHd, packet);
		}
	}

	const GameXmlContent& content = GetGameXmlContent();

	SendProfileCharactersWeapons(clientHd);

	GuildStore& guildStore = GetGuildStore();
	std::lock_guard<std::recursive_mutex> guildLock(guildStore.mutex);
	const GuildRecord* guild = guildStore.FindByMember(account.nickname);
	if(guild) {
		PacketWriter<Sv::SN_MyGuild> packet;
		packet.WriteStringObj(guild->tag.data());
		packet.Write<i64>(guild->dissolution);
		packet.Write<u8>(0);
		SendPacket(clientHd, packet);
	}

	// SN_ProfileMasterGears
	{
		PacketWriter<Sv::SN_ProfileMasterGears> packet;

		packet.Write<u16>(0); // masterGears_count

		SendPacket(clientHd, packet);
	}

	// SN_ProfileItems
	{
		const AccountInventory inventory = AccountInventoryLoad(account.nickname.data());
		PacketWriter<Sv::SN_ProfileItems,8192> packet;
		packet.Write<u8>(1);
		packet.Write<u16>((u16)inventory.items.size());
		foreach_const(it, inventory.items) WriteInventoryItem(packet, *it);
		SendPacket(clientHd, packet);
	}

	SendProfileSkillsSkins(clientHd);

	// SN_AccountInfo
	{
		PacketWriter<Sv::SN_AccountInfo> packet;

		packet.WriteStringObj(account.nickname.data()); // nick
		const AccountProgress progress = AccountProgressLoad(account.nickname.data());
		packet.Write<i32>(progress.inventoryLines[0]);
		packet.Write<i32>(progress.inventoryLines[1]);
		packet.Write<i32>(progress.inventoryLines[2]);
#if 0
		packet.Write<i32>(320080005);
		packet.Write<i32>(320080005);
#else
		packet.Write<i32>(-1);
		packet.Write<i32>(-1);
#endif
		packet.Write<i32>(1); // warehouseLineCount
		packet.Write<i32>(-1); // tutorialState
		packet.Write<i32>(3600); // masterGearDurability
		packet.Write<u8>(0); // badgeType

		SendPacket(clientHd, packet);
	}

	// SN_AccountExtraInfo
	{
		PacketWriter<Sv::SN_AccountExtraInfo> packet;

		// membership
		Sv::SN_AccountExtraInfo::UserGrade grade = {
			5,
			1,
			CurrentFiletimeTimestampUTC() + (1*24*3600*10000000ull),
			0,
			0,
			0
		};

		packet.WriteVec(&grade, 1);
		packet.Write<i32>(0); // activityPoint
		packet.Write<u8>(0); // activityRewaredState

		SendPacket(clientHd, packet);
	}

	// SN_AccountEquipmentList
	{
		PacketWriter<Sv::SN_AccountEquipmentList> packet;

		packet.Write<i32>(-1);

		SendPacket(clientHd, packet);
	}

	{
		const AccountStages stages = AccountStagesLoad(account.nickname.data());
		PacketWriter<Sv::SN_LoadClearedStages,4096> packet;
		packet.Write<u16>((u16)stages.cleared.size());
		foreach_const(s, stages.cleared) packet.Write<i32>(*s);
		SendPacket(clientHd, packet);
	}

	// SN_Unknown_62472
	{
		PacketWriter<Sv::SN_Unknown_62472> packet;

		packet.Write<u8>(1);

		SendPacket(clientHd, packet);
	}

	if(guild) {
		PacketWriter<Sv::SN_GuildChannelEnter> packet;
		packet.WriteStringObj(guild->name.data());
		packet.WriteStringObj(account.nickname.data());
		packet.Write<u8>(0);
		SendPacket(clientHd, packet);
	}

	// SN_PveComradeInfo
	{
		PacketWriter<Sv::SN_PveComradeInfo> packet;

		packet.Write<i32>(5); // availableComradeCount
		packet.Write<i32>(5); // maxComradeCount

		SendPacket(clientHd, packet);
	}

	// SN_AchieveUpdate
	{
		PacketWriter<Sv::SN_AchieveUpdate> packet;

		packet.Write<i32>(800); // achievementScore
		packet.Write<i32>(300190005); // index
		packet.Write<i32>(1); // type
		packet.Write<u8>(0); // isCleared
		packet.Write<u16>(0); // achievedList_count
		packet.Write<i64>(6); // progressInt64
		packet.Write<i64>(6); // date

		SendPacket(clientHd, packet);
	}

	MailRegisterAccount(account.nickname.data());
	SendMailUnreadNotice(clientHd, account.nickname.data());

	// SN_WarehouseItems
	{
		PacketWriter<Sv::SN_WarehouseItems> packet;

		packet.Write<u16>(0); // items_count

		SendPacket(clientHd, packet);
	}

	// SN_MutualFriendList
	{
		PacketWriter<Sv::SN_MutualFriendList> packet;

		packet.Write<u16>(0); // candidates_count

		SendPacket(clientHd, packet);
	}


	// SN_Money
	Sv::SN_Money money;
	money.nMoney = account.gold;
	money.nReason = 1;
	SendPacket(clientHd, money);
	SendCCoin(clientHd, account.ccoin, MEASURED_CCOIN_REASON_LOGIN);

}

void HubReplication::SendUpdateEntrySystem(ClientHandle clientHd)
{
	// SN_UpdateEntrySystem
	{
		PacketWriter<Sv::SN_UpdateEntrySystem,2048> packet;

		packet.Write<u16>(7); // entrySystemListCount

		{
			packet.Write<u32>(210036011); // entrySystemIndex

			// areaList
			const Sv::SN_UpdateEntrySystem::Area areaList[] = {
				{ 2, AreaIndex(190009205) }
			};
			packet.WriteVec(areaList, ARRAY_COUNT(areaList));

			// stageList
			packet.Write<u16>(1);
			packet.Write<u8>(2); // areaKey
			packet.Write<i32>(200101330); // stageIndex
			const u8 gametypes[] = { 4, 6 };
			packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
		}
		{
			packet.Write<u32>(210036010); // entrySystemIndex

			// areaList
			const Sv::SN_UpdateEntrySystem::Area areaList[] = {
				{ 3, AreaIndex(190009204) }
			};
			packet.WriteVec(areaList, ARRAY_COUNT(areaList));

			// stageList
			packet.Write<u16>(1);
			packet.Write<u8>(3); // areaKey
			packet.Write<i32>(200101320); // stageIndex
			const u8 gametypes[] = { 4, 6 };
			packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
		}
		{
			packet.Write<u32>(210037002); // entrySystemIndex

			// areaList
			const Sv::SN_UpdateEntrySystem::Area areaList[] = {
				{ 4, AreaIndex(190004000) }
			};
			packet.WriteVec(areaList, ARRAY_COUNT(areaList));

			// stageList
			packet.Write<u16>(1);
			packet.Write<u8>(4); // areaKey
			packet.Write<i32>(200000100); // stageIndex
			const u8 gametypes[] = { 1 };
			packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
		}
		{
			packet.Write<u32>(210037000); // entrySystemIndex

			// areaList
			const Sv::SN_UpdateEntrySystem::Area areaList[] = {
				{ 5, AreaIndex(190000006) },
				{ 6, AreaIndex(190000007) },
				{ 7, AreaIndex(190000008) },
				{ 8, AreaIndex(190000009) },
				{ 9, AreaIndex(190000010) },
				{ 10, AreaIndex(190000011) },
			};
			packet.WriteVec(areaList, ARRAY_COUNT(areaList));

			// stageList
			packet.Write<u16>(12);
			{
				packet.Write<u8>(5); // areaKey
				packet.Write<i32>(200007101); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(5); // areaKey
				packet.Write<i32>(200007103); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(6); // areaKey
				packet.Write<i32>(200007201); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(6); // areaKey
				packet.Write<i32>(200007203); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(7); // areaKey
				packet.Write<i32>(200007301); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(7); // areaKey
				packet.Write<i32>(200007303); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(8); // areaKey
				packet.Write<i32>(200007401); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(8); // areaKey
				packet.Write<i32>(200007403); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(9); // areaKey
				packet.Write<i32>(200007501); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(9); // areaKey
				packet.Write<i32>(200007503); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(10); // areaKey
				packet.Write<i32>(200007601); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(10); // areaKey
				packet.Write<i32>(200007603); // stageIndex
				const u8 gametypes[] = { 1 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
		}
		{
			packet.Write<u32>(210037006); // entrySystemIndex

			// areaList
			const Sv::SN_UpdateEntrySystem::Area areaList[] = {
				{ 11, AreaIndex(190001000) }
			};
			packet.WriteVec(areaList, ARRAY_COUNT(areaList));

			// stageList
			packet.Write<u16>(1);
			packet.Write<u8>(11); // areaKey
			packet.Write<i32>(200011109); // stageIndex
			const u8 gametypes[] = { 1 };
			packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
		}
		{
			packet.Write<u32>(210036812); // entrySystemIndex

			// areaList
			const Sv::SN_UpdateEntrySystem::Area areaList[] = {
				{ 1, AreaIndex(190002101) }
			};
			packet.WriteVec(areaList, ARRAY_COUNT(areaList));

			// stageList
			// stageList
			packet.Write<u16>(3);
			{
				packet.Write<u8>(1); // areaKey
				packet.Write<i32>(200020102); // stageIndex
				const u8 gametypes[] = { 4, 6 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(1); // areaKey
				packet.Write<i32>(200101000); // stageIndex
				const u8 gametypes[] = { 4, 6, 5 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
			{
				packet.Write<u8>(1); // areaKey
				packet.Write<i32>(200006112); // stageIndex
				const u8 gametypes[] = { 4 };
				packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
			}
		}
		{
			packet.Write<u32>(210037003); // entrySystemIndex

			// areaList
			const Sv::SN_UpdateEntrySystem::Area areaList[] = {
				{ 18, AreaIndex(190002200) }
			};
			packet.WriteVec(areaList, ARRAY_COUNT(areaList));

			// stageList
			packet.Write<u16>(1);
			packet.Write<u8>(18); // areaKey
			packet.Write<i32>(200006203); // stageIndex
			const u8 gametypes[] = { 7 };
			packet.WriteVec(gametypes, ARRAY_COUNT(gametypes));
		}


		SendPacket(clientHd, packet);
	}
}

template<typename Writer>
static void WriteMailEx(Writer& packet, const MailMessage& m)
{
	packet.Write<i64>((i64)m.ID);
	packet.Write<i64>(m.sendFiletime);
	packet.Write<i64>(m.expireFiletime);
	packet.Write<u8>(m.read);
	packet.Write<u8>(0);
	packet.Write<u8>(0);
	packet.WriteStringObj(m.from.data());
	packet.WriteStringObj(m.subject.data());
	packet.Write<u16>(0);
}

void HubReplication::SendMailUnreadNotice(ClientHandle clientHd, const wchar* owner)
{
	const MailCounts counts = MailGetCounts(owner);
	PacketWriter<Sv::SN_MailUnreadNotice> packet;
	for(i32 i = 0; i < 3; i++) packet.Write<u16>(counts.unread[i]);
	for(i32 i = 0; i < 3; i++) packet.Write<u16>(counts.total[i]);
	packet.Write<u16>(0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendMailList(ClientHandle clientHd, const wchar* owner, u8 mailbox)
{
	eastl::vector<MailMessage> mails;
	MailList(owner, mailbox, &mails);
	PacketWriter<Sv::SN_MailList,32768> packet;
	packet.Write<u8>(0xFF);
	packet.Write<u8>(mailbox);
	packet.Write<u16>((u16)mails.size());
	foreach_const(m, mails) WriteMailEx(packet, *m);
	SendPacket(clientHd, packet);
}

void HubReplication::SendMailNewInbox(ClientHandle clientHd, const MailMessage& mail)
{
	PacketWriter<Sv::SN_MailListNewInbox,2048> packet;
	packet.Write<u8>(mail.mailbox);
	packet.Write<u16>(1);
	WriteMailEx(packet, mail);
	SendPacket(clientHd, packet);
}

void HubReplication::SendMailRead(ClientHandle clientHd, const MailMessage& mail)
{
	PacketWriter<Sv::SN_MailRead,2048> packet;
	packet.Write<i64>((i64)mail.ID);
	packet.Write<u8>(mail.read);
	packet.Write<u8>(0);
	packet.WriteStringObj(mail.from.data());
	packet.WriteStringObj(mail.subject.data());
	packet.WriteStringObj(mail.content.data());
	packet.Write<i64>(mail.sendFiletime);
	packet.Write<i64>(mail.expireFiletime);
	packet.Write<i64>(0);
	packet.Write<i64>(0);
	packet.Write<i64>(0);
	packet.Write<u32>(0);
	packet.Write<u16>(0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendMailWriteResult(ClientHandle clientHd, ErrorType result)
{
	Sv::SN_MailWriteResult packet;
	packet.nErrorType = (u32)result;
	SendPacket(clientHd, packet);
}

void HubReplication::SendMailMoveResult(ClientHandle clientHd, ErrorType result)
{
	Sv::SN_MailMoveResult packet;
	packet.nErrorType = (u32)result;
	SendPacket(clientHd, packet);
}

template<typename Writer>
static void WriteMissionInfo(Writer& packet, const ActiveMission& m)
{
	packet.template Write<u32>((u32)m.mission->ID);
	packet.template Write<i64>(m.progress);
	packet.template Write<u16>(m.changedCount);
	packet.template Write<u8>(m.cleared ? 1 : 0);
	packet.template Write<u8>(m.rewarded ? 1 : 0);
}

void HubReplication::SendMissionList(ClientHandle clientHd, const Account& account)
{
	const i64 now = (i64)time(nullptr);
	const AccountMissions saved = AccountMissionsLoad(account.nickname.data());
	ActiveMissionList list;
	const MissionTimers timers = MissionBuildActive(account.nickname.data(), account.level, now, saved, &list);

	PacketWriter<Sv::SN_MissionList,2048> packet;
	packet.Write<u16>((u16)list.size());
	foreach_const(m, list) WriteMissionInfo(packet, *m);
	packet.Write<u32>((u32)(timers.dailyRemainSec * 1000));
	packet.Write<u32>((u32)(timers.weeklyRemainSec * 1000));
	packet.Write<u32>((u32)(timers.monthlyRemainSec * 1000));
	SendPacket(clientHd, packet);
}

void HubReplication::SendMissionReward(ClientHandle clientHd, u32 result, const ActiveMission& mission)
{
	PacketWriter<Sv::SA_RequestMissionReward> packet;
	packet.Write<u32>(result);
	WriteMissionInfo(packet, mission);
	SendPacket(clientHd, packet);
}

void HubReplication::SendDailyMissionChange(ClientHandle clientHd, u32 result, i32 step, const ActiveMission& mission)
{
	PacketWriter<Sv::SA_DailyMissionChange> packet;
	packet.Write<u32>(result);
	packet.Write<u32>((u32)step);
	WriteMissionInfo(packet, mission);
	SendPacket(clientHd, packet);
}

void HubReplication::SendMoney(ClientHandle clientHd, i64 gold, i32 reason)
{
	Sv::SN_Money money;
	money.nMoney = gold;
	money.nReason = reason;
	SendPacket(clientHd, money);
}

void HubReplication::SendExpGain(ClientHandle clientHd, ActorUID actorUID, const wchar* name, bool levelUp, u16 level, u32 levelExp, u32 gained)
{
	Sv::SN_ExpGain gain;
	gain.amount = gained;
	SendPacket(clientHd, gain);

	PacketWriter<Sv::SN_Exp> packet;
	packet.Write<LocalActorID>(GetLocalActorID(clientHd, actorUID));
	packet.WriteStringObj(name);
	packet.Write<u8>(levelUp ? 1 : 0);
	packet.Write<u16>(level);
	packet.Write<u32>(levelExp);
	SendPacket(clientHd, packet);
}

void HubReplication::SendGameReady(ClientHandle clientHd)
{
	Sv::SA_GameReady ready;
	ready.waitingTimeMS = 3000;
	ready.serverTimestamp = (i64)TimeDiffMs(TimeRelNow());
	ready.readyElapsedMS = 0;
	SendPacket(clientHd, ready);

	Sv::SN_NotifyIngameSkillPoint notify;
	notify.userId = UserID(1);
	notify.skillPoint = 1;
	SendPacket(clientHd, notify);

	Sv::SN_NotifyTimestamp notifyTimestamp;
	notifyTimestamp.serverTimestamp = (i64)TimeDiffMs(TimeRelNow());
	notifyTimestamp.curCount = 4;
	notifyTimestamp.maxCount = 5;
	SendPacket(clientHd, notifyTimestamp);

	/*
	// Sv::SN_RunClientLevelEventSeq
	{
		Sv::SN_RunClientLevelEventSeq seq;
		seq.needCompleteTriggerAckID = -1;
		seq.rootEventID = 218;
		seq.caller = 0;
		seq.serverTime = (i64)TimeDiffMs(TimeRelNow());
		LOG("[client%03d] Server :: SN_RunClientLevelEventSeq", clientID);
		SendPacket(clientHd, seq);
	}
	// Sv::SN_RunClientLevelEventSeq
	{
		Sv::SN_RunClientLevelEventSeq seq;
		seq.needCompleteTriggerAckID = -1;
		seq.rootEventID = 219;
		seq.caller = 0;
		seq.serverTime = (i64)TimeDiffMs(TimeRelNow());
		LOG("[client%03d] Server :: SN_RunClientLevelEventSeq", clientID);
		SendPacket(clientHd, seq);
	}
	// Sv::SN_RunClientLevelEventSeq
	{
		Sv::SN_RunClientLevelEventSeq seq;
		seq.needCompleteTriggerAckID = -1;
		seq.rootEventID = 274;
		seq.caller = 0;
		seq.serverTime = (i64)TimeDiffMs(TimeRelNow());
		LOG("[client%03d] Server :: SN_RunClientLevelEventSeq", clientID);
		SendPacket(clientHd, seq);
	}
	// Sv::SN_RunClientLevelEvent
	{
		Sv::SN_RunClientLevelEvent event;
		event.eventID = 48;
		event.caller = 0;
		event.serverTime = (i64)TimeDiffMs(TimeRelNow());
		LOG("[client%03d] Server :: SN_RunClientLevelEvent", clientID);
		SendPacket(clientHd, event);
	}
	// Sv::SN_RunClientLevelEventSeq
	{
		Sv::SN_RunClientLevelEventSeq seq;
		seq.needCompleteTriggerAckID = 1000000;
		seq.rootEventID = 1000001;
		seq.caller = 21035;
		seq.serverTime = (i64)TimeDiffMs(TimeRelNow());
		LOG("[client%03d] Server :: SN_RunClientLevelEventSeq", clientID);
		SendPacket(clientHd, seq);
	}
	// Sv::SN_RunClientLevelEvent
	{
		Sv::SN_RunClientLevelEvent event;
		event.eventID = 150;
		event.caller = 21035;
		event.serverTime = (i64)TimeDiffMs(TimeRelNow());
		LOG("[client%03d] Server :: SN_RunClientLevelEvent", clientID);
		SendPacket(clientHd, event);
	}
	*/

	/*
	Sv::SN_NotifyIsInSafeZone safe;
	safe.userID = 1;
	safe.inSafeZone = 1;
	LOG("[client%03d] Server :: SN_NotifyIsInSafeZone", clientID);
	SendPacket(clientHd, safe);
	*/
}

static const i64 FILETIME_UNIX_EPOCH = 116444736000000000LL;
static const i64 FILETIME_PER_SEC = 10000000LL;
static const i64 DAY_SEC = 24 * 3600;
static const i32 EASTERN_STANDARD_OFFSET_HOURS = 5;
static const i32 EASTERN_DAYLIGHT_OFFSET_HOURS = 4;
static const i32 CALENDAR_DAYS_BEFORE_SUNDAY = 8;
static const i32 CALENDAR_DAYS_AFTER_SUNDAY = 14;
static const u8 CALENDAR_TYPE_AREA_EVENT = 4;
static const u32 WEEKLY_AREA_EVENTS[7] = { 80602005, 80602001, 80602002, 80602003, 80602004, 80602006, 80602007 };
static const u32 WEEKEND_AREA_EVENT = 70600002;
static const u32 MONTHLY_AREA_EVENTS[] = { 70600011, 70600012 };

static i64 UnixToFiletime(i64 unixSec) { return unixSec * FILETIME_PER_SEC + FILETIME_UNIX_EPOCH; }

static i64 NthSundayUtc(i32 year, i32 month, i32 nth, i32 hourUtc)
{
	tm t = {};
	t.tm_year = year - 1900;
	t.tm_mon = month;
	t.tm_mday = 1;
	t.tm_hour = hourUtc;
	const i64 first = (i64)_mkgmtime(&t);
	const i32 firstWeekday = (i32)(((first / DAY_SEC) + 4) % 7);
	return first + (i64)(((7 - firstWeekday) % 7) + (nth - 1) * 7) * DAY_SEC;
}

static i32 EasternOffsetHours(i64 unixUtc)
{
	const time_t tt = (time_t)unixUtc;
	tm t = {};
	gmtime_s(&t, &tt);
	const i32 year = t.tm_year + 1900;
	const i64 dstStart = NthSundayUtc(year, 2, 2, 7);
	const i64 dstEnd = NthSundayUtc(year, 10, 1, 6);
	return unixUtc >= dstStart && unixUtc < dstEnd ? EASTERN_DAYLIGHT_OFFSET_HOURS : EASTERN_STANDARD_OFFSET_HOURS;
}

static i64 EasternDayStartUtc(i64 unixUtc)
{
	const i64 offset = (i64)EasternOffsetHours(unixUtc) * 3600;
	const i64 local = unixUtc - offset;
	const i64 dayLocal = local - (((local % DAY_SEC) + DAY_SEC) % DAY_SEC);
	return dayLocal + (i64)EasternOffsetHours(dayLocal + offset) * 3600;
}

static i32 EasternWeekday(i64 dayStartUtc)
{
	const i64 local = dayStartUtc - (i64)EasternOffsetHours(dayStartUtc) * 3600;
	return (i32)((((local / DAY_SEC) + 4) % 7 + 7) % 7);
}

struct CalendarEvent { u8 type; u32 index; i64 start; i64 end; };

static void CalendarEventsOfDay(i64 dayStartUtc, eastl::fixed_vector<CalendarEvent,8,false>& out)
{
	const i32 weekday = EasternWeekday(dayStartUtc);
	const i64 next = EasternDayStartUtc(dayStartUtc + DAY_SEC + 3600);
	out.push_back({ CALENDAR_TYPE_AREA_EVENT, WEEKLY_AREA_EVENTS[weekday], UnixToFiletime(dayStartUtc), UnixToFiletime(next) });
	if(weekday == 6 || weekday == 0) {
		const i64 saturday = weekday == 6 ? dayStartUtc : EasternDayStartUtc(dayStartUtc - DAY_SEC + 3600);
		const i64 monday = EasternDayStartUtc(saturday + 2 * DAY_SEC + 3600);
		out.push_back({ CALENDAR_TYPE_AREA_EVENT, WEEKEND_AREA_EVENT, UnixToFiletime(saturday), UnixToFiletime(monday) });
	}
	const time_t tt = (time_t)(dayStartUtc - (i64)EasternOffsetHours(dayStartUtc) * 3600);
	tm t = {};
	gmtime_s(&t, &tt);
	tm first = {};
	first.tm_year = t.tm_year;
	first.tm_mon = t.tm_mon;
	first.tm_mday = 1;
	tm nextMonth = first;
	nextMonth.tm_mon += 1;
	const i64 monthStart = EasternDayStartUtc((i64)_mkgmtime(&first) + 12 * 3600);
	const i64 monthEnd = EasternDayStartUtc((i64)_mkgmtime(&nextMonth) + 12 * 3600);
	for(const u32 m : MONTHLY_AREA_EVENTS) {
		if(!out.full()) out.push_back({ CALENDAR_TYPE_AREA_EVENT, m, UnixToFiletime(monthStart), UnixToFiletime(monthEnd) });
	}
}

void HubReplication::SendCalendar(ClientHandle clientHd)
{
	const u64 nowFt = CurrentFiletimeTimestampUTC();
	const i64 nowUnix = ((i64)nowFt - FILETIME_UNIX_EPOCH) / FILETIME_PER_SEC;
	const i64 today = EasternDayStartUtc(nowUnix);
	{
		PacketWriter<Sv::SA_CalendarDetail,2048> packet;
		packet.Write<u64>(nowFt);
		eastl::fixed_vector<CalendarEvent,24,false> all;
		for(i32 d = -1; d <= 1; d++) {
			eastl::fixed_vector<CalendarEvent,8,false> day;
			CalendarEventsOfDay(EasternDayStartUtc(today + d * DAY_SEC + 3600), day);
			foreach_const(e, day) if(!all.full()) all.push_back(*e);
		}
		packet.Write<u16>((u16)all.size());
		foreach_const(e, all) {
			packet.Write<u8>(e->type);
			packet.Write<i32>((i32)e->index);
			packet.Write<u64>((u64)e->start);
			packet.Write<u64>((u64)e->end);
		}
		SendPacket(clientHd, packet);
	}
}

void HubReplication::SendCalendarSchedule(ClientHandle clientHd)
{
	const u64 nowFt = CurrentFiletimeTimestampUTC();
	const i64 nowUnix = ((i64)nowFt - FILETIME_UNIX_EPOCH) / FILETIME_PER_SEC;
	const i64 today = EasternDayStartUtc(nowUnix);
	const i64 sunday = EasternDayStartUtc(today - (i64)EasternWeekday(today) * DAY_SEC + 3600);
	PacketWriter<Sv::SN_Calendar,4096> packet;
	const i32 days = CALENDAR_DAYS_BEFORE_SUNDAY + CALENDAR_DAYS_AFTER_SUNDAY + 1;
	packet.Write<u16>((u16)days);
	for(i32 d = -CALENDAR_DAYS_BEFORE_SUNDAY; d <= CALENDAR_DAYS_AFTER_SUNDAY; d++) {
		const i64 day = EasternDayStartUtc(sunday + d * DAY_SEC + 3600);
		eastl::fixed_vector<CalendarEvent,8,false> events;
		CalendarEventsOfDay(day, events);
		packet.Write<u64>((u64)UnixToFiletime(day));
		packet.Write<u16>((u16)events.size());
		foreach_const(e, events) {
			packet.Write<u8>(e->type);
			packet.Write<u32>(e->index);
			packet.Write<u64>((u64)e->start);
			packet.Write<u64>((u64)e->end);
		}
	}
	SendPacket(clientHd, packet);
}

void HubReplication::SendAreaPopularity(ClientHandle clientHd, AreaIndex areaID)
{
	const GameXmlContent& content = GetGameXmlContent();
	bool known = false;
	for(auto& area : content.areaStages) {
		if(area.ID == areaID) { known = true; break; }
	}

	{
		Sv::SA_AreaPopularity packet;
		packet.errCode = known ? 0 : 1;
		SendPacket(clientHd, packet);
	}

	{
		PacketWriter<Sv::SN_AreaPopularity> packet;

		packet.Write(areaID);
		if(!known) {
			packet.Write<u16>(0);
			SendPacket(clientHd, packet);
			return;
		}
		i32 gameType = 1;
		for(auto& entry : content.entrySystems) {
			for(auto id : entry.areas) {
				if(id != areaID) continue;
				if(entry.entryType == "ENTRY_TYPE_SPORTS") gameType = 7;
			}
			for(auto id : entry.scheduleAreas) {
				if(id != areaID) continue;
				if(entry.entryType == "ENTRY_TYPE_SPORTS") gameType = 7;
			}
		}
		u16 count = 0;
		for(auto& area : content.areaStages) {
			if(area.ID != areaID) continue;
			for(auto stageID : area.stages) {
				(void)stageID;
				count++;
			}
		}
		packet.Write<u16>(count);
		for(auto& area : content.areaStages) {
			if(area.ID != areaID) continue;
			for(auto stageID : area.stages) {
				packet.Write(stageID);
				packet.Write<i32>(gameType);
				packet.Write<i32>(1);
			}
		}

		SendPacket(clientHd, packet);
	}
}

void HubReplication::SendPartyCreateSucess(ClientHandle clientHd, UserID ownerUserID, StageType stageType)
{
	PacketWriter<Sv::SA_PartyCreate> packet;

	//packet.Write<i32>(175); // retval (ERROR_TYPE_PARTY_CREATE_PENALTY_TIME) <- this one is silent
	packet.Write<i32>(0); // retval: success
	packet.Write(ownerUserID); // ownerUserID
	packet.Write(stageType); // stageType

	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyInvite(ClientHandle clientHd, const wchar* inviterNick, StageIndex stageIndex, GameType gameType, i32 partyID)
{
	PacketWriter<Sv::SN_PartyInvite> packet;
	packet.Write<u32>(0);
	packet.WriteStringObj(inviterNick);
	packet.Write<u16>(1);
	packet.Write(stageIndex);
	packet.Write<u32>((u32)gameType);
	packet.Write<u32>((u32)GameDefinition::System);
	packet.Write<u32>((u32)StageRule::Unfair);
	packet.Write<i32>(partyID);
	packet.Write<u32>(0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyInviteResult(ClientHandle clientHd, u32 result, const wchar* nickname)
{
	PacketWriter<Sv::SA_PartyInvite> packet;
	packet.Write<u32>(result);
	packet.WriteStringObj(nickname);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyJoinAnswer(ClientHandle clientHd, i32 partyID, StageType stageType, GameType gameType, const PartyUser* members, i32 count)
{
	PacketWriter<Sv::SA_PartyJoin,1024> packet;
	packet.Write<i32>(0);
	packet.Write<i32>(partyID);
	packet.Write<u32>((u32)stageType);
	packet.Write<u32>((u32)gameType);
	packet.Write<u32>((u32)GameDefinition::System);
	packet.Write<u16>((u16)count);
	for(i32 i = 0; i < count; i++) {
		packet.Write<u32>(members[i].userId);
		packet.WriteStringObj(members[i].nickname);
		packet.Write<u8>(0);
		packet.Write<i32>(0);
		packet.Write<i32>(0);
		packet.Write<i32>(0);
		packet.Write<f32>(0.f);
	}
	packet.Write<u32>(0);
	packet.Write<u16>(0);
	packet.Write<u32>(0);
	packet.Write<u16>(0);
	packet.Write<u8>(0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyJoin(ClientHandle clientHd, u32 userId, const wchar* nickname, CreatureIndex leaderCreature, bool receiverIsOwner, GameType gameType, u8 slot)
{
	PacketWriter<Sv::SN_PartyJoin> packet;
	packet.Write<u32>(userId);
	packet.WriteStringObj(nickname);
	packet.Write<u8>(0);
	packet.Write(leaderCreature);
	packet.Write<u8>(receiverIsOwner ? 1 : 0);
	packet.Write<u32>((u32)gameType);
	packet.Write<u32>((u32)GameDefinition::System);
	packet.Write<u32>(0);
	packet.Write<u8>(slot);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyLeave(ClientHandle clientHd, u32 userId, const wchar* nickname, bool receiverIsOwner)
{
	PacketWriter<Sv::SN_PartyLeave> packet;
	packet.Write<u32>(userId);
	packet.WriteStringObj(nickname);
	packet.Write<u8>(receiverIsOwner ? 1 : 0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyBreakupAnswer(ClientHandle clientHd, i32 remainMemberCount)
{
	PacketWriter<Sv::SA_PartyBreakup> packet;
	packet.Write<i32>(0);
	packet.Write<i32>(remainMemberCount);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyLeaveAnswer(ClientHandle clientHd)
{
	PacketWriter<Sv::SA_PartyLeave> packet;
	packet.Write<i32>(0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyBreakup(ClientHandle clientHd)
{
	SendPacketData<Sv::SN_PartyBreakup>(clientHd, 0, nullptr);
}

void HubReplication::SendPartyCancelInvite(ClientHandle clientHd, i32 partyID)
{
	PacketWriter<Sv::SN_PartyCancelInvite> packet;
	packet.Write<i32>(partyID);
	SendPacket(clientHd, packet);
}

template<typename Writer>
static void WriteFriendEntry(Writer& packet, const HubReplication::FriendEntry& e)
{
	packet.WriteStringObj(e.nickname);
	packet.template Write<i32>(e.leaderCreature);
	packet.template Write<u8>(e.state);
	packet.template Write<u16>(e.level);
	packet.template Write<u8>(e.favorite);
	packet.template Write<u8>(0);
	packet.template Write<u8>(0);
	packet.template Write<i64>(FILETIME_UNIX_EPOCH_100NS + (i64)time(nullptr) * 10000000LL);
}

void HubReplication::SendFriendList(ClientHandle clientHd, const FriendEntry* friends, i32 count)
{
	PacketWriter<Sv::SN_FriendList,4096> packet;
	packet.Write<u16>((u16)count);
	for(i32 i = 0; i < count; i++) WriteFriendEntry(packet, friends[i]);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendRequestList(ClientHandle clientHd, const FriendEntry* requests, i32 count)
{
	PacketWriter<Sv::SN_FriendRequestList,2048> packet;
	packet.Write<u16>((u16)count);
	for(i32 i = 0; i < count; i++) {
		packet.WriteStringObj(requests[i].nickname);
		packet.Write<i32>(requests[i].leaderCreature);
	}
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendStateChanged(ClientHandle clientHd, const wchar* nickname, u8 state)
{
	PacketWriter<Sv::SN_FriendStateChanged> packet;
	packet.WriteStringObj(nickname);
	packet.Write<u8>(state);
	packet.Write<u8>(0);
	packet.Write<i64>(FILETIME_UNIX_EPOCH_100NS + (i64)time(nullptr) * 10000000LL);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendRequestResult(ClientHandle clientHd, u32 result, const wchar* nickname)
{
	PacketWriter<Sv::SA_FriendRequest> packet;
	packet.Write<u32>(result);
	packet.WriteStringObj(nickname);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendRequested(ClientHandle clientHd, const wchar* nickname, i32 leaderCreature)
{
	PacketWriter<Sv::SN_FriendRequested> packet;
	packet.WriteStringObj(nickname);
	packet.Write<i32>(leaderCreature);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendRespondResult(ClientHandle clientHd, u32 result, const wchar* nickname, u8 accept)
{
	PacketWriter<Sv::SA_FriendRespond> packet;
	packet.Write<u32>(result);
	packet.WriteStringObj(nickname);
	packet.Write<u8>(accept);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendAdded(ClientHandle clientHd, const FriendEntry& entry)
{
	PacketWriter<Sv::SN_FriendAdded> packet;
	WriteFriendEntry(packet, entry);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendRemoveResult(ClientHandle clientHd, u32 result, const wchar* nickname)
{
	PacketWriter<Sv::SA_FriendRemove> packet;
	packet.Write<u32>(result);
	packet.WriteStringObj(nickname);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendRemoved(ClientHandle clientHd, const wchar* nickname)
{
	PacketWriter<Sv::SN_FriendRemoved> packet;
	packet.WriteStringObj(nickname);
	SendPacket(clientHd, packet);
}

void HubReplication::SendNicknameResult(ClientHandle clientHd, u16 netID, u32 result, const wchar* nickname)
{
	PacketWriter<Sv::SA_FriendSetFavorite,256> packet;
	packet.Write<u32>(result);
	packet.WriteStringObj(nickname);
	NT_LOG("[client%x] Replication :: result %u netID=%d nickname='%ls'", clientHd, result, (i32)netID, nickname);
	server->SendPacketData(clientHd, netID, (u16)packet.size, packet.data);
}

void HubReplication::SendBlockList(ClientHandle clientHd, const WideString* names, i32 count)
{
	PacketWriter<Sv::SN_BlockList,2048> packet;
	packet.Write<u16>((u16)count);
	for(i32 i = 0; i < count; i++) packet.WriteStringObj(names[i].data());
	SendPacket(clientHd, packet);
}

void HubReplication::SendCommunityStatusMessage(ClientHandle clientHd, const wchar* nickname, const wchar* message)
{
	PacketWriter<Sv::SN_CommunityStatusMessage> packet;
	packet.WriteStringObj(nickname);
	packet.WriteStringObj(message);
	SendPacket(clientHd, packet);
}

void HubReplication::SendFriendPartyCreation(ClientHandle clientHd, const wchar* nickname, StageIndex stageIndex, GameType gameType)
{
	PacketWriter<Sv::SN_FriendPartycreation> packet;
	packet.WriteStringObj(nickname);
	packet.Write(stageIndex);
	packet.Write<u8>((u8)gameType);
	packet.Write<u8>((u8)GameDefinition::System);
	SendPacket(clientHd, packet);
}

void HubReplication::SendCommunityStatusMessageResult(ClientHandle clientHd, u32 result)
{
	PacketWriter<Sv::SA_CommunityStatusMessage> packet;
	packet.Write<u32>(result);
	SendPacket(clientHd, packet);
}

void HubReplication::SendCancelMatchingQueue(ClientHandle clientHd)
{
	PacketWriter<Sv::SN_CancelMatchingQueue> packet;
	packet.Write<u8>(0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPartyEnqueue(ClientHandle clientHd, StageIndex stageIndex, bool leader)
{
	if(leader) {
		Sv::SA_EnqueueGame packet;
		packet.retval = 0;
		SendPacket(clientHd, packet);
	}

	Sv::SN_EnqueueMatchingQueue matching;
	matching.stageIndex = stageIndex;
	matching.currentMatchingTimeMs = 0;
	matching.avgMatchingTimeMs = 121634;
	matching.disableMatchExpansion = 0;
	matching.isMatchingExpanded = 0;
	SendPacket(clientHd, matching);
}

void HubReplication::SendItemTradeResult(ClientHandle clientHd, u32 result, i64 gold, const AccountItem* items, i32 count)
{
	PacketWriter<Sv::SA_ItemTrade,256> packet;
	packet.Write<u32>(result);
	packet.Write<i64>(gold);
	packet.Write<u16>((u16)count);
	for(i32 i = 0; i < count; i++) {
		packet.Write<u32>(items[i].uid);
		packet.Write<i32>(items[i].docIndex);
		packet.Write<u32>((u32)items[i].count);
	}
	SendPacket(clientHd, packet);
}

void HubReplication::SendMatchingPartyConfirm(ClientHandle clientHd, u8 row, u32 teamType, bool confirm)
{
	PacketWriter<Sv::SN_MatchingPartyConfirm> packet;
	packet.Write<u32>(row);
	packet.Write<u32>(teamType);
	packet.Write<u8>(confirm ? 1 : 0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendMatchingPartyFound(ClientHandle clientHd, const In::MN_MatchingPartyFound& matchingParty, StageIndex stageIndex, const eastl::fixed_vector<UserID,16>& rowIDs, u8 myTeam)
{
	PacketWriter<Sv::SQ_MatchingPartyFound,512> packet;

	packet.Write(matchingParty.sortieUID); // sortieID
	packet.Write(stageIndex);
	packet.Write(matchingParty.gameType);
	packet.Write(GameDefinition::System); // gameDefinitionType
	packet.Write(StageRule::Unfair); // stageRule

	// allies
	i32 alliesCount = 0;
	for(int i = 0; i < matchingParty.playerCount; i++) {
		if(matchingParty.playerList[i].team == myTeam) alliesCount++;
	}
	packet.Write<u16>(alliesCount);

	for(int i = 0; i < matchingParty.playerCount; i++) {
		const auto& p = matchingParty.playerList[i];
		if(p.team == myTeam) {
			packet.Write(rowIDs[i]);
			packet.WriteStringObj(p.name.data, p.name.len); // nickname
			packet.Write<u8>(p.isBot); // isBot
			packet.Write<i32>(0); // tier
			packet.Write<i32>(0); // tierGroupRanking
			packet.Write<i32>(0); // tierSeriesFlag
			packet.Write<i32>(0); // pvpRate
		}
	}

	// enemies
	i32 enemiesCount = 0;
	for(int i = 0; i < matchingParty.playerCount; i++) {
		if(matchingParty.playerList[i].team != myTeam) enemiesCount++;
	}
	packet.Write<u16>(enemiesCount);

	for(int i = 0; i < matchingParty.playerCount; i++) {
		const auto& p = matchingParty.playerList[i];
		if(p.team != myTeam) {
			packet.Write(rowIDs[i]);
			packet.WriteStringObj(p.name.data, p.name.len); // nickname
			packet.Write<u8>(p.isBot); // isBot
			packet.Write<i32>(0); // tier
			packet.Write<i32>(0); // tierGroupRanking
			packet.Write<i32>(0); // tierSeriesFlag
			packet.Write<i32>(0); // pvpRate
		}
	}

	// spectators
	packet.Write<u16>(0);

	// NOTE: if timeToWaitInSec is 0 there is no popup to accept
	packet.Write<i32>(15); // timeToWaitInSec
	packet.Write<u8>(0); // elementMain
	packet.Write<u8>(0); // elementSub

	SendPacket(clientHd, packet);
}

void HubReplication::OnClientDisconnect(ClientHandle clientHd)
{
	const i32 clientID = plidMap->Get(clientHd);
	playerState[clientID] = PlayerState::DISCONNECTED;
	playerClientHd[clientID] = ClientHandle::INVALID;
}

void HubReplication::PlayerRegisterMasterActor(ClientHandle clientHd, ActorUID masterActorUID, ClassType classType)
{
	LocalActorID laiLeader = (LocalActorID)((u32)LocalActorID::FIRST_SELF_MASTER + (i32)classType);
	ASSERT(laiLeader >= LocalActorID::FIRST_SELF_MASTER && laiLeader < LocalActorID::LAST_SELF_MASTER);

	PlayerForceLocalActorID(clientHd, masterActorUID, laiLeader);
}

void HubReplication::PlayerForceLocalActorID(ClientHandle clientHd, ActorUID actorUID, LocalActorID localActorID)
{
	const i32 clientID = plidMap->Get(clientHd);
	auto& map = playerLocalInfo[clientID].localActorIDMap;
	ASSERT(map.find(actorUID) == map.end());
	map.emplace(actorUID, localActorID);
}

LocalActorID HubReplication::GetLocalActorID(ClientHandle clientHd, ActorUID actorUID)
{
	const i32 clientID = plidMap->Get(clientHd);
	const auto& map = playerLocalInfo[clientID].localActorIDMap;
	auto found = map.find(actorUID);
	if(found != map.end()) {
		return found->second;
	}
	return LocalActorID::INVALID;
}

ActorUID HubReplication::GetWorldActorUID(ClientHandle clientHd, LocalActorID localActorID)
{
	const i32 clientID = plidMap->Get(clientHd);
	// TODO: second map, for this reverse lookup
	const auto& map = playerLocalInfo[clientID].localActorIDMap;
	foreach(it, map) {
		if(it->second == localActorID) {
			return it->first;
		}
	}

	return ActorUID::INVALID;
}

void HubReplication::UpdatePlayersLocalState()
{
	for(int clientID = 0; clientID < MAX_CLIENTS; clientID++) {
		if(playerState[clientID] != PlayerState::IN_GAME) continue;

		PlayerLocalInfo& localInfo = playerLocalInfo[clientID];
		auto& playerActorUIDSet = localInfo.actorUIDSet; // replicated actors UID set

		eastl::fixed_vector<ActorUID,2048,true> removedList;
		eastl::fixed_vector<ActorUID,2048,true> addedList;
		eastl::set_difference(playerActorUIDSet.begin(), playerActorUIDSet.end(), frameCur->actorUIDSet.begin(), frameCur->actorUIDSet.end(), eastl::back_inserter(removedList));
		eastl::set_difference(frameCur->actorUIDSet.begin(), frameCur->actorUIDSet.end(), playerActorUIDSet.begin(), playerActorUIDSet.end(), eastl::back_inserter(addedList));

		const ClientHandle clientHd = playerClientHd[clientID];

		// send destroy entity for deleted actors
		foreach(setIt, removedList) {
			const ActorUID actorUID = *setIt;

#ifdef CONF_DEBUG // we don't actually need to verify the actor was in the previous frame, but do it in debug mode anyway
			const auto actorIt = framePrev->actorUIDSet.find(actorUID);
			ASSERT(actorIt != framePrev->actorUIDSet.end());
			auto type = framePrev->actorType.find(actorUID);
			ASSERT(type != framePrev->actorType.end());
			switch(type->second) {
				case ActorType::PLAYER: {
					auto pm = framePrev->playerMap.find(actorUID);
					ASSERT(pm != framePrev->playerMap.end());
					ASSERT(pm->second->actorUID == actorUID);
				} break;

				case ActorType::NPC: {
					auto pm = framePrev->npcMap.find(actorUID);
					ASSERT(pm != framePrev->npcMap.end());
					ASSERT(pm->second->actorUID == actorUID);
				} break;

				default: {
					ASSERT_MSG(0, "case not handled");
				} break;
			}
#endif

			SendActorDestroy(clientHd, actorUID);

			// Remove LocalActorID link
			DeleteLocalActorID(clientHd, actorUID);
		}

		// send new spawns
		foreach(setIt, addedList) {
			const ActorUID actorUID = *setIt;

			// Create a LocalActorID link if none exists already
			// If one exists already, we have pre-allocated it (like with leader master)
			if(GetLocalActorID(clientHd, actorUID) == LocalActorID::INVALID) {
				CreateLocalActorID(clientHd, actorUID);
			}

			auto type = frameCur->actorType.find(actorUID);
			ASSERT(type != frameCur->actorType.end());

			switch(type->second) {
				case ActorType::PLAYER: {
					const auto pm = frameCur->playerMap.find(actorUID);
					ASSERT(pm != frameCur->playerMap.end());
					ASSERT(pm->second->actorUID == actorUID);
					SendActorPlayerSpawn(clientHd, *pm->second);
				} break;

				case ActorType::NPC: {
					const auto pm = frameCur->npcMap.find(actorUID);
					ASSERT(pm != frameCur->npcMap.end());
					ASSERT(pm->second->actorUID == actorUID);
					SendActorNpcSpawn(clientHd, *pm->second);
				} break;

				case ActorType::JUKEBOX: {
					ASSERT(actorUID == frameCur->jukebox.actorUID);
					SendJukeboxSpawn(clientHd, frameCur->jukebox);
				} break;

				default: {
					ASSERT_MSG(0, "case not handled");
				} break;
			}
		}

		playerActorUIDSet = frameCur->actorUIDSet;

		// TODO: remove, extra checks
#ifdef CONF_DEBUG
		auto& localActorIDMap = localInfo.localActorIDMap;
		foreach(it, playerActorUIDSet) {
			ASSERT(localActorIDMap.find(*it) != localActorIDMap.end());
		}
		foreach(it, localActorIDMap) {
			ASSERT(playerActorUIDSet.find(it->first) != playerActorUIDSet.end());
		}
		eastl::fixed_set<LocalActorID,2048> laiSet;
		foreach(it, localActorIDMap) {
			ASSERT(laiSet.find(it->second) == laiSet.end());
			laiSet.emplace(it->second);
		}
#endif
	}
}

void HubReplication::FrameDifference()
{
	// send position update

	eastl::fixed_vector<eastl::pair<ActorUID,Frame::Transform>, 2048> tfToSendList;
	eastl::fixed_vector<eastl::pair<ActorUID,Frame::ActionState>, 2048> atToSendList;

	// find if the position has changed since last frame
	foreach(it, frameCur->actorUIDSet) {
		const ActorUID actorUID = *it;
		auto type = frameCur->actorType.find(actorUID);
		ASSERT(type != frameCur->actorType.end());

		switch(type->second) {
			case ActorType::PLAYER: {
				// was present in last frame
				if(framePrev->actorUIDSet.find(actorUID) != framePrev->actorUIDSet.end()) {
					// transform
					{
						auto pf = framePrev->transformMap.find(actorUID);
						ASSERT(pf != framePrev->transformMap.end());
						const Frame::Transform& prev = pf->second;

						auto cf = frameCur->transformMap.find(actorUID);
						ASSERT(cf != frameCur->transformMap.end());
						const Frame::Transform& cur = cf->second;

						if(!prev.HasNotChanged(cur)) {
							tfToSendList.emplace_back(actorUID, cur);
						}
					}
				}

				// action state
				{
					auto cf = frameCur->actionStateMap.find(actorUID);
					ASSERT(cf != frameCur->actionStateMap.end());
					const Frame::ActionState& cur = cf->second;

					if(cur.actionState != ActionStateID::INVALID) {
						atToSendList.emplace_back(actorUID, cur);
					}
				}
			} break;

			case ActorType::NPC: {
				// nothing for now, NPCs don't change
			} break;

			case ActorType::JUKEBOX: {
				// was present in last frame
				if(framePrev->actorUIDSet.find(actorUID) != framePrev->actorUIDSet.end()) {
					const ActorJukebox& prev = framePrev->jukebox;
					const ActorJukebox& cur = frameCur->jukebox;

					ASSERT(prev.actorUID == cur.actorUID); // actually the same jukebox

					if(cur.currentSong.songID != SongID::INVALID) {
						if(prev.currentSong.songID != cur.currentSong.songID || prev.playStartTime != cur.playStartTime) {

							for(int clientID= 0; clientID < MAX_CLIENTS; clientID++) {
								if(playerState[clientID] != PlayerState::IN_GAME) continue;

								SendJukeboxPlay(playerClientHd[clientID], cur.currentSong.songID, cur.currentSong.requesterNick.data(), cur.playPosition);
							}
						}
					}

					bool doSendTracks = false;
					if(prev.tracks.size() == cur.tracks.size()) {
						for(int t = 0; t < cur.tracks.size(); t++) {
							if(prev.tracks[t].songID != cur.tracks[t].songID ||
							   prev.tracks[t].requesterNick.compare(cur.tracks[t].requesterNick)) {
								doSendTracks = true;
								break;
							}
						}
					}
					else {
						doSendTracks = true;
					}

					if(doSendTracks) {
						for(int clientID= 0; clientID < MAX_CLIENTS; clientID++) {
							if(playerState[clientID] != PlayerState::IN_GAME) continue;

							SendJukeboxQueue(playerClientHd[clientID], cur.tracks.data(), cur.tracks.size());
						}
					}
				}
			} break;

			default: ASSERT_MSG(0, "case not handled"); break;
		}
	}

	// send updates
	foreach(it, tfToSendList) {
		const auto& e = *it;
		const Frame::Transform& tf = e.second;

		Sv::SN_GamePlayerSyncByInt sync;
		sync.p3nPos = v2f(tf.pos);
		sync.p3nDir = v2f(tf.dir);
		sync.p3nEye = v2f(tf.eye);
		sync.nRotate = tf.rotate;
		sync.nSpeed = tf.speed;
		sync.nState = -1;
		sync.nActionIDX = -1;

		for(int clientID = 0; clientID < MAX_CLIENTS; clientID++) {
			if(playerState[clientID] != PlayerState::IN_GAME) continue;

			const ClientHandle clientHd = playerClientHd[clientID];
			sync.characterID = GetLocalActorID(clientHd, e.first);
			SendPacket(clientHd, sync);
		}
	}

	foreach(it, atToSendList) {
		const auto& e = *it;
		const Frame::ActionState& at = e.second;

		Sv::SN_PlayerSyncActionStateOnly packet;
		memset(&packet, 0, sizeof(packet));
		packet.state = at.actionState;
		packet.param1 = at.actionParam1;
		packet.param2 = at.actionParam2;
		packet.rotate = at.rotate;
		packet.upperRotate = at.upperRotate;

		for(int clientID= 0; clientID < MAX_CLIENTS; clientID++) {
			if(playerState[clientID] != PlayerState::IN_GAME) continue;

			const ClientHandle clientHd = playerClientHd[clientID];
			packet.characterID = GetLocalActorID(clientHd, e.first);
			SendPacket(clientHd, packet);
		}
	}
}

void HubReplication::SendActorPlayerSpawn(ClientHandle clientHd, const ActorPlayer& actor)
{
	DBG_ASSERT(actor.actorUID != ActorUID::INVALID);
	const LocalActorID localActorID = GetLocalActorID(clientHd, actor.actorUID);
	ASSERT(localActorID != LocalActorID::INVALID);

	// this is the main actor
	if(actor.parentActorUID == ActorUID::INVALID) {
		// SN_GameCreateActor
		{
			PacketWriter<Sv::SN_GameCreateActor> packet;

			packet.Write<LocalActorID>(localActorID); // objectID
			packet.Write<i32>(1);
			packet.Write<CreatureIndex>(actor.docID);
			packet.Write<i32>(-1);

			packet.Write(actor.pos);
			packet.Write(actor.dir);
			packet.Write<i32>(actor.spawnAnim);
			packet.Write<ActionStateID>(actor.actionState);
			packet.Write<i32>(actor.ownerID);
			packet.Write<u8>(actor.dirToNearPC);
			packet.Write<i32>(actor.wanderDist);
			packet.Write<i32>(actor.tagID);
			packet.Write<i32>(actor.faction);
			packet.Write<ClassType>(actor.classType);
			packet.Write<SkinIndex>(actor.skinIndex);
			packet.Write<i32>(actor.seed);


			WriteInitStat(packet, GetGameXmlContent().GetMaster(actor.classType).baseStats);




			packet.Write<u8>(1); // isInSight
			packet.Write<u8>(0); // isDead
			packet.Write<i64>((i64)TimeDiffMs(TimeRelNow())); // serverTime

			packet.Write<u16>(0); // meshChangeActionHistory_count

			SendPacket(clientHd, packet);
		}
	}
	// this is the sub actor
	else {
		const LocalActorID parentLocalActorID = GetLocalActorID(clientHd, actor.parentActorUID);
		ASSERT(parentLocalActorID != LocalActorID::INVALID);

		// SN_GameCreateSubActor
		{
			PacketWriter<Sv::SN_GameCreateSubActor> packet;

			packet.Write<LocalActorID>(localActorID); // objectID
			packet.Write<LocalActorID>(parentLocalActorID); // mainEntityID
			packet.Write<i32>(1);
			packet.Write<CreatureIndex>(actor.docID);
			packet.Write<i32>(-1);

			packet.Write(actor.pos);
			packet.Write(actor.dir);
			packet.Write<i32>(actor.spawnAnim);
			packet.Write<ActionStateID>(actor.actionState);
			packet.Write<i32>(actor.ownerID);
			packet.Write<i32>(actor.tagID);
			packet.Write<i32>(actor.faction);
			packet.Write<ClassType>(actor.classType);
			packet.Write<SkinIndex>(actor.skinIndex);
			packet.Write<i32>(actor.seed);


			WriteInitStat(packet, GetGameXmlContent().GetMaster(actor.classType).baseStats);




			packet.Write<u16>(0); // meshChangeActionHistory_count

			SendPacket(clientHd, packet);
		}
	}

	// SN_GamePlayerStock
	{
		PacketWriter<Sv::SN_GamePlayerStock> packet;

		packet.Write<LocalActorID>(localActorID); // playerID
		packet.WriteStringObj(actor.name.data()); // name
		packet.Write<ClassType>(actor.classType); // class_
#if 0
		packet.Write<i32>(320080005); // displayTitleIDX
		packet.Write<i32>(320080005); // statTitleIDX
#else // disable titles
		packet.Write<i32>(-1); // displayTitleIDX
		packet.Write<i32>(-1); // statTitleIDX
#endif
		packet.Write<u8>(0); // badgeType
		packet.Write<u8>(0); // badgeTierLevel
		packet.WriteStringObj(actor.guildTag.data()); // guildTag
		packet.Write<u8>(0); // vipLevel
		packet.Write<u8>(0); // staffType
		packet.Write<u8>(0); // isSubstituted

		SendPacket(clientHd, packet);
	}

	const bool self = (u32)localActorID < (u32)LocalActorID::FIRST_OTHER_PLAYER;
	if(actor.parentActorUID == ActorUID::INVALID && !self) {
		PacketWriter<Sv::SN_Exp> packet;
		packet.Write<LocalActorID>(localActorID);
		packet.WriteStringObj(actor.name.data());
		packet.Write<u8>(0);
		packet.Write<u16>(actor.level);
		packet.Write<u32>(actor.levelExp);
		SendPacket(clientHd, packet);
	}
	if(actor.parentActorUID == ActorUID::INVALID) SendPlayerStateInTown(clientHd, actor.actorUID, self ? -1 : 0, nullptr, 0);

	// SN_GamePlayerEquipWeapon
	{
		PacketWriter<Sv::SN_GamePlayerEquipWeapon> packet;

		packet.Write<LocalActorID>(localActorID); // characterID
		packet.Write<WeaponIndex>(GetGameXmlContent().GetMaster(actor.classType).defaultWeaponIDs[0]);
		packet.Write<f32>(0.f);
		packet.Write<f32>(0.f);

		SendPacket(clientHd, packet);
	}

	if(actor.parentActorUID == ActorUID::INVALID) SendMasterSkillSlots(clientHd, actor);

	/*
	// SN_StatusSnapshot
	{
		PacketWriter<> packet;

		packet.Write<LocalActorID>(localActorID); // objectID
		packet.Write<u16>(1); // statusArray_count

		packet.Write<i32>(1230350000); // statusIndex
		packet.Write<u8>(1); // bEnabled
		packet.Write<i32>(0); // caster
		packet.Write<u8>(1); // overlapCount
		packet.Write<u8>(0); // customValue
		packet.Write<i32>(0); // durationTimeMs
		packet.Write<i32>(0); // remainTimeMs

		LOG("[client%03d] Server :: %s", clientID, PacketSerialize<Sv::SN_StatusSnapshot>(packet.data, packet.size));
		SendPacket<>(clientID, Sv::SN_StatusSnapshotpacket);
	}
	*/
}

void HubReplication::SendActorNpcSpawn(ClientHandle clientHd, const ActorNpc& actor)
{
	const i32 clientID = plidMap->Get(clientHd);

	DBG_ASSERT(actor.actorUID != ActorUID::INVALID);
	auto found = playerLocalInfo[clientID].localActorIDMap.find(actor.actorUID);
	ASSERT(found != playerLocalInfo[clientID].localActorIDMap.end());

	const LocalActorID localActorID = found->second;

	// SN_GameCreateActor
	{
		PacketWriter<Sv::SN_GameCreateActor> packet;

		packet.Write<LocalActorID>(localActorID); // objectID
		packet.Write<i32>(actor.type);
		packet.Write<CreatureIndex>(actor.docID);
		packet.Write<i32>(actor.localID);
		packet.Write(actor.pos);
		packet.Write(actor.dir);
		packet.Write<i32>(actor.spawnAnim);
		packet.Write<ActionStateID>(actor.actionState);
		packet.Write<i32>(actor.ownerID);
		packet.Write<u8>(actor.dirToNearPC);
		packet.Write<i32>(actor.wanderDist);
		packet.Write<i32>(actor.tagID);
		packet.Write<i32>(actor.faction);
		packet.Write<ClassType>(ClassType::NONE);
		packet.Write<SkinIndex>(SkinIndex::DEFAULT);
		packet.Write<i32>(actor.seed);


		packet.Write<u16>(0);
		packet.Write<u16>(0);




		packet.Write<u8>(1); // isInSight
		packet.Write<u8>(0); // isDead
		packet.Write<i64>((i64)TimeDiffMs(TimeRelNow())); // serverTime

		packet.Write<u16>(0); // meshChangeActionHistory_count

		SendPacket(clientHd, packet);
	}

	// SN_SpawnPosForMinimap
	{
		PacketWriter<Sv::SN_SpawnPosForMinimap> packet;

		packet.Write<LocalActorID>(localActorID); // objectID
		packet.Write(actor.pos); // p3nPos

		SendPacket(clientHd, packet);
	}
}

void HubReplication::SendJukeboxSpawn(ClientHandle clientHd, const HubReplication::ActorJukebox& actor)
{
	const i32 clientID = plidMap->Get(clientHd);

	DBG_ASSERT(actor.actorUID != ActorUID::INVALID);
	auto found = playerLocalInfo[clientID].localActorIDMap.find(actor.actorUID);
	ASSERT(found != playerLocalInfo[clientID].localActorIDMap.end());

	const LocalActorID localActorID = found->second;

	// SN_GameCreateActor
	{
		PacketWriter<Sv::SN_GameCreateActor> packet;

		packet.Write<LocalActorID>(localActorID); // objectID
		packet.Write<i32>(actor.type);
		packet.Write<CreatureIndex>(actor.docID);
		packet.Write<i32>(actor.localID);
		packet.Write(actor.pos);
		packet.Write(actor.dir);
		packet.Write<i32>(actor.spawnAnim);
		packet.Write<ActionStateID>(actor.actionState);
		packet.Write<i32>(actor.ownerID);
		packet.Write<u8>(actor.dirToNearPC);
		packet.Write<i32>(actor.wanderDist);
		packet.Write<i32>(actor.tagID);
		packet.Write<i32>(actor.faction);
		packet.Write<ClassType>(ClassType::NONE);
		packet.Write<SkinIndex>(SkinIndex::DEFAULT);
		packet.Write<i32>(actor.seed);


		packet.Write<u16>(0);
		packet.Write<u16>(0);




		packet.Write<u8>(1); // isInSight
		packet.Write<u8>(0); // isDead
		packet.Write<i64>((i64)TimeDiffMs(TimeRelNow())); // serverTime

		packet.Write<u16>(0); // meshChangeActionHistory_count

		SendPacket(clientHd, packet);
	}

	{
		PacketWriter<Sv::SN_JukeboxHotTrackList> packet;
		packet.Write<u16>((u16)ARRAY_COUNT(MEASURED_JUKEBOX_HOT_TRACKS));
		for(const i32 t : MEASURED_JUKEBOX_HOT_TRACKS) packet.Write<i32>(t);
		SendPacket(clientHd, packet);
	}

	SongID songID = actor.currentSong.songID;
	if(songID == SongID::INVALID) {
		songID = SongID::Default; // send lobby song by default
	}

	SendJukeboxPlay(clientHd, songID, actor.currentSong.requesterNick.data(), actor.playPosition);
	SendJukeboxQueue(clientHd, actor.tracks.data(), actor.tracks.size());
}

void HubReplication::SendActorDestroy(ClientHandle clientHd, ActorUID actorUID)
{
	const i32 clientID = plidMap->Get(clientHd);

	auto found = playerLocalInfo[clientID].localActorIDMap.find(actorUID);
	ASSERT(found != playerLocalInfo[clientID].localActorIDMap.end());
	const LocalActorID localActorID = found->second;

	Sv::SN_DestroyEntity packet;
	packet.objectID = localActorID;
	SendPacket(clientHd, packet);
}

void HubReplication::SendJukeboxPlay(ClientHandle clientHd, SongID songID, const wchar* requesterNick, i32 playPosInSec)
{
	PacketWriter<Sv::SN_JukeboxPlay> packet;

	packet.Write<i32>(0); // result
	packet.Write<SongID>(songID); // trackID
	packet.WriteStringObj(requesterNick); // nickname
	packet.Write<u16>(playPosInSec); // playPositionSec

	SendPacket(clientHd, packet);
}

void HubReplication::SendJukeboxQueue(ClientHandle clientHd, const ActorJukebox::Track* tracks, const i32 trackCount)
{
	PacketWriter<Sv::SN_JukeboxEnqueuedList> packet;

	packet.Write<u16>(trackCount); // trackList_count
	for(int i = 0; i < trackCount; i++) {
		packet.Write<SongID>(tracks[i].songID);
		packet.WriteStringObj(tracks[i].requesterNick.data(), tracks[i].requesterNick.size());
	}

	SendPacket(clientHd, packet);
}

void HubReplication::SendMasterSkillSlots(ClientHandle clientHd, const HubReplication::ActorPlayer& actor)
{
	DBG_ASSERT(actor.actorUID != ActorUID::INVALID);
	const LocalActorID localActorID = GetLocalActorID(clientHd, actor.actorUID);
	ASSERT(localActorID != LocalActorID::INVALID);

	const GameXmlContent& content = GetGameXmlContent();

	// SN_PlayerSkillSlot
	{
		PacketWriter<Sv::SN_PlayerSkillSlot> packet;

		packet.Write<LocalActorID>(localActorID); // characterID

		const GameXmlContent::Master& master = content.GetMaster(actor.classType);
		ASSERT(master.skillUnlocked.size() == master.skillIDs.size());

		packet.Write<u16>(master.skillIDs.size());
		for(size_t i = 0; i < master.skillIDs.size(); i++) {
			packet.Write<SkillID>(master.skillIDs[i]);
			packet.Write<u32>(0);
			packet.Write<u8>(1);
			packet.Write<u16>(0);
			packet.Write<u8>(master.skillUnlocked[i]);
			packet.Write<u8>(1);
		}

		packet.Write<SkillID>(SkillID::INVALID); // stageSkillIndex1
		packet.Write<SkillID>(SkillID::INVALID); // stageSkillIndex2
		packet.Write<SkillID>(master.skillIDs[0]); // currentSkillSlot1
		packet.Write<SkillID>(master.skillIDs[1]); // currentSkillSlot2
		packet.Write<SkillID>(master.skillIDs.back()); // shirkSkillSlot

		SendPacket(clientHd, packet);
	}
}

void HubReplication::SendInitialFrame(ClientHandle clientHd)
{
	SendPacketData<Sv::SN_ScanEnd>(clientHd, 0, nullptr);
	SendTownBurst(clientHd);
}

void HubReplication::SendSelfExp(ClientHandle clientHd)
{
	const Account* account = playerAccount[plidMap->Get(clientHd)];
	if(!account) return;
	foreach_const(p, frameCur->playerList) {
		if(p->parentActorUID != ActorUID::INVALID) continue;
		const LocalActorID localActorID = GetLocalActorID(clientHd, p->actorUID);
		if(localActorID == LocalActorID::INVALID || (u32)localActorID >= (u32)LocalActorID::FIRST_OTHER_PLAYER) continue;
		PacketWriter<Sv::SN_Exp> packet;
		packet.Write<LocalActorID>(localActorID);
		packet.WriteStringObj(account->nickname.data());
		packet.Write<u8>(0);
		packet.Write<u16>(account->level);
		packet.Write<u32>(account->levelExp);
		SendPacket(clientHd, packet);
		return;
	}
}

void HubReplication::SendMasterRotation(ClientHandle clientHd)
{
	PacketWriter<Sv::SN_MasterRotationInfo> packet;
	packet.Write<i32>(0);
	eastl::fixed_vector<CreatureIndex,100,false> freeList;
	foreach_const(m, GetGameXmlContent().masters) {
		if(m->skillIDs.empty() || m->skinIDs.empty() || m->defaultWeaponIDs.empty() || freeList.full()) continue;
		freeList.push_back(CreatureIndex(100000000 + (i32)m->classType));
	}
	packet.WriteVec(freeList.data(), freeList.size());
	packet.WriteVec((CreatureIndex*)nullptr, 0);
	packet.WriteVec((CreatureIndex*)nullptr, 0);
	SendPacket(clientHd, packet);
}

void HubReplication::SendPlayerStateInTown(ClientHandle clientHd, ActorUID actorUID, i8 state, const u8* modes, i32 modeCount)
{
	const LocalActorID localActorID = GetLocalActorID(clientHd, actorUID);
	if(localActorID == LocalActorID::INVALID) return;
	PacketWriter<Sv::SN_PlayerStateInTown> packet;
	packet.Write<LocalActorID>(localActorID);
	packet.Write<i8>(state);
	packet.Write<u16>((u16)modeCount);
	for(i32 i = 0; i < modeCount; i++) packet.Write<u8>(modes[i]);
	SendPacket(clientHd, packet);
}

void HubReplication::SendTownBurst(ClientHandle clientHd)
{
	const Account* account = playerAccount[plidMap->Get(clientHd)];

	{
		PacketWriter<Sv::SN_PveTodayStageInfo> packet;
		packet.Write<u16>(0);
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_FatiguePointInfo> packet;
		packet.Write<u16>(0);
		packet.Write<u16>(MEASURED_MAX_FATIGUE_POINT);
		SendPacket(clientHd, packet);
	}
	SendSelfExp(clientHd);
	SendUpdateEntrySystem(clientHd);
	{
		PacketWriter<Sv::SN_GameEventActivated> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_GameEventPending> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	if(account) SendMissionList(clientHd, *account);
	SendCalendarSchedule(clientHd);
	SendMasterRotation(clientHd);
	{
		PacketWriter<Sv::SN_AddictionWarning> packet;
		packet.Write<u8>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_NotifyAasRestricted> packet;
		packet.Write<u8>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_ItemLifetimeExpired> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_EffectLifetimeExpired> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_EffectLifetimeList> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_CurrentGuideMission> packet;
		packet.Write<u8>(0xff);
		packet.Write<u8>(0);
		packet.Write<u8>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_TownHudStatistics> packet;
		const u32 wins = account ? account->wins : 0;
		const u32 draws = account ? account->draws : 0;
		const u32 losses = account ? account->losses : 0;
		packet.Write<u8>((u8)GameModeType::GAME_MODE_DEATH_MATCH);
		packet.Write<u8>((u8)GameType::PVP_NORMAL);
		packet.Write<u16>(4);
		packet.Write<i32>(wins + losses > 0 ? (i32)((u64)wins * 10000 / (wins + losses)) : 0);
		packet.Write<i32>((i32)wins);
		packet.Write<i32>((i32)draws);
		packet.Write<i32>((i32)losses);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_ServerUtcTime> packet;
		packet.Write<i64>(FILETIME_UNIX_EPOCH_100NS + (i64)time(nullptr) * 10000000LL);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_GmDisabledMasters> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_GmDisabledSkins> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_GmDisabledStages> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	SendSelfExp(clientHd);
	{
		PacketWriter<Sv::SN_HudEventList> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	{
		PacketWriter<Sv::SN_CshopEventList> packet;
		packet.Write<u16>(0);
		SendPacket(clientHd, packet);
	}
	foreach_const(p, frameCur->playerList) {
		if(p->parentActorUID != ActorUID::INVALID) continue;
		const LocalActorID localActorID = GetLocalActorID(clientHd, p->actorUID);
		if(localActorID == LocalActorID::INVALID || (u32)localActorID >= (u32)LocalActorID::FIRST_OTHER_PLAYER) continue;
		SendPlayerStateInTown(clientHd, p->actorUID, 0, nullptr, 0);
		break;
	}
}

void HubReplication::CreateLocalActorID(ClientHandle clientHd, ActorUID actorUID)
{
	const i32 clientID = plidMap->Get(clientHd);

	PlayerLocalInfo& localInfo = playerLocalInfo[clientID];
	auto& localActorIDMap = localInfo.localActorIDMap;
	localActorIDMap.emplace(actorUID, localInfo.nextPlayerLocalActorID);
	localInfo.nextPlayerLocalActorID = (LocalActorID)((u32)localInfo.nextPlayerLocalActorID + 1);
	// TODO: find first free LocalActorID

	// TODO: start at 5000 for NPCs? Does it even matter?
}

void HubReplication::DeleteLocalActorID(ClientHandle clientHd, ActorUID actorUID)
{
	const i32 clientID = plidMap->Get(clientHd);

	auto& localActorIDMap = playerLocalInfo[clientID].localActorIDMap;
	localActorIDMap.erase(localActorIDMap.find(actorUID));
}

bool HubReplication::Frame::Transform::HasNotChanged(const Frame::Transform& other) const
{
	const f32 posEpsilon = 0.1f;
	if(fabs(pos.x - other.pos.x) > posEpsilon) return false;
	if(fabs(pos.y - other.pos.y) > posEpsilon) return false;
	if(fabs(pos.z - other.pos.z) > posEpsilon) return false;
	if(fabs(dir.x - other.dir.x) > posEpsilon) return false;
	if(fabs(dir.y - other.dir.y) > posEpsilon) return false;
	if(fabs(dir.z - other.dir.z) > posEpsilon) return false;
	if(fabs(eye.x - other.eye.x) > posEpsilon) return false;
	if(fabs(eye.y - other.eye.y) > posEpsilon) return false;
	if(fabs(eye.z - other.eye.z) > posEpsilon) return false;
	/*const f32 rotEpsilon = 0.01f;
	if(fabs(rotate - other.rotate) > rotEpsilon) return false;*/
	const f32 speedEpsilon = 0.1f;
	if(fabs(speed - other.speed) > speedEpsilon) return false;
	return true;
}

bool HubReplication::Frame::ActionState::HasNotChanged(const HubReplication::Frame::ActionState& other) const
{
	if(other.actionState == ActionStateID::INVALID) return true;
	if(actionState != other.actionState) return false;
	if(actionParam1 != other.actionParam1) return false;
	if(actionParam2 != other.actionParam2) return false;
	return true;
}
