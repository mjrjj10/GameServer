#include "game.h"
#include "coordinator.h"
#include "guild_store.h"
#include "town_rules.h"
#include "mission_rules.h"
#include <mxm/game_content.h>
#include <mxm/account_progress.h>
#include <EASTL/algorithm.h>
#include <time.h>

static i64 NowFiletime()
{
	return FILETIME_UNIX_EPOCH_100NS + (i64)time(nullptr) * 10000000LL;
}

template<typename String>
static bool ReadWideString(ConstBuffer& buff, String* out, i32 maxLen)
{
	if(!buff.CanRead(2)) return false;
	const u16 len = buff.Read<u16>();
	if(len > maxLen || !buff.CanRead(len * 2)) return false;
	out->assign((const wchar*)buff.ReadRaw(len * 2), len);
	return true;
}

static bool ValidGuildText(const WideString& text, i32 minLen, i32 maxLen)
{
	if((i32)text.size() < minLen || (i32)text.size() > maxLen) return false;
	foreach_const(c, text) {
		const wchar w = *c;
		const bool letter = (w >= L'a' && w <= L'z') || (w >= L'A' && w <= L'Z') || (w >= L'0' && w <= L'9');
		if(!letter) return false;
	}
	return true;
}

static i32 GuildLevel(const GuildRecord& guild)
{
	return MAX(1, GetGameXmlContent().GuildLevelForPoints(guild.point));
}

static i32 GuildMemberMax(const GuildRecord&)
{
	return 10 + GetGameXmlContent().GuildSkillValue("GUILD_MEMBER_MAX_UP", 0);
}

static const GuildClass* FindGuildClass(const GuildRecord& guild, i32 membershipID)
{
	foreach_const(c, guild.classes) if(c->id == membershipID) return &*c;
	return nullptr;
}

static bool HasGuildRight(const GuildRecord& guild, const GuildMemberRecord& member, i32 right)
{
	if(member.membershipID == MEASURED_GUILD_MASTER_MEMBERSHIP) return true;
	const GuildClass* c = FindGuildClass(guild, member.membershipID);
	return c && c->rights[right] != 0;
}

template<typename Writer>
static void WriteGuildMemberProfile(Writer& packet, const GuildMemberRecord& member, u16 leaderClass)
{
	const AccountProgress progress = AccountProgressLoad(member.nickname.data());
	packet.WriteStringObj(member.nickname.data());
	packet.template Write<i32>(member.membershipID);
	packet.template Write<u16>(progress.level);
	packet.template Write<u16>(leaderClass);
	packet.template Write<u16>((u16)GetGameXmlContent().masters.size());
	packet.template Write<i32>(0);
	packet.template Write<u8>(0);
	packet.template Write<u16>(0);
	packet.template Write<i32>(member.contributedPoint);
	packet.template Write<i32>(member.contributedFund);
	packet.template Write<u16>(0);
	packet.template Write<u16>(0);
	packet.template Write<i64>(member.lastLogout);
}

u16 HubGame::GuildLeaderClass(const WideString& nickname)
{
	const i32 memberID = FindUserByNickname(nickname);
	if(memberID < 0) return 0;
	WorldHub::ActorPlayer* actor = world.FindPlayerActor(playerActorUID[memberID]);
	return actor ? (u16)actor->classType : 0;
}

void HubGame::SendGuildResult(ClientHandle clientHd, u16 netID, u32 result)
{
	PacketWriter<Sv::SA_QuitGuild> packet;
	packet.Write<u32>(result);
	replication.server->SendPacketData(clientHd, netID, (u16)packet.size, packet.data);
}

void HubGame::SendGuildNickResult(ClientHandle clientHd, u16 netID, u32 result, const WideString& text)
{
	replication.SendNicknameResult(clientHd, netID, result, text.data());
}

void HubGame::SendMyGuild(ClientHandle clientHd, const GuildRecord* guild, u8 rollcall)
{
	PacketWriter<Sv::SN_MyGuild> packet;
	packet.WriteStringObj(guild ? guild->tag.data() : L"");
	packet.Write<i64>(guild ? guild->dissolution : 0);
	packet.Write<u8>(rollcall);
	replication.SendWriter(clientHd, packet);
}

void HubGame::SendGuildChannelEnter(ClientHandle clientHd, const GuildRecord& guild, const WideString& nickname, u8 status)
{
	PacketWriter<Sv::SN_GuildChannelEnter> packet;
	packet.WriteStringObj(guild.name.data());
	packet.WriteStringObj(nickname.data());
	packet.Write<u8>(status);
	replication.SendWriter(clientHd, packet);
}

void HubGame::SendGuildStatus(ClientHandle clientHd, const WideString* nicks, const u8* states, i32 count)
{
	PacketWriter<Sv::SN_GuildMemberStatus,2048> packet;
	packet.Write<u16>((u16)count);
	for(i32 i = 0; i < count; i++) {
		packet.WriteStringObj(nicks[i].data());
		packet.Write<u8>(states[i]);
	}
	replication.SendWriter(clientHd, packet);
}

void HubGame::SendGuildPointAndFund(const GuildRecord& guild)
{
	PacketWriter<Sv::SN_GuildpointandfundChange> packet;
	packet.Write<i64>(NowFiletime());
	packet.Write<u32>((u32)guild.point);
	packet.Write<u32>(0);
	packet.Write<u32>((u32)guild.fund);
	ForEachOnlineGuildMember(guild, [&](i32, ClientHandle hd) { replication.SendWriter(hd, packet); });
}

void HubGame::GuildOnConnect(i32 userID)
{
	GuildStore& store = GetGuildStore();
	std::lock_guard<std::recursive_mutex> lock(store.mutex);
	const WideString& nick = playerAccountData[userID]->nickname;
	GuildRecord* guild = store.FindByMember(nick);
	if(!guild) return;
	const ClientHandle clientHd = playerMap[userID]->clientHd;
	GuildMemberRecord& me = *store.FindMember(*guild, nick);
	const i64 day = ComputeMissionCalendar((i64)time(nullptr)).periodDay;
	if(guild->rollcallDay != day) {
		guild->rollcallDay = day;
		guild->todayRollCall = 0;
	}
	if(me.lastRollcallDay != day) {
		me.lastRollcallDay = day;
		guild->todayRollCall++;
		const i32 points = GetGameXmlContent().guildRollcallPoint;
		const i32 before = GuildLevel(*guild);
		guild->point += points;
		guild->fund += points;
		me.contributedPoint += points;
		GuildLevelCheck(*guild, before);
		SendGuildPointAndFund(*guild);
		LOG("[client%x] Guild :: %ls roll call in %ls (+%d)", clientHd, nick.data(), guild->name.data(), points);
	}
	store.Save();
	eastl::fixed_vector<WideString,64,false> nicks;
	eastl::fixed_vector<u8,64,false> states;
	ForEachOnlineGuildMember(*guild, [&](i32 memberID, ClientHandle hd) {
		if(hd != clientHd) {
			SendGuildChannelEnter(hd, *guild, nick, 0);
			const u8 online = MEASURED_FRIEND_STATE_ONLINE;
			SendGuildStatus(hd, &nick, &online, 1);
		}
		if(!nicks.full()) {
			nicks.push_back(playerAccountData[memberID]->nickname);
			states.push_back(MEASURED_FRIEND_STATE_ONLINE);
		}
	});
	SendGuildStatus(clientHd, nicks.data(), states.data(), (i32)nicks.size());
}

void HubGame::GuildOnDisconnect(i32 userID)
{
	GuildStore& store = GetGuildStore();
	std::lock_guard<std::recursive_mutex> lock(store.mutex);
	const WideString& nick = playerAccountData[userID]->nickname;
	GuildRecord* guild = store.FindByMember(nick);
	if(!guild) return;
	store.FindMember(*guild, nick)->lastLogout = NowFiletime();
	store.Save();
	PacketWriter<Sv::SN_GuildchannelLeave> packet;
	packet.WriteStringObj(guild->name.data());
	packet.WriteStringObj(nick.data());
	const ClientHandle self = playerMap[userID]->clientHd;
	ForEachOnlineGuildMember(*guild, [&](i32, ClientHandle hd) { if(hd != self) replication.SendWriter(hd, packet); });
}

void HubGame::GuildBroadcastState(i32 userID, u8 state)
{
	GuildStore& store = GetGuildStore();
	std::lock_guard<std::recursive_mutex> lock(store.mutex);
	const WideString& nick = playerAccountData[userID]->nickname;
	GuildRecord* guild = store.FindByMember(nick);
	if(!guild || state == MEASURED_FRIEND_STATE_OFFLINE) return;
	ForEachOnlineGuildMember(*guild, [&](i32, ClientHandle hd) { SendGuildStatus(hd, &nick, &state, 1); });
}

bool HubGame::GuildChat(i32 userID, const wchar* msg, i32 msgLen)
{
	GuildStore& store = GetGuildStore();
	std::lock_guard<std::recursive_mutex> lock(store.mutex);
	const WideString& nick = playerAccountData[userID]->nickname;
	GuildRecord* guild = store.FindByMember(nick);
	if(!guild) return true;
	ForEachOnlineGuildMember(*guild, [&](i32, ClientHandle hd) { replication.SendChatMessageToClient(hd, nick.data(), MEASURED_CHAT_TYPE_GUILD, msg, msgLen); });
	return true;
}

void HubGame::GuildLevelCheck(GuildRecord& guild, i32 levelBefore)
{
	const i32 levelAfter = GuildLevel(guild);
	if(levelAfter <= levelBefore) return;
	GetGuildStore().AddHistory(guild, MEASURED_GUILD_HISTORY_LEVEL, MEASURED_GUILD_EVENT_LEVELUP, NowFiletime(), WideString(), WideString(), levelBefore, levelAfter);
	PacketWriter<Sv::SN_GuildLevelup> packet;
	packet.Write<u8>((u8)levelBefore);
	packet.Write<u8>((u8)levelAfter);
	ForEachOnlineGuildMember(guild, [&](i32, ClientHandle hd) { replication.SendWriter(hd, packet); });
}

void HubGame::SendGuildProfile(ClientHandle clientHd, const GuildRecord* guild)
{
	PacketWriter<Sv::SA_GetGuildProfile,2048> packet;
	if(!guild) {
		packet.Write<i32>(MEASURED_GUILD_NOT_JOINED);
		replication.server->SendPacketData(clientHd, Sv::SA_GetGuildProfile::NET_ID, (u16)packet.size, packet.data);
		return;
	}
	const GameXmlContent& content = GetGameXmlContent();
	packet.Write<i32>(0);
	packet.WriteStringObj(guild->name.data());
	packet.WriteStringObj(guild->tag.data());
	packet.Write<i32>(guild->emblemIndex);
	packet.Write<u8>((u8)GuildLevel(*guild));
	packet.Write<u8>((u8)GuildMemberMax(*guild));
	const GuildMemberRecord* master = nullptr;
	foreach_const(m, guild->members) if(m->membershipID == MEASURED_GUILD_MASTER_MEMBERSHIP) master = &*m;
	packet.WriteStringObj(master ? master->nickname.data() : L"");
	packet.Write<i64>(guild->created);
	packet.Write<i64>(guild->dissolution);
	packet.Write<u8>(guild->joinType);
	for(u8 v : guild->interest) packet.Write<u8>(v);
	packet.WriteStringObj(guild->intro.data(), (i32)guild->intro.size());
	packet.WriteStringObj(guild->notice.data(), (i32)guild->notice.size());
	packet.Write<i32>(guild->point);
	packet.Write<i32>(guild->fund);
	packet.Write<i32>(0);
	packet.Write<u16>(0);
	packet.Write<u16>(0);
	packet.Write<u16>(0);
	packet.Write<i32>(-1);
	packet.Write<u16>((u16)guild->classes.size());
	foreach_const(c, guild->classes) {
		packet.Write<i32>(c->id);
		packet.Write<u8>(c->type);
		packet.Write<u8>(c->iconIndex);
		packet.WriteStringObj(c->name.data());
		for(u8 v : c->rights) packet.Write<u8>(v);
	}
	packet.Write<u16>(0);
	packet.Write<i32>(0);
	packet.Write<i32>(content.guildActivityCapWeekday);
	packet.Write<i32>(0);
	packet.Write<i32>(content.guildActivityCapWeekday);
	packet.Write<u8>((u8)guild->todayRollCall);
	replication.SendWriter(clientHd, packet);
}

void HubGame::SendGuildMemberList(ClientHandle clientHd, const GuildRecord* guild)
{
	PacketWriter<Sv::SA_GetGuildMemberList,8192> packet;
	packet.Write<i32>(guild ? 0 : MEASURED_GUILD_NOT_JOINED);
	packet.Write<u16>(guild ? (u16)guild->members.size() : 0);
	if(guild) foreach_const(m, guild->members) WriteGuildMemberProfile(packet, *m, GuildLeaderClass(m->nickname));
	replication.SendWriter(clientHd, packet);
	if(!guild) return;
	eastl::fixed_vector<WideString,64,false> nicks;
	eastl::fixed_vector<u8,64,false> states;
	ForEachOnlineGuildMember(*guild, [&](i32 memberID, ClientHandle) {
		if(nicks.full()) return;
		nicks.push_back(playerAccountData[memberID]->nickname);
		states.push_back(MEASURED_FRIEND_STATE_ONLINE);
	});
	SendGuildStatus(clientHd, nicks.data(), states.data(), (i32)nicks.size());
}

void HubGame::SendGuildHistory(ClientHandle clientHd, const GuildRecord* guild)
{
	PacketWriter<Sv::SA_GetGuildHistoryList,8192> packet;
	packet.Write<i32>(guild ? 0 : MEASURED_GUILD_NOT_JOINED);
	packet.Write<u16>(guild ? (u16)guild->history.size() : 0);
	if(guild) foreach_const(h, guild->history) {
		packet.Write<u8>(h->historyType);
		packet.Write<u8>(h->eventType);
		packet.Write<i64>(h->date);
		packet.WriteStringObj(h->s1.data());
		packet.WriteStringObj(h->s2.data());
		packet.Write<i32>(h->l1);
		packet.Write<i32>(h->l2);
	}
	replication.SendWriter(clientHd, packet);
}

void HubGame::SendGuildPublicProfile(ClientHandle clientHd, const GuildRecord* guild, u16 netID, const WideString* memberNick)
{
	PacketWriter<Sv::SA_GetGuildpublicprofile,2048> packet;
	packet.Write<u32>(guild ? 0 : MEASURED_GUILD_NOT_FOUND);
	if(guild) {
		packet.WriteStringObj(guild->name.data());
		packet.WriteStringObj(guild->tag.data());
		packet.Write<u32>((u32)guild->emblemIndex);
		packet.Write<u8>((u8)GuildLevel(*guild));
		packet.Write<u8>((u8)GuildMemberMax(*guild));
		const GuildMemberRecord* master = nullptr;
		foreach_const(m, guild->members) if(m->membershipID == MEASURED_GUILD_MASTER_MEMBERSHIP) master = &*m;
		packet.WriteStringObj(master ? master->nickname.data() : L"");
		packet.Write<i64>(guild->created);
		packet.Write<i64>(guild->dissolution);
		packet.Write<u8>(guild->joinType);
		for(u8 v : guild->interest) packet.Write<u8>(v);
		packet.WriteStringObj(guild->intro.data(), (i32)guild->intro.size());
		packet.Write<u8>((u8)guild->members.size());
		if(memberNick) packet.WriteStringObj(memberNick->data());
	}
	replication.server->SendPacketData(clientHd, netID, (u16)packet.size, packet.data);
}

void HubGame::GuildAddMember(GuildRecord& guild, const WideString& nick)
{
	GuildStore& store = GetGuildStore();
	GuildMemberRecord member;
	member.nickname = nick;
	member.membershipID = MEASURED_GUILD_REGULAR_MEMBERSHIP;
	member.lastLogout = NowFiletime();
	guild.members.push_back(member);
	guild.joinRequests.erase(eastl::remove(guild.joinRequests.begin(), guild.joinRequests.end(), nick), guild.joinRequests.end());
	guild.invitations.erase(eastl::remove_if(guild.invitations.begin(), guild.invitations.end(), [&](const GuildInvitation& i) { return i.invitee == nick; }), guild.invitations.end());
	store.AddHistory(guild, MEASURED_GUILD_HISTORY_MANAGEMENT, MEASURED_GUILD_EVENT_JOIN, NowFiletime(), nick, WideString(), 0, 0);
	store.Save();

	PacketWriter<Sv::SN_GuildmemberJoin,512> joined;
	WriteGuildMemberProfile(joined, guild.members.back(), GuildLeaderClass(nick));
	const i32 newID = FindUserByNickname(nick);
	ForEachOnlineGuildMember(guild, [&](i32 memberID, ClientHandle hd) {
		if(memberID == newID) return;
		replication.SendWriter(hd, joined);
		SendGuildChannelEnter(hd, guild, nick, MEASURED_FRIEND_STATE_ONLINE);
	});
	if(newID >= 0 && playerMap[newID] != playerList.end()) {
		const ClientHandle hd = playerMap[newID]->clientHd;
		PacketWriter<Sv::SN_GuildJoinapproval> approval;
		approval.WriteStringObj(guild.name.data());
		approval.WriteStringObj(guild.tag.data());
		replication.SendWriter(hd, approval);
		SendMyGuild(hd, &guild, 0);
		SendGuildChannelEnter(hd, guild, nick, 0);
		auto found = GetAccountManager().accountMap.find(playerAccountData[newID]->UID);
		if(found != GetAccountManager().accountMap.end()) found->second->guildTag = guild.tag;
	}
	LOG("Guild :: %ls joins %ls", nick.data(), guild.name.data());
}

void HubGame::GuildRemoveMember(GuildRecord& guild, const WideString& nick, bool kicked)
{
	GuildStore& store = GetGuildStore();
	PacketWriter<Sv::SN_GuildmemberQuit> quit;
	quit.WriteStringObj(nick.data());
	quit.Write<u8>(kicked ? 1 : 0);
	PacketWriter<Sv::SN_GuildchannelLeave> leave;
	leave.WriteStringObj(guild.name.data());
	leave.WriteStringObj(nick.data());
	ForEachOnlineGuildMember(guild, [&](i32, ClientHandle hd) {
		replication.SendWriter(hd, quit);
		replication.SendWriter(hd, leave);
	});
	guild.members.erase(eastl::remove_if(guild.members.begin(), guild.members.end(), [&](const GuildMemberRecord& m) { return m.nickname == nick; }), guild.members.end());
	store.AddHistory(guild, MEASURED_GUILD_HISTORY_MANAGEMENT, kicked ? MEASURED_GUILD_EVENT_KICK : MEASURED_GUILD_EVENT_QUIT, NowFiletime(), nick, WideString(), 0, 0);
	store.Save();
	const i32 leftID = FindUserByNickname(nick);
	if(leftID >= 0 && playerMap[leftID] != playerList.end()) {
		SendMyGuild(playerMap[leftID]->clientHd, nullptr, 0);
		auto found = GetAccountManager().accountMap.find(playerAccountData[leftID]->UID);
		if(found != GetAccountManager().accountMap.end()) found->second->guildTag.clear();
	}
	LOG("Guild :: %ls %s %ls", nick.data(), kicked ? "kicked from" : "leaves", guild.name.data());
}

void HubGame::OnGuildPacket(ClientHandle clientHd, u16 netID, const u8* data, i32 size)
{
	const i32 userID = plidMap->Get(clientHd);
	ASSERT(playerAccountData[userID]);
	auto foundAccount = GetAccountManager().accountMap.find(playerAccountData[userID]->UID);
	ASSERT(foundAccount != GetAccountManager().accountMap.end());
	Account& account = *foundAccount->second;
	const WideString& nick = account.nickname;
	const GameXmlContent& content = GetGameXmlContent();
	GuildStore& store = GetGuildStore();
	std::lock_guard<std::recursive_mutex> lock(store.mutex);
	GuildRecord* guild = store.FindByMember(nick);
	GuildMemberRecord* me = guild ? store.FindMember(*guild, nick) : nullptr;
	ConstBuffer buff(data, size);
	NT_LOG("[client%x] Client :: guild packet %u size=%d", clientHd, (u32)netID, size);

	switch(netID) {
		case Cl::CQ_GetGuildProfile::NET_ID: SendGuildProfile(clientHd, guild); break;
		case Cl::CQ_GetGuildMemberList::NET_ID: SendGuildMemberList(clientHd, guild); break;
		case Cl::CQ_GetGuildHistoryList::NET_ID: SendGuildHistory(clientHd, guild); break;

		case Cl::CQ_GetGuildMemberStatusList::NET_ID: {
			if(!guild) break;
			eastl::fixed_vector<WideString,64,false> nicks;
			eastl::fixed_vector<u8,64,false> states;
			ForEachOnlineGuildMember(*guild, [&](i32 memberID, ClientHandle) {
				if(nicks.full()) return;
				nicks.push_back(playerAccountData[memberID]->nickname);
				states.push_back(MEASURED_FRIEND_STATE_ONLINE);
			});
			SendGuildStatus(clientHd, nicks.data(), states.data(), (i32)nicks.size());
		} break;

		case Cl::CQ_CreateGuild::NET_ID: {
			WideString name, tag;
			if(!ReadWideString(buff, &name, 32) || !ReadWideString(buff, &tag, 8)) break;
			u32 result = 0;
			if(guild) result = MEASURED_GUILD_ALREADY_JOINED;
			else if(account.level < content.guildFoundationLevel) result = MEASURED_GUILD_NOT_ALLOWED_LEVEL_TO_CREATE;
			else if(account.gold < content.guildFoundationGold) result = MEASURED_GUILD_NOT_ENOUGH_GOLD_TO_CREATE;
			else if(!ValidGuildText(name, 2, 15)) result = MEASURED_GUILD_ILLEGAL_NAME;
			else if(!ValidGuildText(tag, 2, 4)) result = MEASURED_GUILD_ILLEGAL_TAG;
			else if(store.FindByName(name)) result = MEASURED_GUILD_DUPLICATED_NAME;
			PacketWriter<Sv::SA_CreateGuild,512> answer;
			answer.Write<u32>(result);
			answer.WriteStringObj(name.data());
			answer.WriteStringObj(tag.data());
			answer.Write<u8>(0);
			answer.Write<u16>(0);
			answer.Write<u32>(0);
			if(result != 0) {
				replication.SendWriter(clientHd, answer);
				break;
			}
			AccountProgress progress = AccountProgressLoad(nick.data());
			progress.gold -= content.guildFoundationGold;
			AccountProgressSave(nick.data(), progress);
			account.gold = progress.gold;
			GuildRecord created;
			created.id = store.nextID++;
			created.name = name;
			created.tag = tag;
			created.emblemIndex = content.validGuildEmblems.empty() ? 0 : content.validGuildEmblems[0];
			created.created = NowFiletime();
			created.joinType = MEASURED_GUILD_JOIN_PUBLIC;
			GuildClass regular;
			regular.id = MEASURED_GUILD_REGULAR_MEMBERSHIP;
			regular.type = MEASURED_GUILD_CLASS_REGULAR;
			regular.iconIndex = MEASURED_GUILD_REGULAR_ICON;
			regular.name = L"Member";
			created.classes.push_back(regular);
			GuildMemberRecord master;
			master.nickname = nick;
			master.membershipID = MEASURED_GUILD_MASTER_MEMBERSHIP;
			master.lastLogout = created.created;
			master.lastRollcallDay = ComputeMissionCalendar((i64)time(nullptr)).periodDay;
			created.members.push_back(master);
			created.rollcallDay = master.lastRollcallDay;
			created.todayRollCall = 1;
			store.AddHistory(created, MEASURED_GUILD_HISTORY_MANAGEMENT, MEASURED_GUILD_EVENT_JOIN, created.created, nick, WideString(), 0, 0);
			store.guilds.push_back(created);
			store.Save();
			account.guildTag = tag;
			LOG("[client%x] Guild :: %ls creates %ls [%ls]", clientHd, nick.data(), name.data(), tag.data());
			replication.SendWriter(clientHd, answer);
			replication.SendMoney(clientHd, account.gold, MEASURED_MISSION_REWARD_MONEY_REASON);
			GuildRecord& g = store.guilds.back();
			SendMyGuild(clientHd, &g, 0);
			SendGuildChannelEnter(clientHd, g, nick, 0);
			const u8 state = MEASURED_FRIEND_STATE_ONLINE;
			SendGuildStatus(clientHd, &nick, &state, 1);
		} break;

		case Cl::CQ_DissolveGuild::NET_ID: {
			u32 result = 0;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(me->membershipID != MEASURED_GUILD_MASTER_MEMBERSHIP) result = MEASURED_GUILD_NO_PERMISSION_DISSOLVE;
			else if(guild->members.size() > 1) result = MEASURED_GUILD_KICKOUT_OTHERS_BEFORE_DISSOLVE;
			PacketWriter<Sv::SA_DissolveGuild> answer;
			answer.Write<u32>(result);
			answer.Write<i64>(0);
			replication.SendWriter(clientHd, answer);
			if(result != 0) break;
			LOG("[client%x] Guild :: %ls dissolves %ls", clientHd, nick.data(), guild->name.data());
			const u32 id = guild->id;
			store.guilds.erase(eastl::remove_if(store.guilds.begin(), store.guilds.end(), [&](const GuildRecord& g) { return g.id == id; }), store.guilds.end());
			store.Save();
			account.guildTag.clear();
			SendMyGuild(clientHd, nullptr, 0);
		} break;

		case MEASURED_CQ_CANCEL_GUILD_DISSOLUTION: SendGuildResult(clientHd, Sv::SA_CancelGuilddissolution::NET_ID, MEASURED_GUILD_NOT_DISSOLVING); break;

		case Cl::CQ_QuitGuild::NET_ID: {
			u32 result = 0;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(me->membershipID == MEASURED_GUILD_MASTER_MEMBERSHIP) result = MEASURED_GUILD_INCORRECT_MASTER_DELEGATION;
			SendGuildResult(clientHd, Sv::SA_QuitGuild::NET_ID, result);
			if(result == 0) GuildRemoveMember(*guild, nick, false);
		} break;

		case Cl::CQ_KickGuildMember::NET_ID: {
			WideString target;
			if(!ReadWideString(buff, &target, 32)) break;
			u32 result = 0;
			GuildMemberRecord* targetMember = guild ? store.FindMember(*guild, target) : nullptr;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(!HasGuildRight(*guild, *me, MEASURED_GUILD_RIGHT_EXPEL)) result = MEASURED_GUILD_NO_PERMISSION_KICKOUT;
			else if(!targetMember || targetMember->membershipID == MEASURED_GUILD_MASTER_MEMBERSHIP || target == nick) result = MEASURED_GUILD_NOT_FOUND_INVITEE;
			SendGuildNickResult(clientHd, Sv::SA_KickGuildmember::NET_ID, result, target);
			if(result == 0) GuildRemoveMember(*guild, target, true);
		} break;

		case MEASURED_CQ_JOIN_GUILD: {
			WideString name;
			if(!ReadWideString(buff, &name, 32)) break;
			GuildRecord* target = store.FindByName(name);
			u32 result = 0;
			if(guild) result = MEASURED_GUILD_ALREADY_JOINED;
			else if(!target) result = MEASURED_GUILD_NOT_FOUND;
			else if((i32)target->members.size() >= GuildMemberMax(*target)) result = MEASURED_GUILD_NO_ROOM_TO_JOIN;
			else if(target->joinType == MEASURED_GUILD_JOIN_PRIVATE) result = MEASURED_GUILD_INCORRECT_JOINTYPE;
			else if(target->joinType == MEASURED_GUILD_JOIN_APPROVAL && eastl::find(target->joinRequests.begin(), target->joinRequests.end(), nick) != target->joinRequests.end()) result = MEASURED_GUILD_DUPLICATED_JOINREQUEST;
			PacketWriter<Sv::SA_JoinGuild> answer;
			answer.Write<u32>(result);
			answer.WriteStringObj(name.data());
			answer.Write<u8>(target ? target->joinType : 0);
			replication.SendWriter(clientHd, answer);
			if(result != 0) break;
			if(target->joinType == MEASURED_GUILD_JOIN_APPROVAL) {
				target->joinRequests.push_back(nick);
				store.Save();
				LOG("[client%x] Guild :: %ls asks to join %ls", clientHd, nick.data(), name.data());
			}
			else GuildAddMember(*target, nick);
		} break;

		case Cl::CQ_GetGuildJoinRequestList::NET_ID: {
			PacketWriter<Sv::SA_GetGuildjoinrequestlist,4096> answer;
			answer.Write<i32>(guild ? 0 : MEASURED_GUILD_NOT_JOINED);
			answer.Write<u16>(guild ? (u16)guild->joinRequests.size() : 0);
			if(guild) foreach_const(r, guild->joinRequests) {
				answer.WriteStringObj(r->data());
				answer.Write<u16>(AccountProgressLoad(r->data()).level);
				answer.Write<u16>(GuildLeaderClass(*r));
				answer.Write<u16>((u16)content.masters.size());
				answer.Write<u32>(0);
				answer.Write<u8>(0);
				answer.Write<u16>(0);
			}
			replication.SendWriter(clientHd, answer);
		} break;

		case Cl::CQ_RespondGuildJoinRequest::NET_ID: {
			WideString target;
			if(!ReadWideString(buff, &target, 32) || !buff.CanRead(1)) break;
			const bool accept = buff.Read<u8>() != 0;
			u32 result = 0;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(!HasGuildRight(*guild, *me, MEASURED_GUILD_RIGHT_INVITE)) result = MEASURED_GUILD_NO_PERMISSION_APPROVEJOINREQUEST;
			else if(eastl::find(guild->joinRequests.begin(), guild->joinRequests.end(), target) == guild->joinRequests.end()) result = MEASURED_GUILD_NOT_FOUND_JOINREQUEST;
			else if(accept && store.FindByMember(target)) result = MEASURED_GUILD_ALREADY_JOINED;
			else if(accept && (i32)guild->members.size() >= GuildMemberMax(*guild)) result = MEASURED_GUILD_NO_ROOM_TO_JOIN;
			PacketWriter<Sv::SA_RespondGuildjoinrequest,512> answer;
			answer.Write<u32>(result);
			answer.Write<u8>(accept ? 1 : 0);
			GuildMemberRecord candidate;
			candidate.nickname = target;
			candidate.membershipID = MEASURED_GUILD_REGULAR_MEMBERSHIP;
			WriteGuildMemberProfile(answer, candidate, GuildLeaderClass(target));
			replication.SendWriter(clientHd, answer);
			if(result != 0) break;
			guild->joinRequests.erase(eastl::remove(guild->joinRequests.begin(), guild->joinRequests.end(), target), guild->joinRequests.end());
			if(accept) GuildAddMember(*guild, target);
			else store.Save();
		} break;

		case Cl::CQ_InviteGuildMember::NET_ID: {
			WideString target;
			if(!ReadWideString(buff, &target, 32)) break;
			u32 result = 0;
			const i32 targetID = FindUserByNickname(target);
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(!HasGuildRight(*guild, *me, MEASURED_GUILD_RIGHT_INVITE)) result = MEASURED_GUILD_NO_PERMISSION_INVITE;
			else if(targetID < 0 || target == nick) result = MEASURED_GUILD_NOT_FOUND_INVITEE;
			else if(store.FindByMember(target)) result = MEASURED_GUILD_ALREADY_JOINED;
			else if(eastl::find_if(guild->invitations.begin(), guild->invitations.end(), [&](const GuildInvitation& i) { return i.invitee == target; }) != guild->invitations.end()) result = MEASURED_GUILD_DUPLICATED_INVITATION;
			else if((i32)guild->members.size() >= GuildMemberMax(*guild)) result = MEASURED_GUILD_NO_ROOM_TO_JOIN;
			SendGuildNickResult(clientHd, Sv::SA_InviteGuildmember::NET_ID, result, target);
			if(result != 0) break;
			GuildInvitation inv;
			inv.invitee = target;
			inv.inviter = nick;
			guild->invitations.push_back(inv);
			store.Save();
			PacketWriter<Sv::SN_GuildInvitation> note;
			note.WriteStringObj(guild->name.data());
			note.WriteStringObj(nick.data());
			if(playerMap[targetID] != playerList.end()) replication.SendWriter(playerMap[targetID]->clientHd, note);
			LOG("[client%x] Guild :: %ls invites %ls to %ls", clientHd, nick.data(), target.data(), guild->name.data());
		} break;

		case Cl::CQ_GetGuildInvitationList::NET_ID: {
			PacketWriter<Sv::SA_GetGuildinvitationlist,2048> answer;
			answer.Write<u32>(0);
			u16 count = 0;
			foreach_const(g, store.guilds) foreach_const(i, g->invitations) if(i->invitee == nick) count++;
			answer.Write<u16>(count);
			foreach_const(g, store.guilds) foreach_const(i, g->invitations) {
				if(i->invitee != nick) continue;
				answer.WriteStringObj(g->name.data());
				answer.WriteStringObj(i->inviter.data());
			}
			replication.SendWriter(clientHd, answer);
		} break;

		case Cl::CQ_RespondGuildInvitation::NET_ID: {
			WideString name;
			if(!ReadWideString(buff, &name, 32) || !buff.CanRead(1)) break;
			const bool accept = buff.Read<u8>() != 0;
			GuildRecord* target = store.FindByName(name);
			u32 result = 0;
			if(!target || eastl::find_if(target->invitations.begin(), target->invitations.end(), [&](const GuildInvitation& i) { return i.invitee == nick; }) == target->invitations.end()) result = MEASURED_GUILD_NOT_FOUND_INVITATION;
			else if(accept && guild) result = MEASURED_GUILD_ALREADY_JOINED;
			else if(accept && (i32)target->members.size() >= GuildMemberMax(*target)) result = MEASURED_GUILD_NO_ROOM_TO_JOIN;
			PacketWriter<Sv::SA_RespondGuildinvitation> answer;
			answer.Write<u32>(result);
			answer.Write<u8>(accept ? 1 : 0);
			answer.WriteStringObj(name.data());
			answer.WriteStringObj(target ? target->tag.data() : L"");
			replication.SendWriter(clientHd, answer);
			if(result != 0) break;
			target->invitations.erase(eastl::remove_if(target->invitations.begin(), target->invitations.end(), [&](const GuildInvitation& i) { return i.invitee == nick; }), target->invitations.end());
			if(accept) GuildAddMember(*target, nick);
			else store.Save();
		} break;

		case Cl::CQ_GetGuildPublicProfile::NET_ID: {
			WideString name;
			if(!ReadWideString(buff, &name, 32)) break;
			SendGuildPublicProfile(clientHd, store.FindByName(name), Sv::SA_GetGuildpublicprofile::NET_ID, nullptr);
		} break;

		case MEASURED_CQ_GET_GUILD_PUBLIC_PROFILE_FROM_USER: {
			WideString target;
			if(!ReadWideString(buff, &target, 32)) break;
			SendGuildPublicProfile(clientHd, store.FindByMember(target), MEASURED_SA_GET_GUILD_PUBLIC_PROFILE_FROM_USER, &target);
		} break;

		case Cl::CQ_EditGuildNotice::NET_ID:
		case Cl::CQ_EditGuildIntro::NET_ID: {
			const bool notice = netID == Cl::CQ_EditGuildNotice::NET_ID;
			GuildText text;
			if(!ReadWideString(buff, &text, 200)) break;
			u32 result = 0;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(!HasGuildRight(*guild, *me, notice ? MEASURED_GUILD_RIGHT_NOTICE : MEASURED_GUILD_RIGHT_INTRO)) result = notice ? MEASURED_GUILD_NO_PERMISSION_EDITNOTICE : MEASURED_GUILD_NO_PERMISSION_EDITINTRO;
			PacketWriter<Sv::SA_EditGuildnotice,1024> answer;
			answer.Write<u32>(result);
			answer.WriteStringObj(text.data(), (i32)text.size());
			replication.server->SendPacketData(clientHd, notice ? Sv::SA_EditGuildnotice::NET_ID : Sv::SA_EditGuildintro::NET_ID, (u16)answer.size, answer.data);
			if(result != 0) break;
			if(notice) guild->notice = text;
			else guild->intro = text;
			store.Save();
			if(notice) {
				PacketWriter<Sv::SN_GuildNotice,1024> note;
				note.WriteStringObj(text.data(), (i32)text.size());
				ForEachOnlineGuildMember(*guild, [&](i32, ClientHandle hd) { if(hd != clientHd) replication.SendWriter(hd, note); });
			}
		} break;

		case Cl::CQ_SetGuildInterest::NET_ID: {
			if(!buff.CanRead(7)) break;
			u32 result = 0;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(!HasGuildRight(*guild, *me, MEASURED_GUILD_RIGHT_INTEREST)) result = MEASURED_GUILD_NO_PERMISSION_SETINTEREST;
			if(result == 0) {
				for(i32 i = 0; i < 7; i++) guild->interest[i] = buff.Read<u8>() ? 1 : 0;
				store.Save();
			}
			SendGuildResult(clientHd, Sv::SA_SetGuildinterest::NET_ID, result);
		} break;

		case Cl::CQ_SetGuildJoinType::NET_ID: {
			if(!buff.CanRead(1)) break;
			const u8 joinType = buff.Read<u8>();
			u32 result = 0;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(!HasGuildRight(*guild, *me, MEASURED_GUILD_RIGHT_JOINTYPE)) result = MEASURED_GUILD_NO_PERMISSION_CHGJOINTYPE;
			else if(joinType < MEASURED_GUILD_JOIN_PUBLIC || joinType > MEASURED_GUILD_JOIN_PRIVATE) result = MEASURED_GUILD_INCORRECT_JOINTYPE;
			if(result == 0) {
				guild->joinType = joinType;
				store.Save();
			}
			PacketWriter<Sv::SA_SetGuildjointype> answer;
			answer.Write<u32>(result);
			answer.Write<u8>(joinType);
			replication.SendWriter(clientHd, answer);
		} break;

		case MEASURED_CQ_DONATE_TO_GUILD: {
			if(!buff.CanRead(4)) break;
			const u32 gold = buff.Read<u32>();
			u32 result = 0;
			if(!guild) result = MEASURED_GUILD_NOT_JOINED;
			else if(gold == 0 || gold % (u32)content.guildDonationUnit != 0) result = MEASURED_GUILD_PARAMETER_ERROR;
			else if(account.gold < (i64)gold) result = MEASURED_TRADE_NOT_ENOUGH_GOLD;
			PacketWriter<Sv::SA_DonateToGuild> answer;
			answer.Write<u32>(result);
			const u32 gain = gold / (u32)content.guildFundExchangeRate;
			answer.Write<u32>(gold);
			answer.Write<u32>(result == 0 ? gain : 0);
			if(result != 0) {
				answer.Write<u32>(guild ? (u32)guild->fund : 0);
				replication.server->SendPacketData(clientHd, MEASURED_SA_DONATE_TO_GUILD, (u16)answer.size, answer.data);
				break;
			}
			AccountProgress progress = AccountProgressLoad(nick.data());
			progress.gold -= gold;
			AccountProgressSave(nick.data(), progress);
			account.gold = progress.gold;
			guild->fund += gain;
			me->contributedFund += gain;
			store.AddHistory(*guild, MEASURED_GUILD_HISTORY_ASSET, MEASURED_GUILD_EVENT_DONATION, NowFiletime(), nick, WideString(), (i32)gold, (i32)gain);
			store.Save();
			answer.Write<u32>((u32)guild->fund);
			replication.server->SendPacketData(clientHd, MEASURED_SA_DONATE_TO_GUILD, (u16)answer.size, answer.data);
			replication.SendMoney(clientHd, account.gold, MEASURED_MISSION_REWARD_MONEY_REASON);
			PacketWriter<Sv::SN_GuildfundAdded> note;
			note.WriteStringObj(nick.data());
			note.Write<u32>(gain);
			note.Write<u32>((u32)guild->fund);
			ForEachOnlineGuildMember(*guild, [&](i32, ClientHandle hd) { replication.SendWriter(hd, note); });
			LOG("[client%x] Guild :: %ls donates %u gold to %ls (+%u fund)", clientHd, nick.data(), gold, guild->name.data(), gain);
		} break;

		case Cl::CQ_GetRecommendedGuildList::NET_ID: {
			const u8 wanted = buff.CanRead(1) ? buff.Read<u8>() : 8;
			PacketWriter<Sv::SA_GetRecommendedguildlist,4096> answer;
			answer.Write<i32>(0);
			u16 count = 0;
			foreach_const(g, store.guilds) if(count < wanted && (!guild || g->id != guild->id) && g->joinType != MEASURED_GUILD_JOIN_PRIVATE) count++;
			answer.Write<u16>(count);
			u16 written = 0;
			foreach_const(g, store.guilds) {
				if(written >= count || (guild && g->id == guild->id) || g->joinType == MEASURED_GUILD_JOIN_PRIVATE) continue;
				answer.WriteStringObj(g->name.data());
				answer.WriteStringObj(g->tag.data());
				answer.Write<u32>((u32)g->emblemIndex);
				answer.Write<u8>((u8)GuildLevel(*g));
				answer.Write<u8>(g->joinType);
				answer.Write<u8>((u8)g->members.size());
				answer.Write<u8>((u8)GuildMemberMax(*g));
				for(u8 v : g->interest) answer.Write<u8>(v);
				written++;
			}
			replication.SendWriter(clientHd, answer);
		} break;

		default: {
			WARN("[client%x] Guild :: packet %u not handled", clientHd, (u32)netID);
		} break;
	}
}
