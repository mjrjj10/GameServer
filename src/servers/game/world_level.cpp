#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const u32 PVP_ANNOUNCE_MONSTER_KILL = 5;
static const u8 LEVEL_STAT_HEALTH = 0;
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
		p->propertyPoints = 0;
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
		if(info.propertyPointsEvery > 0 && player.ingameLevel % info.propertyPointsEvery == 0) player.propertyPoints += info.propertyPointsStep;
	}
	if(player.ingameLevel >= info.maxLevel) player.ingameExp = 0;
	if(player.clientHd != ClientHandle::INVALID) replication->SendExpIngame(player.clientHd, player.userID, gain, player.ingameExp, target);
	if(!levelUp) return;
	replication->SendLevelIngameToAll(player.userID, info.ExpToLevelUp(player.ingameLevel), player.ingameLevel);
	ApplyLevelStats(player);
	if(player.clientHd != ClientHandle::INVALID && player.propertyPoints > 0) {
		replication->SendCharacterPropertyPoint(player.clientHd, player.Main().UID, player.propertyPoints);
		replication->SendCharacterPropertyPoint(player.clientHd, player.Sub().UID, player.propertyPoints);
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
		const i32 hpMax = (i32)(MasterStat(chara.classType, LEVEL_STAT_HEALTH) + LevelBonus(player, chara.classType, LEVEL_STAT_HEALTH));
		if(chara.hp > 0) chara.hp = MIN(chara.hp + MAX(hpMax - chara.hpMax, 0), hpMax);
		chara.hpMax = hpMax;
		replication->SendStatToAll(chara.UID, LEVEL_STAT_HEALTH, (f32)chara.hp, (f32)chara.hpMax);
		const WeaponIndex weapon = (chara.UID == player.Main().UID) == ((player.mainCharaID & 1) == 0) ? player.mainWeapon : player.subWeapon;
		replication->SendStatToAll(chara.UID, LEVEL_STAT_ATTACK, 0, GetGameXmlContent().GetWeaponAttack((i32)weapon) + LevelBonus(player, chara.classType, LEVEL_STAT_ATTACK));
		replication->SendStatToAll(chara.UID, LEVEL_STAT_SKILLPOWER, 0, MasterStat(chara.classType, LEVEL_STAT_SKILLPOWER) + LevelBonus(player, chara.classType, LEVEL_STAT_SKILLPOWER));
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
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	auto found = info.monsters.find((i32)npc.docID);
	ActorMaster* killer = FindMasterActor(killerUID);
	if(found == info.monsters.end() || found->second.points <= 0 || !killer || !killer->parent || !KillRewarded(npc, *killer->parent)) return;
	replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_MONSTER_KILL, 0, killerUID, npc.UID, (u32)found->second.points);
	AddPoints(*killer->parent, found->second.points);
}

void World::UpdateIngameLevels()
{
	if(!map || !map->ingameLevels || frozen || nextPassiveExpAt == Time::ZERO || localTime < nextPassiveExpAt) return;
	const GameXmlContent::IngameLevelInfo& info = GetGameXmlContent().ingameLevel;
	nextPassiveExpAt = TimeAdd(nextPassiveExpAt, TimeMsToTime(info.passivePeriod * 1000.0));
	foreach(p, players) GrantIngameExp(*p, info.passiveExp, ActorUID::INVALID);
}
