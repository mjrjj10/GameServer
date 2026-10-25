#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

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
