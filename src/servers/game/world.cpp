#include "world.h"
#include <mxm/game_content.h>

static const f32 COMBAT_ATTACK_STAT = 200.0f;
static const i32 WEAPON_HIT_DAMAGE = 120;
static const f64 WEAPON_HIT_INTERVAL_MS = 80.0;
static const f32 WEAPON_PROJECTILE_SPEED = 4000.0f;
static const f32 WEAPON_MELEE_MAX_RANGE = 600.0f;
static const f32 WEAPON_MELEE_HALF_ANGLE = 1.05f;
static const f32 WEAPON_RANGE_SCALE = 1.1f;
static const f32 WEAPON_TARGET_RADIUS = 110.0f;
static const f32 WEAPON_HIT_TOLERANCE = 50.0f;
static const f32 PROJECTILE_TARGET_RADIUS = 70.0f;
static const f32 PROJECTILE_HIT_TOLERANCE = 20.0f;
static const f32 WEAPON_AOE_RADIUS = 250.0f;
static const f32 WEAPON_SHOT_RELIABLE_DISTANCE = 100.0f;
static const f32 WEAPON_SHOT_MAX_DISTANCE = 2500.0f;
static const f32 WEAPON_PROJECTILE_HIT_RADIUS = 130.0f;
static const f32 WEAPON_PROJECTILE_HIT_HEIGHT = 300.0f;
static const i32 SKILL_FALLBACK_DAMAGE = 250;
static const i32 RESPAWN_DELAY_MS = 5000;

static bool InFrontBox(const vec3& origin, const vec2& dir, f32 length, f32 halfWidth, const vec3& pos)
{
	const vec2 rel = vec2(pos - origin);
	const f32 along = glm::dot(rel, dir);
	if(along < 0 || along > length) return false;
	return fabsf(rel.x * dir.y - rel.y * dir.x) <= halfWidth;
}

static f32 ClassAttackRange(ClassType classType)
{
	static const f32 ranges[] = {
		0, 820, 1600, 450, 730, 480, 1100, 300, 600, 400,
		1200, 900, 380, 1200, 400, 800, 800, 800, 1200, 1000,
		1500, 800, 800, 550, 300, 300, 300, 1400, 1400, 400,
		300, 480, 500, 1100, 450, 1100, 300, 300, 300, 1200,
		950,
	};
	const i32 i = (i32)classType;
	if(i > 0 && i < (i32)(sizeof(ranges)/sizeof(ranges[0]))) return ranges[i];
	return 800;
}

static bool ClassIsMelee(ClassType classType)
{
	return ClassAttackRange(classType) * WEAPON_RANGE_SCALE <= WEAPON_MELEE_MAX_RANGE;
}

static f32 SegmentPointDist2D(const vec2& a, const vec2& b, const vec2& p)
{
	const vec2 ab = b - a;
	const f32 len2 = glm::dot(ab, ab);
	f32 t = len2 > 0 ? glm::dot(p - a, ab) / len2 : 0;
	t = clamp(t, 0.0f, 1.0f);
	return glm::length(p - (a + ab * t));
}

void World::Init(Replication* replication_)
{
	replication = replication_;
	nextActorUID = 1;

	auto& ctx = PhysContext();
	ctx.CreateScene(&physics);
}

void World::Cleanup()
{
	physics.Destroy();
}

void World::Update(Time localTime_)
{
	ProfileFunction();

	const f64 tdelta = TimeDurationSec(localTime, localTime_);
	localTime = localTime_;
	physics.localTime = localTime;

	vec3 prevPos = players.front().body->GetWorldPos();

	foreach(it, players) {
		it->posHistory[it->posHistoryHead] = it->body->GetWorldPos();
		it->posHistoryHead = (it->posHistoryHead + 1) % it->posHistory.size();
		it->posHistoryCount = MIN(it->posHistoryCount + 1, (u32)it->posHistory.size());
	}

	// players: handle input
	foreach(it, players) {
		Player& p = *it;

		PhysicsDynamicBody& body = *p.body;

		p.movement.rot = p.input.rot;

		if(p.dead) {
			p.input.moveTo = body.GetWorldPos();
			p.input.tag = 0;
			p.input.jump = 0;
			p.input.cast.skillID = SkillID::INVALID;
			p.weaponFired = false;
			if(localTime > p.tRespawn) {
				RespawnPlayer(p);
			}
		}

		// tag
		if(p.input.tag) {
			p.input.tag = 0;
			p.mainCharaID ^= 1;
		}

		// cast skills
		if(p.input.cast.skillID != SkillID::INVALID) {
			// TODO: check for requirements in general
			// TODO: check for skill
			// TODO: check for cost
			const auto& cast = p.input.cast;
			PlayerCastSkill(p, cast.skillID, cast.pos, Slice<const ActorUID>(cast.targetList.data(), cast.targetList.size()), cast.clientTime);
			p.input.cast.skillID = SkillID::INVALID;
		}

		if(p.weaponFired) {
			p.weaponFired = false;
			if(ClassIsMelee(p.Main().classType) && localTime > p.tNextWeaponHit) {
				p.tNextWeaponHit = TimeAdd(localTime, TimeMsToTime(WEAPON_HIT_INTERVAL_MS));
				PlayerWeaponFire(p);
			}
		}

		// move
		vec2 delta = vec2(p.input.moveTo - body.GetWorldPos());
		f32 deltaLen = glm::length(delta);
		if(deltaLen > 1.0f && p.input.speed > 0.f) {
			vec2 dir = NormalizeSafe(delta);
			p.movement.moveDir = dir;

			// we're close enough that we might miss the point by going full speed in a step, slow down
			if(deltaLen < (p.input.speed * UPDATE_RATE)) {
				p.movement.moveSpeed = f32(deltaLen/UPDATE_RATE);
			}
			else {
				p.movement.moveSpeed = p.input.speed;
			}
		}
		else {
			p.movement.moveDir = vec2(0);
			p.movement.moveSpeed = 0.0f;
		}

		// jump
		p.movement.hasJumped = false;
		if(p.input.jump) {
			p.input.jump = 0;
			p.movement.hasJumped = true;
			body.vel.z = GetGlobalTweakableVars().jumpForce;
		}

		// apply 2D velocity
		const f32 s = p.movement.moveSpeed;
		body.vel.x = p.movement.moveDir.x * s;
		body.vel.y = p.movement.moveDir.y * s;
	}

	foreach(it, players) {
		if(it->pendingShot && TimeDiffSec(TimeDiff(it->pendingShotTime, localTime)) > 0.15) {
			it->pendingShot = false;
			PlayerWeaponShot(*it, it->pendingShotOrigin, it->pendingShotDir, it->pendingShotDistance);
		}
		if(it->pendingAim && TimeDiffSec(TimeDiff(it->pendingAimTime, localTime)) > 0.15) {
			it->pendingAim = false;
		}
	}

	UpdateProjectiles((f32)tdelta);
	UpdateRemotes((f32)tdelta);
	UpdateUltimateGauges((f32)tdelta);

	// execute skill programs
	for(auto it = skillProgramList.begin(); it != skillProgramList.end(); ) {
		if(it->IsDoneExecuting()) {
			it = skillProgramList.erase_unsorted(it);
		}
		else {
			ExecuteSkillProgram(*it);
			++it;
		}
	}

	physics.Step();

	foreach(it, players) {
		if(it->clientHd != ClientHandle::INVALID || it->dead || it->posHistoryCount == 0) continue;
		const f32 expected = it->movement.moveSpeed * (f32)tdelta;
		if(expected < 1.0f) continue;
		const vec3 prev = it->posHistory[(it->posHistoryHead + it->posHistory.size() - 1) % it->posHistory.size()];
		const f32 moved = glm::length(vec2(it->body->GetWorldPos() - prev));
		if(moved < expected * 0.3f) {
			it->input.moveTo = it->body->GetWorldPos();
		}
	}

	/*
	static f64 accumulatedDiff = 0.0;
	if(players.front().movement.moveDir == vec2(0)) {
		accumulatedDiff = 0.0;
	}

	vec3 deltaPos = players.front().body->pos - prevPos;
	f32 moveDiff = players.front().input.speed * UPDATE_RATE - glm::length(vec2(deltaPos));
	if(moveDiff != 0 && players.front().movement.moveDir != vec2(0)) {
		accumulatedDiff += moveDiff;
		LOG("Move diff = %f  |  accumulatedDiff = %g", moveDiff, accumulatedDiff);
	}
	*/

	Replicate();
}

void World::Replicate()
{
	eastl::fixed_vector<Replication::ActorMaster, 64, false> repMasterList;

	// players
	foreach_const(it, players) {
		const Player& player = *it;

		Replication::Player rep;
		rep.index = player.index;
		rep.userID = player.userID;
		rep.clientHd = player.clientHd;
		rep.name = player.name;
		rep.guildTag = player.guildTag;
		rep.team = player.team;

		rep.mainClass = player.mainClass;
		rep.mainSkin = player.mainSkin;
		rep.subClass = player.subClass;
		rep.subSkin = player.subSkin;
		rep.skills = player.skills;
		rep.mainWeapon = player.mainWeapon;
		rep.subWeapon = player.subWeapon;
		rep.mainWeaponGrade = player.mainWeaponGrade;
		rep.subWeaponGrade = player.subWeaponGrade;
		rep.mainMasterGearNo = player.mainMasterGearNo;
		rep.subMasterGearNo = player.subMasterGearNo;
		rep.mainCharacterType = player.mainCharacterType;
		rep.subCharacterType = player.subCharacterType;

		rep.masters = {
			player.characters[0]->UID,
			player.characters[1]->UID
		};
		rep.mainCharaID = player.mainCharaID;
		rep.hasJumped = player.movement.hasJumped;

		replication->FramePushPlayer(rep);

		// main
		{
			const ActorMaster& chara = player.Main();
			Replication::ActorMaster& rch = repMasterList.push_back();
			rch.actorUID = chara.UID;
			rch.clientHd = player.clientHd;
			rch.playerIndex = player.index;
			rch.classType = chara.classType;
			rch.skinIndex = chara.skinIndex;
			rch.hp = chara.hp;
			rch.hpMax = chara.hpMax;
			rch.seed = chara.seed;
			rch.spawnAnim = chara.spawnAnim;
			rch.ownerID = chara.ownerID;
			rch.dirToNearPC = chara.dirToNearPC;
			rch.wanderDist = chara.wanderDist;
			rch.tagID = chara.tagID;




			rch.pos = player.body->GetWorldPos();
			rch.moveDir = player.movement.moveDir;
			rch.speed = player.input.speed;
			rch.rotation = player.movement.rot;

			rch.actionState = chara.actionState;
			rch.actionParam1 = chara.actionParam1;
			rch.actionParam2 = chara.actionParam2;

			rch.taggedOut = false;
		}

		// sub
		{
			const ActorMaster& chara = player.Sub();
			Replication::ActorMaster& rch = repMasterList.push_back();
			rch.actorUID = chara.UID;
			rch.clientHd = player.clientHd;
			rch.playerIndex = player.index;
			rch.classType = chara.classType;
			rch.skinIndex = chara.skinIndex;
			rch.hp = chara.hp;
			rch.hpMax = chara.hpMax;
			rch.seed = chara.seed;
			rch.spawnAnim = chara.spawnAnim;
			rch.ownerID = chara.ownerID;
			rch.dirToNearPC = chara.dirToNearPC;
			rch.wanderDist = chara.wanderDist;
			rch.tagID = chara.tagID;




			rch.pos = player.body->GetWorldPos();
			rch.moveDir = player.movement.moveDir;
			rch.speed = player.input.speed;
			rch.rotation = player.movement.rot;

			rch.actionState = chara.actionState;
			rch.actionParam1 = chara.actionParam1;
			rch.actionParam2 = chara.actionParam2;

			rch.taggedOut = true;
		}
	}

	replication->FramePushMasterActors(repMasterList.data(), repMasterList.size());

	// clear action state
	foreach(it, actorMasterList) {
		it->actionState = ActionStateID::INVALID;
		it->actionParam1 = -1;
		it->actionParam2 = -1;
	}

	// npcs
	foreach_const(it, actorNpcList) {
		const ActorNpc& actor = *it;

		Replication::ActorNpc rfl;
		rfl.actorUID = actor.UID;
		rfl.docID = actor.docID;
		rfl.pos = actor.pos;
		rfl.dir = actor.rot;
		rfl.localID = actor.localID;
		rfl.faction = actor.faction;
		rfl.spawnAnim = actor.spawnAnim;
		rfl.ownerID = actor.ownerID;
		rfl.dirToNearPC = actor.dirToNearPC;
		rfl.wanderDist = actor.wanderDist;
		rfl.tagID = actor.tagID;
		rfl.actionState = actor.actionState;
		rfl.seed = actor.seed;
		rfl.entityType = actor.entityType;


		replication->FramePushNpcActor(rfl);
	}

	// dynamic
	foreach_const(it, actorDynamicList) {
		const ActorDynamic& actor = *it;

		Replication::ActorDynamic rfl;
		rfl.actorUID = actor.UID;
		rfl.docID = actor.docID;
		rfl.pos = actor.pos;
		rfl.rot = actor.rot;
		rfl.localID = actor.localID;
		rfl.faction = actor.faction;
		rfl.action = actor.action;
		rfl.spawnAnim = actor.spawnAnim;
		rfl.ownerID = actor.ownerID;
		rfl.dirToNearPC = actor.dirToNearPC;
		rfl.wanderDist = actor.wanderDist;
		rfl.tagID = actor.tagID;
		rfl.seed = actor.seed;
		rfl.entityType = actor.entityType;


		replication->FramePushDynamicActor(rfl);
	}
}

World::Player& World::CreatePlayer(const PlayerDescription& desc, const vec3& pos, const RotationHumanoid& rot)
{
	players.emplace_back(players.size(), desc);

	Player& player = players.back();
	player.mainCharaID = 0;
	player.level = 1;
	player.experience = 0;
	player.body = physics.CreateDynamicBody(110, 70, pos); // radius 100 is found in files but 110 (_AILength) matches better
	player.spawnPos = pos;

	// clear input
	player.input.moveTo = pos;
	player.input.speed = 0;
	player.input.rot = rot;
	player.input.tag = 0;
	player.input.jump = 0;
	player.input.action = ActionStateID::INVALID;

	const ActorUID mainUID = NewActorUID();
	const ActorUID subUID = NewActorUID();

	actorMasterList.emplace_back(mainUID);
	ActorMaster& main = actorMasterList.back();
	ActorMasterHandle hMain = --actorMasterList.end();
	actorMasterMap.emplace(mainUID, hMain);

	actorMasterList.emplace_back(subUID);
	ActorMaster& sub = actorMasterList.back();
	ActorMasterHandle hSub = --actorMasterList.end();
	actorMasterMap.emplace(subUID, hSub);

	player.characters = {
		hMain,
		hSub
	};

	main.parent = &player;
	main.classType = player.mainClass;
	main.skinIndex = player.mainSkin;
	main.hp = 2400;
	main.hpMax = 2400;
	main.seed = (i32)RandUint();




	sub.parent = &player;
	sub.classType = player.subClass;
	sub.skinIndex = player.subSkin;
	sub.hp = 2400;
	sub.hpMax = 2400;
	sub.seed = (i32)RandUint();



	return player;
}

World::ActorNpc& World::SpawnNpcActor(CreatureIndex docID, i32 localID)
{
	ActorUID actorUID = NewActorUID();

	actorNpcList.emplace_back(actorUID);
	ActorNpc& actor = actorNpcList.back();
	actor.docID = (CreatureIndex)docID;
	actor.localID = localID;

	actorNpcMap.emplace(actorUID, --actorNpcList.end());
	return actor;
}

World::ActorDynamic& World::SpawnDynamic(CreatureIndex docID, i32 localID)
{
	ActorUID actorUID = NewActorUID();

	actorDynamicList.emplace_back(actorUID);
	auto& actor = actorDynamicList.back();
	actor.docID = (CreatureIndex)docID;
	actor.localID = localID;
	actor.faction = Faction::DYNAMIC;
	actor.action = ActionStateID::DYNAMIC_NORMAL_STAND;
	actor.tLastActionChange = localTime;

	actorDynamicMap.emplace(actorUID, --actorDynamicList.end());
	return actor;
}

World::Player& World::GetPlayer(u32 playerIndex)
{
	return players[playerIndex];
}

World::ActorMaster* World::FindMasterActor(ActorUID actorUID) const
{
	auto it = actorMasterMap.find(actorUID);
	if(it == actorMasterMap.end()) return nullptr;
	return &(*it->second);
}

World::ActorNpc* World::FindNpcActor(ActorUID actorUID) const
{
	auto it = actorNpcMap.find(actorUID);
	if(it == actorNpcMap.end()) return nullptr;
	return &(*it->second);
}

World::ActorNpc* World::FindNpcActorByCreatureID(CreatureIndex docID)
{
	foreach(it, actorNpcList) {
		if(it->docID == docID) {
			return &(*it);
		}
	}
	return nullptr;
}

ActorUID World::NewActorUID()
{
	return (ActorUID)nextActorUID++;
}

World::ActorMasterHandle World::MasterInvalidHandle()
{
	return actorMasterList.end();
}

void World::PlayerCastSkill(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 clientTime)
{
	// TODO: check if can cast

	// access method is kinda convoluted
	const auto& content = GetGameXmlContent();
	const auto& skill = content.skillMap.at(skillID);
	const ActionStateID actionState = skill.action;

	const f32 angle = player.input.rot.upperYaw;
	const vec2 dir = vec2(cosf(angle), sinf(angle));

	Replication::SkillCast rpCast;
	rpCast.clientHd = player.clientHd;
	rpCast.casterUID = player.Main().UID;
	rpCast.skillID = skillID;
	rpCast.castPos = castPos;
	rpCast.actionID = actionState;

	rpCast.casterPos = player.body->GetWorldPos();
	rpCast.casterMoveDir = dir;
	rpCast.casterRot = { angle, 0, angle };
	rpCast.casterSpeed = player.input.speed; // FIXME: should not come from input
	rpCast.clientTime = clientTime;

	eastl::copy(targets.begin(), targets.end(), eastl::back_inserter(rpCast.targetList));

	replication->FramePushSkillCast(rpCast);


	/*
	 * After the cast, we start executing a "skill program"
	 * It is a set of simple instructions in the world of MxM
	 * Each "program" is described in ActionBase.xml
	 */

	// Trigger new skill execution
	player.Main().actionState = actionState;

	SkillProgram prog;
	prog.skillID = skillID;
	prog.actionID = actionState;
	prog.castPos = castPos;
	prog.castAngle = angle;
	prog.casterUID = player.Main().UID;
	eastl::copy(targets.begin(), targets.end(), eastl::back_inserter(prog.targetList));
	prog.startTime = localTime;
	prog.commandID = 0;
	skillProgramList.push_back(prog);

	// go through the skill program to find out how much the master moves
	f32 distance = 0;
	f32 moveDuration = 0;

	const auto& action = content.GetSkillAction(player.Main().classType, actionState);

	foreach_const(cmd, action.commands) {
		switch(cmd->type) {
			case ActionCommand::Type::GRAPH_MOVE_HORZ: {
				distance = cmd->graphMoveHorz.distance;
				moveDuration = action.seqLength;
			} break;

			case ActionCommand::Type::MOVE: {
				switch(cmd->move.preset) {
					case ActionCommand::MovePreset::WARP: {
						distance = (f32)cmd->move.param2;
						moveDuration = 0.01f; // warping
					} break;
				}
			} break;
		}
	}

	Replication::SkillExec rpExec;
	rpExec.casterUID = player.Main().UID;
	rpExec.skillID = skillID;
	rpExec.castPos = castPos;
	rpExec.actionID = actionState;
	eastl::copy(targets.begin(), targets.end(), eastl::back_inserter(rpExec.targetList));

	rpExec.moveDuration = moveDuration;
	rpExec.startPos = player.body->GetWorldPos();
	rpExec.moveDir = dir;
	rpExec.rot = { angle, 0, angle };
	rpExec.speed = player.input.speed; // FIXME: should not come from input

	if(distance != 0) {
		const vec3 endPos = physics.FindMovePos(player.body, vec3(dir * distance, 0), moveDuration);
		rpExec.endPos = endPos;
	}

	replication->FramePushSkillExec(rpExec);

	if(!ScheduleSkillRemotes(player, skillID, castPos, targets, angle)) {
		PlayerSkillDamage(player, skillID, castPos, targets, angle);
	}
}

void World::PlayerWeaponFire(Player& player)
{
	const f32 range = ClassAttackRange(player.Main().classType) * WEAPON_RANGE_SCALE;
	const vec2 dir = vec2(cosf(player.weaponAimYaw), sinf(player.weaponAimYaw));
	const vec3 origin = player.body->GetWorldPos();

	if(ClassIsMelee(player.Main().classType)) {
		foreach(it, players) {
			if(it->team == player.team || it->dead) continue;
			const vec3 pos = it->body->GetWorldPos();
			if(fabsf(pos.z - origin.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
			const vec2 rel = LagClosestPos(*it, vec2(origin)) - vec2(origin);
			const f32 dist = glm::length(rel);
			if(dist > range + WEAPON_TARGET_RADIUS + WEAPON_HIT_TOLERANCE) continue;
			if(dist > WEAPON_TARGET_RADIUS) {
				const f32 cosAngle = glm::dot(rel / dist, dir);
				if(cosAngle < cosf(WEAPON_MELEE_HALF_ANGLE)) continue;
			}
			DamagePlayer(player, *it, SkillID::INVALID, WEAPON_HIT_DAMAGE);
		}
	}
}

void World::PlayerWeaponShot(Player& player, const vec3& origin, const vec2& dir, f32 distance)
{
	if(player.dead) return;
	const ClassType classType = player.Main().classType;
	if(ClassIsMelee(classType)) return;
	if(projectileList.full()) return;

	const auto& content = GetGameXmlContent();
	const Remote* remote = content.FindRemote(RemoteIdx(1430000000 + (i32)classType * 10000 + 10));
	const Remote* bomb = remote ? content.FindRemote(remote->destroyCreateRemote) : nullptr;

	const f32 classRange = ClassAttackRange(classType) * WEAPON_RANGE_SCALE;
	const bool reliable = distance >= WEAPON_SHOT_RELIABLE_DISTANCE;
	const bool graph = remote && remote->behaviorType == Remote::BehaviorType::GRAPH;

	Projectile proj;
	proj.ownerIndex = player.index;
	proj.pos = reliable ? vec3(vec2(origin), player.body->GetWorldPos().z) : player.body->GetWorldPos();
	proj.dir = reliable ? dir : vec2(cosf(player.weaponAimYaw), sinf(player.weaponAimYaw));
	proj.travelled = 0;
	proj.start = proj.pos;
	proj.speed = (remote && remote->maxSpeed > 0) ? remote->maxSpeed : WEAPON_PROJECTILE_SPEED;

	f32 remoteRange = classRange;
	if(remote && remote->maxDistance > 0) remoteRange = remote->maxDistance;
	else if(remote && remote->lifeTime > 0) remoteRange = remote->lifeTime * proj.speed;

	if(remote && remote->maxDistance > 0) remoteRange *= 1.05f;

	proj.range = graph ? (reliable ? MIN(distance, WEAPON_SHOT_MAX_DISTANCE) : classRange) : remoteRange;
	proj.graph = graph;
	proj.halfWidth = remote ? remote->boundSize[0] * 0.5f : 20.0f;
	proj.hitsLeft = remote ? MAX(remote->activateCount, 1) : 1;
	proj.forwardReach = (remote && remote->boundType == Remote::BoundType::E_BOUND_BOX) ? MAX(remote->boundSize[1] + 120.0f, PROJECTILE_TARGET_RADIUS) : PROJECTILE_TARGET_RADIUS;
	proj.aoeRadius = bomb ? bomb->boundSize[0] * 0.5f : 0;

	proj.remoteDoc = remote ? remote->ID : RemoteIdx::INVALID;
	proj.pendingAdvance = player.shotLeadSec * proj.speed;
	if(player.pendingShotRawSize >= 12) {
		proj.remoteID = *(const u32*)(player.pendingShotRaw.data() + 8) + 1;
	}
	replication->SendAttackActionToOthers(player.clientHd, player.Main().UID, WorldYawToMxmYaw(atan2f(proj.dir.y, proj.dir.x)));

	if(player.pendingShotRawSize > 0) {
		const vec3 target = vec3(vec2(origin) + proj.dir * proj.range, origin.z);
		replication->RelayRemoteCreate(player.clientHd, player.pendingShotRaw.data(), player.pendingShotRawSize, target, remote ? remote->lifeTime : 0, remote ? remote->ID : RemoteIdx::INVALID);
		player.pendingShotRawSize = 0;
	}
	proj.damage = WEAPON_HIT_DAMAGE;
	LOG("Shot :: %ls class=%d remote=%d graph=%d reliable=%d dist=%.0f range=%.0f speed=%.0f width=%.0f hits=%d aoe=%.0f", player.name.data(), (i32)classType, remote ? (i32)remote->ID : -1, graph, reliable, distance, proj.range, proj.speed, proj.halfWidth, proj.hitsLeft, proj.aoeRadius);
	projectileList.push_back(proj);
}

void World::Explode(Player& owner, const vec3& center, f32 radius, i32 damage)
{
	LOG("Explosion :: at (%.0f, %.0f) radius=%.0f", center.x, center.y, radius);
	foreach(it, players) {
		if(it->team == owner.team || it->dead) continue;
		const vec3 pos = it->body->GetWorldPos();
		if(fabsf(pos.z - center.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
		if(LagDistToPoint(*it, vec2(center)) > radius + WEAPON_TARGET_RADIUS * 0.5f + WEAPON_HIT_TOLERANCE) continue;
		DamagePlayer(owner, *it, SkillID::INVALID, damage);
	}
}

void World::UpdateProjectiles(f32 delta)
{
	for(auto proj = projectileList.begin(); proj != projectileList.end(); ) {
		Player& owner = players[proj->ownerIndex];
		const f32 step = MIN(proj->speed * delta + proj->pendingAdvance, proj->range - proj->travelled);
		proj->pendingAdvance = 0;
		const bool lastStep = proj->travelled + step >= proj->range - 0.01f;
		const vec2 from = vec2(proj->pos);
		const vec2 to = from + proj->dir * step;
		bool destroyed = false;

		if(!proj->graph) {
			foreach(it, players) {
				if(it->team == owner.team || it->dead) continue;
				if(eastl::find(proj->hitList.begin(), proj->hitList.end(), it->index) != proj->hitList.end()) continue;
				const vec3 pos = it->body->GetWorldPos();
				const f32 lagDist = LagDistToSegment(*it, from, to);
				proj->closest = MIN(proj->closest, lagDist);
				if(fabsf(pos.z - proj->pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
				if(!LagSegmentHits(*it, from, to, proj->halfWidth + PROJECTILE_TARGET_RADIUS + PROJECTILE_HIT_TOLERANCE, lastStep ? proj->forwardReach : PROJECTILE_TARGET_RADIUS)) continue;

				if(!proj->hitList.full()) proj->hitList.push_back(it->index);
				DamagePlayer(owner, *it, SkillID::INVALID, proj->damage, &*proj);
				proj->hitsLeft--;
				if(proj->hitsLeft <= 0) {
					if(proj->remoteID != 0) replication->SendDestroyRemoteToAll(proj->remoteID);
					proj->pos = vec3(vec2(pos), proj->pos.z);
					destroyed = true;
					break;
				}
			}
		}

		if(!destroyed) {
			proj->pos += vec3(proj->dir * step, 0);
			proj->travelled += step;
			destroyed = proj->travelled >= proj->range - 0.01f;
		}

		if(destroyed) {
			const Player* nearPlayer = nullptr;
			f32 nearDist = 999999.0f;
			foreach_const(it, players) {
				if(it->team == owner.team || it->dead) continue;
				const f32 d = SegmentPointDist2D(vec2(proj->start), vec2(proj->pos), vec2(it->body->GetWorldPos()));
				if(d < nearDist) {
					nearDist = d;
					nearPlayer = &*it;
				}
			}
			if(nearPlayer) {
				const vec2 rel = vec2(nearPlayer->body->GetWorldPos()) - vec2(proj->start);
				LOG("ProjectileEnd :: start=(%.0f, %.0f, %.0f) dir=(%.2f, %.2f) travelled=%.0f hit=%d nearest=%ls at (%.0f, %.0f, %.0f) along=%.0f side=%.0f lagClosest=%.0f", proj->start.x, proj->start.y, proj->start.z, proj->dir.x, proj->dir.y, proj->travelled, proj->hitsLeft <= 0, nearPlayer->name.data(), nearPlayer->body->GetWorldPos().x, nearPlayer->body->GetWorldPos().y, nearPlayer->body->GetWorldPos().z, glm::dot(rel, proj->dir), glm::dot(rel, vec2(-proj->dir.y, proj->dir.x)), proj->closest);
			}
			if(proj->aoeRadius > 0) {
				Explode(owner, proj->pos, proj->aoeRadius, proj->damage);
			}
			proj = projectileList.erase_unsorted(proj);
		}
		else {
			++proj;
		}
	}
}

static f32 SkillDamage(SkillID skillID)
{
	const auto& content = GetGameXmlContent();
	auto found = content.skillMap.find(skillID);
	if(found == content.skillMap.end()) return (f32)SKILL_FALLBACK_DAMAGE;
	SkillNormalModel& skill = const_cast<SkillNormalModel&>(found->second);
	SkillNormalLevelModel& level = *skill.getSkillNormalLevelByIndex(0);
	const f32 damage = level.getBaseDamage() + COMBAT_ATTACK_STAT * level.getAttackMultiplier();
	return damage > 0 ? damage : (f32)SKILL_FALLBACK_DAMAGE;
}

static vec2 Perp(const vec2& v)
{
	return vec2(-v.y, v.x);
}

static bool SegmentReachesPoint(const vec2& a, const vec2& b, const vec2& p, f32 lateral, f32 forwardReach)
{
	const vec2 ab = b - a;
	const f32 len = glm::length(ab);
	if(len < 0.001f) return glm::length(p - a) <= lateral;
	const vec2 dir = ab / len;
	const vec2 rel = p - a;
	const f32 along = glm::dot(rel, dir);
	if(along < -PROJECTILE_TARGET_RADIUS || along > len + forwardReach) return false;
	return fabsf(rel.x * dir.y - rel.y * dir.x) <= lateral;
}

bool World::LagSegmentHits(const Player& p, const vec2& a, const vec2& b, f32 lateral, f32 forwardReach) const
{
	if(SegmentReachesPoint(a, b, vec2(p.body->GetWorldPos()), lateral, forwardReach)) return true;
	for(u32 i = 0; i < p.posHistoryCount; i++) {
		if(SegmentReachesPoint(a, b, vec2(p.posHistory[i]), lateral, forwardReach)) return true;
	}
	return false;
}

f32 World::LagDistToSegment(const Player& p, const vec2& a, const vec2& b) const
{
	f32 best = SegmentPointDist2D(a, b, vec2(p.body->GetWorldPos()));
	for(u32 i = 0; i < p.posHistoryCount; i++) {
		best = MIN(best, SegmentPointDist2D(a, b, vec2(p.posHistory[i])));
	}
	return best;
}

f32 World::LagDistToPoint(const Player& p, const vec2& point) const
{
	return glm::length(LagClosestPos(p, point) - point);
}

vec2 World::LagClosestPos(const Player& p, const vec2& point) const
{
	vec2 best = vec2(p.body->GetWorldPos());
	f32 bestDist = glm::length(best - point);
	for(u32 i = 0; i < p.posHistoryCount; i++) {
		const vec2 h = vec2(p.posHistory[i]);
		const f32 d = glm::length(h - point);
		if(d < bestDist) {
			bestDist = d;
			best = h;
		}
	}
	return best;
}

void World::OnWeaponShotPacket(Player& player, const vec3& origin, const vec2& dir, f32 distance, const u8* rawPacket, i32 rawSize)
{
	player.pendingShotRawSize = MIN(rawSize, (i32)player.pendingShotRaw.size());
	memmove(player.pendingShotRaw.data(), rawPacket, player.pendingShotRawSize);

	if(player.pendingAim) {
		player.pendingAim = false;
		PlayerWeaponShot(player, origin, vec2(cosf(player.pendingAimYaw), sinf(player.pendingAimYaw)), distance);
		return;
	}
	player.pendingShot = true;
	player.pendingShotOrigin = origin;
	player.pendingShotDir = dir;
	player.pendingShotDistance = distance;
	player.pendingShotTime = localTime;
}

void World::OnWeaponAimPacket(Player& player, f32 aimYaw)
{
	if(player.pendingShot) {
		player.pendingShot = false;
		PlayerWeaponShot(player, player.pendingShotOrigin, vec2(cosf(aimYaw), sinf(aimYaw)), player.pendingShotDistance);
		return;
	}
	player.pendingAim = true;
	player.pendingAimYaw = aimYaw;
	player.pendingAimTime = localTime;
}

static const f32 ULTIMATE_GAUGE_MAX = 100.0f;
static const f32 ULTIMATE_RECHARGE_SEC = 45.0f;

void World::UpdateUltimateGauges(f32 delta)
{
	foreach(it, players) {
		if(it->clientHd == ClientHandle::INVALID) continue;
		it->ultGauge = MIN(it->ultGauge + ULTIMATE_GAUGE_MAX / ULTIMATE_RECHARGE_SEC * delta, ULTIMATE_GAUGE_MAX);
		if(localTime < it->tNextUltSend) continue;
		it->tNextUltSend = TimeAdd(localTime, TimeMsToTime(1000));
		it->ultGaugeSent = it->ultGauge;
		foreach(c, it->characters) {
			replication->SendStatToAll((*c)->UID, (i32)StatType::STAT_TYPE_ULTIMATE, it->ultGauge, ULTIMATE_GAUGE_MAX);
		}
	}
}

bool World::ScheduleSkillRemotes(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 angle)
{
	const auto& content = GetGameXmlContent();
	const auto& skill = content.skillMap.at(skillID);
	const auto& action = content.GetSkillAction(player.Main().classType, skill.action);

	i32 anim = 0;
	if(player.lastComboSkill == skillID && TimeDiffSec(TimeDiff(player.lastComboTime, localTime)) < 3.0) {
		foreach_const(cmd, action.commands) {
			if(cmd->animIndex == 1 && cmd->type == ActionCommand::Type::REMOTE) anim = 1;
		}
	}
	player.lastComboSkill = anim == 0 ? skillID : SkillID::INVALID;
	player.lastComboTime = localTime;

	{
		SkillNormalModel& skillModel = const_cast<SkillNormalModel&>(skill);
		const f32 consumeUG = skillModel.getSkillNormalLevelByIndex(0)->getConsumeUG();
		if(consumeUG > 0) {
			player.ultGauge = 0.0f;
			player.tNextUltSend = Time::ZERO;
		}
	}

	const u32 castID = nextCastID++;
	const i32 damage = (i32)SkillDamage(skillID);
	const ActorUID currentTarget = targets.size() > 0 ? targets[0] : ActorUID::INVALID;

	bool scheduled = false;
	f32 t = 0;
	foreach_const(cmd, action.commands) {
		if(cmd->animIndex != anim) continue;
		if(cmd->type == ActionCommand::Type::REMOTE && !pendingRemotes.full()) {
			PendingRemote pr;
			pr.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			pr.idx = cmd->remote.idx;
			pr.preset = (i32)cmd->remote.targetPreset;
			pr.offsetSide = cmd->remote.offsetSide;
			pr.offsetFront = cmd->remote.offsetFront;
			pr.ownerIndex = player.index;
			pr.skillID = skillID;
			pr.castID = castID;
			pr.damage = damage;
			pr.castPos = castPos;
			pr.angle = angle;
			pr.target = cmd->remote.targetPreset == ActionCommand::TargetPreset::MEMORIZED_TFT ? player.memorizedTarget : currentTarget;
			pendingRemotes.push_back(pr);
			scheduled = true;
		}
		t += cmd->delay;
	}

	LOG("Skill :: %ls skill=%d anim=%d scheduled=%d damage=%d castPos=(%.0f, %.0f)", player.name.data(), (i32)skillID, anim, scheduled, damage, castPos.x, castPos.y);
	return scheduled;
}

void World::SpawnRemote(u32 ownerIndex, RemoteIdx idx, const vec3& pos, const vec2& dir, ActorUID target, SkillID skillID, u32 castID, i32 damage, bool broadcast)
{
	const Remote* remote = GetGameXmlContent().FindRemote(idx);
	if(!remote || remoteInsts.full()) return;

	RemoteInst inst;
	inst.remote = remote;
	inst.ownerIndex = ownerIndex;
	inst.skillID = skillID;
	inst.castID = castID;
	inst.damage = damage;
	inst.pos = pos;
	inst.dir = dir;
	inst.travelled = 0;
	inst.speed = remote->maxSpeed;
	inst.range = remote->maxDistance;
	if(remote->behaviorType == Remote::BehaviorType::GRAPH && remote->fixedGraphLength > 0) inst.range = remote->fixedGraphLength;
	if(inst.range <= 0 && remote->lifeTime > 0 && inst.speed > 0) inst.range = remote->lifeTime * inst.speed;
	inst.spawnTime = localTime;
	inst.target = target;
	inst.hitsLeft = MAX(remote->activateCount, 1);
	inst.areaDone = false;

	if(broadcast) {
		const u32 seed = nextRemoteSeed;
		nextRemoteSeed += 2;
		inst.clientRemoteID = seed + 1;
		vec3 targetPos = pos + vec3(dir * (inst.range > 0 ? inst.range : 1000.0f), 0);
		const ActorMaster* targetMaster = target != ActorUID::INVALID ? FindMasterActor(target) : nullptr;
		if(targetMaster) targetPos = targetMaster->parent->body->GetWorldPos();
		const f32 sendYaw = atan2f(dir.y, dir.x) + (inst.speed <= 0 && remote->boundType == Remote::BoundType::E_BOUND_BOX ? PI : 0.0f);
		replication->SendRemoteCreateToAll(players[ownerIndex].Main().UID, seed, idx, pos, sendYaw, targetPos, targetMaster ? target : ActorUID::INVALID);
	}

	remoteInsts.push_back(inst);

	LOG("Remote :: spawn %d at (%.0f, %.0f) dir=(%.2f, %.2f) speed=%.0f range=%.0f", (i32)idx, pos.x, pos.y, dir.x, dir.y, inst.speed, inst.range);
}

void World::RemoteHit(RemoteInst& inst, Player& target)
{
	const Remote& remote = *inst.remote;
	if(remote.damageGroup != Remote::DamageGroup::eENEMY && remote.damageGroup != Remote::DamageGroup::eALL) return;

	Player& owner = players[inst.ownerIndex];
	if(target.team == owner.team || target.dead) return;

	const i32 damage = (i32)(inst.damage * remote.attackMultiplier);
	if(remote.memorizeTarget) owner.memorizedTarget = target.Main().UID;
	if(damage <= 0) return;

	foreach(h, castHits) {
		if(h->castID == inst.castID && h->playerIndex == target.index && localTime < h->until) return;
	}
	if(!castHits.full()) {
		CastHit h;
		h.castID = inst.castID;
		h.playerIndex = target.index;
		h.until = TimeAdd(localTime, TimeMsToTime(MAX(remote.hitInvalidTime, 0.1f) * 1000.0));
		castHits.push_back(h);
	}

	DamagePlayer(owner, target, inst.skillID, damage);
}

void World::RemoteApplyArea(RemoteInst& inst)
{
	const Remote& remote = *inst.remote;
	if(remote.boundType == Remote::BoundType::E_BOUND_SPHERE) {
		const bool cone = remote.angle > 0;
		const f32 radius = remote.boundSize[0] * 0.5f;
		const f32 margin = WEAPON_TARGET_RADIUS;
		foreach(it, players) {
			const vec3 pos = it->body->GetWorldPos();
			if(fabsf(pos.z - inst.pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
			const vec2 rel = LagClosestPos(*it, vec2(inst.pos)) - vec2(inst.pos);
			const f32 dist = glm::length(rel);
			if(dist > radius + margin) continue;
			if(remote.angle > 0 && dist > WEAPON_TARGET_RADIUS) {
				if(glm::dot(rel / dist, inst.dir) < cosf(MIN(remote.angle * 0.5f + 15.0f, 180.0f) * PI / 180.0f)) continue;
			}
			RemoteHit(inst, *it);
		}
	}
	else if(remote.boundType == Remote::BoundType::E_BOUND_BOX) {
		const f32 halfWidth = remote.boundSize[0] * 0.5f;
		const f32 length = remote.boundSize[1];
		foreach(it, players) {
			const vec3 pos = it->body->GetWorldPos();
			if(fabsf(pos.z - inst.pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
			const vec2 center = vec2(inst.pos) + inst.dir * (remote.boundSize[1] * 0.5f);
			const vec2 rel = LagClosestPos(*it, center) - vec2(inst.pos);
			const f32 along = glm::dot(rel, inst.dir);
			const f32 side = fabsf(glm::dot(rel, Perp(inst.dir)));
			if(along < -WEAPON_TARGET_RADIUS - WEAPON_HIT_TOLERANCE || along > length + WEAPON_TARGET_RADIUS + WEAPON_HIT_TOLERANCE) continue;
			if(side > halfWidth + WEAPON_TARGET_RADIUS + WEAPON_HIT_TOLERANCE) continue;
			RemoteHit(inst, *it);
		}
	}
}

void World::UpdateRemotes(f32 delta)
{
	for(auto pr = pendingRemotes.begin(); pr != pendingRemotes.end(); ) {
		if(localTime < pr->time) {
			++pr;
			continue;
		}

		Player& owner = players[pr->ownerIndex];
		const vec2 dir = vec2(cosf(pr->angle), sinf(pr->angle));
		const Remote* remote = GetGameXmlContent().FindRemote(pr->idx);
		const f32 offsetY = remote ? remote->offsetY : 0;
		vec3 pos = owner.body->GetWorldPos() + vec3(dir * offsetY, 158.0f);

		switch((ActionCommand::TargetPreset)pr->preset) {
			case ActionCommand::TargetPreset::POS_FFF:
			case ActionCommand::TargetPreset::POS_TFF:
			case ActionCommand::TargetPreset::POS_TTT_1:
			case ActionCommand::TargetPreset::POS_TFF_1_800_1000: {
				const vec3 base = glm::length(vec2(pr->castPos)) > 1.0f ? pr->castPos : owner.body->GetWorldPos();
				pos = vec3(vec2(base) + dir * pr->offsetFront + Perp(dir) * pr->offsetSide, owner.body->GetWorldPos().z);
			} break;
		}

		if(!owner.dead) {
			SpawnRemote(pr->ownerIndex, pr->idx, pos, dir, pr->target, pr->skillID, pr->castID, pr->damage, true);
		}
		pr = pendingRemotes.erase_unsorted(pr);
	}

	for(int i = 0; i < (i32)remoteInsts.size(); ) {
		RemoteInst& inst = remoteInsts[i];
		const Remote& remote = *inst.remote;
		const f32 elapsed = (f32)TimeDiffSec(TimeDiff(inst.spawnTime, localTime));
		bool destroyed = false;

		switch(remote.behaviorType) {
			case Remote::BehaviorType::TARGET:
			case Remote::BehaviorType::FOLLOW_TARGET:
			case Remote::BehaviorType::HOMING: {
				const ActorMaster* targetMaster = inst.target != ActorUID::INVALID ? FindMasterActor(inst.target) : nullptr;
				if(!targetMaster || targetMaster->parent->dead) {
					destroyed = true;
					break;
				}
				Player& target = *targetMaster->parent;
				const vec3 tpos = target.body->GetWorldPos();
				const vec2 rel = vec2(tpos - inst.pos);
				const f32 dist = glm::length(rel);
				const f32 step = (inst.speed > 0 ? inst.speed : 3000.0f) * delta;
				if(dist <= step + remote.boundSize[0] * 0.5f + WEAPON_TARGET_RADIUS) {
					inst.pos = vec3(vec2(tpos), inst.pos.z);
					RemoteHit(inst, target);
					destroyed = true;
				}
				else {
					inst.dir = rel / dist;
					inst.pos += vec3(inst.dir * step, 0);
				}
			} break;

			case Remote::BehaviorType::GRAPH: {
				const f32 step = MIN(inst.speed * delta, inst.range - inst.travelled);
				inst.pos += vec3(inst.dir * step, 0);
				inst.travelled += step;
				destroyed = inst.travelled >= inst.range - 0.01f;
			} break;

			default: {
				if(inst.speed > 0) {
					const f32 step = inst.range > 0 ? MIN(inst.speed * delta, inst.range - inst.travelled) : inst.speed * delta;
					const vec2 from = vec2(inst.pos);
					const vec2 to = from + inst.dir * step;
					const f32 halfWidth = remote.boundSize[0] * 0.5f;
					if(remote.damageGroup != Remote::DamageGroup::eNONE) {
						foreach(it, players) {
							if(it->team == players[inst.ownerIndex].team || it->dead) continue;
							const vec3 pos = it->body->GetWorldPos();
							if(fabsf(pos.z - inst.pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
							if(!LagSegmentHits(*it, from, to, halfWidth + PROJECTILE_TARGET_RADIUS + PROJECTILE_HIT_TOLERANCE, PROJECTILE_TARGET_RADIUS)) continue;
							RemoteHit(inst, *it);
							inst.hitsLeft--;
							if(inst.hitsLeft <= 0) {
								if(inst.clientRemoteID != 0) replication->SendDestroyRemoteToAll(inst.clientRemoteID);
								inst.destroyedByHit = true;
								inst.pos = vec3(vec2(pos), inst.pos.z);
								destroyed = true;
								break;
							}
						}
					}
					if(!destroyed) {
						inst.pos += vec3(inst.dir * step, 0);
						inst.travelled += step;
						if(inst.range > 0 && inst.travelled >= inst.range - 0.01f) destroyed = true;
					}
				}
				else if(!inst.areaDone) {
					inst.areaDone = true;
					RemoteApplyArea(inst);
				}
			} break;
		}

		if(!destroyed && remote.lifeTime > 0 && elapsed >= remote.lifeTime) destroyed = true;
		if(!destroyed && remote.lifeTime <= 0 && inst.speed <= 0 && inst.areaDone) destroyed = true;

		if(destroyed && inst.speed > 0) {
			f32 nearest = 999999.0f;
			const Player* nearestPlayer = nullptr;
			foreach_const(it, players) {
				if(it->team == players[inst.ownerIndex].team || it->dead) continue;
				const f32 d = glm::length(vec2(it->body->GetWorldPos()) - vec2(inst.pos));
				if(d < nearest) { nearest = d; nearestPlayer = &*it; }
			}
			LOG("SkillRemoteEnd :: %d travelled=%.0f hitsLeft=%d nearest=%ls dist=%.0f", (i32)remote.ID, inst.travelled, inst.hitsLeft, nearestPlayer ? nearestPlayer->name.data() : L"-", nearest);
		}

		if(destroyed) {
			const RemoteIdx next = remote.destroyCreateRemote;
			const RemoteInst copy = inst;
			remoteInsts.erase_unsorted(remoteInsts.begin() + i);
			if(next != RemoteIdx::INVALID) {
				const Remote* nextRemote = GetGameXmlContent().FindRemote(next);
				const f32 nextOffset = nextRemote ? nextRemote->offsetY : 0;
				SpawnRemote(copy.ownerIndex, next, copy.pos + vec3(copy.dir * nextOffset, 0), copy.dir, copy.target, copy.skillID, copy.castID, copy.damage, copy.destroyedByHit && copy.clientRemoteID != 0);
			}
		}
		else {
			i++;
		}
	}

	for(auto h = castHits.begin(); h != castHits.end(); ) {
		if(localTime >= h->until) h = castHits.erase_unsorted(h);
		else ++h;
	}
}

void World::PlayerSkillDamage(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 angle)
{
	const auto& content = GetGameXmlContent();
	auto found = content.skillMap.find(skillID);
	if(found == content.skillMap.end()) return;

	SkillNormalModel& skill = const_cast<SkillNormalModel&>(found->second);
	if(skill.type != SkillType::NORMAL && skill.type != SkillType::COMBO) return;

	SkillNormalLevelModel& level = *skill.getSkillNormalLevelByIndex(0);
	i32 damage = (i32)(level.getBaseDamage() + COMBAT_ATTACK_STAT * level.getAttackMultiplier());
	if(damage <= 0) damage = SKILL_FALLBACK_DAMAGE;

	const f32 rangeX = level.getSkillRangeLengthX();
	const f32 rangeY = level.getSkillRangeLengthY();
	const vec2 dir = vec2(cosf(angle), sinf(angle));
	const vec3 origin = player.body->GetWorldPos();
	const bool targetPosition = glm::length(castPos) > 1.0f;

	foreach(it, players) {
		if(it->team == player.team || it->dead) continue;
		const vec3 pos = it->body->GetWorldPos();

		bool hit = false;
		foreach_const(t, targets) {
			const ActorMaster* target = FindMasterActor(*t);
			if(target && target->parent == &*it) hit = true;
		}

		if(!hit) {
			if(targetPosition) {
				const f32 radius = MAX(rangeX * 0.5f, 300.0f);
				hit = glm::length(vec2(pos - castPos)) <= radius;
			}
			else {
				const f32 length = rangeY > 0 ? rangeY : 600.0f;
				const f32 halfWidth = rangeX > 0 ? rangeX * 0.5f + 60.0f : 200.0f;
				hit = InFrontBox(origin, dir, length, halfWidth, pos);
			}
		}

		if(hit) {
			DamagePlayer(player, *it, skillID, damage);
		}
	}
}

void World::DamagePlayer(Player& attacker, Player& target, SkillID skillID, i32 damage, const Projectile* proj)
{
	if(target.dead) return;

	ActorMaster& chara = target.Main();
	chara.hp = MAX(chara.hp - damage, 0);

	LOG("Damage :: %ls -> %ls skill=%d damage=%d hp=%d/%d", attacker.name.data(), target.name.data(), (i32)skillID, damage, chara.hp, chara.hpMax);
	if(proj && proj->remoteID != 0) {
		replication->SendRemoteDamageToAll(proj->remoteID, proj->remoteDoc, attacker.Main().UID, chara.UID, target.body->GetWorldPos(), proj->dir, damage, chara.hp, chara.hpMax);
	}
	else {
		replication->SendDamageToAll(attacker.Main().UID, chara.UID, skillID, damage, chara.hp, chara.hpMax);
	}

	if(chara.hp == 0) {
		target.dead = true;
		target.tRespawn = TimeAdd(localTime, TimeMsToTime(RESPAWN_DELAY_MS));
		LOG("Death :: %ls killed by %ls", target.name.data(), attacker.name.data());
		attacker.kills++;
		target.deaths++;
		i32 teamKills = 0;
		foreach_const(it, players) {
			if(it->team == attacker.team) teamKills += it->kills;
		}
		replication->SendScoreToAll(attacker.userID, attacker.team, attacker.kills, teamKills);
		replication->SendDeathToAll(attacker.Main().UID, chara.UID, CreatureIndex(100000000 + (i32)chara.classType), target.userID, RESPAWN_DELAY_MS);
	}
}

void World::RespawnPlayer(Player& player)
{
	player.dead = false;
	foreach(c, player.characters) {
		(*c)->hp = (*c)->hpMax;
	}

	player.body->collider->setFootPosition(PxExtendedVec3(player.spawnPos.x, player.spawnPos.y, player.spawnPos.z));
	player.body->vel = vec3(0);
	player.input.moveTo = player.spawnPos;

	LOG("Respawn :: %ls", player.name.data());
	replication->SendReviveToAll(player.userID, player.Main().UID, player.Sub().UID, player.spawnPos);
	foreach(c, player.characters) {
		replication->SendHpToAll((*c)->UID, (*c)->hp, (*c)->hpMax);
	}
}

void World::ExecuteSkillProgram(SkillProgram& prog)
{
	// TODO: each skill is executed following a list of commands from ActionBase.xml
	// process them at runtime for now
	// but since they never change, produce logic code from ActionBase.xml

	ActorMaster* caster = FindMasterActor(prog.casterUID);

	const vec2 dir = vec2(cosf(prog.castAngle), sinf(prog.castAngle));

	// access method is kinda convoluted
	const auto& content = GetGameXmlContent();
	const auto& action = content.GetSkillAction(caster->classType, prog.actionID);

	if(TimeDiffSec(TimeDiff(prog.startTime, localTime)) > action.commands[prog.commandID].relativeEndTimeFromStart) {
		prog.commandID++;

		// program is done
		if(prog.commandID >= action.commands.size()) {
			prog.Finish();
			return;
		}
	}
	else {
		// we have not changed command / instruction, nothing to be done
		return;
	}

	bool running = true;
	while(running) {
		running = false;
		const auto& cmd = action.commands[prog.commandID];

		f32 distance = 0;
		f32 moveDuration = 0;

		switch(cmd.type) {
			case ActionCommand::Type::STATE_BLOCK: {
				// lock WASD input type movement during skill execution
				caster->parent->body->lockedMoveUntil = TimeAddSec(localTime, cmd.delay);
			} break
				;
			case ActionCommand::Type::GRAPH_MOVE_HORZ: {
				distance = cmd.graphMoveHorz.distance;
				moveDuration = action.seqLength;
			} break;

			case ActionCommand::Type::MOVE: {
				switch(cmd.move.preset) {
					case ActionCommand::MovePreset::WARP: {
						distance = (f32)cmd.move.param2;
						moveDuration = 0.01f; // warping
					} break;
				}
			} break;
		}

		if(distance != 0) {
			caster->parent->input.moveTo = caster->parent->body->GetWorldPos() + vec3(dir * distance, 0);
			caster->parent->body->vel = vec3(0);
			physics.Move(caster->parent->body, vec3(dir * distance, 0), moveDuration);
		}

		if(cmd.delay == 0) {
			running = true;
			prog.commandID++;

			// program is done
			if(prog.commandID >= action.commands.size()) {
				prog.Finish();
				return;
			}
		}
	}
}
