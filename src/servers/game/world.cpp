#include "world.h"
#include <mxm/game_content.h>

static const f32 COMBAT_ATTACK_STAT = 0.0f; // skill power only comes from in-game ability upgrades (CREATURE_CHARACTER _STAT_UP), none yet
static const i32 STAT_STAMINA = 2;
static const i32 STAT_STAMINAREGEN = 5;
static const i32 STAT_CRITICALRATE = 9;
static const i32 STAT_CRITICALDAMAGE = 10;
static const i32 STAT_ULTIMATE = 37;
static const i32 STAT_ULTIMATEREGEN = 42;
static const i32 STAT_SKILL_CRITICALRATE = 63;
static const i32 STAT_SKILL_CRITICALDAMAGE = 64;
static const u32 PACKET_DAMAGE_TYPE_NORMAL = 0;   // client enum PACKET_DAMAGE_TYPE
static const u32 PACKET_DAMAGE_TYPE_CRITICAL = 2;
static const i32 STAT_ELEMENT_DAMAGE_BONUS = 54;       // weapon damage bonus % (CHARACTER_BASE_STATS)
static const i32 STAT_ELEMENT_SKILL_DAMAGE_BONUS = 55; // skill damage bonus % (CHARACTER_BASE_STATS)

static f32 MasterStat(ClassType classType, i32 statType)
{
	const auto& master = GetGameXmlContent().GetMaster(classType);
	foreach_const(st, master.baseStats) {
		if(st->type == statType) return st->value;
	}
	return 0;
}

// affinity triangle: Argent (red) > Helix (green) > Kinetic (blue) > Argent
static bool ElementBeats(i32 a, i32 b)
{
	return (a == 1 && b == 2) || (a == 2 && b == 3) || (a == 3 && b == 1);
}

static f32 ElementMultiplier(const World::Player& attacker, const World::Player& target, bool skill)
{
	const auto& content = GetGameXmlContent();
	const ClassType ac = attacker.Main().classType;
	const i32 ea = content.GetCreatureElement((i32)content.GetMaster(ac).ID);
	const i32 et = content.GetCreatureElement((i32)content.GetMaster(target.Main().classType).ID);
	const i32 stat = skill ? STAT_ELEMENT_SKILL_DAMAGE_BONUS : STAT_ELEMENT_DAMAGE_BONUS;
	// advantage: attacker deals +bonus% to the element it counters, and takes -bonus% from it
	if(ElementBeats(ea, et)) return 1.0f + MasterStat(ac, stat) / 100.0f;
	if(ElementBeats(et, ea)) return 1.0f - MasterStat(target.Main().classType, stat) / 100.0f;
	return 1.0f;
}

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

		if(!p.dead && p.clientHd == ClientHandle::INVALID && localTime < p.ccUntil) {
			if(localTime < p.pushUntil) {
				p.input.moveTo = p.pushTarget;
				p.input.speed = p.pushSpeed;
			}
			else {
				p.input.moveTo = body.GetWorldPos();
			}
			p.input.tag = 0;
			p.input.jump = 0;
		}

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
	UpdateStatuses();

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
	auto skillFound = content.skillMap.find(skillID);
	if(skillFound == content.skillMap.end()) {
		WARN("PlayerCastSkill :: unknown skill %d", (i32)skillID);
		return;
	}
	const auto& skill = skillFound->second;
	const ActionStateID actionState = skill.action;

	if(GetGameXmlContent().GetSkillConsumeStamina((i32)skillID) > 0 && !PlayerDodge(player, (i32)skillID)) {
		LOG("Dodge :: %ls refused (not enough stamina)", player.name.data());
		return;
	}

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

// weapon overheat (FIRING_METHOD _GAUGE): returns false while the weapon is overheated
static bool WeaponHeatShot(World::Player& player, Time now)
{
	const u32 m = player.mainCharaID & 1;
	const WeaponIndex weapon = m == 0 ? player.mainWeapon : player.subWeapon;
	const WeaponHeatInfo* h = GetGameXmlContent().FindWeaponHeat((i32)weapon);
	if(!h) return true;

	if(player.overheatUntil[m] != Time::ZERO) {
		if(now < player.overheatUntil[m]) return false;
		player.overheatUntil[m] = Time::ZERO;
		player.heat[m] = h->overheatResetGauge;
		player.heatLastShot[m] = now;
	}

	// cooling since the last shot, after the regen delay
	const f32 idle = player.heatLastShot[m] == Time::ZERO ? 999.0f : (f32)TimeDiffSec(TimeDiff(player.heatLastShot[m], now));
	if(idle > h->autoRegenDelay && h->autoRegenInterval > 0) {
		player.heat[m] = MAX(player.heat[m] + h->autoRegen * (idle - h->autoRegenDelay) / h->autoRegenInterval, 0.0f);
	}

	player.heat[m] += h->consumption;
	player.heatLastShot[m] = now;
	if(player.heat[m] >= h->maxGauge) {
		player.heat[m] = h->maxGauge;
		player.overheatUntil[m] = TimeAdd(now, TimeMsToTime(h->overheatDuration * 1000.0));
		LOG("Overheat :: %ls weapon=%d locked %.1fs", player.name.data(), (i32)weapon, h->overheatDuration);
	}
	return true;
}

void World::OnWeaponShotPacket(Player& player, const vec3& origin, const vec2& dir, f32 distance, const u8* rawPacket, i32 rawSize)
{
	if(!WeaponHeatShot(player, localTime)) {
		LOG("Overheat :: %ls shot ignored (weapon overheated)", player.name.data());
		return;
	}

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

void World::UpdateUltimateGauges(f32 delta)
{
	foreach(it, players) {
		if(it->clientHd == ClientHandle::INVALID) continue;
		const ClassType cls = it->Main().classType;

		const f32 ultMax = MAX(MasterStat(cls, STAT_ULTIMATE), 1.0f);
		if(it->ultGauge < 0) it->ultGauge = ultMax;
		it->ultGauge = MIN(it->ultGauge + MasterStat(cls, STAT_ULTIMATEREGEN) * delta, ultMax);

		const f32 staminaMax = MasterStat(cls, STAT_STAMINA);
		if(staminaMax > 0) {
			if(it->stamina < 0) it->stamina = staminaMax;
			it->stamina = MIN(it->stamina + MasterStat(cls, STAT_STAMINAREGEN) * delta, staminaMax);
			if(localTime >= it->tNextStaminaSend) {
				it->tNextStaminaSend = TimeAdd(localTime, TimeMsToTime(250));
				replication->SendStatToAll(it->Main().UID, STAT_STAMINA, it->stamina, staminaMax);
			}
		}

		if(localTime < it->tNextUltSend) continue;
		it->tNextUltSend = TimeAdd(localTime, TimeMsToTime(1000));
		it->ultGaugeSent = it->ultGauge;
		foreach(c, it->characters) {
			replication->SendStatToAll((*c)->UID, (i32)StatType::STAT_TYPE_ULTIMATE, it->ultGauge, ultMax);
		}
	}
}

bool World::PlayerDodge(Player& player, i32 skillID)
{
	// dodge = SKILL_TYPE_SHIRK skill cast (right click), cost SKILL.xml _ConsumeStamina
	const f32 cost = GetGameXmlContent().GetSkillConsumeStamina(skillID);
	LOG("Dodge :: %ls skill=%d stamina=%.0f cost=%.0f", player.name.data(), skillID, player.stamina, cost);
	if(cost <= 0 || player.stamina < 0) return true;
	if(player.stamina < cost) return false;
	player.stamina -= cost;
	player.tNextStaminaSend = Time::ZERO;
	return true;
}

static const f32 STATUS_PUSH_BASE_DISTANCE = 400.0f;
static const f32 ORBIT_RADIUS = 150.0f;

bool World::HasStatusType(const Player& p, const char* type) const
{
	foreach_const(it, activeStatuses) {
		if(it->targetIndex == p.index && it->info && it->info->type == type) return true;
	}
	return false;
}

void World::HealPlayer(Player& healer, Player& target, i32 amount)
{
	if(target.dead || amount <= 0) return;
	ActorMaster& chara = target.Main();
	chara.hp = MIN(chara.hp + amount, chara.hpMax);
	LOG("Heal :: %ls -> %ls amount=%d hp=%d/%d", healer.name.data(), target.name.data(), amount, chara.hp, chara.hpMax);
	replication->SendHpToAll(chara.UID, chara.hp, chara.hpMax);
}

bool World::RemoteCanAffect(const RemoteInst& inst, const Player& p, bool includeOwner) const
{
	if(p.dead) return false;
	if(inst.excludeIndex >= 0 && (i32)p.index == inst.excludeIndex) return false;
	const Player& owner = players[inst.ownerIndex];
	const Remote& remote = *inst.remote;
	const auto group = remote.damageGroup;
	const bool friendContact = group == Remote::DamageGroup::eFRIEND || group == Remote::DamageGroup::eALL || remote.contactFriend;
	if(p.index == owner.index) {
		if(!includeOwner && !remote.vsOwner) return false;
		return remote.vsOwner || group == Remote::DamageGroup::eFRIEND || group == Remote::DamageGroup::eALL;
	}
	const bool enemy = p.team != owner.team;
	if(enemy) return group == Remote::DamageGroup::eENEMY || group == Remote::DamageGroup::eALL;
	return friendContact;
}

void World::ApplyStatus(Player& caster, Player& target, i32 statusID, f32 power, f32 durationOverride)
{
	if(target.dead) return;
	const StatusInfo* info = GetGameXmlContent().FindStatus(statusID);
	f32 duration = durationOverride > 0 ? durationOverride : (info && info->durationSec > 0 ? info->durationSec : 1.0f);

	const bool displacement = info && (info->type == "STATUS_TYPE_PUSH" || info->type == "STATUS_TYPE_FORCEBACK" ||
		info->type == "STATUS_TYPE_KNOCKDOWN" || info->type == "STATUS_TYPE_AIRBORNE" || info->type == "STATUS_TYPE_DRAGGED");
	if(displacement && HasStatusType(target, "STATUS_TYPE_SUPERARMOR")) {
		LOG("Status :: %ls resisted %d (superarmor)", target.name.data(), statusID);
		return;
	}

	foreach(it, activeStatuses) {
		if(it->statusID == statusID && it->targetIndex == target.index) {
			const Time until = TimeAdd(localTime, TimeMsToTime(duration * 1000.0));
			if(until > it->until) it->until = until;
			if(info && it->overlap < info->maxOverlap) it->overlap++;
			LOG("Status :: %ls -> %ls status=%d refreshed overlap=%d", caster.name.data(), target.name.data(), statusID, it->overlap);
			const vec3 pos = target.body->GetWorldPos();
			replication->SendAddStatusToAll(statusID, it->targetUID, it->casterUID, duration, pos, pos, 0);
			return;
		}
	}
	if(activeStatuses.full()) return;

	ActiveStatus st;
	st.statusID = statusID;
	st.info = info;
	st.targetIndex = target.index;
	st.casterIndex = caster.index;
	st.targetUID = target.Main().UID;
	st.casterUID = caster.Main().UID;
	st.until = TimeAdd(localTime, TimeMsToTime(MAX(duration, 0.1f) * 1000.0));
	st.nextTick = TimeAdd(localTime, TimeMsToTime((info && info->intervalSec > 0 ? info->intervalSec : 1.0f) * 1000.0));
	st.overlap = 1;
	st.shieldHP = (info && info->type == "STATUS_TYPE_SHIELD") ? MAX(power, 100.0f) * (info->paramRatio > 0 ? info->paramRatio : 1.0f) : 0;
	activeStatuses.push_back(st);

	if(info && (info->kind == StatusInfo::Kind::HARD_CC || info->kind == StatusInfo::Kind::ROOT) && st.until > target.ccUntil) {
		target.ccUntil = st.until;
	}

	const vec3 from = target.body->GetWorldPos();
	vec3 to = from;
	const f32 graphDuration = duration * (info && info->timeRatio > 0 ? info->timeRatio : 1.0f);
	if(info && displacement && info->type != "STATUS_TYPE_AIRBORNE") {
		vec2 dir = vec2(from - caster.body->GetWorldPos());
		const f32 len = glm::length(dir);
		dir = len > 1.0f ? dir / len : vec2(1, 0);
		if(info->type == "STATUS_TYPE_DRAGGED") {
			to = caster.body->GetWorldPos() + vec3(dir * (info->dragOffset > 0 ? info->dragOffset : 200.0f), 0);
		}
		else {
			const f32 dist = info->type == "STATUS_TYPE_FORCEBACK" && info->distance > 0 ? info->distance :
				STATUS_PUSH_BASE_DISTANCE * (info->distanceRatio > 0 ? info->distanceRatio : 1.0f);
			to = from + vec3(dir * dist, 0);
		}
		if(target.clientHd == ClientHandle::INVALID && graphDuration > 0) {
			target.pushTarget = to;
			target.pushSpeed = glm::length(vec2(to - from)) / graphDuration;
			target.pushUntil = TimeAdd(localTime, TimeMsToTime(graphDuration * 1000.0));
			if(target.pushUntil > target.ccUntil) target.ccUntil = target.pushUntil;
		}
	}

	LOG("Status :: %ls -> %ls status=%d type=%s duration=%.2f shield=%.0f", caster.name.data(), target.name.data(), statusID, info ? info->type.c_str() : "?", duration, st.shieldHP);
	replication->SendAddStatusToAll(statusID, st.targetUID, st.casterUID, duration, from, to, graphDuration);
}

void World::UpdateStatuses()
{
	for(auto ps = pendingStatuses.begin(); ps != pendingStatuses.end(); ) {
		if(localTime < ps->time) { ++ps; continue; }
		Player& caster = players[ps->casterIndex];
		const ActorMaster* targetMaster = ps->targetUID != ActorUID::INVALID ? FindMasterActor(ps->targetUID) : nullptr;
		if(!caster.dead && targetMaster) {
			ApplyStatus(caster, *targetMaster->parent, ps->statusID, ps->power, ps->duration);
		}
		ps = pendingStatuses.erase_unsorted(ps);
	}

	for(auto it = activeStatuses.begin(); it != activeStatuses.end(); ) {
		Player& target = players[it->targetIndex];
		if(it->info && it->info->dotDamage > 0 && it->info->intervalSec > 0) {
			while(localTime >= it->nextTick && localTime < it->until && !target.dead) {
				it->nextTick = TimeAdd(it->nextTick, TimeMsToTime(it->info->intervalSec * 1000.0));
				DamagePlayer(players[it->casterIndex], target, SkillID::INVALID, (i32)(it->info->dotDamage * it->overlap));
			}
		}

		const bool shieldBroken = it->info && it->info->type == "STATUS_TYPE_SHIELD" && it->shieldHP <= 0;
		if(localTime >= it->until || target.dead || shieldBroken) {
			replication->SendRemoveStatusToAll(it->statusID, it->targetUID, it->casterUID);
			it = activeStatuses.erase_unsorted(it);
		}
		else {
			++it;
		}
	}
}

bool World::ScheduleSkillRemotes(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 angle, i32 forceAnim)
{
	const auto& content = GetGameXmlContent();
	auto skillFound = content.skillMap.find(skillID);
	if(skillFound == content.skillMap.end()) return false;
	const auto& skill = skillFound->second;
	const auto& action = content.GetSkillAction(player.Main().classType, skill.action);

	i32 anim = 0;
	if(player.lastComboSkill == skillID && TimeDiffSec(TimeDiff(player.lastComboTime, localTime)) < 3.0) {
		foreach_const(cmd, action.commands) {
			if(cmd->animIndex == 1 && cmd->type == ActionCommand::Type::REMOTE) anim = 1;
		}
	}
	if(forceAnim >= 0) {
		anim = forceAnim;
	}
	else {
		player.lastComboSkill = anim == 0 ? skillID : SkillID::INVALID;
		player.lastComboTime = localTime;
	}

	{
		SkillNormalModel& skillModel = const_cast<SkillNormalModel&>(skill);
		const f32 consumeUG = skillModel.getSkillNormalLevelByIndex(0)->getConsumeUG();
		if(consumeUG > 0 && forceAnim < 0) {
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
		if((cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET) && !pendingRemotes.full()) {
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
			pr.atTarget = cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET;
			pr.nodeName = cmd->nodeName;
			pendingRemotes.push_back(pr);
			scheduled = true;
		}
		if((cmd->type == ActionCommand::Type::STATUS || cmd->type == ActionCommand::Type::STATUS_SKILL_TARGET) && cmd->status.statusIndex != 0 && !pendingStatuses.full()) {
			PendingStatus ps;
			ps.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			ps.statusID = cmd->status.statusIndex;
			ps.casterIndex = player.index;
			const auto preset = cmd->status.targetPreset;
			const bool self = preset == ActionCommand::TargetPreset::SELF_FFF || preset == ActionCommand::TargetPreset::SELF_FFF_1 ||
				preset == ActionCommand::TargetPreset::SELF_TFF || preset == ActionCommand::TargetPreset::SELF_TTF || preset == ActionCommand::TargetPreset::SELF_TTT;
			ps.targetUID = self ? player.Main().UID : currentTarget;
			ps.duration = cmd->status.removeAtEnd ? MAX(action.seqLength - t, 0.3f) : -1.0f;
			ps.power = (f32)damage;
			if(ps.targetUID != ActorUID::INVALID) pendingStatuses.push_back(ps);
		}
		t += cmd->delay;
	}

	LOG("Skill :: %ls skill=%d anim=%d scheduled=%d damage=%d castPos=(%.0f, %.0f)", player.name.data(), (i32)skillID, anim, scheduled, damage, castPos.x, castPos.y);
	return scheduled;
}

World::RemoteInst* World::SpawnRemote(u32 ownerIndex, RemoteIdx idx, const vec3& pos, const vec2& dir, ActorUID target, SkillID skillID, u32 castID, i32 damage, bool broadcast, const vec3* aimPos, u16 nodeName)
{
	const Remote* remote = GetGameXmlContent().FindRemote(idx);
	if(!remote || remoteInsts.full()) return nullptr;

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
	// lobbed remotes (bezier graph) land on the aimed point, clamped to their max reach
	if(remote->behaviorType == Remote::BehaviorType::GRAPH && remote->fixedGraphLength <= 0 && aimPos && glm::length(vec2(*aimPos)) > 1.0f) {
		const f32 aimDist = glm::length(vec2(*aimPos) - vec2(pos));
		inst.range = inst.range > 0 ? MIN(aimDist, inst.range) : aimDist;
	}
	inst.spawnTime = localTime;
	inst.target = target;
	inst.hitsLeft = MAX(remote->activateCount, 1);
	inst.areaDone = false;
	if(remote->timeEventDelay >= 0) {
		inst.timeEventActive = true;
		inst.nextTimeEvent = TimeAdd(localTime, TimeMsToTime(remote->timeEventDelay * 1000.0));
	}

	if(broadcast) {
		const u32 seed = nextRemoteSeed;
		nextRemoteSeed += 2;
		inst.clientRemoteID = seed + 1;
		vec3 targetPos = pos + vec3(dir * (inst.range > 0 ? inst.range : 1000.0f), 0);
		const ActorMaster* targetMaster = target != ActorUID::INVALID ? FindMasterActor(target) : nullptr;
		if(targetMaster) targetPos = targetMaster->parent->body->GetWorldPos();
		if(remote->behaviorType == Remote::BehaviorType::GRAPH && aimPos && glm::length(vec2(*aimPos)) > 1.0f) {
			targetPos = pos + vec3(dir * inst.range, 0);
			targetPos.z = aimPos->z;
		}
		// the client draws plain (non-fan) static boxes facing the opposite way of their rotation
		const bool plainStaticBox = inst.speed <= 0 && remote->boundType == Remote::BoundType::E_BOUND_BOX && remote->angle <= 0;
		const f32 sendYaw = atan2f(dir.y, dir.x) + (plainStaticBox ? PI : 0.0f);
		replication->SendRemoteCreateToAll(players[ownerIndex].Main().UID, seed, idx, pos, sendYaw, targetPos, targetMaster ? target : ActorUID::INVALID, GetGameXmlContent().GetNodeName(nodeName));
	}

	remoteInsts.push_back(inst);

	LOG("Remote :: spawn %d at (%.0f, %.0f) dir=(%.2f, %.2f) speed=%.0f range=%.0f", (i32)idx, pos.x, pos.y, dir.x, dir.y, inst.speed, inst.range);
	return &remoteInsts.back();
}

static vec2 RotateDir(const vec2& d, f32 deg)
{
	const f32 a = deg * PI / 180.0f;
	const f32 c = cosf(a), s = sinf(a);
	return vec2(d.x * c - d.y * s, d.x * s + d.y * c);
}

struct WorldRemoteSpawner
{
	static World::RemoteInst* Spawn(World& w, u32 ownerIndex, RemoteIdx idx, const vec3& pos, const vec2& dir, ActorUID target, SkillID skillID, u32 castID, i32 damage)
	{
		return w.SpawnRemote(ownerIndex, idx, pos, dir, target, skillID, castID, damage);
	}
};

static void SpawnRemoteRules(World& world, const World::RemoteInst& parent, const Remote::SpawnRules& rules, const vec3& pos, i32 excludeIndex, RemoteIdx skipFirst)
{
	bool skipped = false;
	foreach_const(rule, rules) {
		if(!skipped && rule->kind == Remote::SPAWN_SINGLE && rule->idx == skipFirst) {
			skipped = true;
			continue;
		}
		const Remote* child = GetGameXmlContent().FindRemote(rule->idx);
		if(!child) continue;

		for(i32 n = 0; n < rule->maxNum; n++) {
			vec2 dir = parent.dir;
			vec3 at = pos;
			switch(rule->kind) {
				case Remote::SPAWN_RADIATE: {
					if(rule->maxNum > 1 && rule->maxAngle > 0) {
						if(rule->maxAngle >= 360.0f) dir = RotateDir(parent.dir, 360.0f * n / rule->maxNum);
						else dir = RotateDir(parent.dir, -rule->maxAngle * 0.5f + rule->maxAngle * n / (rule->maxNum - 1));
					}
				} break;
				case Remote::SPAWN_CIRCLE: {
					const f32 a = (RandUint() % 3600) * 0.1f * PI / 180.0f;
					const f32 r = rule->radius * sqrtf((RandUint() % 1000) / 1000.0f);
					at = pos + vec3(cosf(a) * r, sinf(a) * r, 0);
				} break;
				case Remote::SPAWN_DEFAULT: {
					dir = RotateDir(parent.dir, rule->angleY);
				} break;
			}
			World::RemoteInst* inst = WorldRemoteSpawner::Spawn(world, parent.ownerIndex, rule->idx, at + vec3(dir * child->offsetY, 0), dir, parent.target, parent.skillID, parent.castID, parent.damage);
			if(inst) inst->excludeIndex = excludeIndex;
		}
	}
}

void World::RemoteHit(RemoteInst& inst, Player& target)
{
	const Remote& remote = *inst.remote;
	Player& owner = players[inst.ownerIndex];
	if(!RemoteCanAffect(inst, target, true)) return;
	const bool enemy = target.team != owner.team;

	const i32 amount = (i32)(inst.damage * remote.attackMultiplier);
	if(enemy && remote.memorizeTarget) owner.memorizedTarget = target.Main().UID;
	if(amount <= 0 && remote.statuses.empty() && remote.friendlyStatuses.empty() && remote.onActivate.empty() && remote.onHitCharacter.empty()) return;

	// damaging hits share one hit window per cast (a target takes a skill's damage once per window),
	// effect-only hits keep a window per remote so lingering zones keep applying their effects
	const RemoteIdx hitKey = amount > 0 ? RemoteIdx::INVALID : remote.ID;
	foreach(h, castHits) {
		if(h->castID == inst.castID && h->remoteID == hitKey && h->playerIndex == target.index && localTime < h->until) return;
	}
	if(!castHits.full()) {
		CastHit h;
		h.remoteID = hitKey;
		h.castID = inst.castID;
		h.playerIndex = target.index;
		h.until = TimeAdd(localTime, TimeMsToTime(MAX(remote.hitInvalidTime, 0.1f) * 1000.0));
		castHits.push_back(h);
	}

	if(amount > 0) {
		if(enemy) DamagePlayer(owner, target, inst.skillID, amount, nullptr, inst.clientRemoteID, remote.ID, inst.dir);
		else if(remote.damageGroup == Remote::DamageGroup::eFRIEND) HealPlayer(owner, target, amount);
	}

	inst.lastHitIndex = (i32)target.index;
	if(!target.dead) {
		const bool friendGroup = remote.damageGroup == Remote::DamageGroup::eFRIEND;
		if(enemy || friendGroup) {
			foreach_const(st, remote.statuses) {
				if(st->rate >= 100 || (i32)(RandUint() % 100) < st->rate) ApplyStatus(owner, target, st->statusID, (f32)inst.damage);
			}
		}
		if(!enemy) {
			foreach_const(st, remote.friendlyStatuses) {
				if(st->rate >= 100 || (i32)(RandUint() % 100) < st->rate) ApplyStatus(owner, target, st->statusID, (f32)inst.damage);
			}
		}
	}

	if(!remote.onActivate.empty() || !remote.onHitCharacter.empty()) {
		const RemoteInst copy = inst;
		const vec3 at = vec3(vec2(target.body->GetWorldPos()), inst.pos.z);
		SpawnRemoteRules(*this, copy, remote.onActivate, at, -1, RemoteIdx::INVALID);
		SpawnRemoteRules(*this, copy, remote.onHitCharacter, at, (i32)target.index, RemoteIdx::INVALID);
	}

	if(enemy) {
		const i32 hitEventRemote = GetGameXmlContent().FindSkillHitEventRemote((i32)inst.skillID);
		if(hitEventRemote != 0 && hitEventRemote == (i32)remote.ID &&
		   eastl::find(comboTriggeredCasts.begin(), comboTriggeredCasts.end(), inst.castID) == comboTriggeredCasts.end()) {
			if(comboTriggeredCasts.full()) comboTriggeredCasts.erase(comboTriggeredCasts.begin());
			comboTriggeredCasts.push_back(inst.castID);
			owner.memorizedTarget = target.Main().UID;
			const ActorUID targetUID = target.Main().UID;
			const vec2 rel = vec2(target.body->GetWorldPos() - owner.body->GetWorldPos());
			LOG("ComboHit :: %ls skill=%d remote=%d -> next part", owner.name.data(), (i32)inst.skillID, (i32)remote.ID);
			ScheduleSkillRemotes(owner, inst.skillID, target.body->GetWorldPos(), Slice<const ActorUID>(&targetUID, 1), atan2f(rel.y, rel.x), 1);
		}
	}
}

void World::RemoteApplyShape(RemoteInst& inst)
{
	const Remote& remote = *inst.remote;
	f32 halfWidth = remote.boundSize[0] * 0.5f;
	f32 start = 0;
	f32 length = inst.range > 0 ? inst.range : remote.boundSize[1];
	switch(remote.boundType) {
		case Remote::BoundType::E_BOUND_CAPSULE: {
			length = remote.boundSize[1] + remote.boundSize[0];
			start = -remote.boundSize[0] * 0.5f;
		} break;
		case Remote::BoundType::E_BOUND_HAMMER:
		case Remote::BoundType::E_BOUND_BEAM:
		case Remote::BoundType::E_BOUND_BEAM_NIF:
		case Remote::BoundType::E_BOUND_LASER:
		case Remote::BoundType::E_BOUND_RAY: {
			if(length <= 0) length = MAX((f32)remote.boundSize[0], 300.0f);
		} break;
		default: return;
	}
	if(length <= 0) return;

	foreach(it, players) {
		const vec3 pos = it->body->GetWorldPos();
		if(fabsf(pos.z - inst.pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
		const vec2 center = vec2(inst.pos) + inst.dir * (start + length * 0.5f);
		const vec2 rel = LagClosestPos(*it, center) - vec2(inst.pos);
		const f32 along = glm::dot(rel, inst.dir);
		const f32 side = fabsf(glm::dot(rel, Perp(inst.dir)));
		if(along < start - WEAPON_TARGET_RADIUS || along > start + length + WEAPON_TARGET_RADIUS) continue;
		if(side > halfWidth + WEAPON_TARGET_RADIUS) continue;
		RemoteHit(inst, *it);
	}
}

void World::RemoteApplyArea(RemoteInst& inst)
{
	const Remote& remote = *inst.remote;
	if(remote.boundType != Remote::BoundType::E_BOUND_SPHERE && remote.boundType != Remote::BoundType::E_BOUND_BOX) {
		RemoteApplyShape(inst);
		return;
	}
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
		if(pr->atTarget) {
			const ActorMaster* targetMaster = pr->target != ActorUID::INVALID ? FindMasterActor(pr->target) : nullptr;
			if(targetMaster) pos = targetMaster->parent->body->GetWorldPos();
			else if(glm::length(vec2(pr->castPos)) > 1.0f) pos = vec3(vec2(pr->castPos), owner.body->GetWorldPos().z);
		}

		if(!owner.dead) {
			SpawnRemote(pr->ownerIndex, pr->idx, pos, dir, pr->target, pr->skillID, pr->castID, pr->damage, true, &pr->castPos, pr->nodeName);
		}
		pr = pendingRemotes.erase_unsorted(pr);
	}

	for(int i = 0; i < (i32)remoteInsts.size(); ) {
		RemoteInst& inst = remoteInsts[i];
		const Remote& remote = *inst.remote;
		const f32 elapsed = (f32)TimeDiffSec(TimeDiff(inst.spawnTime, localTime));
		bool destroyed = false;
		Player& instOwner = players[inst.ownerIndex];

		const bool attached = remote.attachFireObject || remote.behaviorType == Remote::BehaviorType::ORBIT ||
			remote.behaviorType == Remote::BehaviorType::CASTER_TARGET || remote.behaviorType == Remote::BehaviorType::BEAM;
		if(attached) {
			if(instOwner.dead) {
				destroyed = true;
			}
			else {
				const f32 yaw = instOwner.input.rot.upperYaw;
				inst.dir = vec2(cosf(yaw), sinf(yaw));
				inst.pos = instOwner.body->GetWorldPos() + vec3(inst.dir * remote.offsetY, 158.0f);
			}
		}

		if(!destroyed && inst.timeEventActive && localTime >= inst.nextTimeEvent) {
			if(remote.timeEventRecycle && remote.timeEventDelay > 0.01f) inst.nextTimeEvent = TimeAdd(inst.nextTimeEvent, TimeMsToTime(remote.timeEventDelay * 1000.0));
			else inst.timeEventActive = false;
			const RemoteInst copy = inst;
			SpawnRemoteRules(*this, copy, remote.onTime, copy.pos, copy.excludeIndex, RemoteIdx::INVALID);
		}

		if(destroyed) {
		}
		else if(remote.behaviorType == Remote::BehaviorType::CHAIN) {
			if(!inst.areaDone) {
				inst.areaDone = true;
				const f32 sight = remote.sight > 0 ? remote.sight : 1450.0f;
				Player* best = nullptr;
				f32 bestDist = sight + WEAPON_TARGET_RADIUS;
				foreach(it, players) {
					if(!RemoteCanAffect(inst, *it, remote.vsOwner)) continue;
					const f32 d = glm::length(vec2(it->body->GetWorldPos()) - vec2(inst.pos));
					if(d < bestDist) { bestDist = d; best = &*it; }
				}
				if(best) {
					inst.dir = glm::length(vec2(best->body->GetWorldPos() - inst.pos)) > 1.0f ? glm::normalize(vec2(best->body->GetWorldPos() - inst.pos)) : inst.dir;
					inst.pos = vec3(vec2(best->body->GetWorldPos()), inst.pos.z);
					RemoteHit(inst, *best);
					inst.excludeIndex = (i32)best->index;
				}
				else {
					inst.chainEnded = true;
					destroyed = true;
				}
			}
		}
		else if(attached) {
			if(remote.behaviorType == Remote::BehaviorType::ORBIT) {
				const f32 radius = remote.boundSize[0] * 0.5f + ORBIT_RADIUS;
				foreach(it, players) {
					if(!RemoteCanAffect(inst, *it, false)) continue;
					if(glm::length(vec2(it->body->GetWorldPos()) - vec2(instOwner.body->GetWorldPos())) <= radius + WEAPON_TARGET_RADIUS) RemoteHit(inst, *it);
				}
			}
			else if(remote.boundType != Remote::BoundType::E_BOUND_NONE && remote.damageGroup != Remote::DamageGroup::eNONE) {
				RemoteApplyArea(inst);
			}
		}
		else switch(remote.behaviorType) {
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
					if(remote.damageGroup != Remote::DamageGroup::eNONE && remote.damageGroup != Remote::DamageGroup::INVALID) {
						foreach(it, players) {
							if(!RemoteCanAffect(inst, *it, false)) continue;
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
				else if(remote.activateCount > 1 && remote.lifeTime > 0) {
					// lingering zone: hits again once each target's hit window expires
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
			if(!copy.chainEnded) {
				const i32 childExclude = remote.behaviorType == Remote::BehaviorType::CHAIN ? copy.lastHitIndex : copy.excludeIndex;
				if(next != RemoteIdx::INVALID) {
					const Remote* nextRemote = GetGameXmlContent().FindRemote(next);
					const f32 nextOffset = nextRemote ? nextRemote->offsetY : 0;
					RemoteInst* child = SpawnRemote(copy.ownerIndex, next, copy.pos + vec3(copy.dir * nextOffset, 0), copy.dir, copy.target, copy.skillID, copy.castID, copy.damage, copy.destroyedByHit && copy.clientRemoteID != 0);
					if(child) child->excludeIndex = childExclude;
				}
				SpawnRemoteRules(*this, copy, remote.onDestroy, copy.pos, childExclude, next);
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

void World::DamagePlayer(Player& attacker, Player& target, SkillID skillID, i32 damage, const Projectile* proj, u32 remoteID, RemoteIdx remoteDoc, vec2 remoteDir)
{
	if(target.dead) return;
	if(HasStatusType(target, "STATUS_TYPE_GODMODE")) return;

	// element advantage: weapon shots use the weapon bonus, skills the skill bonus, DoTs none
	u32 damageType = PACKET_DAMAGE_TYPE_NORMAL;
	if(proj || skillID != SkillID::INVALID) {
		const f32 mul = ElementMultiplier(attacker, target, !proj);
		if(mul != 1.0f) damage = (i32)(damage * mul);

		// critical hit: master crit stats (weapon or skill) + the remote's own _CriticalRate
		const ClassType ac = attacker.Main().classType;
		const RemoteIdx doc = proj ? proj->remoteDoc : remoteDoc;
		const Remote* critRemote = doc != RemoteIdx::INVALID ? GetGameXmlContent().FindRemote(doc) : nullptr;
		const f32 rate = MasterStat(ac, proj ? STAT_CRITICALRATE : STAT_SKILL_CRITICALRATE) + (critRemote ? critRemote->criticalRate : 0);
		if(rate > 0 && (RandUint() % 10000) < (u32)(rate * 100.0f)) {
			const f32 critDamage = MasterStat(ac, proj ? STAT_CRITICALDAMAGE : STAT_SKILL_CRITICALDAMAGE);
			damage = (i32)(damage * (critDamage > 0 ? critDamage : 100.0f) / 100.0f);
			damageType = PACKET_DAMAGE_TYPE_CRITICAL;
		}
	}

	foreach(it, activeStatuses) {
		if(damage <= 0) break;
		if(it->targetIndex != target.index || it->shieldHP <= 0) continue;
		const f32 absorbed = MIN(it->shieldHP, (f32)damage);
		it->shieldHP -= absorbed;
		damage -= (i32)absorbed;
	}
	if(damage <= 0) return;

	ActorMaster& chara = target.Main();
	chara.hp = MAX(chara.hp - damage, HasStatusType(target, "STATUS_TYPE_NEARLY_IMMORTAL") ? 1 : 0);

	LOG("Damage :: %ls -> %ls skill=%d damage=%d%s hp=%d/%d", attacker.name.data(), target.name.data(), (i32)skillID, damage, damageType == PACKET_DAMAGE_TYPE_CRITICAL ? " CRIT" : "", chara.hp, chara.hpMax);
	if(proj && proj->remoteID != 0) {
		replication->SendRemoteDamageToAll(proj->remoteID, proj->remoteDoc, attacker.Main().UID, chara.UID, target.body->GetWorldPos(), proj->dir, damage, chara.hp, chara.hpMax, damageType);
	}
	else if(remoteID != 0) {
		replication->SendRemoteDamageToAll(remoteID, remoteDoc, attacker.Main().UID, chara.UID, target.body->GetWorldPos(), remoteDir, damage, chara.hp, chara.hpMax, damageType);
	}
	else {
		replication->SendDamageToAll(attacker.Main().UID, chara.UID, skillID, damage, chara.hp, chara.hpMax, damageType);
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
