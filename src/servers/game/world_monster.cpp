#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const f64 MONSTER_START_DELAY_MS = 90.0;
static const f64 MONSTER_SEARCH_MS = 250.0;
static const f64 MONSTER_SYNC_MS = 500.0;
static const f32 MONSTER_WAYPOINT_REACHED = 60.0f;
static const f32 MONSTER_REACH_TOLERANCE = 1.0f;
static const u8 MONSTER_ATTACK_STAT = 6;
static const u32 NPC_DEAD_NO_KILLER_DOC = 0;

static f32 MonsterAttackPower(const World::ActorNpc& npc)
{
	const GameXmlContent& content = GetGameXmlContent();
	const auto* stats = content.FindMonsterStats((i32)npc.docID);
	if(!stats) return 0;
	const auto& list = *stats;
	foreach_const(st, list) if(st->type == MONSTER_ATTACK_STAT) return (st->value + content.MonsterGrowth((i32)npc.docID, MONSTER_ATTACK_STAT) * npc.growthLevel) * npc.statScale;
	return 0;
}

static bool IsTeamFaction(Faction f)
{
	return f == Faction::RED || f == Faction::BLUE;
}

static bool FactionsOppose(Faction a, Faction b, bool hostileMonsters)
{
	if((a == Faction::RED && b == Faction::BLUE) || (a == Faction::BLUE && b == Faction::RED)) return true;
	return hostileMonsters && ((a == Faction::MONSTER && IsTeamFaction(b)) || (b == Faction::MONSTER && IsTeamFaction(a)));
}

static bool MonsterNeutral(const World::ActorNpc& npc, bool hostileMonsters)
{
	return npc.faction == Faction::ORANGE || (npc.faction == Faction::MONSTER && !hostileMonsters);
}

static GameXmlContent::MonsterAttack::Target MonsterTargetKind(const GameXmlContent::MonsterAi& ai)
{
	static const u32 minionTag = StatusTypeKey("Minion");
	static const u32 titanTag = StatusTypeKey("Titan");
	if(ai.tagKey == minionTag) return GameXmlContent::MonsterAttack::MINION;
	return ai.tagKey == titanTag ? GameXmlContent::MonsterAttack::TITAN : GameXmlContent::MonsterAttack::STRUCTURE;
}

bool World::MonsterHostile(const ActorNpc& monster, const ActorNpc& other) const
{
	if(&monster == &other || !other.monsterAi || other.hp <= 0 || other.removeAt != Time::ZERO) return false;
	if(other.despawnAt != Time::ZERO && localTime >= other.despawnAt) return false;
	return FactionsOppose(monster.faction, other.faction, hostileMonsters) && monster.monsterAi->Hates(other.monsterAi->tagKey);
}

bool World::MonsterHostile(const ActorNpc& monster, const Player& player) const
{
	static const u32 playerTag = StatusTypeKey("Player");
	if(player.dead || player.untargetable || !player.body) return false;
	if(MonsterNeutral(monster, hostileMonsters)) return monster.monsterTarget == player.Main().UID;
	return FactionsOppose(monster.faction, (Faction)player.team, hostileMonsters) && (monster.monsterAi->Hates(playerTag) || (hostileMonsters && monster.monsterAi->enemyTags.empty()));
}

static const u32 MONSTER_REMOTE_ID_BASE = 0xC0000000u;
static const u32 MONSTER_REMOTE_ID_MASK = 0x3FFFFFFFu;
static const f32 DEATH_HIT_KEEP_SEC = 1.0f;

void World::ApplyDeathHit(const DeathHit& hit)
{
	ActorNpc* helper = FindNpcActor(hit.helper);
	if(!helper) return;
	const GameXmlContent::DeathRemote& d = *hit.remote;
	const bool teamed = helper->faction == Faction::RED || helper->faction == Faction::BLUE;
	const i32 damage = (i32)(MonsterAttackPower(*helper) * d.multiplier + 0.5f);
	const i32 firstStatus = d.statuses.empty() ? 0 : d.statuses[0];
	if(d.pc) {
		foreach(p, players) {
			if(p->dead || !p->body || p->untargetable) continue;
			const bool ally = teamed && (Faction)p->team == helper->faction;
			if(ally != d.ally || (!teamed && d.ally)) continue;
			if(glm::length(vec2(p->body->GetWorldPos()) - vec2(helper->pos)) > d.radius) continue;
			if(damage > 0 && !d.ally) DamagePlayerFromNpc(*helper, *p, damage, d.remote, firstStatus, hit.remoteID);
			else if(firstStatus != 0) ApplyStatusFromNpc(*helper, *p, firstStatus);
			for(i32 s = 1; s < (i32)d.statuses.size(); s++) if(!p->dead) ApplyStatusFromNpc(*helper, *p, d.statuses[s]);
		}
	}
	if(d.npc) {
		foreach(o, actorNpcList) {
			if(&*o == helper || o->hp <= 0 || o->removeAt != Time::ZERO || !o->monsterAi || (o->faction != Faction::RED && o->faction != Faction::BLUE)) continue;
			const bool ally = teamed && o->faction == helper->faction;
			if(ally != d.ally) continue;
			if(glm::length(vec2(o->pos) - vec2(helper->pos)) > d.radius + o->monsterAi->radius) continue;
			if(damage > 0 && !d.ally) DamageNpcFromNpc(*helper, *o, damage, d.remote, hit.remoteID, firstStatus);
			else if(firstStatus != 0) ApplyStatusToNpcFromNpc(*helper, *o, firstStatus);
			for(i32 s = 1; s < (i32)d.statuses.size(); s++) if(o->hp > 0) ApplyStatusToNpcFromNpc(*helper, *o, d.statuses[s]);
		}
	}
	LOG("Monster :: creature=%d death remote %d (%s, radius %.0f, damage %d)", (i32)helper->docID, d.remote, d.ally ? "allies" : "enemies", d.radius, damage);
}

void World::MonsterDie(ActorNpc& npc, ActorUID killerUID, CreatureIndex killerDoc)
{
	if(npc.removeAt != Time::ZERO) return;
	npc.hp = 0;
	npc.killerUID = killerUID;
	npc.actionState = ActionStateID::DIE_BEHAVIORSTATE;
	replication->SendAiActionStateToAll(npc.UID, ActionStateID::DIE_BEHAVIORSTATE, npc.rot.z, npc.pos, false);
	replication->SendNpcDeadAckToAll(npc.UID, killerUID, killerUID != ActorUID::INVALID ? (u32)killerDoc : NPC_DEAD_NO_KILLER_DOC);
	if(npc.monsterTarget != ActorUID::INVALID) replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
	npc.monsterTarget = ActorUID::INVALID;
	const f32 destroy = npc.monsterAi ? npc.monsterAi->destroy : 0.0f;
	npc.removeAt = TimeAdd(localTime, TimeMsToTime(MAX(destroy, 1.0f) * 1000.0));
	LOG("Monster :: creature=%d (local %d) dies at (%.0f, %.0f) t=%.1f killer=%d", (i32)npc.docID, npc.localID, npc.pos.x, npc.pos.y, GameTimeSec(), (i32)killerDoc);
	if(ActorMaster* killer = killerUID != ActorUID::INVALID ? FindMasterActor(killerUID) : nullptr) {
		if(killer->parent) killer->parent->pveKills++;
	}
	DefenceMonsterDied(npc);
	if(stageClearMonster != 0 && (i32)npc.docID == stageClearMonster && ++stageClearKills >= MAX(stageClearCount, 1)) {
		stageCleared = true;
		LOG("Stage :: clear monster %d killed", stageClearMonster);
	}
	if(npc.monsterAi && npc.monsterAi->teamBuff != 0) {
		foreach(p, players) if((Faction)p->team == npc.faction) ConsumeStatus(p->index, npc.monsterAi->teamBuff);
	}
	if(const auto* deaths = GetGameXmlContent().FindDeathRemotes((i32)npc.docID)) {
		f32 longest = 0;
		const u32 remoteID = MONSTER_REMOTE_ID_BASE | (nextMonsterRemoteID++ & MONSTER_REMOTE_ID_MASK);
		for(const auto& d : *deaths) {
			if(deathHits.full()) break;
			deathHits.push_back({ npc.UID, TimeAdd(localTime, TimeMsToTime(d.delay * 1000.0)), &d, remoteID });
			longest = MAX(longest, d.delay);
		}
		const Time keep = TimeAdd(localTime, TimeMsToTime((longest + DEATH_HIT_KEEP_SEC) * 1000.0));
		if(npc.removeAt < keep) npc.removeAt = keep;
	}
	GrantMonsterKillExp(npc, killerUID);
	GrantMonsterKillPoints(npc, killerUID);
	OnMonsterDeath(npc, killerUID);
	if(const auto* spawns = GetGameXmlContent().FindMonsterDieSpawn((i32)npc.docID)) {
		const vec3 pos = npc.pos;
		const vec3 rot = npc.rot;
		const Faction faction = npc.faction;
		const bool titan = npc.monsterAi && MonsterTargetKind(*npc.monsterAi) == GameXmlContent::MonsterAttack::TITAN;
		eastl::fixed_vector<ActorUID,4,false> helpers;
		const Faction helperFaction = titan && (faction == Faction::RED || faction == Faction::BLUE) ? (faction == Faction::RED ? Faction::BLUE : Faction::RED) : Faction::MONSTER;
		for(const i32 s : *spawns) {
			ActorNpc& helper = SpawnMonster((CreatureIndex)s, 0, pos, rot, helperFaction, -1);
			if(!helpers.full()) helpers.push_back(helper.UID);
		}
		if(titan && !helpers.empty()) StartTitanVictory(faction, pos, helpers.front(), helpers.back());
	}
}

static const f32 TITAN_VICTORY_RANGE = 1600.0f;
static const f64 TITAN_VICTORY_START_MS = 500.0;
static const i32 STATUS_TITAN_VICTORY_FLAG = 1200100045;
static const i32 STATUS_TITAN_TARGET_FLAG = 1200100047;
static const i32 STATUS_TITAN_READY_FLAG = 1200100046;

void World::StartTitanVictory(Faction deadFaction, const vec3& deadPos, ActorUID soulUID, ActorUID triggerUID)
{
	ActorNpc* victor = nullptr;
	f32 best = TITAN_VICTORY_RANGE;
	foreach(o, actorNpcList) {
		if(o->hp <= 0 || o->removeAt != Time::ZERO || !o->monsterAi || o->monsterAi->victoryStates[0] == ActionStateID::INVALID) continue;
		if(!FactionsOppose(o->faction, deadFaction, hostileMonsters) || MonsterTargetKind(*o->monsterAi) != GameXmlContent::MonsterAttack::TITAN) continue;
		const f32 d = glm::length(vec2(o->pos) - vec2(deadPos));
		if(d < best) { best = d; victor = &*o; }
	}
	if(!victor || victor->victoryStep != 0) return;
	if(const ActorNpc* trigger = FindNpcActor(triggerUID)) ApplyStatusToNpcFromNpc(*trigger, *victor, STATUS_TITAN_VICTORY_FLAG);
	if(const ActorNpc* soul = FindNpcActor(soulUID)) ApplyStatusToNpcFromNpc(*soul, *victor, STATUS_TITAN_TARGET_FLAG);
	victor->victoryPendingAt = TimeAdd(localTime, TimeMsToTime(TITAN_VICTORY_START_MS));
	LOG("Titan :: creature=%d (local %d) absorbs a dead titan", (i32)victor->docID, victor->localID);
}

void World::UpdateNpcVictory(ActorNpc& npc)
{
	const GameXmlContent::MonsterAi& ai = *npc.monsterAi;
	if(npc.victoryStep == 0) {
		if(npc.victoryPendingAt == Time::ZERO || localTime < npc.victoryPendingAt || localTime < npc.monsterHoldUntil) return;
		npc.victoryPendingAt = Time::ZERO;
		npc.victoryStep = 1;
		npc.stateEndAt = Time::ZERO;
		npc.actionState = ai.victoryStates[0];
		replication->SendMonsterStateToAll(npc.UID, ai.victoryStates[0], npc.pos, 0.0f);
		npc.victoryNext = TimeAdd(localTime, TimeMsToTime(ai.victoryTimes[0] * 1000.0));
		npc.monsterHoldUntil = TimeAdd(npc.victoryNext, TimeMsToTime(ai.victoryTimes[1] * 1000.0));
		return;
	}
	if(npc.victoryStep == 1 && localTime >= npc.victoryNext) {
		npc.victoryStep = 2;
		npc.actionState = ai.victoryStates[1];
		replication->SendMonsterStateToAll(npc.UID, ai.victoryStates[1], npc.pos, 0.0f);
		ApplyMonsterStatus(npc, STATUS_TITAN_READY_FLAG, true);
		npc.victoryNext = TimeAdd(localTime, TimeMsToTime(ai.victoryStatusDelay * 1000.0));
		return;
	}
	if(npc.victoryStep == 2 && localTime >= npc.victoryNext) {
		npc.victoryStep = 3;
		f32 healDelay = 0;
		foreach_const(st, ai.victoryStatuses) {
			ApplyMonsterStatus(npc, *st, true);
			const StatusInfo* info = GetGameXmlContent().FindStatus(*st);
			if(info && info->type == StatusType::HEAL) healDelay = MAX(info->intervalSec, info->durationSec);
		}
		if(ai.victoryHeal > 0) npc.victoryHealAt = TimeAdd(localTime, TimeMsToTime(healDelay * 1000.0));
		npc.victoryNext = TimeAdd(localTime, TimeMsToTime(MAX(ai.victoryTimes[1] - ai.victoryStatusDelay, 0.0f) * 1000.0));
		return;
	}
	if(npc.victoryStep == 3) {
		if(npc.victoryHealAt != Time::ZERO && localTime >= npc.victoryHealAt) {
			npc.victoryHealAt = Time::ZERO;
			npc.hp = MIN(npc.hp + (i32)(npc.hpMax * ai.victoryHeal), npc.hpMax);
			replication->SendHpToAll(npc.UID, npc.hp, npc.hpMax);
			LOG("Titan :: creature=%d (local %d) healed to %d/%d by absorbing", (i32)npc.docID, npc.localID, npc.hp, npc.hpMax);
		}
		if(localTime >= npc.victoryNext && npc.victoryHealAt == Time::ZERO) {
			npc.victoryStep = 0;
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, 0.0f);
		}
	}
}


void World::SendNpcHit(const ActorNpc& attacker, ActorUID targetUID, const vec3& targetPos, i32 remote, u32 remoteID, i32 damage, i32 hp, i32 hpMax)
{
	if(remote <= 0 || remoteID == 0) {
		replication->SendDamageToAll(attacker.UID, targetUID, SkillID::INVALID, damage, hp, hpMax);
		return;
	}
	const Remote* info = GetGameXmlContent().FindRemote((RemoteIdx)remote);
	const vec2 rel = vec2(targetPos) - vec2(attacker.pos);
	const f32 len = glm::length(rel);
	Replication::RemoteDamage hit;
	hit.remoteID = remoteID;
	hit.remoteDoc = (RemoteIdx)remote;
	hit.attackerUID = attacker.UID;
	hit.targetUID = targetUID;
	hit.dir = len > 1.0f ? rel / len : vec2(cosf(attacker.rot.z), sinf(attacker.rot.z));
	hit.remotePos = info && info->maxSpeed > 0 ? targetPos : attacker.pos;
	hit.targetPos = targetPos;
	hit.damage = damage;
	hit.hp = hp;
	hit.hpMax = hpMax;
	replication->SendRemoteDamageToAll(hit);
}

void World::ApplyMonsterAllyStatus(ActorUID casterUID, const GameXmlContent::MonsterAttack& attack)
{
	const ActorNpc* caster = FindNpcActor(casterUID);
	if(!caster || caster->hp <= 0) return;
	const vec2 center = vec2(caster->pos);
	foreach(o, actorNpcList) {
		if(&*o == caster || o->hp <= 0 || o->removeAt != Time::ZERO || !o->monsterAi || o->faction != caster->faction) continue;
		if(glm::length(vec2(o->pos) - center) > attack.allyRadius) continue;
		ApplyStatusToNpcFromNpc(*caster, *o, attack.allyStatus);
	}
	foreach(p, players) {
		if(p->dead || !p->body || (Faction)p->team != caster->faction) continue;
		if(glm::length(vec2(p->body->GetWorldPos()) - center) > attack.allyRadius) continue;
		ApplyStatusFromNpc(*caster, *p, attack.allyStatus);
	}
}

void World::MonsterAttackHit(ActorUID attackerUID, ActorUID targetUID, const GameXmlContent::MonsterAttack& attack, u32 remoteID)
{
	auto attackerIt = actorNpcMap.find(attackerUID);
	if(attackerIt == actorNpcMap.end()) return;
	ActorNpc& attacker = *attackerIt->second;
	const i32 damage = (i32)(MonsterAttackPower(attacker) * attack.multiplier + 0.5f);
	if(damage <= 0) return;
	auto targetIt = actorNpcMap.find(targetUID);
	ActorMaster* master = targetIt == actorNpcMap.end() ? FindMasterActor(targetUID) : nullptr;
	if(attack.radius > 0) {
		vec3 center = attacker.pos;
		if(targetIt != actorNpcMap.end()) center = targetIt->second->pos;
		else if(master && master->parent) center = master->parent->body->GetWorldPos();
		foreach(p, players) {
			if(p->dead || !p->body || !MonsterHostile(attacker, *p) || glm::length(vec2(p->body->GetWorldPos()) - vec2(center)) > attack.radius) continue;
			DamagePlayerFromNpc(attacker, *p, damage, attack.hitRemote, attack.status, remoteID);
		}
		foreach(o, actorNpcList) {
			if(&*o == &attacker || o->hp <= 0 || !MonsterHostile(attacker, *o) || glm::length(vec2(o->pos) - vec2(center)) > attack.radius) continue;
			const bool execute = attack.executeMinion && MonsterTargetKind(*o->monsterAi) == GameXmlContent::MonsterAttack::MINION;
			DamageNpcFromNpc(attacker, *o, execute ? o->hpMax : damage, attack.hitRemote, remoteID, attack.status, execute);
		}
		return;
	}
	if(targetIt != actorNpcMap.end()) {
		ActorNpc& npcTarget = *targetIt->second;
		const bool execute = attack.executeMinion && npcTarget.monsterAi && MonsterTargetKind(*npcTarget.monsterAi) == GameXmlContent::MonsterAttack::MINION;
		DamageNpcFromNpc(attacker, npcTarget, execute ? npcTarget.hpMax : damage, attack.hitRemote, remoteID, attack.status, execute);
		return;
	}
	if(master && master->parent) DamagePlayerFromNpc(attacker, *master->parent, damage, attack.hitRemote, attack.status, remoteID);
}

void World::UpdateMonsterHits()
{
	for(i32 i = 0; i < (i32)deathHits.size(); ) {
		if(localTime < deathHits[i].at) { i++; continue; }
		const DeathHit hit = deathHits[i];
		deathHits.erase(deathHits.begin() + i);
		ApplyDeathHit(hit);
	}
	for(i32 i = 0; i < (i32)monsterHits.size(); ) {
		if(localTime < monsterHits[i].at) { i++; continue; }
		const MonsterHit hit = monsterHits[i];
		monsterHits.erase(monsterHits.begin() + i);
		if(hit.ally) ApplyMonsterAllyStatus(hit.attacker, *hit.attack);
		else MonsterAttackHit(hit.attacker, hit.target, *hit.attack, hit.remoteID);
		if(!hit.ally && hit.attack->selfDie) {
			auto self = actorNpcMap.find(hit.attacker);
			if(self != actorNpcMap.end() && self->second->hp > 0) MonsterDie(*self->second, ActorUID::INVALID, CreatureIndex(0));
		}
	}
}

void World::UpdateMonsterAi(f32 delta)
{
	UpdateMonsterHits();
	if(frozen) return;
	foreach(it, actorNpcList) {
		ActorNpc& npc = *it;
		if(npc.selfDestructAt != Time::ZERO && localTime >= npc.selfDestructAt && npc.hp > 0 && npc.removeAt == Time::ZERO) {
			MonsterDie(npc, ActorUID::INVALID, CreatureIndex(0));
			continue;
		}
		if(!npc.monsterAi || npc.hp <= 0 || npc.removeAt != Time::ZERO) continue;
		if(npc.despawnAt != Time::ZERO && localTime >= npc.despawnAt) continue;
		const GameXmlContent::MonsterAi& ai = *npc.monsterAi;
		const f32 walk = ai.walk * npc.speedMul;
		const f32 run = ai.run * npc.speedMul;
		UpdateNpcDefeat(npc);
		UpdateNpcVictory(npc);
		if(!npc.monsterStarted) {
			if(localTime < TimeAdd(npc.spawnedAt, TimeMsToTime(MAX(MONSTER_START_DELAY_MS, (f64)ai.spawnTime * 1000.0)))) continue;
			npc.monsterStarted = true;
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			if(ai.idle != ActionStateID::INVALID) {
				npc.actionState = ai.idle;
				replication->SendMonsterStateToAll(npc.UID, ai.idle, npc.pos, 0.0f);
			}
			if(npc.path && walk > 0) {
				npc.monsterWalking = true;
				replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, walk);
			}
		}

		const vec3* targetPos = nullptr;
		const GameXmlContent::MonsterAttack* attack = nullptr;
		vec3 targetAt;
		bool targetLost = false;
		if(npc.monsterTarget != ActorUID::INVALID) {
			auto t = actorNpcMap.find(npc.monsterTarget);
			if(t != actorNpcMap.end() && MonsterHostile(npc, *t->second)) {
				targetAt = t->second->pos;
				targetPos = &targetAt;
				attack = ai.byTarget[MonsterTargetKind(*t->second->monsterAi)];
			}
			else if(ActorMaster* m = FindMasterActor(npc.monsterTarget)) {
				if(m->parent && MonsterHostile(npc, *m->parent)) {
					targetAt = m->parent->body->GetWorldPos();
					targetPos = &targetAt;
					attack = ai.byTarget[GameXmlContent::MonsterAttack::MASTER];
				}
			}
			if(targetPos && glm::length(vec2(*targetPos) - vec2(npc.pos)) > MAX(ai.lost, ai.perception)) targetPos = nullptr;
			if(!targetPos || !attack) {
				targetPos = nullptr;
				npc.monsterTarget = ActorUID::INVALID;
				npc.monsterNextSearch = localTime;
				targetLost = true;
			}
		}
		const bool neutral = MonsterNeutral(npc, hostileMonsters);
		if(neutral && targetLost) {
			targetLost = false;
			replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
		}
		if(neutral && targetPos && glm::length(vec2(npc.pos) - vec2(npc.home)) > MAX(ai.lost, ai.perception)) {
			targetPos = nullptr;
			npc.monsterTarget = ActorUID::INVALID;
			npc.monsterReturning = true;
			replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
		}
		if(neutral && !targetPos) {
			const vec2 rel = vec2(npc.home) - vec2(npc.pos);
			const f32 dist = glm::length(rel);
			if(dist > MONSTER_WAYPOINT_REACHED && run > 0) {
				const f32 yaw = atan2f(rel.y, rel.x);
				npc.pos = vec3(vec2(npc.pos) + rel / dist * MIN(run * delta, dist), npc.pos.z);
				npc.rot.z = yaw;
				if(!npc.monsterReturning || localTime >= npc.monsterNextSync) {
					npc.monsterReturning = true;
					npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
					replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.home, yaw, run);
				}
			}
			else if(npc.monsterReturning) {
				npc.monsterReturning = false;
				npc.monsterChasing = false;
				npc.hp = npc.hpMax;
				if(ai.idle != ActionStateID::INVALID) {
					npc.monsterAwake = false;
					npc.actionState = ai.idle;
					replication->SendMonsterStateToAll(npc.UID, ai.idle, npc.pos, 0.0f);
				}
				replication->SendHpToAll(npc.UID, npc.hp, npc.hpMax);
				LOG("Monster :: neutral creature=%d (local %d) back home, healed", (i32)npc.docID, npc.localID);
				replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.pos, npc.rot.z, 0.0f);
			}
			continue;
		}
		if(!targetPos && ai.perception > 0 && localTime >= npc.monsterNextSearch) {
			npc.monsterNextSearch = TimeAdd(localTime, TimeMsToTime(MONSTER_SEARCH_MS));
			f32 best = ai.perception;
			ActorUID chosen = ActorUID::INVALID;
			if(ai.byTarget[GameXmlContent::MonsterAttack::MASTER]) {
				foreach(p, players) {
					if(!MonsterHostile(npc, *p)) continue;
					const f32 d = glm::length(vec2(p->body->GetWorldPos()) - vec2(npc.pos));
					if(d < best) { best = d; chosen = p->Main().UID; targetAt = p->body->GetWorldPos(); attack = ai.byTarget[GameXmlContent::MonsterAttack::MASTER]; }
				}
			}
			foreach_const(o, actorNpcList) {
				if(!MonsterHostile(npc, *o)) continue;
				const GameXmlContent::MonsterAttack* a = ai.byTarget[MonsterTargetKind(*o->monsterAi)];
				if(!a) continue;
				const f32 d = glm::length(vec2(o->pos) - vec2(npc.pos)) - o->monsterAi->radius;
				if(d < best) { best = d; chosen = o->UID; targetAt = o->pos; attack = a; }
			}
			if(chosen != ActorUID::INVALID) {
				LOG("Monster :: creature=%d (local %d) at (%.0f, %.0f) targets %u at %.0f", (i32)npc.docID, npc.localID, npc.pos.x, npc.pos.y, (u32)chosen, best);
				npc.monsterTarget = chosen;
				targetPos = &targetAt;
				targetLost = false;
				replication->SendAiSetTargetToAll(npc.UID, chosen);
			}
		}
		if(targetLost) replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);

		if(targetPos && attack && ai.awake != ActionStateID::INVALID && !npc.monsterAwake) {
			npc.monsterAwake = true;
			npc.actionState = ai.awake;
			replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, 0.0f);
			replication->SendMonsterStateToAll(npc.UID, ai.awake, npc.pos, 0.0f);
			replication->SendAiSetTargetToAll(npc.UID, npc.monsterTarget);
			npc.monsterHoldUntil = TimeAdd(localTime, TimeMsToTime(ai.awakeTime * 1000.0));
			LOG("Monster :: creature=%d (local %d) wakes up", (i32)npc.docID, npc.localID);
			continue;
		}
		if(npc.monsterWeakUntil != Time::ZERO && localTime >= npc.monsterWeakUntil) {
			npc.monsterWeakUntil = Time::ZERO;
			if(npc.monsterWeakHit && ai.groggyChance > 0 && (f32)(RandUint() % 10000) < ai.groggyChance * 10000.0f) {
				LOG("Monster :: creature=%d (local %d) is groggy", (i32)npc.docID, npc.localID);
				npc.actionState = ActionStateID::DEFEAT_GROGGY_BEHAVIORSTATE;
				replication->SendMonsterStateToAll(npc.UID, ActionStateID::DEFEAT_GROGGY_BEHAVIORSTATE, npc.pos, 0.0f);
				foreach_const(st, ai.groggyStatuses) ApplyMonsterStatus(npc, *st, true);
				npc.monsterHoldUntil = TimeAdd(localTime, TimeMsToTime(ai.groggyTime * 1000.0));
			}
		}
		if(localTime < npc.monsterHoldUntil) continue;
		if(targetPos && attack) {
			const vec2 rel = vec2(*targetPos) - vec2(npc.pos);
			const f32 dist = glm::length(rel);
			const f32 yaw = dist > 1.0f ? atan2f(rel.y, rel.x) : npc.rot.z;
			const f32 reach = attack->range;
			if(dist > reach + MONSTER_REACH_TOLERANCE) {
				if(run <= 0) continue;
				npc.monsterWalking = false;
				const f32 step = MIN(run * delta, dist - reach);
				npc.pos = vec3(vec2(npc.pos) + rel / dist * step, npc.pos.z);
				npc.rot.z = yaw;
				if(!npc.monsterChasing || localTime >= npc.monsterNextSync) {
					npc.monsterChasing = true;
					npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
					replication->SendMonsterMoveToAll(npc.UID, npc.pos, vec3(vec2(*targetPos) - rel / dist * reach, npc.pos.z), yaw, run);
				}
				continue;
			}
			if(npc.monsterChasing) {
				npc.monsterChasing = false;
				replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.pos, yaw, 0.0f);
			}
			npc.monsterWalking = false;
			if(localTime < npc.monsterNextAttack) continue;
			npc.monsterNextAttack = TimeAdd(localTime, TimeMsToTime(attack->cooldown * 1000.0));
			eastl::fixed_vector<i32,16,false> ready;
			f32 totalShare = attack->share;
			const f32 hpRatio = npc.hpMax > 0 ? (f32)npc.hp / (f32)npc.hpMax : 1.0f;
			for(i32 s = 0; s < (i32)ai.attacks.size() && s < (i32)npc.monsterSkillReady.size(); s++) {
				const GameXmlContent::MonsterAttack& special = ai.attacks[s];
				if(!special.special || special.target != attack->target || localTime < npc.monsterSkillReady[s]) continue;
				if(hpRatio < special.hpMin || hpRatio > special.hpMax) continue;
				if(npc.monsterSkillReady[s] == Time::ZERO) { npc.monsterSkillReady[s] = TimeAdd(localTime, TimeMsToTime(special.cooldown * 1000.0)); continue; }
				if(dist > special.range + (reach - attack->range)) continue;
				ready.push_back(s);
				totalShare += special.share;
			}
			f32 roll = (f32)(RandUint() % 10000) / 10000.0f * totalShare - attack->share;
			foreach_const(s, ready) {
				const GameXmlContent::MonsterAttack& special = ai.attacks[*s];
				if(roll >= special.share) { roll -= special.share; continue; }
				if(roll < 0) break;
				npc.monsterSkillReady[*s] = TimeAdd(localTime, TimeMsToTime(special.cooldown * 1000.0));
				attack = &special;
				LOG("Monster :: creature=%d (local %d) uses %s", (i32)npc.docID, npc.localID, ActionStateToString(special.state));
				break;
			}
			npc.rot.z = yaw;
			npc.actionState = attack->state;
			if(ai.masterMark != 0 && attack->target == GameXmlContent::MonsterAttack::MASTER) {
				ActorMaster* marked = FindMasterActor(npc.monsterTarget);
				if(marked && marked->parent) ApplyStatusFromNpc(npc, *marked->parent, ai.masterMark);
			}
			replication->SendNpcTurnToAll(npc.UID, yaw);
			replication->SendMonsterStateToAll(npc.UID, attack->state, npc.pos, run);
			if(attack->anim > 0) {
				npc.stateEndAt = TimeAdd(localTime, TimeMsToTime(attack->anim * 1000.0));
				if(npc.monsterHoldUntil < npc.stateEndAt) npc.monsterHoldUntil = npc.stateEndAt;
			}
			foreach_const(st, attack->selfStatuses) ApplyMonsterStatus(npc, *st, true);
			if(attack->weakTime > 0) {
				npc.monsterWeakUntil = TimeAdd(localTime, TimeMsToTime(attack->weakTime * 1000.0));
				npc.monsterWeakHit = false;
			}
			const f32 travel = attack->speed > 0 ? dist / attack->speed : 0.0f;
			const u32 remoteID = MONSTER_REMOTE_ID_BASE | (nextMonsterRemoteID++ & MONSTER_REMOTE_ID_MASK);
			for(i32 h = 0; !attack->noDamage && h < (i32)attack->hits && !monsterHits.full(); h++) {
				monsterHits.push_back({ npc.UID, npc.monsterTarget, TimeAdd(localTime, TimeMsToTime((attack->delay + travel + h * attack->interval) * 1000.0)), attack, remoteID, false });
			}
			if(attack->allyStatus != 0 && !monsterHits.full()) monsterHits.push_back({ npc.UID, npc.UID, TimeAdd(localTime, TimeMsToTime(attack->allyDelay * 1000.0)), attack, 0, true });
			continue;
		}

		if(!npc.path || walk <= 0 || npc.pathPoint >= (i32)npc.path->size()) continue;
		if(!npc.monsterWalking) {
			npc.monsterWalking = true;
			npc.monsterChasing = false;
			f32 best = FLT_MAX;
			for(i32 i = npc.pathPoint; i < (i32)npc.path->size(); i++) {
				const f32 d = glm::length(vec2((*npc.path)[i]) - vec2(npc.pos));
				if(d < best) { best = d; npc.pathPoint = i; }
			}
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, walk);
		}
		const i32 pointBefore = npc.pathPoint;
		f32 budget = walk * delta;
		while(budget > 0 && npc.pathPoint < (i32)npc.path->size()) {
			const vec3& goal = (*npc.path)[npc.pathPoint];
			const vec2 rel = vec2(goal) - vec2(npc.pos);
			const f32 dist = glm::length(rel);
			if(dist <= MONSTER_WAYPOINT_REACHED) { npc.pathPoint++; continue; }
			const f32 step = MIN(budget, dist);
			npc.pos = vec3(vec2(npc.pos) + rel / dist * step, goal.z);
			npc.rot.z = atan2f(rel.y, rel.x);
			budget -= step;
		}
		if(npc.pathPoint < (i32)npc.path->size() && (npc.pathPoint != pointBefore || localTime >= npc.monsterNextSync)) {
			npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
			replication->SendMonsterMoveToAll(npc.UID, npc.pos, (*npc.path)[npc.pathPoint], npc.rot.z, walk);
		}
	}
}
