#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const f64 MONSTER_START_DELAY_MS = 90.0;
static const f64 MONSTER_SEARCH_MS = 250.0;
static const f64 MONSTER_SYNC_MS = 500.0;
static const f32 MONSTER_WAYPOINT_REACHED = 60.0f;
static const u8 MONSTER_ATTACK_STAT = 6;
static const u32 NPC_DEAD_NO_KILLER_DOC = 0;

static f32 MonsterAttackPower(const World::ActorNpc& npc)
{
	const GameXmlContent& content = GetGameXmlContent();
	const auto* stats = content.FindMonsterStats((i32)npc.docID);
	if(!stats) return 0;
	const auto& list = *stats;
	foreach_const(st, list) if(st->type == MONSTER_ATTACK_STAT) return st->value + content.MonsterGrowth((i32)npc.docID, MONSTER_ATTACK_STAT) * npc.growthLevel;
	return 0;
}

static bool FactionsOppose(Faction a, Faction b)
{
	return (a == Faction::RED && b == Faction::BLUE) || (a == Faction::BLUE && b == Faction::RED);
}

static bool MonsterNeutral(const World::ActorNpc& npc)
{
	return npc.faction == Faction::ORANGE || npc.faction == Faction::MONSTER;
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
	return FactionsOppose(monster.faction, other.faction) && monster.monsterAi->Hates(other.monsterAi->tagKey);
}

bool World::MonsterHostile(const ActorNpc& monster, const Player& player) const
{
	static const u32 playerTag = StatusTypeKey("Player");
	if(player.dead || player.untargetable || !player.body) return false;
	if(MonsterNeutral(monster)) return monster.monsterTarget == player.Main().UID;
	return FactionsOppose(monster.faction, (Faction)player.team) && monster.monsterAi->Hates(playerTag);
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
	GrantMonsterKillExp(npc, killerUID);
	GrantMonsterKillPoints(npc, killerUID);
	OnMonsterDeath(npc, killerUID);
	if(const auto* spawns = GetGameXmlContent().FindMonsterDieSpawn((i32)npc.docID)) {
		const vec3 pos = npc.pos;
		const vec3 rot = npc.rot;
		for(const i32 s : *spawns) SpawnMonster((CreatureIndex)s, 0, pos, rot, Faction::MONSTER, -1);
	}
}

static const u32 MONSTER_REMOTE_ID_BASE = 0xC0000000u;
static const u32 MONSTER_REMOTE_ID_MASK = 0x3FFFFFFFu;

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
	for(i32 i = 0; i < (i32)monsterHits.size(); ) {
		if(localTime < monsterHits[i].at) { i++; continue; }
		const MonsterHit hit = monsterHits[i];
		monsterHits.erase(monsterHits.begin() + i);
		MonsterAttackHit(hit.attacker, hit.target, *hit.attack, hit.remoteID);
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
		UpdateNpcDefeat(npc);
		if(!npc.monsterStarted) {
			if(localTime < TimeAdd(npc.spawnedAt, TimeMsToTime(MAX(MONSTER_START_DELAY_MS, (f64)ai.spawnTime * 1000.0)))) continue;
			npc.monsterStarted = true;
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			if(ai.idle != ActionStateID::INVALID) {
				npc.actionState = ai.idle;
				replication->SendMonsterStateToAll(npc.UID, ai.idle, npc.pos, 0.0f);
			}
			if(npc.path && ai.walk > 0) {
				npc.monsterWalking = true;
				replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, ai.walk);
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
		const bool neutral = MonsterNeutral(npc);
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
			if(dist > MONSTER_WAYPOINT_REACHED && ai.run > 0) {
				const f32 yaw = atan2f(rel.y, rel.x);
				npc.pos = vec3(vec2(npc.pos) + rel / dist * MIN(ai.run * delta, dist), npc.pos.z);
				npc.rot.z = yaw;
				if(!npc.monsterReturning || localTime >= npc.monsterNextSync) {
					npc.monsterReturning = true;
					npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
					replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.home, yaw, ai.run);
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
			if(dist > reach) {
				if(ai.run <= 0) continue;
				npc.monsterWalking = false;
				const f32 step = MIN(ai.run * delta, dist - reach);
				npc.pos = vec3(vec2(npc.pos) + rel / dist * step, npc.pos.z);
				npc.rot.z = yaw;
				if(!npc.monsterChasing || localTime >= npc.monsterNextSync) {
					npc.monsterChasing = true;
					npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
					replication->SendMonsterMoveToAll(npc.UID, npc.pos, vec3(vec2(*targetPos) - rel / dist * reach, npc.pos.z), yaw, ai.run);
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
			replication->SendNpcTurnToAll(npc.UID, yaw);
			replication->SendMonsterStateToAll(npc.UID, attack->state, npc.pos, ai.run);
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
				monsterHits.push_back({ npc.UID, npc.monsterTarget, TimeAdd(localTime, TimeMsToTime((attack->delay + travel + h * attack->interval) * 1000.0)), attack, remoteID });
			}
			continue;
		}

		if(!npc.path || ai.walk <= 0 || npc.pathPoint >= (i32)npc.path->size()) continue;
		if(!npc.monsterWalking) {
			npc.monsterWalking = true;
			npc.monsterChasing = false;
			f32 best = FLT_MAX;
			for(i32 i = npc.pathPoint; i < (i32)npc.path->size(); i++) {
				const f32 d = glm::length(vec2((*npc.path)[i]) - vec2(npc.pos));
				if(d < best) { best = d; npc.pathPoint = i; }
			}
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, ai.walk);
		}
		f32 budget = ai.walk * delta;
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
	}
}
