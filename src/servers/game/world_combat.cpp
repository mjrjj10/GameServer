#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const i32 STAT_CRITICALRATE = 9;
static const i32 STAT_CRITICALDAMAGE = 10;
static const i32 STAT_RAGE_ATTACK_RECOVERY_VALUE = 51;
static const u32 STAT_REASON_HIT = 1;
static const i32 STAT_SKILL_CRITICALRATE = 63;
static const i32 STAT_SKILL_CRITICALDAMAGE = 64;
static const u32 PACKET_DAMAGE_TYPE_NORMAL = 0;
static const u32 PACKET_DAMAGE_TYPE_CRITICAL = 2;
static const i32 STAT_ELEMENT_DAMAGE_BONUS = 54;
static const i32 STAT_ELEMENT_SKILL_DAMAGE_BONUS = 55;
static const i32 STAT_DEFENCE = 7;
static const i32 STAT_DEFENCE_PENETRATION = 20;
static const i32 STAT_SKILL_PENETRATION = 21;
static const i32 STAT_SKILLDAMAGE_REDUCE_RATIO = 58;
static const f32 DEFENCE_SCALE = 100.0f;
static const u32 PVP_ANNOUNCE_PLAIN_KILL = 0;
static const u32 PVP_ANNOUNCE_KILL = 1;
static const u32 PVP_ANNOUNCE_FIRST_BLOOD = 2;
static const u32 PVP_ANNOUNCE_MULTI_KILL = 3;
static const u32 PVP_ANNOUNCE_KILL_STREAK = 4;
static const u32 PVP_ANNOUNCE_TEAM_ALL_DEAD = 6;
static const u32 PVP_ANNOUNCE_SHUTDOWN = 7;
static const f64 MULTI_KILL_WINDOW_MS = 15000.0;
static const f64 DEAD_DAMAGE_WINDOW_MS = 15000.0;
static const f32 PENETRATION_CAP = 50.0f;
static const f32 SKILLDAMAGE_REDUCE_CAP = 80.0f;
static const i32 STATUS_ELECTRO_PASSIVE_BUFF_MPREGEN = 1230040011;
static const f64 RESPAWN_BASE_UNTIL_SEC[] = { 50.0, 170.0 };
static const i32 RESPAWN_BASE_MS[] = { 3000, 4000, 5000 };
static const i32 RESPAWN_LEAD_MIN[] = { 1, 2, 4 };
static const i32 RESPAWN_LEAD_BONUS_MS[] = { 2000, 4000, 7000 };

static i32 RespawnDelayMs(f64 elapsedSec, i32 lead)
{
	i32 step = 0;
	while(step < (i32)ARRAY_COUNT(RESPAWN_BASE_UNTIL_SEC) && elapsedSec >= RESPAWN_BASE_UNTIL_SEC[step]) step++;
	i32 delay = RESPAWN_BASE_MS[step];
	for(i32 i = ARRAY_COUNT(RESPAWN_LEAD_MIN) - 1; i >= 0; i--) {
		if(lead >= RESPAWN_LEAD_MIN[i]) { delay += RESPAWN_LEAD_BONUS_MS[i]; break; }
	}
	return delay;
}

static bool ElementBeats(Element a, Element b)
{
	return (a == Element::RED && b == Element::GREEN) || (a == Element::GREEN && b == Element::BLUE) || (a == Element::BLUE && b == Element::RED);
}

static u32 ElementRelation(f32 elementMul)
{
	return elementMul > 1.0f ? 1 : (elementMul < 1.0f ? 2 : 0);
}

static f32 ElementMultiplier(const World::Player& attacker, const World::Player& target, bool skill)
{
	const auto& content = GetGameXmlContent();
	const ClassType ac = attacker.Main().classType;
	const Element ea = content.GetCreatureElement((i32)content.GetMaster(ac).ID);
	const Element et = content.GetCreatureElement((i32)content.GetMaster(target.Main().classType).ID);
	const i32 stat = skill ? STAT_ELEMENT_SKILL_DAMAGE_BONUS : STAT_ELEMENT_DAMAGE_BONUS;
	if(ElementBeats(ea, et)) return 1.0f + MasterStat(ac, stat) / 100.0f;
	if(ElementBeats(et, ea)) return 1.0f - MasterStat(target.Main().classType, stat) / 100.0f;
	return 1.0f;
}

void World::PassiveOnWeaponHit(Player& attacker)
{
	if(attacker.dead || localTime < attacker.hitRecoverReadyAt) return;
	const ActiveStatus* st = FindPassiveOfType(attacker, StatusType::HIT_RECOVER);
	const HitRecoverRule* rule = st ? FindHitRecoverRule(st->statusID) : nullptr;
	ActorMaster& chara = attacker.Main();
	if(!rule || chara.hp >= chara.hpMax) return;
	attacker.hitRecoverReadyAt = TimeAdd(localTime, TimeMsToTime(rule->cooldownMs));
	const i32 amount = (i32)(chara.hpMax * rule->hpRatio);
	chara.hp = MIN(chara.hp + amount, chara.hpMax);
	replication->SendStatToAll(chara.UID, (i32)StatType::STAT_TYPE_HEALTH, (f32)chara.hp, (f32)chara.hpMax, STAT_REASON_HIT);
	LOG("Passive :: %ls weapon hit recovers %d hp (status %d)", attacker.name.data(), amount, st->statusID);
}

void World::HealPlayer(Player& healer, Player& target, i32 amount)
{
	if(target.dead || amount <= 0) return;
	ActorMaster& chara = target.Main();
	chara.hp = MIN(chara.hp + amount, chara.hpMax);
	LOG("Heal :: %ls -> %ls amount=%d hp=%d/%d", healer.name.data(), target.name.data(), amount, chara.hp, chara.hpMax);
	if(healer.index != target.index && healer.Main().classType == ClassType::ELECTRO && FindPassiveOfType(healer, StatusType::ELECTRO)) ApplyStatus(healer, healer, STATUS_ELECTRO_PASSIVE_BUFF_MPREGEN);
	replication->SendHpToAll(chara.UID, chara.hp, chara.hpMax);
}

f32 World::HitRadius(const Player& p) const
{
	if(p.Main().classType == ClassType::NONE) return 0;
	return (f32)GetGameXmlContent().GetMaster(p.Main().classType).character.getColliderRadius();
}

f32 World::BodyMidHeight(const Player& p) const
{
	if(p.Main().classType == ClassType::NONE) return 0;
	return (f32)GetGameXmlContent().GetMaster(p.Main().classType).character.getColliderHeight() * 0.5f;
}

void World::CounterAttack(Player& attacker, Player& target, bool weaponHit)
{
	i32 stun = 0;
	SkillID skill = SkillID::INVALID;
	foreach(it, activeStatuses) {
		if(it->targetIndex != target.index || !it->info || it->info->type != StatusType::COUNTERSTATUS || it->until <= localTime) continue;
		it->until = localTime;
		stun = (i32)it->info->ParamInt("_CustomAddStatusIndex");
		skill = SkillID(it->info->skillIndex);
		break;
	}
	if(skill == SkillID::INVALID || !weaponHit || attacker.dead || !WeaponIsMelee(attacker)) return;
	LOG("Status :: %ls counters %ls (skill %d, status %d)", target.name.data(), attacker.name.data(), (i32)skill, stun);
	const ActorUID attackerUID = attacker.Main().UID;
	PlayerCastSkill(target, skill, attacker.body->GetWorldPos(), Slice<const ActorUID>(&attackerUID, 1), 0, ActionStateID::EXTRAMOTION_3_BEHAVIORSTATE);
	if(stun != 0) ApplyStatus(target, attacker, stun);
}

void World::DamagePlayer(Player& attacker, Player& target, SkillID skillID, i32 damage, const Projectile* proj, u32 remoteID, RemoteIdx remoteDoc, vec2 remoteDir)
{
	if(target.dead || target.untargetable) return;
	if(attacker.index != target.index) {
		attacker.lastHitTargetIndex = (i32)target.index;
		attacker.lastHitAt = localTime;
		MarkInBattle(attacker);
		MarkInBattle(target);
	}
	if(HasStatusType(target, StatusType::GODMODE)) return;
	if(attacker.team != target.team && HasStatusType(target, StatusType::BURROW)) return;
	if(attacker.team != target.team && !proj && skillID != SkillID::INVALID && HasStatusType(target, StatusType::IMMUNESKILL)) {
		LOG("Status :: %ls immune to skill %d", target.name.data(), (i32)skillID);
		return;
	}
	const i32 damageIn = damage;
	f32 elementMul = 1.0f;
	if(HasStatusType(target, StatusType::EVADE) && attacker.team != target.team) {
		LOG("Evade :: %ls evaded %ls", target.name.data(), attacker.name.data());
		replication->SendEvadeToAll(attacker.Main().UID, target.Main().UID, proj ? proj->remoteID : remoteID, proj ? proj->remoteDoc : remoteDoc, proj ? proj->seedOwnerHd : ClientHandle::INVALID);
		OnEvade(target);
		return;
	}

	{
		const RemoteIdx doc = proj ? proj->remoteDoc : remoteDoc;
		const Remote* pr = doc != RemoteIdx::INVALID ? GetGameXmlContent().FindRemote(doc) : nullptr;
		if(pr && pr->distPenaltyRate > 0 && pr->penaltyApplyDistance > 0 &&
		   glm::length(vec2(target.body->GetWorldPos()) - vec2(attacker.body->GetWorldPos())) < pr->penaltyApplyDistance) {
			damage = (i32)(damage * (1.0f - pr->distPenaltyRate / 100.0f));
		}
		if(pr && pr->statusTypeMultiplyKey != 0 && pr->statusTypeMultiplier != 1.0f) {
			foreach_const(st, activeStatuses) {
				if(st->targetIndex != target.index || !st->info || st->info->typeKey != pr->statusTypeMultiplyKey || st->until <= localTime) continue;
				damage = (i32)(damage * pr->statusTypeMultiplier);
				break;
			}
		}
	}

	if(attacker.index != target.index) {
		const bool skillHit = !proj && skillID != SkillID::INVALID;
		const f32 def = MasterStat(target.Main().classType, STAT_DEFENCE) * MAX(1.0f + StatusModMulSum(target, StatusMod::DEFENCE) / 100.0f, 0.0f);
		const f32 pen = MIN(MAX(MasterStat(attacker.Main().classType, skillHit ? STAT_SKILL_PENETRATION : STAT_DEFENCE_PENETRATION), 0.0f), PENETRATION_CAP) / 100.0f;
		f32 mul = DEFENCE_SCALE / (DEFENCE_SCALE + MAX(def * (1.0f - pen), 0.0f));
		if(skillHit) mul *= 1.0f - MIN(MAX(MasterStat(target.Main().classType, STAT_SKILLDAMAGE_REDUCE_RATIO), 0.0f), SKILLDAMAGE_REDUCE_CAP) / 100.0f;
		damage = MAX((i32)(damage * mul + 0.5f), damage > 0 ? 1 : 0);
	}

	u32 damageType = PACKET_DAMAGE_TYPE_NORMAL;
	if(proj || skillID != SkillID::INVALID) {
		const f32 mul = ElementMultiplier(attacker, target, !proj);
		if(mul != 1.0f) damage = (i32)(damage * mul);
		elementMul = mul;

		const ClassType ac = attacker.Main().classType;
		const RemoteIdx doc = proj ? proj->remoteDoc : remoteDoc;
		const Remote* critRemote = doc != RemoteIdx::INVALID ? GetGameXmlContent().FindRemote(doc) : nullptr;
		const f32 rate = MasterStat(ac, proj ? STAT_CRITICALRATE : STAT_SKILL_CRITICALRATE) + (critRemote ? critRemote->criticalRate : 0) +
			StatusModSum(attacker, proj ? StatusMod::CRITICALRATE : StatusMod::SKILL_CRITICALRATE);
		f32 bonus = 0;
		foreach_const(st, activeStatuses) {
			if(st->targetIndex != target.index || !st->info || localTime >= st->until) continue;
			if(st->info->type == StatusType::WEAK_ON_CRITICAL && attacker.team != target.team) bonus = 100.0f;
		}
		if(rate + bonus > 0 && (RandUint() % 10000) < (u32)((rate + bonus) * 100.0f)) {
			const f32 critDamage = MasterStat(ac, proj ? STAT_CRITICALDAMAGE : STAT_SKILL_CRITICALDAMAGE);
			damage = (i32)(damage * (critDamage > 0 ? critDamage : 100.0f) / 100.0f);
			damageType = PACKET_DAMAGE_TYPE_CRITICAL;
		}
	}

	foreach(it, activeStatuses) {
		if(it->targetIndex == target.index && it->info && it->info->nextCondition == StatusInfo::NextCondition::REMOVE_ONHIT) it->removeOnHit = true;
	}

	foreach(it, activeStatuses) {
		if(it->targetIndex != target.index || it->shieldHits <= 0 || it->until <= localTime || attacker.team == target.team) continue;
		it->shieldHits--;
		LOG("Status :: %ls shield %d absorbs a hit (%d left)", target.name.data(), it->statusID, it->shieldHits);
		CounterAttack(attacker, target, proj != nullptr);
		return;
	}
	foreach(it, activeStatuses) {
		if(damage <= 0) break;
		if(it->targetIndex != target.index || it->shieldHP <= 0) continue;
		const f32 absorbed = MIN(it->shieldHP, (f32)damage);
		it->shieldHP -= absorbed;
		it->absorbed += absorbed;
		damage -= (i32)absorbed;
	}
	if(damage <= 0) return;

	ActorMaster& chara = target.Main();
	ShowcaseOnDamage(target, damage);
	target.damageTaken += damage;
	target.hitsTaken++;
	chara.hp = MAX(chara.hp - damage, (HasStatusType(target, StatusType::NEARLY_IMMORTAL) || HasStatusType(target, StatusType::UNDEAD)) ? 1 : 0);
	if(ShowcaseKeepsAlive(target)) chara.hp = MAX(chara.hp, 1);
	if(chara.hp == 0) PassiveOnLethal(target);
	if(!reflecting && attacker.team != target.team && attacker.index != target.index && (proj || skillID != SkillID::INVALID)) {
		const f32 ratio = StatusModSum(target, StatusMod::REFLECT_DAMAGE_RATIO);
		if(ratio > 0 && !attacker.dead) {
			reflecting = true;
			LOG("Status :: %ls reflects %d to %ls", target.name.data(), (i32)(damage * ratio / 100.0f), attacker.name.data());
			DamagePlayer(target, attacker, SkillID::INVALID, (i32)(damage * ratio / 100.0f));
			reflecting = false;
		}
	}

	if(attacker.res[1] >= 0) {
		const ClassType ac = attacker.Main().classType;
		const f32 rageMax = MasterStat(ac, RESOURCES[1].maxStat);
		if(rageMax > 0) {
			attacker.rageGainAt = localTime;
			if(attacker.res[1] < rageMax) {
				attacker.res[1] = MIN(attacker.res[1] + MasterStat(ac, STAT_RAGE_ATTACK_RECOVERY_VALUE), rageMax);
				attacker.resDirty = true;
			}
		}
	}

	LOG("Damage :: %ls -> %ls skill=%d damage=%d%s hp=%d/%d in=%d remote=%u element=%d", attacker.name.data(), target.name.data(), (i32)skillID, damage, damageType == PACKET_DAMAGE_TYPE_CRITICAL ? " CRIT" : "", chara.hp, chara.hpMax,
		damageIn, (u32)(proj ? proj->remoteDoc : remoteDoc), (i32)(elementMul * 100.0f));
	if(attacker.team != target.team) {
		for(i32 k = 0; k < (i32)target.recentDamage.size(); ) {
			if(localTime > TimeAdd(target.recentDamage[k].at, TimeMsToTime(DEAD_DAMAGE_WINDOW_MS))) target.recentDamage.erase(target.recentDamage.begin() + k);
			else k++;
		}
		if(target.recentDamage.full()) target.recentDamage.erase(target.recentDamage.begin());
		target.recentDamage.push_back({ attacker.index, attacker.userID, damageAttackerOverride != ActorUID::INVALID ? damageAttackerOverride : attacker.Main().UID, proj ? SkillID::INVALID : skillID, damageStatusID, damage, (u8)(damageType == PACKET_DAMAGE_TYPE_CRITICAL ? 1 : 0), localTime });
	}
	f32 carryLift = 0;
	foreach_const(st, activeStatuses) {
		if(st->targetIndex != target.index || localTime >= st->until) continue;
		for(const CarryLiftRule& rule : MEASURED_CARRY_LIFTS) if(rule.status == st->statusID) carryLift = rule.lift;
	}
	if(proj && proj->remoteID != 0) {
		Replication::RemoteDamage hit;
		hit.remoteID = proj->remoteID;
		hit.remoteDoc = proj->remoteDoc;
		hit.attackerUID = attacker.Main().UID;
		hit.targetUID = chara.UID;
		hit.remotePos = vec3(vec2(proj->pos), target.body->GetWorldPos().z + carryLift);
		hit.targetPos = target.body->GetWorldPos() + vec3(0, 0, carryLift);
		hit.dir = proj->dir;
		hit.damageType = damageType;
		hit.elementRelation = ElementRelation(elementMul);
		hit.damage = damage;
		hit.hp = chara.hp;
		hit.hpMax = chara.hpMax;
		hit.seedOwnerHd = proj->seedOwnerHd;
		replication->SendRemoteDamageToAll(hit);
	}
	else if(remoteID != 0) {
		Replication::RemoteDamage hit;
		hit.remoteID = remoteID;
		hit.remoteDoc = remoteDoc;
		hit.attackerUID = damageAttackerOverride != ActorUID::INVALID ? damageAttackerOverride : attacker.Main().UID;
		hit.targetUID = chara.UID;
		hit.remotePos = statusForceCenterSet ? statusForceCenter : target.body->GetWorldPos() + vec3(0, 0, carryLift);
		hit.targetPos = target.body->GetWorldPos() + vec3(0, 0, carryLift);
		hit.dir = remoteDir;
		hit.damageType = damageType;
		hit.elementRelation = ElementRelation(elementMul);
		hit.damage = damage;
		hit.hp = chara.hp;
		hit.hpMax = chara.hpMax;
		hit.seedOwnerHd = damageSeedOwnerOverride;
		replication->SendRemoteDamageToAll(hit);
	}
	else {
		replication->SendDamageToAll(attacker.Main().UID, chara.UID, skillID, damage, chara.hp, chara.hpMax, damageType, damageStatusID);
	}
	if(proj && attacker.index != target.index && attacker.team != target.team) PassiveOnWeaponHit(attacker);

	if(chara.hp > 0) PassiveOnDamaged(target);
	if(chara.hp == 0) {
		target.dead = true;
		attacker.kills++;
		target.deaths++;
		GrantSkillPoint(target);
		i32 teamKills = 0;
		i32 victimTeamKills = 0;
		foreach_const(it, players) {
			if(it->team == attacker.team) teamKills += it->kills;
			if(it->team == target.team) victimTeamKills += it->kills;
		}
		const i32 respawnMs = gameStartedAt == Time::ZERO ? RESPAWN_BASE_MS[ARRAY_COUNT(RESPAWN_BASE_MS) - 1] : RespawnDelayMs(TimeDiffSec(TimeDiff(gameStartedAt, localTime)), victimTeamKills - teamKills);
		target.tRespawn = TimeAdd(localTime, TimeMsToTime(respawnMs));
		LOG("Death :: %ls killed by %ls (respawn %d ms)", target.name.data(), attacker.name.data(), respawnMs);
		if(target.hiddenUID != ActorUID::INVALID) {
			ShowHiddenToEnemies(target, target.hiddenUID, ActorUID::INVALID);
			target.hiddenUID = ActorUID::INVALID;
		}
		bool specialAnnounced = false;
		if(attacker.team != target.team) {
			if(!firstBloodDone) {
				firstBloodDone = true;
				specialAnnounced = true;
				replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_FIRST_BLOOD, 0, attacker.Main().UID, chara.UID);
			}
			const bool chained = attacker.lastKillAt != Time::ZERO && localTime < TimeAdd(attacker.lastKillAt, TimeMsToTime(MULTI_KILL_WINDOW_MS));
			attacker.multiKill = chained ? attacker.multiKill + 1 : 1;
			attacker.lastKillAt = localTime;
			attacker.killStreak++;
			if(attacker.multiKill >= 2) replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_MULTI_KILL, (u32)attacker.multiKill, attacker.Main().UID, chara.UID);
			if(attacker.killStreak >= 2) replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_KILL_STREAK, (u32)attacker.killStreak, attacker.Main().UID, chara.UID);
			if(target.killStreak >= 2) replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_SHUTDOWN, 0, attacker.Main().UID, chara.UID);
			specialAnnounced = specialAnnounced || attacker.multiKill >= 2 || attacker.killStreak >= 2 || target.killStreak >= 2;
		}
		if(!specialAnnounced) replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_PLAIN_KILL, 0, attacker.Main().UID, chara.UID);
		target.killStreak = 0;
		replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_KILL, 0, attacker.Main().UID, chara.UID);
		if(attacker.team != target.team) {
			bool allDead = true;
			foreach_const(it, players) if(it->team == target.team && !it->dead) allDead = false;
			if(allDead) replication->SendPvpAnnouncementToAll(PVP_ANNOUNCE_TEAM_ALL_DEAD, (u32)(target.team == 0 ? TeamType::RED : TeamType::BLUE), attacker.Main().UID, chara.UID);
		}
		replication->SendScoreToAll(attacker.userID, attacker.team, attacker.kills, teamKills, target.userID, target.team, target.deaths, victimTeamKills);
		for(i32 k = 0; k < (i32)target.recentDamage.size(); ) {
			if(localTime > TimeAdd(target.recentDamage[k].at, TimeMsToTime(DEAD_DAMAGE_WINDOW_MS))) target.recentDamage.erase(target.recentDamage.begin() + k);
			else k++;
		}
		Time firstDamageAt = localTime;
		foreach_const(d, target.recentDamage) if(d->at < firstDamageAt) firstDamageAt = d->at;
		foreach(it, players) {
			if(it->index == attacker.index || it->team == target.team) continue;
			bool assisted = false;
			foreach_const(d, target.recentDamage) {
				if(d->attackerIndex == it->index && d->damage > 0) assisted = true;
			}
			if(!assisted) continue;
			it->assists++;
			i32 teamScore = 0;
			foreach_const(p, players) if(p->team == it->team) teamScore += p->kills;
			replication->SendAssistScoreToAll(it->userID, it->team, it->assists, it->kills, teamScore);
		}
		replication->SendDeadDamageInfo(target.clientHd, attacker.index, target.recentDamage.data(), (i32)target.recentDamage.size(), (i32)TimeDiffMs(TimeDiff(firstDamageAt, localTime)));
		target.recentDamage.clear();
		replication->SendDeathToAll(attacker.Main().UID, chara.UID, CreatureIndex(100000000 + (i32)attacker.Main().classType), proj ? proj->remoteID : remoteID, proj ? proj->remoteDoc : remoteDoc, target.userID, respawnMs);
	}
}

bool World::NpcHittableBy(const ActorNpc& npc, const Player& attacker) const
{
	if(npc.hp <= 0 || npc.ownerUID == ActorUID::INVALID || npc.removeAt != Time::ZERO) return false;
	if(npc.despawnAt != Time::ZERO && localTime >= npc.despawnAt) return false;
	return players[npc.ownerIndex].team != attacker.team;
}

void World::DamageNpc(Player& attacker, ActorNpc& npc, SkillID skillID, i32 damage)
{
	if(damage <= 0 || !NpcHittableBy(npc, attacker)) return;
	const i32 dealt = MAX((i32)(damage * DEFENCE_SCALE / (DEFENCE_SCALE + MAX(npc.defence, 0.0f)) + 0.5f), 1);
	npc.hp = MAX(npc.hp - dealt, 0);
	replication->SendDamageToAll(attacker.Main().UID, npc.UID, skillID, dealt, npc.hp, npc.hpMax);
	LOG("Summon :: creature=%d takes %d from %ls hp=%d/%d", (i32)npc.docID, dealt, attacker.name.data(), npc.hp, npc.hpMax);
	if(npc.hp <= 0) {
		npc.killerUID = attacker.Main().UID;
		npc.despawnAt = localTime;
	}
}
