#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>
#include <EASTL/sort.h>

static const u32 PVP_ANNOUNCE_MONSTER_KILL = 5;
static const u8 LEVEL_STAT_HEALTH = 0;
static const f64 TRAIT_RESET_COOLDOWN_MS = 20000.0;
static const u8 LEVEL_STAT_ATTACK = 6;
static const u8 LEVEL_STAT_SKILLPOWER = 8;

static bool KillRewarded(const World::ActorNpc& npc, const World::Player& killer)
{
	return npc.faction != (Faction)killer.team;
}

void World::StartIngameLevels()
{
	if(!map || !map->ingameLevels) return;
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	foreach(p, players) {
		p->ingameLevel = 1;
		p->ingameExp = 0;
		p->Main().traits = {};
		p->Main().traitPoints = 0;
		p->Sub().traits = {};
		p->Sub().traitPoints = 0;
		replication->SendLevelIngameToAll(p->userID, info.ExpToLevelUp(1), 1);
	}
	nextPassiveExpAt = info.passivePeriod > 0 ? TimeAdd(localTime, TimeMsToTime(info.passivePeriod * 1000.0)) : Time::ZERO;
}

void World::GrantIngameExp(Player& player, i32 gain, ActorUID target)
{
	if(!map || !map->ingameLevels || gain <= 0) return;
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	if(player.ingameLevel >= info.maxLevel) return;
	player.ingameExp += gain;
	bool levelUp = false;
	while(player.ingameLevel < info.maxLevel && player.ingameExp >= info.ExpToLevelUp(player.ingameLevel)) {
		player.ingameExp -= info.ExpToLevelUp(player.ingameLevel);
		player.ingameLevel++;
		levelUp = true;
		if(info.propertyPointsEvery > 0 && player.ingameLevel % info.propertyPointsEvery == 0) {
			player.Main().traitPoints += info.propertyPointsStep;
			player.Sub().traitPoints += info.propertyPointsStep;
		}
	}
	if(player.ingameLevel >= info.maxLevel) player.ingameExp = 0;
	if(player.clientHd != ClientHandle::INVALID) replication->SendExpIngame(player.clientHd, player.userID, gain, player.ingameExp, target);
	if(!levelUp) return;
	replication->SendLevelIngameToAll(player.userID, info.ExpToLevelUp(player.ingameLevel), player.ingameLevel);
	ApplyLevelStats(player);
	if(player.clientHd != ClientHandle::INVALID) {
		if(player.Main().traitPoints > 0) replication->SendCharacterPropertyPoint(player.clientHd, player.Main().UID, player.Main().traitPoints);
		if(player.Sub().traitPoints > 0) replication->SendCharacterPropertyPoint(player.clientHd, player.Sub().UID, player.Sub().traitPoints);
	}
	LOG("Level :: %ls reaches level %d", player.name.data(), player.ingameLevel);
}

void World::GrantMonsterKillExp(const ActorNpc& npc, ActorUID killerUID)
{
	if(!map || !map->ingameLevels) return;
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	auto found = info.monsters.find((i32)npc.docID);
	if(found == info.monsters.end()) return;
	ActorMaster* killerMaster = FindMasterActor(killerUID);
	if(!killerMaster || !killerMaster->parent) return;
	Player& killer = *killerMaster->parent;
	if(!KillRewarded(npc, killer)) return;
	i32 base = found->second.exp;
	if(found->second.growth > 0 && found->second.growthPeriod > 0 && gameStartedAt != Time::ZERO) {
		base += found->second.growth * (i32)(TimeDiffSec(TimeDiff(gameStartedAt, localTime)) / found->second.growthPeriod);
	}
	eastl::fixed_vector<Player*,10,false> helpers;
	foreach(p, players) {
		if(&*p == &killer || p->team != killer.team || p->dead || !p->body) continue;
		if(glm::length(vec2(p->body->GetWorldPos()) - vec2(npc.pos)) <= info.shareRadius) helpers.push_back(&*p);
	}
	if(info.shares.empty()) {
		GrantIngameExp(killer, base, npc.UID);
		return;
	}
	const auto& share = info.shares[MIN((i32)helpers.size(), (i32)info.shares.size() - 1)];
	GrantIngameExp(killer, MAX((i32)(base * share.first + 0.5f), 1), npc.UID);
	foreach(h, helpers) GrantIngameExp(**h, MAX((i32)(base * share.second + 0.5f), 1), npc.UID);
}

i32 World::IngameRespawnDelayMs(const Player& player) const
{
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	const f64 elapsed = gameStartedAt != Time::ZERO ? TimeDiffSec(TimeDiff(gameStartedAt, localTime)) : 0.0;
	const f64 ms = info.respawnBaseMs + info.respawnPerLevelMs * player.ingameLevel + info.respawnPerSecMs * elapsed;
	return (i32)(floor(ms / 1000.0 + 0.5) * 1000.0);
}

f32 World::LevelBonus(const Player& player, ClassType classType, u8 statType) const
{
	if(!map || !map->ingameLevels) return 0;
	return GetGameXmlContent().MasterLevelBonus(classType, statType, player.ingameLevel);
}

void World::ApplyLevelStats(Player& player)
{
	for(i32 i = 0; i < 2; i++) {
		ActorMaster& chara = i == 0 ? player.Main() : player.Sub();
		RefreshMasterHealth(player, chara);
		const WeaponIndex weapon = (chara.UID == player.Main().UID) == ((player.mainCharaID & 1) == 0) ? player.mainWeapon : player.subWeapon;
		replication->SendStatToAll(chara.UID, LEVEL_STAT_ATTACK, 0, GetGameXmlContent().GetWeaponAttack((i32)weapon) + LevelBonus(player, chara.classType, LEVEL_STAT_ATTACK));
		replication->SendStatToAll(chara.UID, LEVEL_STAT_SKILLPOWER, 0, MasterStat(chara.classType, LEVEL_STAT_SKILLPOWER) + LevelBonus(player, chara.classType, LEVEL_STAT_SKILLPOWER));
	}
}

void World::RefreshMasterHealth(Player& player, ActorMaster& chara)
{
	const i32 hpMax = (i32)(MasterStat(chara.classType, LEVEL_STAT_HEALTH) + LevelBonus(player, chara.classType, LEVEL_STAT_HEALTH) + GetGameXmlContent().MasterTraitBonus(chara.classType, chara.traits, LEVEL_STAT_HEALTH));
	if(chara.hp > 0) chara.hp = MIN(chara.hp + MAX(hpMax - chara.hpMax, 0), hpMax);
	chara.hpMax = hpMax;
	replication->SendStatToAll(chara.UID, LEVEL_STAT_HEALTH, (f32)chara.hp, (f32)chara.hpMax);
}

f32 World::StatBonus(const Player& player, u8 statType) const
{
	const ActorMaster& chara = player.Main();
	return LevelBonus(player, chara.classType, statType) + GetGameXmlContent().MasterTraitBonus(chara.classType, chara.traits, statType);
}

void World::SendTraitStats(Player& player, ActorMaster& chara)
{
	const GameXmlContent& content = GetGameXmlContent();
	auto f = content.masterTraits.find((i32)chara.classType);
	if(f == content.masterTraits.end()) return;
	for(i32 i = 0; i < 4; i++) {
		for(const StatValue& st : f->second[i]) {
			if(st.type == LEVEL_STAT_HEALTH) {
				RefreshMasterHealth(player, chara);
				continue;
			}
			const f32 value = MasterStat(chara.classType, st.type) + LevelBonus(player, chara.classType, st.type) + content.MasterTraitBonus(chara.classType, chara.traits, st.type);
			replication->SendStatToAll(chara.UID, st.type, 0, value);
		}
	}
}

u32 World::UpgradeTraits(Player& player, ActorUID characterUID, const u8* indices, i32 count)
{
	static const u32 ERR_NOT_USING_MODE = 600;
	static const u32 ERR_NOT_EXIST_CHARACTER = 603;
	static const u32 ERR_NOT_ENOUGH_POINT = 604;
	static const u32 ERR_NOT_EXIST_DATA = 605;
	static const u32 ERR_COUNT_LIMIT = 606;
	const GameXmlContent& content = GetGameXmlContent();
	if(!map || !map->ingameLevels) return ERR_NOT_USING_MODE;
	static const u32 ERR_NOT_IN_SAFE_ZONE = 601;
	ActorMaster* chara = player.Main().UID == characterUID ? &player.Main() : player.Sub().UID == characterUID ? &player.Sub() : nullptr;
	if(!chara) return ERR_NOT_EXIST_CHARACTER;
	if(!player.inSafeZone) return ERR_NOT_IN_SAFE_ZONE;
	if(count <= 0 || count > chara->traitPoints) return ERR_NOT_ENOUGH_POINT;
	eastl::array<u8,4> levels = chara->traits;
	for(i32 i = 0; i < count; i++) {
		if(indices[i] >= 4) return ERR_NOT_EXIST_DATA;
		if(levels[indices[i]] >= content.traitMaxPoints) return ERR_COUNT_LIMIT;
		levels[indices[i]]++;
	}
	chara->traits = levels;
	chara->traitPoints -= count;
	SendTraitStats(player, *chara);
	if(player.clientHd != ClientHandle::INVALID) replication->SendCharacterPropertyPoint(player.clientHd, chara->UID, chara->traitPoints);
	replication->SendCharacterPropertyInfoToAll(chara->UID, chara->traits);
	LOG("Traits :: %ls master %d traits %d/%d/%d/%d (%d points left)", player.name.data(), (i32)chara->classType, chara->traits[0], chara->traits[1], chara->traits[2], chara->traits[3], chara->traitPoints);
	return 0;
}

u32 World::ResetTraits(Player& player, ActorUID characterUID)
{
	static const u32 ERR_NOT_USING_MODE = 600;
	static const u32 ERR_NOT_EXIST_CHARACTER = 603;
	if(!map || !map->ingameLevels) return ERR_NOT_USING_MODE;
	ActorMaster* chara = player.Main().UID == characterUID ? &player.Main() : player.Sub().UID == characterUID ? &player.Sub() : nullptr;
	static const u32 ERR_RESET_COOLTIME = 602;
	if(!chara) return ERR_NOT_EXIST_CHARACTER;
	if(chara->traitResetAt != Time::ZERO && localTime < TimeAdd(chara->traitResetAt, TimeMsToTime(TRAIT_RESET_COOLDOWN_MS))) return ERR_RESET_COOLTIME;
	chara->traitResetAt = localTime;
	for(u8 v : chara->traits) chara->traitPoints += v;
	chara->traits = {};
	SendTraitStats(player, *chara);
	replication->SendCharacterPropertyResetToAll(chara->UID);
	if(player.clientHd != ClientHandle::INVALID) replication->SendCharacterPropertyPoint(player.clientHd, chara->UID, chara->traitPoints);
	LOG("Traits :: %ls master %d reset (%d points)", player.name.data(), (i32)chara->classType, chara->traitPoints);
	return 0;
}

static const u32 VOTE_TYPE_SURRENDER = 0;
static const u32 VOTE_TYPE_TITAN = 1;
static const i32 TITAN_READY_STATUS = 1239000002;
static const f64 VOTE_SURRENDER_PERIOD_MS = 30000.0;
static const f64 VOTE_SURRENDER_FAIL_COOLDOWN_MS = 90000.0;
static const u32 VOTE_STATE_PASSED = 1;
static const u32 VOTE_STATE_FAILED = 2;
static const u32 VOTE_REASON_PASSED = 1;
static const u32 VOTE_REASON_REJECTED = 2;
static const i8 VOTE_YES = 0;
static const i8 VOTE_NONE = -1;

void World::RequestVote(Player& player, u32 type)
{
	if(winnerTeam >= 0 || gameStartedAt == Time::ZERO) return;
	const GameXmlContent::TitanInfo& titan = GetGameXmlContent().titan;
	f32 periodMs = 0;
	if(type == VOTE_TYPE_SURRENDER) {
		if(TimeDiffMs(TimeDiff(gameStartedAt, localTime)) < (f64)replication->surrenderAbleTime) return;
		periodMs = (f32)VOTE_SURRENDER_PERIOD_MS;
	}
	else if(type == VOTE_TYPE_TITAN) {
		if(titan.forms.empty() || player.dead || player.titanForm >= 0 || teamTitanPieces[player.team] < titan.cost || localTime < player.titanVoteBlockedUntil) return;
		periodMs = titan.votePeriod;
	}
	else return;
	TeamVote& vote = teamVotes[player.team];
	if(vote.active || localTime < vote.cooldownUntil) return;
	vote.active = true;
	vote.type = type;
	vote.periodMs = periodMs;
	vote.requestor = (i32)player.index;
	vote.endAt = TimeAdd(localTime, TimeMsToTime(periodMs));
	vote.choices.fill(VOTE_NONE);
	i32 voters = 0;
	foreach(p, players) {
		if(p->team != player.team || (type == VOTE_TYPE_TITAN && p->index == player.index)) continue;
		voters++;
	}
	foreach(p, players) {
		if(p->team == player.team && (p->index == player.index || p->clientHd == ClientHandle::INVALID)) vote.choices[p->index] = VOTE_YES;
	}
	foreach(p, players) {
		if(p->team == player.team && p->clientHd != ClientHandle::INVALID) replication->SendRequestVoting(p->clientHd, player.userID, type, (u32)periodMs, voters, false);
	}
	foreach(voter, players) {
		if(voter->team != player.team || vote.choices[voter->index] != VOTE_YES) continue;
		if(type == VOTE_TYPE_TITAN && voter->index == player.index) continue;
		foreach(p, players) {
			if(p->team == player.team && p->clientHd != ClientHandle::INVALID) replication->SendVote(p->clientHd, type, (u32)VOTE_YES);
		}
	}
	LOG("Vote :: %ls starts vote %d for team %d", player.name.data(), type, (i32)player.team);
	if(type == VOTE_TYPE_TITAN) ApplyStatus(player, player, TITAN_READY_STATUS, 0, periodMs / 1000.0f);
	UpdateVotes();
}

void World::CastVote(Player& player, u32 choice)
{
	TeamVote& vote = teamVotes[player.team];
	if(!vote.active || vote.choices[player.index] != VOTE_NONE) return;
	vote.choices[player.index] = (i8)choice;
	foreach(p, players) {
		if(p->team == player.team && p->clientHd != ClientHandle::INVALID) replication->SendVote(p->clientHd, vote.type, choice);
	}
	UpdateVotes();
}

void World::FinishVote(i32 team, bool passed)
{
	TeamVote& vote = teamVotes[team];
	vote.active = false;
	const UserID requestor = vote.requestor >= 0 ? players[vote.requestor].userID : UserID(0);
	u32 reason = passed ? VOTE_REASON_PASSED : VOTE_REASON_REJECTED;
	u32 playerCool = 0;
	u32 teamCool = 0;
	if(vote.type == VOTE_TYPE_TITAN) {
		reason = VOTE_REASON_PASSED;
		if(passed) teamCool = (u32)GetGameXmlContent().titan.teamCoolTime;
		else playerCool = (u32)GetGameXmlContent().titan.failPlayerCool;
	}
	else if(!passed) teamCool = (u32)VOTE_SURRENDER_FAIL_COOLDOWN_MS;
	if(teamCool > 0) vote.cooldownUntil = TimeAdd(localTime, TimeMsToTime((f64)teamCool));
	if(playerCool > 0 && vote.requestor >= 0) players[vote.requestor].titanVoteBlockedUntil = TimeAdd(localTime, TimeMsToTime((f64)playerCool));
	foreach(p, players) {
		if(p->team == team && p->clientHd != ClientHandle::INVALID) replication->SendVotingResult(p->clientHd, requestor, vote.type, passed ? VOTE_STATE_PASSED : VOTE_STATE_FAILED, reason, playerCool, teamCool);
	}
	LOG("Vote :: team %d vote %d %s", team, vote.type, passed ? "passed" : "rejected");
	if(vote.type == VOTE_TYPE_TITAN && vote.requestor >= 0) ConsumeStatus((u32)vote.requestor, TITAN_READY_STATUS);
	if(!passed) return;
	if(vote.type == VOTE_TYPE_SURRENDER && winnerTeam < 0) winnerTeam = team == 0 ? 1 : 0;
	if(vote.type == VOTE_TYPE_TITAN && vote.requestor >= 0) TransformTitan(players[vote.requestor], 0, true);
}

void World::UpdateVotes()
{
	for(i32 team = 0; team < 2; team++) {
		TeamVote& vote = teamVotes[team];
		if(!vote.active) continue;
		i32 voters = 0, yes = 0, no = 0;
		foreach_const(p, players) {
			if(p->team != team) continue;
			voters++;
			if(vote.choices[p->index] == VOTE_YES) yes++;
			else if(vote.choices[p->index] != VOTE_NONE) no++;
		}
		const i32 needed = vote.type == VOTE_TYPE_TITAN ? voters / 2 + 1 : voters <= 3 ? voters : voters - 1;
		if(yes >= needed) FinishVote(team, true);
		else if(voters - no < needed || localTime >= vote.endAt) FinishVote(team, false);
	}
}

u32 World::KillPoints() const
{
	return map && map->ingameLevels ? (u32)GetGameXmlContent().ingameLevel.killPoints : 1;
}

void World::AddPoints(Player& player, i32 points)
{
	if(!map || !map->ingameLevels || points <= 0) return;
	player.points += points;
	i32 teamPoints = 0;
	foreach_const(p, players) if(p->team == player.team) teamPoints += p->points;
	replication->SendPointsToAll(player.userID, player.team, player.points, teamPoints);
}

void World::GrantMonsterKillPoints(const ActorNpc& npc, ActorUID killerUID)
{
	if(!map || !map->ingameLevels) return;
	static const u32 minionTag = StatusTypeKey("Minion");
	static const u32 towerCenterTag = StatusTypeKey("Tower_Center");
	static const u32 towerSideTag = StatusTypeKey("Tower_Side");
	static const u32 chiefTag = StatusTypeKey("Chief");
	if(ActorMaster* master = FindMasterActor(killerUID)) {
		if(master->parent && npc.monsterAi) {
			const u32 tag = npc.monsterAi->tagKey;
			i32 kind = -1;
			if(tag == minionTag) kind = END_KILL_MINION;
			else if(tag == towerCenterTag || tag == towerSideTag) kind = END_KILL_TOWER;
			else if(tag == chiefTag || npc.faction == Faction::ORANGE) kind = END_KILL_BOSS;
			else if(npc.faction == Faction::MONSTER) kind = END_KILL_NEUTRAL;
			if(kind >= 0 && master->parent->monsterKills[kind] < 255) master->parent->monsterKills[kind]++;
		}
	}
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	auto found = info.monsters.find((i32)npc.docID);
	ActorMaster* killer = FindMasterActor(killerUID);
	if(found == info.monsters.end() || found->second.points <= 0 || !killer || !killer->parent || !KillRewarded(npc, *killer->parent)) return;
	replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_MONSTER_KILL, 0, killerUID, npc.UID, (u32)found->second.points);
	AddPoints(*killer->parent, found->second.points);
}

static const f64 GIANT_FIRST_WAVE_SEC = 222.0;
static const f64 GIANT_WAVE_PERIOD_SEC = 180.0;
static const u32 GIANT_TEAM_SPREAD_MS = 40000;

void World::BuildGiantTriggers()
{
	giantTriggersBuilt = true;
	const auto& content = GetGameXmlContent();
	foreach_const(n, map->levelEvents) {
		if(n->kind != GameXmlContent::LevelEventNode::AI_TRIGGER) continue;
		eastl::fixed_vector<i32,16,false> open;
		foreach_const(o, n->outputs) open.push_back(*o);
		for(i32 k = 0; k < (i32)open.size() && k < 16; k++) {
			const GameXmlContent::LevelEventNode* c = map->FindLevelEvent(open[k]);
			if(!c) continue;
			if(c->kind == GameXmlContent::LevelEventNode::CALL_MONSTER) {
				if((c->team == Faction::RED || c->team == Faction::BLUE) && content.FindMonsterDieSpawn(c->monster) && !giantTriggers.full()) giantTriggers.push_back({ n->aiIndex, (i32)c->team, c->monster });
				break;
			}
			if(c->kind != GameXmlContent::LevelEventNode::DELAY) continue;
			foreach_const(o, c->outputs) if(!open.full()) open.push_back(*o);
		}
	}
	eastl::sort(giantTriggers.begin(), giantTriggers.end(), [](const GiantTrigger& a, const GiantTrigger& b) { return a.monster < b.monster; });
	LOG("Titan :: %d giant triggers", (i32)giantTriggers.size());
}

void World::UpdateGiantWaves()
{
	if(!map || gameStartedAt == Time::ZERO || frozen) return;
	if(!giantTriggersBuilt) BuildGiantTriggers();
	if(giantTriggers.empty()) return;
	if(giantNextWaveAt == Time::ZERO) giantNextWaveAt = TimeAdd(gameStartedAt, TimeMsToTime(GIANT_FIRST_WAVE_SEC * 1000.0));
	if(localTime >= giantNextWaveAt) {
		giantNextWaveAt = TimeAdd(giantNextWaveAt, TimeMsToTime(GIANT_WAVE_PERIOD_SEC * 1000.0));
		const i32 first = (i32)(RandUint() % 2);
		giantTeamAt[first] = localTime;
		giantTeamAt[1 - first] = TimeAdd(localTime, TimeMsToTime((f64)(RandUint() % GIANT_TEAM_SPREAD_MS)));
	}
	for(i32 team = 0; team < 2; team++) {
		if(giantTeamAt[team] == Time::ZERO || localTime < giantTeamAt[team]) continue;
		giantTeamAt[team] = Time::ZERO;
		const auto& order = GetGameXmlContent().titan.giants;
		i32 variants = 0;
		foreach_const(t, giantTriggers) if(t->team == team) variants++;
		if(variants == 0) continue;
		const u32 pick = giantCycle++;
		i32 rank = 0;
		foreach_const(t, giantTriggers) {
			if(t->team != team) continue;
			const bool chosen = order.empty() ? rank == (i32)(pick % (u32)variants) : t->monster == order[pick % (u32)order.size()];
			rank++;
			if(!chosen) continue;
			LOG("Titan :: giant %d for team %d", t->monster, team);
			if(!giantSummons[team].full()) giantSummons[team].push_back(t->monster);
			FireAITrigger(t->aiIndex, -1);
			break;
		}
	}
}

static const i32 TITAN_SPAWN_STATUS = 1239000040;
static const i32 TITAN_SPAWN_SELF_STATUS = 1239000041;
static const i32 AI_TRIGGER_TRANSFORM_TITAN = 300;
static const f64 TITAN_MODE_SWITCH_DELAY_MS = 620.0;

void World::TransformTitan(Player& player, i32 formIndex, bool initial)
{
	const GameXmlContent::TitanInfo& titan = GetGameXmlContent().titan;
	if(formIndex < 0 || formIndex >= (i32)titan.forms.size()) return;
	const GameXmlContent::TitanForm& form = titan.forms[formIndex];
	ActorMaster& chara = player.Main();
	if(initial) {
		AddTeamPieces((i32)player.team, -titan.cost);
		player.titanSavedHp = chara.hp;
		player.titanUntil = TimeAdd(localTime, TimeMsToTime((f64)form.duration));
		chara.hpMax = (i32)form.Stat((u8)StatType::STAT_TYPE_HEALTH);
		chara.hp = chara.hpMax;
		if(player.clientHd != ClientHandle::INVALID) replication->SendTransformAnswer(player.clientHd, chara.UID, form.doc);
	}
	player.titanForm = formIndex;
	replication->SendTitanFormToAll(!initial, chara.UID, form.doc, initial ? (u32)form.duration : 0, form.weapon, form.skill1, form.skill2, form.ug, form.stats.data(), (u16)form.stats.size(), (f32)chara.hp, form.Stat((u8)StatType::STAT_TYPE_STAMINA), 0.0f, form.moveSpeed);
	LOG("Titan :: %ls becomes %d (%d hp)", player.name.data(), form.doc, chara.hp);
	if(!initial) return;
	ApplyStatus(player, player, TITAN_SPAWN_STATUS);
	ApplyStatus(player, player, TITAN_SPAWN_SELF_STATUS);
	FireAITrigger(AI_TRIGGER_TRANSFORM_TITAN, (i32)player.index);
}

void World::RevertTitan(Player& player)
{
	if(player.titanForm < 0) return;
	ActorMaster& chara = player.Main();
	player.titanForm = -1;
	player.titanUntil = Time::ZERO;
	player.titanModeAt = Time::ZERO;
	RefreshMasterHealth(player, chara);
	chara.hp = player.dead ? 0 : MIN(MAX(player.titanSavedHp, 1), chara.hpMax);
	const auto& master = GetGameXmlContent().GetMaster(chara.classType);
	eastl::fixed_vector<StatValue,32,false> stats;
	foreach_const(st, master.baseStats) {
		StatValue v = *st;
		if(v.type == (u8)StatType::STAT_TYPE_HEALTH) v.value = (f32)chara.hpMax;
		if(!stats.full()) stats.push_back(v);
	}
	replication->SendTitanFormToAll(false, chara.UID, 100000000 + (i32)chara.classType, 0, 0, -1, -1, -1, stats.data(), (u16)stats.size(), (f32)chara.hp, MasterStat(chara.classType, (u8)StatType::STAT_TYPE_STAMINA), MAX(player.ultGauge, 0.0f), master.moveSpeed);
	replication->SendStatToAll(chara.UID, (u8)StatType::STAT_TYPE_HEALTH, (f32)chara.hp, (f32)chara.hpMax);
	LOG("Titan :: %ls back to master (%d/%d hp)", player.name.data(), chara.hp, chara.hpMax);
}

void World::TitanOnCast(Player& player, SkillID skill)
{
	if(player.titanForm < 0) return;
	const GameXmlContent::TitanInfo& titan = GetGameXmlContent().titan;
	if((i32)skill != titan.forms[player.titanForm].ug || titan.forms.size() < 2) return;
	player.titanModeAt = TimeAdd(localTime, TimeMsToTime(TITAN_MODE_SWITCH_DELAY_MS));
}

void World::UpdateTitans()
{
	foreach(p, players) {
		if(p->titanForm < 0) continue;
		if(p->titanUntil != Time::ZERO && localTime >= p->titanUntil) {
			RevertTitan(*p);
			continue;
		}
		if(p->titanModeAt != Time::ZERO && localTime >= p->titanModeAt) {
			p->titanModeAt = Time::ZERO;
			const i32 count = (i32)GetGameXmlContent().titan.forms.size();
			TransformTitan(*p, (p->titanForm + 1) % count, false);
		}
	}
}

void World::UpdateIngameLevels()
{
	UpdateVotes();
	UpdateTitans();
	UpdateGiantWaves();
	if(!map || !map->ingameLevels || frozen || nextPassiveExpAt == Time::ZERO || localTime < nextPassiveExpAt) return;
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	nextPassiveExpAt = TimeAdd(nextPassiveExpAt, TimeMsToTime(info.passivePeriod * 1000.0));
	foreach(p, players) GrantIngameExp(*p, info.passiveExp, ActorUID::INVALID);
}
