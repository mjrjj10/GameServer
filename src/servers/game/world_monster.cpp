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

static f32 MonsterAttackPower(CreatureIndex docID)
{
	const auto* stats = GetGameXmlContent().FindMonsterStats((i32)docID);
	if(!stats) return 0;
	const auto& list = *stats;
	foreach_const(st, list) if(st->type == MONSTER_ATTACK_STAT) return st->value;
	return 0;
}

static bool FactionsOppose(Faction a, Faction b)
{
	return (a == Faction::RED && b == Faction::BLUE) || (a == Faction::BLUE && b == Faction::RED);
}

static GameXmlContent::MonsterAttack::Target MonsterTargetKind(const GameXmlContent::MonsterAi& ai)
{
	static const u32 minionTag = StatusTypeKey("Minion");
	return ai.tagKey == minionTag ? GameXmlContent::MonsterAttack::MINION : GameXmlContent::MonsterAttack::STRUCTURE;
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
	LOG("Monster :: creature=%d (local %d) dies at (%.0f, %.0f)", (i32)npc.docID, npc.localID, npc.pos.x, npc.pos.y);
}

void World::MonsterAttackHit(ActorUID attackerUID, ActorUID targetUID, const GameXmlContent::MonsterAttack& attack)
{
	auto attackerIt = actorNpcMap.find(attackerUID);
	if(attackerIt == actorNpcMap.end()) return;
	ActorNpc& attacker = *attackerIt->second;
	const i32 damage = (i32)(MonsterAttackPower(attacker.docID) * attack.multiplier + 0.5f);
	if(damage <= 0) return;
	auto targetIt = actorNpcMap.find(targetUID);
	if(targetIt != actorNpcMap.end()) {
		DamageNpcFromNpc(attacker, *targetIt->second, damage, attack.remote);
		return;
	}
	ActorMaster* master = FindMasterActor(targetUID);
	if(master && master->parent) DamagePlayerFromNpc(attacker, *master->parent, damage, attack.remote, attack.status);
}

void World::UpdateMonsterHits()
{
	for(i32 i = 0; i < (i32)monsterHits.size(); ) {
		if(localTime < monsterHits[i].at) { i++; continue; }
		const MonsterHit hit = monsterHits[i];
		monsterHits.erase(monsterHits.begin() + i);
		MonsterAttackHit(hit.attacker, hit.target, *hit.attack);
	}
}

void World::UpdateMonsterAi(f32 delta)
{
	UpdateMonsterHits();
	if(frozen) return;
	foreach(it, actorNpcList) {
		ActorNpc& npc = *it;
		if(!npc.monsterAi || npc.hp <= 0 || npc.removeAt != Time::ZERO) continue;
		if(npc.despawnAt != Time::ZERO && localTime >= npc.despawnAt) continue;
		const GameXmlContent::MonsterAi& ai = *npc.monsterAi;
		if(!npc.monsterStarted) {
			if(localTime < TimeAdd(npc.spawnedAt, TimeMsToTime(MONSTER_START_DELAY_MS))) continue;
			npc.monsterStarted = true;
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			if(npc.path && ai.walk > 0) {
				npc.monsterWalking = true;
				replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, ai.walk);
			}
		}

		const vec3* targetPos = nullptr;
		const GameXmlContent::MonsterAttack* attack = nullptr;
		vec3 targetAt;
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
				replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
			}
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
				replication->SendAiSetTargetToAll(npc.UID, chosen);
			}
		}

		if(targetPos && attack) {
			const vec2 rel = vec2(*targetPos) - vec2(npc.pos);
			const f32 dist = glm::length(rel);
			const f32 yaw = dist > 1.0f ? atan2f(rel.y, rel.x) : npc.rot.z;
			auto targetNpc = actorNpcMap.find(npc.monsterTarget);
			const f32 reach = attack->range + (targetNpc != actorNpcMap.end() && targetNpc->second->monsterAi ? targetNpc->second->monsterAi->radius : 0.0f);
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
			npc.rot.z = yaw;
			npc.actionState = attack->state;
			replication->SendNpcTurnToAll(npc.UID, yaw);
			replication->SendMonsterStateToAll(npc.UID, attack->state, npc.pos, ai.run);
			const f32 travel = attack->speed > 0 ? dist / attack->speed : 0.0f;
			monsterHits.push_back({ npc.UID, npc.monsterTarget, TimeAdd(localTime, TimeMsToTime((attack->delay + travel) * 1000.0)), attack });
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
