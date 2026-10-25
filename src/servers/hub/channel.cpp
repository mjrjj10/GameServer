#include "channel.h"
#include "guild.h"

#include "coordinator.h"
#include <common/packet_validator.h>
#include <mxm/game_content.h>
#include "game.h"

bool HubPacketHandler::Init(HubGame* game_)
{
	game = game_;
	replication = &game->replication;
	server = game->replication.server;
	return true;
}

void HubPacketHandler::OnClientsConnected(const eastl::pair<ClientHandle, const Account*>* clientList, const i32 count)
{
	for(int i = 0; i < count; i++) {
		auto& it = clientList[i];
		game->OnPlayerConnect(it.first, it.second);
	}
}

void HubPacketHandler::OnClientsDisconnected(const ClientHandle* clientList, const i32 count)
{
	for(int i = 0; i < count; i++) {
		ClientHandle clientHd = clientList[i];
		game->OnPlayerDisconnect(clientHd);
	}
}

void HubPacketHandler::OnNewPacket(ClientHandle clientHd, const NetHeader& header, const u8* packetData)
{
	const i32 packetSize = header.size - sizeof(NetHeader);

#define HANDLE_CASE(PACKET) case Cl::PACKET::NET_ID: { HandlePacket_##PACKET(clientHd, header, packetData, packetSize); } break

	switch(header.netID) {
		HANDLE_CASE(CQ_GetGuildProfile);
		HANDLE_CASE(CQ_GetGuildMemberList);
		HANDLE_CASE(CQ_GetGuildHistoryList);
		HANDLE_CASE(CQ_GetGuildRankingSeasonList);
		HANDLE_CASE(CQ_TierRecord);

		HANDLE_CASE(CN_ReadyToLoadCharacter);
		HANDLE_CASE(CN_ReadyToLoadGameMap);
		HANDLE_CASE(CN_EnterCityScene);
		HANDLE_CASE(CQ_ReturnToCity);
		HANDLE_CASE(CQ_PartyInvite);
		HANDLE_CASE(CQ_PartyInviteAccept);
		HANDLE_CASE(CQ_BreakUpPartyRoom);
		HANDLE_CASE(CQ_LeavePartyRoom);
		HANDLE_CASE(CQ_CancelMatching);
		HANDLE_CASE(CQ_FriendRequest);
		HANDLE_CASE(CQ_FriendRequestRespond);
		HANDLE_CASE(CQ_FriendRemove);
		HANDLE_CASE(CQ_FriendSetFavorite);
		HANDLE_CASE(CQ_FriendUnsetFavorite);
		HANDLE_CASE(CQ_CommunityBlockAdd);
		HANDLE_CASE(CQ_CommunityBlockRemove);
		HANDLE_CASE(CQ_CommunityStatusMessage);
		HANDLE_CASE(CQ_Praise);
		HANDLE_CASE(CQ_Honor);
		HANDLE_CASE(CA_SetGameGvt);
		HANDLE_CASE(CA_CityLobbyJoinCity);
		HANDLE_CASE(CQ_GetCharacterInfo);
		HANDLE_CASE(CN_GamePlayerSyncByInt);
		HANDLE_CASE(CN_ChannelChatMessage);
		HANDLE_CASE(CQ_SetLeaderCharacter);
		HANDLE_CASE(CN_GamePlayerSyncActionStateOnly);
		HANDLE_CASE(CQ_JukeboxQueueSong);
		HANDLE_CASE(CQ_WhisperSend);
		HANDLE_CASE(CQ_PartyCreate);
		HANDLE_CASE(CQ_RTT_Time);
		HANDLE_CASE(CQ_LoadingProgressData);
		HANDLE_CASE(CQ_RequestCalendar);
		HANDLE_CASE(CQ_RequestAreaPopularity);
		HANDLE_CASE(CQ_PartyModify);
		HANDLE_CASE(CQ_PartyOptionModify);
		HANDLE_CASE(CQ_EnqueueGame);
		HANDLE_CASE(CA_SortieRoomFound);
		HANDLE_CASE(CN_SortieRoomConfirm);
		HANDLE_CASE(CQ_MailShowList);
		HANDLE_CASE(CQ_MailRead);
		HANDLE_CASE(CQ_RequestMissionReward);
		HANDLE_CASE(CQ_DailyMissionChange);
		HANDLE_CASE(CQ_MailWrite);
		HANDLE_CASE(CQ_MailMove);

		default: {
			NT_LOG("[client%x] Client :: Unknown packet :: size=%d netID=%d", clientHd, header.size, header.netID);
		} break;
	}

#undef HANDLE_CASE
}

void HubPacketHandler::OnMatchmakerPacket(const NetHeader& header, const u8* packetData)
{
	const i32 packetSize = header.size - sizeof(NetHeader);

	switch(header.netID) {
		// TODO: move this one to connector
		case In::MR_Handshake::NET_ID: {
			const In::MR_Handshake resp = SafeCast<In::MR_Handshake>(packetData, packetSize);
			if(resp.result == 1) {
				LOG("[MM] Connected to Matchmaker server");
			}
			else {
				WARN("[MM] handshake failed (%d)", resp.result);
				ASSERT_MSG(0, "mm handshake failed"); // should not happen
			}
		} break;

		case In::MR_PartyCreated::NET_ID: {
			const In::MR_PartyCreated resp = SafeCast<In::MR_PartyCreated>(packetData, packetSize);
			game->MmOnPartyCreated(resp.partyUID, resp.leader);
		} break;

		case In::MR_PartyEnqueued::NET_ID: {
			const In::MR_PartyEnqueued resp = SafeCast<In::MR_PartyEnqueued>(packetData, packetSize);
			game->MmOnPartyEnqueued(resp.partyUID);
		} break;

		case In::MN_MatchingPartyFound::NET_ID: {
			const In::MN_MatchingPartyFound resp = SafeCast<In::MN_MatchingPartyFound>(packetData, packetSize);
			game->MmOnMatchFound(resp);
		} break;
	}
}

void HubPacketHandler::HandlePacket_CQ_GetGuildProfile(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_GetGuildProfile>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_GetGuildProfile (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_GetGuildProfile>(packetData, packetSize));

	// SA_GetGuildProfile
	{
		PacketWriter<Sv::SA_GetGuildProfile,512> packet;

		const Guild& guild = GetAlphaGuild();
		packet.Write<i32>(0); // result;
		packet.WriteStringObj(guild.name);
		packet.WriteStringObj(guild.tag);
		packet.Write<i32>(guild.emblemIndex);
		packet.Write<u8>(guild.lvl);
		packet.Write<u8>(guild.memberMax);
		packet.WriteStringObj(guild.ownerNickname);
		packet.Write<i64>(guild.createdDate);
		packet.Write<i64>(guild.dissolutionDate);
		packet.Write<u8>(guild.joinType);

		Sv::SA_GetGuildProfile::ST_GuildInterest guildInterest;
		guildInterest.likePveStage = 1;
		guildInterest.likeDefence = 1;
		guildInterest.likePvpNormal = 1;
		guildInterest.likePvpOccupy = 1;
		guildInterest.likePvpGot = 1;
		guildInterest.likePvpRank = 1;
		guildInterest.likeOlympic = 1;
		packet.Write(guildInterest);

		packet.WriteStringObj(guild.intro);
		packet.WriteStringObj(guild.notice);
		packet.Write<i32>(guild.point);
		packet.Write<i32>(guild.fund);

		Sv::SA_GetGuildProfile::ST_GuildPvpRecord guildPvpRecord;
		guildPvpRecord.rp = 0;
		guildPvpRecord.win = 0;
		guildPvpRecord.draw = 0;
		guildPvpRecord.lose = 0;
		packet.Write(guildPvpRecord);

		packet.Write<i32>(guild.rankNo);

		packet.Write<u16>((u16)GetAlphaGuildRanks().size());
		for(auto& rank : GetAlphaGuildRanks()) {
			packet.Write<i32>(rank.id);
			packet.Write<GuildRankType>(rank.type);
			packet.Write<u8>(rank.iconIndex);
			packet.WriteStringObj(rank.name);
			packet.Write(rank.rights);
		}

		packet.Write<u16>((u16)guild.skills.size());
		for(auto& skill : guild.skills) {
			packet.Write<u8>((u8)skill.type);
			packet.Write<u8>(skill.level);
			packet.Write<i64>(0); // expiryDate
			packet.Write<u16>(0); // extensionCount
		}

		packet.Write<i32>(0); // curDailyStageGuildPoint
		packet.Write<i32>(guild.maxDailyStage);
		packet.Write<i32>(0); // curDailyArenaGuildPoint
		packet.Write<i32>(guild.maxDailyArena);
		packet.Write<u8>(0); // todayRollCallCount

		SendPacket(clientHd, packet);
	}
}

void HubPacketHandler::HandlePacket_CQ_GetGuildMemberList(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_GetGuildMemberList>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_GetGuildMemberList (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_GetGuildMemberList>(packetData, packetSize));

	// SA_GetGuildMemberList
	{
		PacketWriter<Sv::SA_GetGuildMemberList,512> packet;

		packet.Write<i32>(0); // result

		const Guild& guild = GetAlphaGuild();
		const i32 userID = game->plidMap->Get(clientHd);
		const WideString& nick = game->playerAccountData[userID]->nickname;
		packet.Write<u16>(2); // guildMemberProfileList_count

		packet.WriteStringObj(guild.owner.nick);
		packet.Write<i32>(guild.owner.membershipID);
		packet.Write<u16>(guild.owner.lvl);
		packet.Write<u16>(guild.owner.leaderClassType);
		packet.Write<u16>(guild.owner.masterCount);
		packet.Write<i32>(guild.owner.achievementScore);
		packet.Write<u8>(0); // topPvpTierGrade
		packet.Write<u16>(0); // topPvpTierPoint
		packet.Write<i32>(0); // contributedGuildPoint
		packet.Write<i32>(0); // contributedGuildFund
		packet.Write<u16>(0); // guildPvpWin
		packet.Write<u16>(0); // guildPvpPlay
		packet.Write<i64>(0); // lastLogoutDate

		packet.WriteStringObj(nick.data(), nick.size());
		packet.Write<i32>(guild.newMemberRankID); // membershipID
		packet.Write<u16>(1); // lvl
		packet.Write<u16>(0); // leaderClassType
		packet.Write<u16>(0); // masterCount
		packet.Write<i32>(0); // achievementScore
		packet.Write<u8>(0); // topPvpTierGrade
		packet.Write<u16>(0); // topPvpTierPoint
		packet.Write<i32>(0); // contributedGuildPoint
		packet.Write<i32>(0); // contributedGuildFund
		packet.Write<u16>(0); // guildPvpWin
		packet.Write<u16>(0); // guildPvpPlay
		packet.Write<i64>(0); // lastLogoutDate

		SendPacket(clientHd, packet);
	}
}

void HubPacketHandler::HandlePacket_CQ_GetGuildHistoryList(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_GetGuildHistoryList>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_GetGuildHistoryList (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_GetGuildHistoryList>(packetData, packetSize));

	// SA_GetGuildMemberList
	{
		PacketWriter<Sv::SA_GetGuildHistoryList> packet;

		packet.Write<i32>(0); // result

		packet.Write<u16>(0); // guildHistories_count

		SendPacket(clientHd, packet);
	}
}

void HubPacketHandler::HandlePacket_CQ_GetGuildRankingSeasonList(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_GetGuildRankingSeasonList>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_GetGuildRankingSeasonList (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_GetGuildRankingSeasonList& rank = SafeCast<Cl::CQ_GetGuildRankingSeasonList>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_GetGuildRankingSeasonList>(packetData, packetSize));

	// SA_GetGuildRankingSeasonList
	{
		PacketWriter<Sv::SA_GetGuildRankingSeasonList> packet;

		packet.Write<i32>(0); // result
		packet.Write<u8>(rank.rankingType); // result

		packet.Write<u16>(0); // rankingSeasonList_count

		SendPacket(clientHd, packet);
	}
}

void HubPacketHandler::HandlePacket_CQ_TierRecord(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_TierRecord>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_TierRecord (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_TierRecord>(packetData, packetSize));

	// SA_TierRecord
	{
		PacketWriter<Sv::SA_TierRecord> packet;

		packet.Write<u8>(1); // seasonId
		packet.Write<i32>(0); // allTierWin
		packet.Write<i32>(0); // allTierDraw
		packet.Write<i32>(0); // allTierLose
		packet.Write<i32>(0); // allTierLeave
		packet.Write<u16>(0); // stageRecordList_count

		SendPacket(clientHd, packet);
	}
}

void HubPacketHandler::HandlePacket_CN_ReadyToLoadCharacter(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CN_ReadyToLoadCharacter>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CN_ReadyToLoadCharacter (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CN_ReadyToLoadCharacter ::", clientHd);
	game->OnPlayerReadyToLoad(clientHd);
}

void HubPacketHandler::HandlePacket_CN_EnterCityScene(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CN_EnterCityScene>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CN_EnterCityScene (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CN_EnterCityScene ::", clientHd);
	game->OnPlayerEnterCity(clientHd);
}

void HubPacketHandler::HandlePacket_CQ_ReturnToCity(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	NT_LOG("[client%x] Client :: CQ_ReturnToCity ::", clientHd);
	game->OnReturnToCity(clientHd);
}

void HubPacketHandler::HandlePacket_CQ_PartyInvite(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	ConstBuffer buff(packetData, packetSize);
	if(!buff.CanRead(2)) {
		WARN("[client%x] WARNING: invalid CQ_PartyInvite (size=%d)", clientHd, packetSize);
		return;
	}
	const u16 len = buff.Read<u16>();
	if(len == 0 || len > 32 || packetSize != 2 + len * 2 + 4) {
		WARN("[client%x] WARNING: invalid CQ_PartyInvite (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_PartyInvite>(packetData, packetSize));
	game->OnPartyInvite(clientHd, (const wchar*)buff.ReadRaw(len * 2), len);
}

void HubPacketHandler::HandlePacket_CQ_PartyInviteAccept(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	ConstBuffer buff(packetData, packetSize);
	if(!buff.CanRead(10)) {
		WARN("[client%x] WARNING: invalid CQ_PartyInviteAccept (size=%d)", clientHd, packetSize);
		return;
	}
	const i32 partyID = buff.Read<i32>();
	buff.Read<u32>();
	const u16 len = buff.Read<u16>();
	if(len > 32 || packetSize != 10 + len * 2) {
		WARN("[client%x] WARNING: invalid CQ_PartyInviteAccept (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_PartyInviteAccept>(packetData, packetSize));
	game->OnPartyInviteAccept(clientHd, partyID);
}

void HubPacketHandler::HandlePacket_CQ_BreakUpPartyRoom(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	NT_LOG("[client%x] Client :: CQ_BreakUpPartyRoom :: size=%d", clientHd, packetSize);
	game->OnPartyBreakUp(clientHd);
}

void HubPacketHandler::HandlePacket_CQ_LeavePartyRoom(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	NT_LOG("[client%x] Client :: CQ_LeavePartyRoom :: size=%d", clientHd, packetSize);
	game->OnPartyLeave(clientHd);
}

void HubPacketHandler::HandlePacket_CQ_CancelMatching(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	NT_LOG("[client%x] Client :: CQ_CancelMatching :: size=%d", clientHd, packetSize);
	game->OnCancelMatching(clientHd);
}

static bool ReadNickname(const u8* packetData, i32 packetSize, i32 trailing, WideString* out)
{
	ConstBuffer buff(packetData, packetSize);
	if(!buff.CanRead(2)) return false;
	const u16 len = buff.Read<u16>();
	if(len == 0 || len > 32 || packetSize != 2 + len * 2 + trailing) return false;
	out->assign((const wchar*)buff.ReadRaw(len * 2), len);
	return true;
}

void HubPacketHandler::HandlePacket_CQ_FriendRequest(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 0, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_FriendRequest (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_FriendRequest>(packetData, packetSize));
	game->OnFriendRequest(clientHd, nickname);
}

void HubPacketHandler::HandlePacket_CQ_FriendRequestRespond(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 1, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_FriendRequestRespond (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_FriendRequestRespond>(packetData, packetSize));
	game->OnFriendRespond(clientHd, nickname, packetData[packetSize - 1] != 0);
}

void HubPacketHandler::HandlePacket_CQ_FriendRemove(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 0, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_FriendRemove (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_FriendRemove>(packetData, packetSize));
	game->OnFriendRemove(clientHd, nickname);
}

void HubPacketHandler::HandlePacket_CQ_FriendSetFavorite(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 0, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_FriendSetFavorite (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CQ_FriendSetFavorite :: nickname='%ls'", clientHd, nickname.data());
	game->OnFriendFavorite(clientHd, nickname, true);
}

void HubPacketHandler::HandlePacket_CQ_FriendUnsetFavorite(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 0, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_FriendUnsetFavorite (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CQ_FriendUnsetFavorite :: nickname='%ls'", clientHd, nickname.data());
	game->OnFriendFavorite(clientHd, nickname, false);
}

void HubPacketHandler::HandlePacket_CQ_CommunityBlockAdd(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 0, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_CommunityBlockAdd (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CQ_CommunityBlockAdd :: nickname='%ls'", clientHd, nickname.data());
	game->OnBlock(clientHd, nickname, true);
}

void HubPacketHandler::HandlePacket_CQ_CommunityBlockRemove(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 0, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_CommunityBlockRemove (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CQ_CommunityBlockRemove :: nickname='%ls'", clientHd, nickname.data());
	game->OnBlock(clientHd, nickname, false);
}

void HubPacketHandler::HandlePacket_CQ_CommunityStatusMessage(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	ConstBuffer buff(packetData, packetSize);
	if(!buff.CanRead(2)) {
		WARN("[client%x] WARNING: invalid CQ_CommunityStatusMessage (size=%d)", clientHd, packetSize);
		return;
	}
	const u16 len = buff.Read<u16>();
	if(len > 60 || packetSize != 2 + len * 2) {
		WARN("[client%x] WARNING: invalid CQ_CommunityStatusMessage (size=%d)", clientHd, packetSize);
		return;
	}
	WideString message;
	message.assign((const wchar*)buff.ReadRaw(len * 2), len);
	NT_LOG("[client%x] Client :: CQ_CommunityStatusMessage :: message='%ls'", clientHd, message.data());
	game->OnStatusMessage(clientHd, message);
}

void HubPacketHandler::HandlePacket_CQ_Honor(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	WideString nickname;
	if(!ReadNickname(packetData, packetSize, 0, &nickname)) {
		WARN("[client%x] WARNING: invalid CQ_Honor (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CQ_Honor :: nickname='%ls'", clientHd, nickname.data());
	game->OnHonor(clientHd, nickname);
}

void HubPacketHandler::HandlePacket_CQ_Praise(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	ConstBuffer buff(packetData, packetSize);
	if(!buff.CanRead(2)) {
		WARN("[client%x] WARNING: invalid CQ_Praise (size=%d)", clientHd, packetSize);
		return;
	}
	const u16 len = buff.Read<u16>();
	if(len == 0 || len > 32 || !buff.CanRead(len * 2) || packetSize != 2 + len * 2) {
		WARN("[client%x] WARNING: invalid CQ_Praise (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_Praise>(packetData, packetSize));
	game->OnPraise(clientHd, (const wchar*)buff.ReadRaw(len * 2), len);
}

void HubPacketHandler::HandlePacket_CN_ReadyToLoadGameMap(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CN_ReadyToLoadGameMap>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CN_ReadyToLoadGameMap (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CN_ReadyToLoadGame ::", clientHd);
	game->OnPlayerReadyToLoad(clientHd);
}

void HubPacketHandler::HandlePacket_CA_SetGameGvt(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CA_SetGameGvt>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CA_SetGameGvt (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CA_SetGameGvt& gvt = SafeCast<Cl::CA_SetGameGvt>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CA_SetGameGvt :: sendTime=%d virtualTime=%d unk=%d", clientHd, gvt.sendTime, gvt.virtualTime, gvt.unk);
}

void HubPacketHandler::HandlePacket_CA_CityLobbyJoinCity(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CA_CityLobbyJoinCity>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CA_CityLobbyJoinCity (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CA_CityLobbyJoinCity ::", clientHd);
	replication->SetPlayerAsInGame(clientHd);
}

void HubPacketHandler::HandlePacket_CQ_GetCharacterInfo(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_GetCharacterInfo>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_GetCharacterInfo (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_GetCharacterInfo& req = SafeCast<Cl::CQ_GetCharacterInfo>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_GetCharacterInfo :: characterID=0x%08x", clientHd, (u32)req.characterID);

	ActorUID actorUID = replication->GetWorldActorUID(clientHd, req.characterID);
	if(actorUID == ActorUID::INVALID) {
		WARN("Client sent an invalid actor (localActorID=%d)", req.characterID);
		return;
	}

	game->OnPlayerGetCharacterInfo(clientHd, actorUID);
}

void HubPacketHandler::HandlePacket_CN_GamePlayerSyncByInt(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CN_GamePlayerSyncByInt>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CN_GamePlayerSyncByInt (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CN_GamePlayerSyncByInt& update = SafeCast<Cl::CN_GamePlayerSyncByInt>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CN_GamePlayerSyncByInt :: { characterID=0x%08x p3nPos=(%g, %g, %g) p3nDir=(%g, %g, %g) p3nEye=(%g, %g, %g) nRotate=%g nSpeed=%g nState=%d nActionIDX=%d", clientHd, (u32)update.characterID, update.p3nPos.x, update.p3nPos.y, update.p3nPos.z, update.p3nDir.x, update.p3nDir.y, update.p3nDir.z, update.p3nEye.x, update.p3nEye.y, update.p3nEye.z, update.nRotate, update.nSpeed, (i32)update.nState, update.nActionIDX);

	ActorUID actorUID = replication->GetWorldActorUID(clientHd, update.characterID);
	if(actorUID == ActorUID::INVALID) {
		WARN("Client sent an invalid actor (localActorID=%d)", update.characterID);
		return;
	}

	game->OnPlayerUpdatePosition(clientHd, actorUID, f2v(update.p3nPos), f2v(update.p3nDir), f2v(update.p3nEye), update.nRotate, update.nSpeed, update.nState, update.nActionIDX);
}

void HubPacketHandler::HandlePacket_CN_ChannelChatMessage(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CN_ChannelChatMessage>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CN_ChannelChatMessage (size=%d)", clientHd, packetSize);
		return;
	}
	ConstBuffer buff(packetData, packetSize);
	i32 chatType = buff.Read<i32>();
	const u16 msgLen = buff.Read<u16>();
	const wchar* msg = (wchar*)buff.ReadRaw(msgLen * 2);

	NT_LOG("[client%x] Client :: CN_ChannelChatMessage :: chatType=%d msg='%.*S'", clientHd, chatType, msgLen, msg);

	game->OnPlayerChatMessage(clientHd, chatType, msg, msgLen);
}

void HubPacketHandler::HandlePacket_CQ_SetLeaderCharacter(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_SetLeaderCharacter>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_SetLeaderCharacter (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_SetLeaderCharacter& leader = SafeCast<Cl::CQ_SetLeaderCharacter>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_SetLeaderCharacter :: characterID=0x%08x skinIndex=%d", clientHd, (u32)leader.characterID, (i32)leader.skinIndex);

	game->OnPlayerSetLeaderCharacter(clientHd, leader.characterID, leader.skinIndex);
}

void HubPacketHandler::HandlePacket_CN_GamePlayerSyncActionStateOnly(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CN_GamePlayerSyncActionStateOnly>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CN_GamePlayerSyncActionStateOnly (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CN_GamePlayerSyncActionStateOnly& sync = SafeCast<Cl::CN_GamePlayerSyncActionStateOnly>(packetData, packetSize);

	const char* stateStr = ActionStateToString(sync.state);

	NT_LOG("[client%x] Client :: CN_GamePlayerSyncActionStateOnly :: {", clientHd);
	NT_LOG("	characterID=0x%08x", (u32)sync.characterID);
	NT_LOG("	nState=%d (%s)", (i32)sync.state, stateStr);
	NT_LOG("	bApply=%d", sync.bApply);
	NT_LOG("	param1=%d", sync.param1);
	NT_LOG("	param2=%d", sync.param2);
	NT_LOG("	i4=%d", sync.i4);
	NT_LOG("	rotate=%g", sync.rotate);
	NT_LOG("	upperRotate=%g", sync.upperRotate);
	NT_LOG("}");

	ActorUID actorUID = replication->GetWorldActorUID(clientHd, sync.characterID);
	if(actorUID == ActorUID::INVALID) {
		WARN("Client sent an invalid actor (localActorID=%d)", sync.characterID);
		return;
	}

	game->OnPlayerSyncActionState(clientHd, actorUID, sync.state, sync.param1, sync.param2, sync.rotate, sync.upperRotate);
}

void HubPacketHandler::HandlePacket_CQ_JukeboxQueueSong(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_JukeboxQueueSong>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_JukeboxQueueSong (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_JukeboxQueueSong& queue = SafeCast<Cl::CQ_JukeboxQueueSong>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_JukeboxQueueSong :: { songID=%d }", clientHd, (i32)queue.songID);

	game->OnPlayerJukeboxQueueSong(clientHd, queue.songID);
}

void HubPacketHandler::HandlePacket_CQ_WhisperSend(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_WhisperSend>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_WhisperSend (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_WhisperSend>(packetData, packetSize));
	ConstBuffer buff(packetData, packetSize);
	WideString destNick;
	eastl::fixed_string<wchar,256,true> msg;

	const u16 destNick_len = buff.Read<u16>();
	const wchar* destNick_str = (wchar*)buff.ReadRaw(destNick_len * sizeof(wchar));
	const u16 msg_len = buff.Read<u16>();
	const wchar* msg_str = (wchar*)buff.ReadRaw(msg_len * sizeof(wchar));

	destNick.assign(destNick_str, destNick_len);
	msg.assign(msg_str, msg_len);

	game->OnPlayerChatWhisper(clientHd, destNick.data(), msg.data());
}

void HubPacketHandler::HandlePacket_CQ_RTT_Time(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_RTT_Time>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_RTT_Time (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_RTT_Time& rtt = SafeCast<Cl::CQ_RTT_Time>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_RTT_Time :: { time=%u }", clientHd, rtt.time);

	Sv::SA_RTT_Time answer;
	answer.clientTimestamp = rtt.time;
	answer.serverTimestamp = (i64)TimeDiffMs(TimeRelNow());
	SendPacket(clientHd, answer);
}

void HubPacketHandler::HandlePacket_CQ_LoadingProgressData(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_LoadingProgressData>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_LoadingProgressData (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_LoadingProgressData& loading = SafeCast<Cl::CQ_LoadingProgressData>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_LoadingProgressData :: { progress=%u }", clientHd, loading.progress);
}

void HubPacketHandler::HandlePacket_CQ_RequestCalendar(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_RequestCalendar>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_RequestCalendar (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_RequestCalendar& req = SafeCast<Cl::CQ_RequestCalendar>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_RequestCalendar :: { filetimeUTC=%llu }", clientHd, req.filetimeUTC);

	replication->SendCalendar(clientHd);
}

void HubPacketHandler::HandlePacket_CQ_RequestAreaPopularity(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_RequestAreaPopularity>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_RequestAreaPopularity (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_RequestAreaPopularity& req = SafeCast<Cl::CQ_RequestAreaPopularity>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_RequestAreaPopularity :: { area=%d }", clientHd, (i32)req.areaID);

	replication->SendAreaPopularity(clientHd, req.areaID);
}

void HubPacketHandler::HandlePacket_CQ_PartyCreate(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_PartyCreate>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_PartyCreate (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_PartyCreate& create = SafeCast<Cl::CQ_PartyCreate>(packetData, packetSize);
	NT_LOG("[client%x] Client :: CQ_PartyCreate :: { entrySysID=%d stageType=%d }", clientHd, create.entrySysID, create.stageType);

	game->OnCreateParty(clientHd, create.entrySysID, create.stageType);
}

void HubPacketHandler::HandlePacket_CQ_PartyModify(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_PartyModify>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_PartyModify (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_PartyModify>(packetData, packetSize));

	ConstBuffer buff(packetData, packetSize);
	const u16 stageCount = buff.Read<u16>();
	if(buff.CanRead((i32)stageCount * 4 + 4)) {
		buff.ReadRaw((i32)stageCount * 4);
		game->OnPartyModify(clientHd, (GameType)buff.Read<i32>());
	}

	Sv::SA_PartyModify packet;
	packet.retval = 0;
	SendPacket(clientHd, packet);
}

void HubPacketHandler::HandlePacket_CQ_PartyOptionModify(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_PartyOptionModify>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_PartyOptionModify (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_PartyOptionModify& req = SafeCast<Cl::CQ_PartyOptionModify>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_PartyOptionModify>(packetData, packetSize));

	Sv::SA_PartyOptionModify packet;
	packet.retval = 0;
	packet.option = req.option;
	packet.enable = req.enable;
	SendPacket(clientHd, packet);
}

void HubPacketHandler::HandlePacket_CQ_EnqueueGame(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_EnqueueGame>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_EnqueueGame (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: CQ_EnqueueGame :: { }", clientHd);

	game->OnEnqueueGame(clientHd);
}

void HubPacketHandler::HandlePacket_CA_SortieRoomFound(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CA_SortieRoomFound>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CA_SortieRoomFound (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CA_SortieRoomFound& packet = SafeCast<Cl::CA_SortieRoomFound>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CA_SortieRoomFound>(packetData, packetSize));

	game->OnSortieRoomFound(clientHd, packet.sortieID);
}

void HubPacketHandler::HandlePacket_CN_SortieRoomConfirm(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CN_SortieRoomConfirm>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CN_SortieRoomConfirm (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CN_SortieRoomConfirm& packet = SafeCast<Cl::CN_SortieRoomConfirm>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CN_SortieRoomConfirm>(packetData, packetSize));

	game->OnSortieRoomConfirm(clientHd, packet.confirm);
}

void HubPacketHandler::HandlePacket_CQ_MailShowList(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_MailShowList>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_MailShowList (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_MailShowList& packet = SafeCast<Cl::CQ_MailShowList>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_MailShowList>(packetData, packetSize));

	game->OnMailShowList(clientHd, packet.mailboxType);
}

void HubPacketHandler::HandlePacket_CQ_MailRead(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_MailRead>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_MailRead (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_MailRead& packet = SafeCast<Cl::CQ_MailRead>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_MailRead>(packetData, packetSize));

	game->OnMailRead(clientHd, packet.target_user_id_u64);
}

void HubPacketHandler::HandlePacket_CQ_RequestMissionReward(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_RequestMissionReward>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_RequestMissionReward (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_RequestMissionReward& packet = SafeCast<Cl::CQ_RequestMissionReward>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_RequestMissionReward>(packetData, packetSize));

	game->OnMissionReward(clientHd, (i32)packet.missionIndex);
}

void HubPacketHandler::HandlePacket_CQ_DailyMissionChange(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_DailyMissionChange>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_DailyMissionChange (size=%d)", clientHd, packetSize);
		return;
	}
	const Cl::CQ_DailyMissionChange& packet = SafeCast<Cl::CQ_DailyMissionChange>(packetData, packetSize);
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_DailyMissionChange>(packetData, packetSize));

	game->OnDailyMissionChange(clientHd, (i32)packet.step);
}

void HubPacketHandler::HandlePacket_CQ_MailWrite(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_MailWrite>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_MailWrite (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_MailWrite>(packetData, packetSize));
	ConstBuffer buff(packetData, packetSize);
	const u16 toLen = buff.Read<u16>();
	const wchar* to = (const wchar*)buff.ReadRaw(toLen * sizeof(wchar));
	const u16 subjectLen = buff.Read<u16>();
	const wchar* subject = (const wchar*)buff.ReadRaw(subjectLen * sizeof(wchar));
	const u16 contentLen = buff.Read<u16>();
	const wchar* content = (const wchar*)buff.ReadRaw(contentLen * sizeof(wchar));

	WideString toNick;
	toNick.assign(to, toLen);
	game->OnMailWrite(clientHd, toNick.data(), subject, subjectLen, content, contentLen);
}

void HubPacketHandler::HandlePacket_CQ_MailMove(ClientHandle clientHd, const NetHeader& header, const u8* packetData, const i32 packetSize)
{
	if(!ValidatePacket<Cl::CQ_MailMove>(packetData, packetSize)) {
		WARN("[client%x] WARNING: invalid CQ_MailMove (size=%d)", clientHd, packetSize);
		return;
	}
	NT_LOG("[client%x] Client :: %s", clientHd, PacketSerialize<Cl::CQ_MailMove>(packetData, packetSize));
	ConstBuffer buff(packetData, packetSize);
	const u8 moveType = buff.Read<u8>();
	const u16 count = buff.Read<u16>();
	eastl::fixed_vector<u64,64,true> ids;
	for(u16 i = 0; i < count; i++) ids.push_back(buff.Read<u64>());

	game->OnMailMove(clientHd, moveType, ids.data(), (i32)ids.size());
}
