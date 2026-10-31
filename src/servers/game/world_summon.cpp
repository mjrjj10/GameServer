#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const i32 RELAY_MODE_STATE_OFFSET = 9;
static const i32 RELAY_IDS_PER_MODE = 3;
static const i32 RELAY_ARTILLERY_MODE = 2;
static const f32 RELAY_LEAP_SPEED = 2000.0f;
static const i32 RELAY_PRISM_MODE = 0;
static const f32 WALL_HEIGHT = 300.0f;
static const struct { i32 creature; f32 width; f32 depth; } WALL_FOOTPRINTS[] = { { 100033400, 300.0f, 80.0f }, { 100033401, 500.0f, 80.0f }, { 100033402, 500.0f, 80.0f } };
static const i32 RELAY_BRUISER_MODE = 1;
static const f32 RELAY_LIFE_SEC = 3600.0f;
static const f32 RELAY_PASSIVE_SEC = 3.2f;
static const f64 SUMMON_DIE_STAND_MS = 90.0;
static const i32 RELAY_STEALTH_STATUS = 1230210030;

void World::SpawnCreatureFromRemote(const RemoteInst& parent, i32 creature, const vec3& pos)
{
	if(pendingSummons.full() || parent.ownerIndex >= (u32)players.size()) return;
	PendingSummon ps;
	ps.time = localTime;
	ps.creature = creature;
	ps.ownerIndex = parent.ownerIndex;
	ps.pos = vec3(vec2(pos), players[parent.ownerIndex].body->GetWorldPos().z);
	ps.rot = vec3(0, 0, atan2f(parent.dir.y, parent.dir.x));
	ps.target = parent.target;
	pendingSummons.push_back(ps);
	LOG("Remote :: %d creates creature %d at (%.0f, %.0f)", (i32)parent.remote->ID, creature, pos.x, pos.y);
}

void World::RunSummonState(ActorNpc& npc, i32 state)
{
	const SummonInfo* info = GetGameXmlContent().FindSummon((i32)npc.docID);
	if(!info || state < 0 || state > 20) return;
	Player& owner = players[npc.ownerIndex];
	Player* current = &owner;
	if(npc.ai == SummonInfo::AI_CHASE) {
		ActorMaster* m = FindMasterActor(npc.chaseTarget);
		current = m ? m->parent : nullptr;
	}
	else if(npc.skillTarget != ActorUID::INVALID) {
		ActorMaster* m = FindMasterActor(npc.skillTarget);
		if(m && !m->parent->dead) current = m->parent;
	}
	f32 yaw = owner.input.rot.upperYaw;
	const ActorNpc* targetNpc = npc.skillTarget != ActorUID::INVALID && (!current || current == &owner) ? FindNpcActor(npc.skillTarget) : nullptr;
	if(current && current != &owner) {
		const vec2 rel = vec2(current->body->GetWorldPos()) - vec2(npc.pos);
		if(glm::length(rel) > 1.0f) yaw = atan2f(rel.y, rel.x);
	}
	else if(targetNpc) {
		const vec2 rel = vec2(targetNpc->pos) - vec2(npc.pos);
		if(glm::length(rel) > 1.0f) yaw = atan2f(rel.y, rel.x);
	}
	{
		eastl::fixed_string<char,48,false> name;
		name.sprintf("SKILL_%d_BEHAVIORSTATE", state);
		const ActionStateID action = ActionStateFromString(name.c_str());
		const vec3 targetPos = targetNpc ? targetNpc->pos : (current ? current->body->GetWorldPos() : npc.pos);
		if(npc.skillTarget != ActorUID::INVALID && current && current != &owner) {
			f32 speed = 0;
			foreach_const(c, info->skillStates[state]) {
				if(c->kind != SummonCommand::REMOTE) continue;
				const Remote* r = GetGameXmlContent().FindRemote(RemoteIdx(c->index));
				if(r && r->behaviorType == Remote::BehaviorType::TARGET && r->maxSpeed > 0) speed = MAX(speed, r->maxSpeed);
			}
			if(speed > 0) {
				const vec3 goal = current->body->GetWorldPos();
				replication->SendNpcMoveToAll(npc.UID, npc.pos, goal, yaw, speed);
				LOG("Summon :: creature=%d moves to its target at %.0f", (i32)npc.docID, speed);
				npc.pos = goal;
			}
		}
		if(action != ActionStateID::INVALID) {
			npc.actionState = action;
			npc.pendingMotion = action;
			npc.motionAt = TimeAdd(localTime, TimeMsToTime(50.0));
			npc.motionTarget = targetPos;
			npc.motionTargetUID = targetNpc ? targetNpc->UID : (current ? current->Main().UID : ActorUID::INVALID);
			npc.motionYaw = yaw;
		}
		LOG("Summon :: creature=%d plays %s", (i32)npc.docID, name.c_str());
	}
	f32 t = 0;
	foreach_const(c, info->skillStates[state]) {
		t += c->delay;
		if(c->kind == SummonCommand::REMOTE) {
			if(pendingSummonCommands.full()) continue;
			PendingSummonCommand pc;
			pc.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			pc.ownerIndex = npc.ownerIndex;
			pc.kind = c->kind;
			pc.index = c->index;
			pc.nodeName = c->nodeName;
			pc.pos = npc.pos;
			pc.yaw = yaw;
			pc.skill = info->skills.empty() ? 0 : info->skills[0];
			pc.until = npc.despawnAt;
			pc.target = current && current != &owner ? current->Main().UID : ActorUID::INVALID;
			pc.source = npc.UID;
			pc.aim = npc.aimPos;
			pendingSummonCommands.push_back(pc);
		}
		else if(c->kind == SummonCommand::STATUS) {
			if(c->onCurrent) {
				if(current && !current->dead) {
					statusCasterOverride = npc.UID;
					ApplyStatus(owner, *current, c->index);
					statusCasterOverride = ActorUID::INVALID;
				}
				continue;
			}
			const StatusInfo* st = GetGameXmlContent().FindStatus(c->index);
			if(!st) continue;
			const f32 duration = st->durationSec > 0 ? st->durationSec : 1.0f;
			replication->SendAddStatusToAll(c->index, npc.UID, npc.UID, StatusSentDuration(*st), npc.pos, npc.pos, 0, 1, 1.0f, -1.0f, false);
			TrackNpcSelfStatus(npc.UID, *st);
			if(st->type == StatusType::SUICIDE) {
				const Time at = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
				if(npc.despawnAt == Time::ZERO || at < npc.despawnAt) npc.despawnAt = at;
			}
			LOG("Summon :: creature=%d self status %d (%s)", (i32)npc.docID, c->index, st->typeName.c_str());
			if(st->type == StatusType::TOGGLE && st->auraStatus == 0 && st->targetingRange > 0) {
				foreach(p, players) {
					if(p->dead) continue;
					bool attached = false;
					foreach_const(as, activeStatuses) if(as->targetIndex == p->index && as->casterUID == npc.UID && as->info && as->info->attachToCaster && as->until > localTime) attached = true;
					if(!attached && glm::length(vec2(p->body->GetWorldPos()) - vec2(npc.pos)) > st->targetingRange) continue;
					statusCasterOverride = npc.UID;
					ApplyStatus(owner, *p, c->index);
					statusCasterOverride = ActorUID::INVALID;
				}
			}
			if(st->auraStatus != 0 && st->targetingRange > 0 && !groundAuras.full()) {
				GroundAura ga;
				ga.ownerIndex = npc.ownerIndex;
				ga.auraStatus = st->ID;
				ga.range = st->targetingRange;
				ga.pos = npc.pos;
				ga.until = TimeAdd(localTime, TimeMsToTime(duration * 1000.0));
				ga.nextPulse = localTime;
				ga.sourceUID = npc.UID;
				groundAuras.push_back(ga);
			}
		}
		else if(c->kind == SummonCommand::DIE) {
			npc.dieMode = c->dieMode;
		}
	}
}

void World::ChildSkillOnExisting(Player& owner, i32 child, const vec3& pos)
{
	ActorNpc* npc = nullptr;
	const SummonInfo* info = nullptr;
	foreach(it, actorNpcList) {
		if(it->ownerIndex != owner.index || (it->despawnAt != Time::ZERO && localTime >= it->despawnAt)) continue;
		const SummonInfo* si = GetGameXmlContent().FindSummon((i32)it->docID);
		if(si && child <= 20 && !si->skillStates[child].empty()) { npc = &*it; info = si; }
	}
	if(!npc) {
		LOG("Summon :: %ls child SKILL_%d has no creature to run it", owner.name.data(), child);
		return;
	}
	f32 travel = 0;
	foreach_const(c, info->skillStates[child]) if(c->kind == SummonCommand::REMOTE) { travel = c->delay; break; }
	const vec3 goal = vec3(vec2(pos), npc->pos.z);
	const f32 dist = glm::length(vec2(goal) - vec2(npc->pos));
	LOG("Summon :: %ls child SKILL_%d on its creature=%d, flies %.0f in %.2fs", owner.name.data(), child, (i32)npc->docID, dist, travel);
	if(dist > 1.0f) {
		replication->SendAiMoveTargetToAll(npc->UID, goal, (u32)ActionCommand::MovePreset::TARGET_POS);
		npc->rot.z = atan2f(goal.y - npc->pos.y, goal.x - npc->pos.x);
		replication->SendNpcTurnToAll(npc->UID, npc->rot.z);
	}
	foreach(st, activeStatuses) {
		if(st->casterIndex != owner.index || !st->info || st->info->type != StatusType::TARGET || localTime >= st->until) continue;
		static const f32 HYP_CARRY_ARRIVE_EARLY_SEC = 0.1f;
		Player& carried = players[st->targetIndex];
		const Time drop = TimeAdd(localTime, TimeMsToTime(travel * 1000.0));
		const f32 flight = MAX(travel - HYP_CARRY_ARRIVE_EARLY_SEC, 0.05f);
		carried.carryReleaseAt = drop;
		if(carried.clientHd == ClientHandle::INVALID && !carried.dead) {
			carried.pushTarget = vec3(vec2(pos), carried.body->GetWorldPos().z);
			carried.pushUntil = TimeAdd(localTime, TimeMsToTime(flight * 1000.0));
			carried.pushSpeed = glm::length(vec2(pos) - vec2(carried.body->GetWorldPos())) / flight;
			if(drop > carried.ccUntil) carried.ccUntil = drop;
		}
		LOG("Summon :: %ls carried to (%.0f, %.0f)", carried.name.data(), pos.x, pos.y);
	}
	npc->pos = goal;
	npc->aimPos = pos;
	RunSummonState(*npc, child);
}

static bool NpcKeepsSpeed(const World::ActorNpc& npc)
{
	const SummonInfo* info = GetGameXmlContent().FindSummon((i32)npc.docID);
	return info && info->moveSpeed > 0;
}

void World::RelayWarp(ActorNpc& npc, const vec3& to)
{
	LOG("Summon :: relay robot warps from (%.0f, %.0f) to (%.0f, %.0f)", npc.pos.x, npc.pos.y, to.x, to.y);
	const SummonInfo* info = GetGameXmlContent().FindSummon((i32)npc.docID);
	replication->SendNpcBehaviorToAll(npc.UID, ActionStateID::EXTRAMOTION_2_BEHAVIORSTATE, npc.pos, to, ActorUID::INVALID, info ? info->moveSpeed : 0.0f);
	npc.pos = to;
}

void World::RelayPassive(ActorNpc& npc, const SummonInfo& info, Player& owner)
{
	const i32 state = npc.relayMode * 2 + 1;
	if(state > 20 || info.skillStates[state].empty()) return;
	f32 reach = info.perception;
	bool needsEnemy = false;
	foreach_const(c, info.skillStates[state]) {
		if(c->kind != SummonCommand::REMOTE) continue;
		needsEnemy = true;
		const Remote* r = GetGameXmlContent().FindRemote(RemoteIdx(c->index));
		if(r && r->maxDistance > 0) reach = MIN(reach, r->maxDistance);
	}
	npc.skillTarget = ActorUID::INVALID;
	npc.aimPos = vec3(0);
	if(needsEnemy) {
		Player* target = nullptr;
		f32 best = reach;
		foreach(e, players) {
			if(e->dead || e->team == owner.team) continue;
			const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(npc.pos));
			if(d < best) { best = d; target = &*e; }
		}
		if(!target) return;
		npc.aimPos = target->body->GetWorldPos();
		if(npc.relayMode == RELAY_BRUISER_MODE) npc.skillTarget = target->Main().UID;
	}
	LOG("Summon :: relay robot of %ls passive SKILL_%d", owner.name.data(), state);
	RunSummonState(npc, state);
	npc.skillTarget = ActorUID::INVALID;
}

static const f32 NPC_LEG_RETARGET_DIST = 100.0f;
static const i32 NPC_LEG_MIN_INTERVAL_MS = 100;
static const i32 NPC_FOLLOW_LEG_MIN_INTERVAL_MS = 800;
static const u8 SUMMON_DEFENCE_STAT = 7;
static const u32 NPC_DEAD_NO_KILLER = 0xFFFFFFFF;
static const f32 NPC_WANDER_MIN_RADIUS = 100.0f;
static const f32 NPC_WANDER_MAX_RADIUS = 300.0f;
static const f32 NPC_WANDER_MIN_PAUSE_MS = 250.0f;
static const f32 NPC_WANDER_MAX_PAUSE_MS = 900.0f;

void World::SyncNpcLeg(ActorNpc& npc, const vec3& goal)
{
	const i32 minInterval = npc.ai == SummonInfo::AI_RELAY ? NPC_FOLLOW_LEG_MIN_INTERVAL_MS : NPC_LEG_MIN_INTERVAL_MS;
	const bool retarget = npc.legMoving && glm::length(vec2(goal) - vec2(npc.legGoal)) > NPC_LEG_RETARGET_DIST && localTime >= TimeAdd(npc.legAt, TimeMsToTime(minInterval));
	if(npc.legMoving && !retarget) return;
	npc.legMoving = true;
	npc.legGoal = goal;
	npc.legAt = localTime;
	replication->SendNpcLegStartToAll(npc.UID, npc.pos, goal);
	replication->SendNpcTurnToAll(npc.UID, atan2f(goal.y - npc.pos.y, goal.x - npc.pos.x));
}

void World::WanderNpc(ActorNpc& npc, f32 moveSpeed, f32 delta)
{
	if(!npc.legMoving) {
		if(localTime < npc.wanderResumeAt) return;
		const f32 angle = (f32)(RandUint() % 3600) * PI / 1800.0f;
		const f32 radius = NPC_WANDER_MIN_RADIUS + (f32)(RandUint() % 1000) / 1000.0f * (NPC_WANDER_MAX_RADIUS - NPC_WANDER_MIN_RADIUS);
		SyncNpcLeg(npc, vec3(vec2(npc.home) + vec2(cosf(angle), sinf(angle)) * radius, npc.pos.z));
		return;
	}
	const vec2 rel = vec2(npc.legGoal) - vec2(npc.pos);
	const f32 dist = glm::length(rel);
	const f32 step = moveSpeed * delta;
	if(dist <= step) {
		npc.pos = vec3(vec2(npc.legGoal), npc.pos.z);
		StopNpcLeg(npc);
		const f32 pause = NPC_WANDER_MIN_PAUSE_MS + (f32)(RandUint() % 1000) / 1000.0f * (NPC_WANDER_MAX_PAUSE_MS - NPC_WANDER_MIN_PAUSE_MS);
		npc.wanderResumeAt = TimeAdd(localTime, TimeMsToTime(pause));
		return;
	}
	npc.pos += vec3(rel / dist * step, 0);
	npc.rot.z = atan2f(rel.y, rel.x);
}

void World::StopNpcLeg(ActorNpc& npc)
{
	if(!npc.legMoving) return;
	npc.legMoving = false;
	replication->SendNpcLegStopToAll(npc.UID, npc.pos, npc.rot.z);
}

void World::UpdateSummonAttack(ActorNpc& npc, f32 delta)
{
	const SummonInfo* info = GetGameXmlContent().FindSummon((i32)npc.docID);
	if(!info || npc.spawnedAt >= localTime) return;
	const Player& owner = players[npc.ownerIndex];
	ActorMaster* tm = FindMasterActor(npc.chaseTarget);
	Player* target = tm && !tm->parent->dead && tm->parent->team != owner.team ? tm->parent : nullptr;
	const f32 lost = info->targetLost > 0 ? info->targetLost : info->perception;
	if(target && glm::length(vec2(target->body->GetWorldPos()) - vec2(npc.pos)) > lost) target = nullptr;
	if(!target) {
		f32 best = info->perception;
		foreach(e, players) {
			if(e->dead || e->team == owner.team) continue;
			const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(npc.pos));
			if(d < best) { best = d; target = &*e; }
		}
	}
	ActorNpc* targetNpc = nullptr;
	if(!target) {
		f32 best = info->perception;
		foreach(e, actorNpcList) {
			if(&*e == &npc || !NpcHittableBy(*e, owner)) continue;
			const f32 d = glm::length(vec2(e->pos) - vec2(npc.pos));
			if(d < best) { best = d; targetNpc = &*e; }
		}
	}
	const ActorUID chosen = target ? target->Main().UID : (targetNpc ? targetNpc->UID : ActorUID::INVALID);
	if(chosen != npc.chaseTarget) {
		replication->SendAiSetTargetToAll(npc.UID, chosen);
		npc.chaseTarget = chosen;
	}
	if(!target && !targetNpc) { StopNpcLeg(npc); return; }
	const vec3 goal = target ? target->body->GetWorldPos() : targetNpc->pos;
	const vec2 rel = vec2(goal) - vec2(npc.pos);
	const f32 dist = glm::length(rel);
	const f32 yaw = dist > 1.0f ? atan2f(rel.y, rel.x) : npc.rot.z;
	if(dist > npc.attackRule->attackRange) {
		if(info->moveSpeed <= 0) return;
		const f32 step = MIN(info->moveSpeed * delta, dist - npc.attackRule->attackRange);
		SyncNpcLeg(npc, vec3(vec2(goal) - rel / dist * npc.attackRule->attackRange, goal.z));
		npc.pos = vec3(vec2(npc.pos) + rel / dist * step, goal.z);
		npc.rot.z = yaw;
		return;
	}
	StopNpcLeg(npc);
	if(localTime < npc.nextAttack) return;
	npc.nextAttack = TimeAdd(localTime, TimeMsToTime(npc.attackRule->cooldownMs));
	npc.rot.z = yaw;
	npc.skillTarget = chosen;
	npc.aimPos = goal;
	npc.actionState = ActionStateID::ATTACK_BEHAVIORSTATE;
	replication->SendAiActionStateToAll(npc.UID, ActionStateID::ATTACK_BEHAVIORSTATE, yaw, npc.pos, info->moveSpeed > 0);
	RunSummonState(npc, 0);
	const f32 animSec = GetGameXmlContent().GetAniSeqTime(info->className.c_str(), info->stateAnim[0]);
	if(animSec > 0) npc.standAt = TimeAdd(localTime, TimeMsToTime(animSec * 1000.0));
	LOG("Summon :: creature=%d attacks %s %d at %.0f", (i32)npc.docID, target ? "master" : "creature", target ? (i32)target->index : (i32)targetNpc->docID, dist);
}

void World::UpdateSummonAi(f32 delta)
{
	const auto& content = GetGameXmlContent();
	foreach(it, actorNpcList) {
		ActorNpc& npc = *it;
		if(npc.pendingMotion != ActionStateID::INVALID && localTime >= npc.motionAt) {
			const SummonInfo* motionInfo = content.FindSummon((i32)npc.docID);
			replication->SendNpcBehaviorToAll(npc.UID, npc.pendingMotion, npc.pos, npc.motionTarget, npc.motionTargetUID, motionInfo ? motionInfo->moveSpeed : 0.0f);
			npc.pendingMotion = ActionStateID::INVALID;
		}
		if(npc.queuedState >= 0 && localTime >= npc.queuedStateAt && npc.pendingMotion == ActionStateID::INVALID) {
			if(glm::length(vec2(npc.queuedGoal)) > 1.0f) {
				replication->SendNpcMoveToAll(npc.UID, npc.pos, npc.queuedGoal, atan2f(npc.queuedGoal.y - npc.pos.y, npc.queuedGoal.x - npc.pos.x), RELAY_LEAP_SPEED);
				npc.pos = npc.queuedGoal;
			}
			const i32 state = npc.queuedState;
			npc.queuedState = -1;
			RunSummonState(npc, state);
		}
		if(npc.spawnedAt >= localTime) continue;
		if(npc.attackRule && npc.removeAt == Time::ZERO && (npc.despawnAt == Time::ZERO || localTime < npc.despawnAt)) {
			UpdateSummonAttack(npc, delta);
			continue;
		}
		if(npc.ai == SummonInfo::AI_NONE) continue;
		if(npc.despawnAt != Time::ZERO && localTime >= npc.despawnAt) continue;
		const SummonInfo* info = content.FindSummon((i32)npc.docID);
		if(!info) continue;
		Player& owner = players[npc.ownerIndex];
		vec3 goal = npc.pos;
		f32 stopDist = 0;

		if(npc.ai == SummonInfo::AI_RELAY) {
			if(owner.dead || (i32)owner.Main().classType != npc.ownerClass) {
				LOG("Summon :: relay robot of %ls leaves with its master", owner.name.data());
				npc.despawnAt = localTime;
				continue;
			}
			goal = owner.body->GetWorldPos();
			stopDist = info->aiDistance;
			if(info->perception > 0 && glm::length(vec2(goal) - vec2(npc.pos)) > info->perception) RelayWarp(npc, goal);
			if(npc.relayMode >= 0 && localTime >= npc.nextPassive && localTime >= npc.passiveBlockedUntil) {
				npc.nextPassive = TimeAdd(localTime, TimeMsToTime(RELAY_PASSIVE_SEC * 1000.0));
				RelayPassive(npc, *info, owner);
			}
		}
		else {
			ActorMaster* tm = FindMasterActor(npc.chaseTarget);
			Player* target = tm ? tm->parent : nullptr;
			if(npc.stuck) {
				if(!target || target->dead) {
					npc.stuck = false;
					if(npc.chaseTarget != ActorUID::INVALID) replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
					npc.chaseTarget = ActorUID::INVALID;
					continue;
				}
				npc.pos = target->body->GetWorldPos();
				continue;
			}
			if(!target || target->dead || glm::length(vec2(target->body->GetWorldPos()) - vec2(npc.pos)) > info->perception) {
				target = nullptr;
				f32 best = info->perception;
				foreach(e, players) {
					if(e->dead || e->team == owner.team) continue;
					bool taken = false;
					foreach_const(o, actorNpcList) {
						if(&*o != &npc && o->ownerIndex == npc.ownerIndex && o->docID == npc.docID && o->chaseTarget == e->Main().UID) taken = true;
					}
					if(taken) continue;
					const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(npc.pos));
					if(d < best) { best = d; target = &*e; }
				}
				const ActorUID chosen = target ? target->Main().UID : ActorUID::INVALID;
				if(chosen != npc.chaseTarget) replication->SendAiSetTargetToAll(npc.UID, chosen);
				npc.chaseTarget = chosen;
			}
			if(!target) {
				if(info->moveSpeed > 0) WanderNpc(npc, info->moveSpeed, delta);
				else StopNpcLeg(npc);
				continue;
			}
			goal = target->body->GetWorldPos();
			const f32 d = glm::length(vec2(goal) - vec2(npc.pos));
			if(d <= WEAPON_TARGET_RADIUS) {
				LOG("Summon :: creature=%d reached %ls", (i32)npc.docID, target->name.data());
				RunSummonState(npc, 1);
				if(npc.despawnAt == Time::ZERO || npc.despawnAt > localTime) npc.stuck = true;
				continue;
			}
		}

		if(info->moveSpeed <= 0) continue;
		const vec2 rel = vec2(goal) - vec2(npc.pos);
		const f32 dist = glm::length(rel);
		if(dist <= stopDist + 1.0f) { StopNpcLeg(npc); continue; }
		const vec2 dir = rel / dist;
		SyncNpcLeg(npc, vec3(vec2(goal) - dir * stopDist, goal.z));
		const vec2 legRel = vec2(npc.legGoal) - vec2(npc.pos);
		const f32 legDist = glm::length(legRel);
		if(legDist > 1.0f) {
			const vec2 legDir = legRel / legDist;
			npc.pos = vec3(vec2(npc.pos) + legDir * MIN(info->moveSpeed * delta, legDist), goal.z);
			npc.rot.z = atan2f(legDir.y, legDir.x);
		}
	}
}

void World::UpdateSummons()
{
	for(auto ps = pendingSummons.begin(); ps != pendingSummons.end(); ) {
		if(!ps->remove || localTime < ps->time) { ++ps; continue; }
		foreach(npc, actorNpcList) {
			if(npc->ownerIndex != ps->ownerIndex || npc->ownerUID == ActorUID::INVALID || (i32)npc->docID != ps->creature || localTime >= npc->despawnAt) continue;
			npc->despawnAt = localTime;
			LOG("Summon :: %ls removes creature=%d", players[ps->ownerIndex].name.data(), ps->creature);
		}
		ps = pendingSummons.erase_unsorted(ps);
	}
	for(auto ps = pendingSummons.begin(); ps != pendingSummons.end(); ) {
		if(localTime < ps->time) { ++ps; continue; }
		Player& owner = players[ps->ownerIndex];
		const SummonInfo* info = GetGameXmlContent().FindSummon(ps->creature);
		if(!owner.dead) {
			const bool relay = info && info->ai == SummonInfo::AI_RELAY;
			ActorNpc* reuse = nullptr;
			if(relay) {
				foreach(other, actorNpcList) {
					if(other->ownerIndex == ps->ownerIndex && other->ai == SummonInfo::AI_RELAY && localTime < other->despawnAt) reuse = &*other;
				}
			}
			ActorNpc& actor = reuse ? *reuse : SpawnNpcActor(CreatureIndex(ps->creature), nextSummonLocalID++);
			const f32 life = relay ? RELAY_LIFE_SEC : info && info->lifeTime > 0 ? info->lifeTime : 5.0f;
			if(!reuse) {
				actor.pos = ps->pos;
				actor.home = ps->pos;
				actor.rot = ps->rot;
				actor.faction = owner.team == 0 ? Faction::RED : Faction::BLUE;
				actor.ownerUID = owner.Main().UID;
				actor.seed = (i32)RandUint();
				actor.despawnAt = TimeAdd(localTime, TimeMsToTime(life * 1000.0));
				actor.ownerIndex = ps->ownerIndex;
				actor.ai = info ? (u8)info->ai : 0;
				actor.ownerClass = (i32)owner.Main().classType;
				for(const auto& w : WALL_FOOTPRINTS) {
					if(w.creature != ps->creature) continue;
					actor.wall = physics.CreateWall(actor.pos, w.width, w.depth, WALL_HEIGHT, actor.rot.z, (i32)owner.team);
					LOG("Summon :: creature=%d is a wall %.0fx%.0f", ps->creature, w.width, w.depth);
				}
				if(info && (info->targetable || actor.wall)) {
					foreach_const(st, info->baseStats) {
						if(st->type == (u8)StatType::STAT_TYPE_HEALTH) actor.hp = actor.hpMax = (i32)st->value;
						if(st->type == SUMMON_DEFENCE_STAT) actor.defence = st->value;
					}
				}
				actor.nextPassive = TimeAdd(localTime, TimeMsToTime(RELAY_PASSIVE_SEC * 1000.0));
				if(actor.actionState == ActionStateID::INVALID) actor.actionState = ActionStateID::SPAWN_BEHAVIORSTATE;
				const f32 spawnSec = info && info->spawnAnim != 0 ? GetGameXmlContent().GetAniSeqTime(info->className.c_str(), info->spawnAnim) : 0.0f;
				if(spawnSec < life) actor.standAt = TimeAdd(localTime, TimeMsToTime(MAX(spawnSec, 0.0f) * 1000.0));
				actor.attackRule = info ? FindSummonAttackRule(info->className.c_str()) : nullptr;
				if(actor.attackRule) actor.nextAttack = TimeAdd(localTime, TimeMsToTime(actor.attackRule->firstDelayMs));
				if(info && info->spawnInteraction != 0) AddCreatureInteraction(actor, info->spawnInteraction, owner.team);
			}
			actor.skillTarget = ps->target;
			owner.lastChildUID = actor.UID;
			if(reuse && info->perception > 0 && glm::length(vec2(ps->pos) - vec2(actor.pos)) > info->perception) RelayWarp(actor, ps->pos);
			if(info && owner.pendingChildSkill > 0 && localTime < owner.pendingChildSkillUntil) {
				const i32 child = owner.pendingChildSkill;
				const i32 modeState = child + RELAY_MODE_STATE_OFFSET;
				if(info->ai == SummonInfo::AI_RELAY && modeState <= 20 && !info->skillStates[modeState].empty()) {
					const i32 mode = (child - 1) / RELAY_IDS_PER_MODE;
					const i32 activeState = mode * 2 + 2;
					LOG("Summon :: %ls relay mode SKILL_%d then active SKILL_%d%s", owner.name.data(), modeState, activeState, reuse ? " (same robot)" : "");
					actor.relayMode = mode;
					actor.nextPassive = TimeAdd(localTime, TimeMsToTime(RELAY_PASSIVE_SEC * 1000.0));
					if(mode == RELAY_PRISM_MODE) {
						const StatusInfo* hide = GetGameXmlContent().FindStatus(RELAY_STEALTH_STATUS);
						actor.passiveBlockedUntil = TimeAdd(localTime, TimeMsToTime((hide ? hide->durationSec : 0.0f) * 1000.0));
					}
					RunSummonState(actor, modeState);
					actor.aimPos = owner.pendingChildPos;
					actor.queuedGoal = mode != RELAY_ARTILLERY_MODE && glm::length(vec2(owner.pendingChildPos)) > 1.0f ? vec3(vec2(owner.pendingChildPos), actor.pos.z) : vec3(0);
					const f32 modeSec = GetGameXmlContent().GetAniSeqTime(info->className.c_str(), info->stateAnim[modeState]);
					actor.queuedState = activeState;
					actor.queuedStateAt = TimeAdd(localTime, TimeMsToTime((f64)modeSec * 1000.0 + 60.0));
					LOG("Summon :: relay mode animation %d lasts %.2fs", info->stateAnim[modeState], modeSec);
				}
				else {
					LOG("Summon :: %ls relay state SKILL_%d", owner.name.data(), child);
					if(owner.pendingChildTarget != ActorUID::INVALID) actor.skillTarget = owner.pendingChildTarget;
					RunSummonState(actor, child);
				}
				owner.pendingChildSkill = 0;
			}
			if(info && !reuse) {
				f32 t = 0;
				foreach_const(c, info->onSpawn) {
					if(!pendingSummonCommands.full()) {
						PendingSummonCommand pc;
						pc.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
						pc.ownerIndex = ps->ownerIndex;
						pc.kind = c->kind;
						pc.index = c->index;
						pc.nodeName = c->nodeName;
						pc.pos = ps->pos;
						pc.yaw = ps->rot.z;
						pc.source = actor.UID;
						pc.skill = info->skills.empty() ? 0 : info->skills[0];
						pc.until = actor.despawnAt;
						pendingSummonCommands.push_back(pc);
					}
					t += c->delay;
				}
			}
			LOG("Summon :: %ls creature=%d at (%.0f, %.0f) life=%.1fs known=%d", owner.name.data(), ps->creature, ps->pos.x, ps->pos.y, life, info != nullptr);
			if(Config().DevHideDetectTest > 0 && info && info->hideDetectRadius > 0 && !reuse) {
				Player* nearest = nullptr;
				f32 best = 0;
				foreach(e, players) {
					if(e->team == owner.team || e->dead) continue;
					const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(ps->pos));
					if(!nearest || d < best) { nearest = &*e; best = d; }
				}
				if(nearest) {
					LOG("Hide :: dev test hides %ls at %.0f from detector", nearest->name.data(), best);
					ApplyStatus(*nearest, *nearest, Config().DevHideDetectTest);
				}
			}
		}
		ps = pendingSummons.erase_unsorted(ps);
	}

	for(auto pc = pendingSummonCommands.begin(); pc != pendingSummonCommands.end(); ) {
		if(localTime < pc->time) { ++pc; continue; }
		if(pc->source != ActorUID::INVALID) {
			const ActorNpc* fresh = FindNpcActor(pc->source);
			if(fresh && fresh->spawnedAt >= localTime) { ++pc; continue; }
		}
		Player& owner = players[pc->ownerIndex];
		if(pc->kind == SummonCommand::REMOTE) {
			if(!pendingRemotes.full()) {
				PendingRemote pr;
				pr.time = localTime;
				pr.idx = RemoteIdx(pc->index);
				pr.preset = (i32)ActionCommand::TargetPreset::POS_FFF;
				const Remote* summonRemote = GetGameXmlContent().FindRemote(RemoteIdx(pc->index));
				const bool aimedGraph = summonRemote && summonRemote->behaviorType == Remote::BehaviorType::GRAPH && glm::length(vec2(pc->aim)) > 1.0f;
				pr.offsetSide = 0;
				pr.offsetFront = 0;
				pr.ownerIndex = pc->ownerIndex;
				pr.skillID = pc->skill != 0 ? SkillID(pc->skill) : SkillID::INVALID;
				pr.castID = nextCastID++;
				const ActorNpc* sourceNpc = pc->source != ActorUID::INVALID ? FindNpcActor(pc->source) : nullptr;
				const SummonInfo* sourceInfo = sourceNpc ? GetGameXmlContent().FindSummon((i32)sourceNpc->docID) : nullptr;
				f32 sourceAttack = 0;
				if(sourceInfo) foreach_const(st, sourceInfo->baseStats) if(st->type == STAT_ATTACK) sourceAttack = st->value;
				if(summonRemote && summonRemote->skillIndex != 0) pr.damage = (i32)PlayerSkillDamage(owner, SkillID(summonRemote->skillIndex));
				else if(summonRemote && sourceAttack > 0) pr.damage = (i32)sourceAttack;
				else pr.damage = pc->skill != 0 ? (i32)PlayerSkillDamage(owner, SkillID(pc->skill)) : 0;
				pr.castPos = aimedGraph ? pc->aim : pc->pos;
				pr.angle = aimedGraph ? atan2f(pc->aim.y - pc->pos.y, pc->aim.x - pc->pos.x) : pc->yaw;
				if(aimedGraph) pr.preset = (i32)ActionCommand::TargetPreset::SELF_FFF;
				pr.target = pc->target;
				pr.sourceUID = pc->source;
				pr.nodeName = pc->nodeName;
				pendingRemotes.push_back(pr);
			}
		}
		else {
			const StatusInfo* st = GetGameXmlContent().FindStatus(pc->index);
			const ActorNpc* source = pc->source != ActorUID::INVALID ? FindNpcActor(pc->source) : nullptr;
			if(st && source && localTime < source->despawnAt) {
				replication->SendAddStatusToAll(pc->index, source->UID, source->UID, StatusSentDuration(*st), source->pos, source->pos, 0, 1, 1.0f, -1.0f, false);
				TrackNpcSelfStatus(source->UID, *st);
			}
			if(st && st->auraStatus != 0 && !groundAuras.full()) {
				GroundAura ga;
				ga.ownerIndex = pc->ownerIndex;
				ga.auraStatus = st->ID;
				ga.range = st->targetingRange;
				ga.pos = pc->pos;
				const Time statusEnd = st->durationSec > 0 ? TimeAdd(localTime, TimeMsToTime(st->durationSec * 1000.0)) : pc->until;
				ga.until = st->durationSec > 0 ? statusEnd : pc->until;
				ga.nextPulse = localTime;
				ga.sourceUID = pc->source;
				groundAuras.push_back(ga);
				LOG("Summon :: %ls aura %d (gives %d statuses within %.0f%s)", owner.name.data(), pc->index, (i32)st->auraStatuses.size(), st->targetingRange, st->auraEnemies ? ", enemies" : "");
			}
		}
		pc = pendingSummonCommands.erase_unsorted(pc);
	}

	for(auto ga = groundAuras.begin(); ga != groundAuras.end(); ) {
		if(localTime >= ga->until) {
			if(const StatusInfo* aura = GetGameXmlContent().FindStatus(ga->auraStatus)) EndAura(ga->ownerIndex, *aura);
			ga = groundAuras.erase_unsorted(ga);
			continue;
		}
		if(localTime >= ga->nextPulse) {
			ga->nextPulse = TimeAdd(localTime, TimeMsToTime(500));
			const StatusInfo* aura = GetGameXmlContent().FindStatus(ga->auraStatus);
			if(aura && ga->range > 0) PulseAura(ga->ownerIndex, *aura, vec2(ga->pos), ga->range, -1, ga->sourceUID);
		}
		++ga;
	}

	for(auto it = actorNpcList.begin(); it != actorNpcList.end(); ) {
		if(it->removeAt != Time::ZERO) {
			if(it->dieStandAt != Time::ZERO && localTime >= it->dieStandAt) {
				it->dieStandAt = Time::ZERO;
				replication->SendAiActionStateToAll(it->UID, ActionStateID::NONE_BEHAVIORSTATE, it->rot.z, it->pos, NpcKeepsSpeed(*it));
			}
			if(localTime >= it->removeAt) {
				actorNpcMap.erase(it->UID);
				it = actorNpcList.erase(it);
			}
			else ++it;
			continue;
		}
		if(it->standAt != Time::ZERO && localTime >= it->standAt) {
			it->standAt = Time::ZERO;
			if(it->actionState == ActionStateID::SPAWN_BEHAVIORSTATE || it->actionState == ActionStateID::ATTACK_BEHAVIORSTATE) {
				it->actionState = ActionStateID::NONE_BEHAVIORSTATE;
				replication->SendAiActionStateToAll(it->UID, ActionStateID::NONE_BEHAVIORSTATE, it->rot.z, it->pos, NpcKeepsSpeed(*it));
			}
		}
		if(it->despawnAt != Time::ZERO && localTime >= it->despawnAt) {
			LOG("Summon :: creature=%d despawned", (i32)it->docID);
			physics.RemoveWall(it->wall);
			it->wall = nullptr;
			const SummonInfo* info = GetGameXmlContent().FindSummon((i32)it->docID);
			if(info && info->clearComboOnDeath != 0 && it->ownerUID != ActorUID::INVALID) {
				Player& owner = players[it->ownerIndex];
				if(owner.comboSlot >= 0 && (i32)owner.comboSkill == info->clearComboOnDeath) {
					replication->SendChangeSkillSlot(owner.clientHd, owner.Main().UID, owner.comboSlot, owner.comboBase, 0);
					LOG("Combo :: %ls combo %d cleared by the death of creature %d", owner.name.data(), info->clearComboOnDeath, (i32)it->docID);
					owner.comboSlot = -1;
					owner.comboAutoAt = Time::ZERO;
					owner.comboPending = SkillID::INVALID;
				}
			}
			if(info) {
				f32 t = 0;
				u8 wanted = 0;
				foreach_const(c, info->onDie) if(c->dieMode == it->dieMode) wanted = it->dieMode;
				foreach_const(c, info->onDie) {
					if(c->dieMode != wanted) continue;
					if(!pendingSummonCommands.full()) {
						PendingSummonCommand pc;
						pc.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
						pc.ownerIndex = it->ownerIndex;
						pc.kind = c->kind;
						pc.index = c->index;
						pc.nodeName = c->nodeName;
						pc.pos = it->pos;
						pc.yaw = it->rot.z;
						pc.skill = info->skills.empty() ? 0 : info->skills[0];
						pc.until = localTime;
						if(it->ownerUID != ActorUID::INVALID) pc.source = it->UID;
						pendingSummonCommands.push_back(pc);
					}
					t += c->delay;
				}
			}
			if(it->ownerUID != ActorUID::INVALID) {
				it->actionState = ActionStateID::DIE_BEHAVIORSTATE;
				replication->SendAiActionStateToAll(it->UID, ActionStateID::DIE_BEHAVIORSTATE, it->rot.z, it->pos, NpcKeepsSpeed(*it));
				const ActorMaster* killer = it->killerUID != ActorUID::INVALID ? FindMasterActor(it->killerUID) : nullptr;
				replication->SendNpcDeadAckToAll(it->UID, killer ? it->killerUID : ActorUID::INVALID, killer ? (u32)(100000000 + (i32)killer->classType) : NPC_DEAD_NO_KILLER);
				it->dieStandAt = TimeAdd(localTime, TimeMsToTime(SUMMON_DIE_STAND_MS));
				it->removeAt = TimeAdd(localTime, TimeMsToTime(MAX((f64)(info ? info->destroyTime : 0.0f) * 1000.0, SUMMON_DIE_STAND_MS)));
				++it;
				continue;
			}
			actorNpcMap.erase(it->UID);
			it = actorNpcList.erase(it);
		}
		else {
			++it;
		}
	}
}
