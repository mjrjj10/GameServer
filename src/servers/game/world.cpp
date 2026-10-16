#include "config.h"
#include <chrono>
#include "world.h"
#include <mxm/game_content.h>

static const f32 COMBAT_ATTACK_STAT = 0.0f; // skill power only comes from in-game ability upgrades (CREATURE_CHARACTER _STAT_UP), none yet
static const i32 STAT_STAMINA = 2;
static const i32 STAT_STAMINAREGEN = 5;
static const i32 STAT_CRITICALRATE = 9;
static const i32 STAT_CRITICALDAMAGE = 10;
static const i32 STAT_MOVESPEED = 14;
static const i32 STAT_MANA = 35;
// secondary resources: max stat, regen stat (per second), starts full
struct ResourceDef { i32 maxStat; i32 regenStat; bool startFull; };
static const ResourceDef RESOURCES[3] = {
	{ 17, 41, true },   // STAT_TYPE_ENERGY / ENERGYREGEN
	{ 36, 40, false },  // STAT_TYPE_RAGE / RAGEREGEN (negative: decays), gained by hits
	{ 56, 57, true },   // STAT_TYPE_BUBBLE / BUBBLEREGEN (charges)
};
static const i32 STAT_RAGE_ATTACK_RECOVERY_VALUE = 51;
static const i32 STAT_MANAREGEN = 39;
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
			// mana and master resources belong to each master
			eastl::swap(p.mana, p.benchMana);
			eastl::swap(p.res, p.benchRes);
			p.tNextManaSend = Time::ZERO;
			p.resSent = { -1, -1, -1 };
			p.tNextResSend = Time::ZERO;
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
	UpdateSummons();
	UpdateSummonAi((f32)tdelta);
	if(Config().DevShowcase) { if(!frozen) UpdateShowcase(); }
	else {
		if(!frozen) UpdateBotCombat();
		if(!frozen) UpdateBotSkills();
	}

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
		rfl.ownerUID = actor.ownerUID;


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

void World::PlayerCastSkill(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 clientTime, ActionStateID forcedAction)
{
	if(frozen) return;
	// TODO: check if can cast

	// access method is kinda convoluted
	const auto& content = GetGameXmlContent();
	auto skillFound = content.skillMap.find(skillID);
	if(skillFound == content.skillMap.end()) {
		WARN("PlayerCastSkill :: unknown skill %d", (i32)skillID);
		return;
	}
	const auto& skill = skillFound->second;
	const bool followUp = forcedAction != ActionStateID::INVALID;
	const ActionStateID actionState = followUp ? forcedAction : skill.action;

	if(!followUp) {
		const bool shirk = GetGameXmlContent().GetSkillConsumeStamina((i32)skillID) > 0;
		if(IsBlocked(player, false, !shirk, shirk)) {
			LOG("Blocked :: %ls skill=%d (input blocked by a status)", player.name.data(), (i32)skillID);
			return;
		}
		if(!PlayerPaySkillCost(player, (i32)skillID)) return;
		PassiveOnSkillCast(player, (i32)skillID);
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

	if(!ScheduleSkillRemotes(player, skillID, castPos, targets, angle, followUp ? 0 : -1, forcedAction) && !followUp) {
		PlayerSkillDamage(player, skillID, castPos, targets, angle);
	}

	const i32 child = content.GetChildSkill(player.Main().classType, actionState);
	if(child > 0) {
		player.pendingChildSkill = child;
		player.pendingChildSkillUntil = TimeAdd(localTime, TimeMsToTime(2000));
	}
}

void World::PlayerWeaponFire(Player& player)
{
	const f32 range = ClassAttackRange(player.Main().classType) * WEAPON_RANGE_SCALE;
	const vec2 dir = vec2(cosf(player.weaponAimYaw), sinf(player.weaponAimYaw));
	const vec3 origin = player.body->GetWorldPos();

	if(ClassIsMelee(player.Main().classType)) {
		foreach(it, players) {
			if(it->team == player.team || it->dead || it->untargetable) continue;
			const vec3 pos = it->body->GetWorldPos();
			if(fabsf(pos.z - origin.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
			const vec2 rel = LagClosestPos(*it, vec2(origin)) - vec2(origin);
			const f32 dist = glm::length(rel);
			if(dist > range + HitRadius(*it) + WEAPON_HIT_TOLERANCE) continue;
			if(dist > HitRadius(*it)) {
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

	f32 aimMax = WEAPON_SHOT_MAX_DISTANCE;
	// FIRING_METHOD of the active weapon: combo step / charge level / every Nth shot / status replacement
	{
		const u32 m = player.mainCharaID & 1;
		const WeaponIndex weapon = m == 0 ? player.mainWeapon : player.subWeapon;
		const WeaponFiring* wf = content.FindWeaponFiring((i32)weapon);
		if(wf) {
			if(wf->maxDistance > 0) aimMax = wf->maxDistance;
			RemoteIdx idx = wf->base;
			if(player.chargeLevel > 0 && player.chargeLevel <= wf->charge.size()) idx = wf->charge[player.chargeLevel - 1];
			else if(player.firingCombo > 0 && player.firingCombo <= wf->combo.size()) idx = wf->combo[player.firingCombo - 1];
			player.shotCount[m]++;
			if(wf->countRate > 0 && wf->countRemote != RemoteIdx::INVALID && player.shotCount[m] % wf->countRate == 0) idx = wf->countRemote;
			if(wf->replaceStatus != 0 && wf->replaceRemote != RemoteIdx::INVALID && FindActiveStatus(player.index, wf->replaceStatus)) {
				idx = wf->replaceRemote;
				ConsumeStatus(player.index, wf->replaceStatus);
				LOG("Passive :: %ls weapon status %d used -> remote %d", player.name.data(), wf->replaceStatus, (i32)idx);
			}
			const Remote* r = content.FindRemote(idx);
			if(r) remote = r;
			LOG("Shot :: %ls firing remote %d (combo %d charge %d count %d)", player.name.data(), (i32)idx, player.firingCombo, player.chargeLevel, player.shotCount[m]);
		}
	}

	// the client picks the weapon remote itself (firing combo step, charge level, status replacement such as
	// passives or stances): look for a weapon remote document index in the fire packet after the fire node name
	if(player.pendingShotRawSize > 44) {
		const u8* raw = player.pendingShotRaw.data();
		const i32 nameLen = *(const u16*)(raw + 42);
		for(i32 off = 44 + nameLen * 2; off + 4 <= player.pendingShotRawSize; off++) {
			const u32 v = *(const u32*)(raw + off);
			if(v < 1430000000 || v >= 1440000000) continue;
			const Remote* fired = content.FindRemote(RemoteIdx(v));
			if(!fired) continue;
			if(!remote || fired->ID != remote->ID) LOG("Shot :: %ls client weapon remote %u (offset %d)", player.name.data(), v, off);
			remote = fired;
			break;
		}
		static i32 dumpCount = 0;
		if(dumpCount < 6) {
			dumpCount++;
			char hex[600] = {};
			i32 n = 0;
			for(i32 i = 44 + nameLen * 2; i < player.pendingShotRawSize && n < 590; i++) { static const char* H = "0123456789abcdef"; hex[n++] = H[raw[i] >> 4]; hex[n++] = H[raw[i] & 15]; }
			LOG("Shot :: fire packet tail (%d bytes, name %d) %s", player.pendingShotRawSize, nameLen, hex);
		}
	}
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

	proj.range = graph ? (reliable ? MIN(distance, aimMax) : classRange) : remoteRange;
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
	else if(player.clientHd == ClientHandle::INVALID && remote) {
		// bot shot: no client packet to relay, create the weapon remote for everyone
		const u32 seed = nextRemoteSeed;
		nextRemoteSeed += 2;
		proj.remoteID = seed + 1;
		const vec3 firePos = proj.pos + vec3(0, 0, 158.0f);
		const vec3 target = firePos + vec3(proj.dir * proj.range, 0);
		replication->SendRemoteCreateToAll(player.Main().UID, seed, remote->ID, firePos, atan2f(proj.dir.y, proj.dir.x), target, ActorUID::INVALID, L"Attack_Fire_Dummy");
	}
	// weapon damage scales with attack: percent attack modifiers of active statuses (skilltt STAT_TYPE_ATTACK MUL)
	proj.damage = (i32)(WEAPON_HIT_DAMAGE * MAX(1.0f + StatusModMulSum(player, StatusMod::ATTACK) / 100.0f, 0.0f));
	LOG("Shot :: %ls class=%d remote=%d graph=%d reliable=%d dist=%.0f range=%.0f speed=%.0f width=%.0f hits=%d aoe=%.0f", player.name.data(), (i32)classType, remote ? (i32)remote->ID : -1, graph, reliable, distance, proj.range, proj.speed, proj.halfWidth, proj.hitsLeft, proj.aoeRadius);
	projectileList.push_back(proj);
}

void World::Explode(Player& owner, const vec3& center, f32 radius, i32 damage)
{
	LOG("Explosion :: at (%.0f, %.0f) radius=%.0f", center.x, center.y, radius);
	foreach(it, players) {
		if(it->team == owner.team || it->dead || it->untargetable) continue;
		const vec3 pos = it->body->GetWorldPos();
		if(fabsf(pos.z - center.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
		if(LagDistToPoint(*it, vec2(center)) > radius + HitRadius(*it) + WEAPON_HIT_TOLERANCE) continue;
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
				if(it->team == owner.team || it->dead || it->untargetable) continue;
				if(eastl::find(proj->hitList.begin(), proj->hitList.end(), it->index) != proj->hitList.end()) continue;
				const vec3 pos = it->body->GetWorldPos();
				const f32 lagDist = LagDistToSegment(*it, from, to);
				proj->closest = MIN(proj->closest, lagDist);
				if(fabsf(pos.z - proj->pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
				if(!LagSegmentHits(*it, from, to, proj->halfWidth + HitRadius(*it) + PROJECTILE_HIT_TOLERANCE, lastStep ? proj->forwardReach : HitRadius(*it))) continue;

				if(!proj->hitList.full()) proj->hitList.push_back(it->index);
				DamagePlayer(owner, *it, SkillID::INVALID, (i32)(proj->damage * PassiveWeaponDamageMul(owner, *it)), &*proj);
				PassiveOnWeaponHit(owner, *it, *proj);
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
				if(it->team == owner.team || it->dead || it->untargetable) continue;
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
	// base damage of the un-upgraded skill: skilltt TTATTR_REMOTE_DAMAGE (SKILL_PROPERTY rows are upgrades)
	const f32 ttBase = content.GetSkillBaseDamage((i32)skillID);
	const f32 base = ttBase > 0 ? ttBase : level.getBaseDamage();
	const f32 damage = base + COMBAT_ATTACK_STAT * level.getAttackMultiplier();
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

// weapon overheat (FIRING_METHOD _GAUGE)
static const WeaponHeatInfo* ActiveWeaponHeat(const World::Player& player)
{
	const u32 m = player.mainCharaID & 1;
	const WeaponIndex weapon = m == 0 ? player.mainWeapon : player.subWeapon;
	return GetGameXmlContent().FindWeaponHeat((i32)weapon);
}

// current gauge of the active master, cooling applied since its last shot
static f32 CurrentHeat(const World::Player& player, const WeaponHeatInfo& h, Time now)
{
	const u32 m = player.mainCharaID & 1;
	if(player.overheatUntil[m] != Time::ZERO && now < player.overheatUntil[m]) return h.maxGauge;
	const f32 idle = player.heatLastShot[m] == Time::ZERO ? 999.0f : (f32)TimeDiffSec(TimeDiff(player.heatLastShot[m], now));
	if(idle <= h.autoRegenDelay || h.autoRegenInterval <= 0) return player.heat[m];
	return MIN(MAX(player.heat[m] + h.autoRegen * (idle - h.autoRegenDelay) / h.autoRegenInterval, 0.0f), h.maxGauge);
}

void World::UpdateWeaponHeat(Player& player)
{
	const WeaponHeatInfo* h = ActiveWeaponHeat(player);
	if(!h) return;
	const u32 m = player.mainCharaID & 1;

	// overheat finished: gauge resets, cooling starts from there
	if(player.overheatUntil[m] != Time::ZERO && localTime >= player.overheatUntil[m]) {
		player.overheatUntil[m] = Time::ZERO;
		player.heat[m] = h->overheatResetGauge;
		player.heatLastShot[m] = TimeAdd(localTime, TimeMsToTime(-h->autoRegenDelay * 1000.0));
		replication->SendGaugeToAll(player.userID, 1, false, 0, (u16)player.heat[m], (u16)h->maxGauge);
		player.heatSent = (i32)player.heat[m];
		LOG("Overheat :: %ls weapon cooled down", player.name.data());
	}

	if(localTime < player.tNextHeatSend) return;
	player.tNextHeatSend = TimeAdd(localTime, TimeMsToTime(100));
	const i32 cur = (i32)CurrentHeat(player, *h, localTime);
	if(cur == player.heatSent) return;
	player.heatSent = cur;
	const bool overheated = player.overheatUntil[m] != Time::ZERO;
	replication->SendGaugeToAll(player.userID, 0, overheated, 0, (u16)cur, (u16)h->maxGauge);
}

// returns false while the weapon is overheated
static bool WeaponHeatShot(World::Player& player, Time now, Replication* replication)
{
	const WeaponHeatInfo* h = ActiveWeaponHeat(player);
	if(!h) return true;
	const u32 m = player.mainCharaID & 1;
	if(player.overheatUntil[m] != Time::ZERO) return false;

	player.heat[m] = CurrentHeat(player, *h, now) + h->consumption;
	player.heatLastShot[m] = now;
	if(player.heat[m] >= h->maxGauge) {
		player.heat[m] = h->maxGauge;
		player.overheatUntil[m] = TimeAdd(now, TimeMsToTime(h->overheatDuration * 1000.0));
		replication->SendGaugeToAll(player.userID, 1, true, h->overheatDuration, (u16)h->maxGauge, (u16)h->maxGauge);
		player.heatSent = (i32)h->maxGauge;
		LOG("Overheat :: %ls locked %.1fs", player.name.data(), h->overheatDuration);
	}
	return true;
}

void World::OnWeaponShotPacket(Player& player, const vec3& origin, const vec2& dir, f32 distance, const u8* rawPacket, i32 rawSize)
{
	if(frozen) return;
	if(IsBlocked(player, true, false, false)) {
		LOG("Blocked :: %ls shot ignored (attack blocked by a status)", player.name.data());
		return;
	}
	if(!WeaponHeatShot(player, localTime, replication)) {
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

		UpdateWeaponHeat(*it);

		// move speed (percent stat, base from master stats) with status percent modifiers; the client moves itself
		{
			const f32 base = MasterStat(cls, STAT_MOVESPEED);
			const f32 speed = MAX(base * (1.0f + StatusModMulSum(*it, StatusMod::MOVESPEED) / 100.0f), 0.0f);
			if(base > 0 && fabsf(speed - it->moveSpeedSent) > 0.01f) {
				it->moveSpeedSent = speed;
				replication->SendStatToAll(it->Main().UID, STAT_MOVESPEED, speed, speed);
				if(fabsf(speed - base) > 0.01f) LOG("MoveSpeed :: %ls %.0f%%", it->name.data(), speed);
			}
		}

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

		bool resChanged = false;
		for(int r = 0; r < 3; r++) {
			const f32 resMax = MasterStat(cls, RESOURCES[r].maxStat);
			if(resMax <= 0) continue;
			if(it->res[r] < 0) it->res[r] = RESOURCES[r].startFull ? resMax : 0;
			it->res[r] = MIN(MAX(it->res[r] + MasterStat(cls, RESOURCES[r].regenStat) * delta, 0.0f), resMax);
			if(fabsf(it->res[r] - it->resSent[r]) >= 0.01f) resChanged = true;
		}
		if(localTime >= it->tNextResRefresh) {
			it->tNextResRefresh = TimeAdd(localTime, TimeMsToTime(1000));
			resChanged = true;
			it->moveSpeedSent = -1.0f; // resend move speed too
		}
		if(resChanged && localTime >= it->tNextResSend) {
			it->tNextResSend = TimeAdd(localTime, TimeMsToTime(250));
			for(int r = 0; r < 3; r++) {
				const f32 resMax = MasterStat(cls, RESOURCES[r].maxStat);
				if(resMax <= 0) continue;
				it->resSent[r] = it->res[r];
				replication->SendStatToAll(it->Main().UID, RESOURCES[r].maxStat, it->res[r], resMax);
			}
		}

		const f32 manaMax = MasterStat(cls, STAT_MANA);
		if(manaMax > 0) {
			if(it->mana < 0) it->mana = manaMax;
			it->mana = MIN(it->mana + MasterStat(cls, STAT_MANAREGEN) * delta, manaMax);
			if(localTime >= it->tNextManaSend) {
				it->tNextManaSend = TimeAdd(localTime, TimeMsToTime(250));
				replication->SendStatToAll(it->Main().UID, STAT_MANA, it->mana, manaMax);
			}
		}

		// tagged-out master regenerates its own pools (its regen stats; STAT_TYPE_TAG_* bonuses only come from items)
		{
			const ClassType benchCls = it->Sub().classType;
			const f32 benchManaMax = MasterStat(benchCls, STAT_MANA);
			if(benchManaMax > 0) {
				if(it->benchMana < 0) it->benchMana = benchManaMax;
				it->benchMana = MIN(it->benchMana + MasterStat(benchCls, STAT_MANAREGEN) * delta, benchManaMax);
			}
			for(int r = 0; r < 3; r++) {
				const f32 resMax = MasterStat(benchCls, RESOURCES[r].maxStat);
				if(resMax <= 0) continue;
				if(it->benchRes[r] < 0) it->benchRes[r] = RESOURCES[r].startFull ? resMax : 0;
				it->benchRes[r] = MIN(MAX(it->benchRes[r] + MasterStat(benchCls, RESOURCES[r].regenStat) * delta, 0.0f), resMax);
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

bool World::PlayerPaySkillCost(Player& player, i32 skillID)
{
	// base costs from SKILL.xml: _ConsumeMP (mana), _ConsumeUG (ultimate gauge), _ConsumeStamina (dodge)
	const auto& content = GetGameXmlContent();
	f32 mp = content.GetSkillConsumeMP(skillID);
	// STATUS_TYPE_NOCOST: the listed skills cost _AddRatioCostMP % more (-100 = free), then the status is used up
	foreach(it, activeStatuses) {
		if(it->targetIndex != player.index || !it->info || it->info->type != "STATUS_TYPE_NOCOST" || it->until <= localTime) continue;
		if(eastl::find(it->info->costSkills.begin(), it->info->costSkills.end(), skillID) == it->info->costSkills.end()) continue;
		mp = MAX(mp * (1.0f + it->info->costRatioMP / 100.0f), 0.0f);
		it->until = localTime;
		LOG("Passive :: %ls skill %d mana cost %.0f (status %d used)", player.name.data(), skillID, mp, it->statusID);
		break;
	}
	const f32 ug = content.GetSkillConsumeUG(skillID);
	const f32 st = content.GetSkillConsumeStamina(skillID);
	const f32 resCost[3] = { content.GetSkillConsume(skillID, 0), content.GetSkillConsume(skillID, 1), content.GetSkillConsume(skillID, 2) };
	const f32 hpCost = content.GetSkillConsume(skillID, 3);
	if(mp <= 0 && ug <= 0 && st <= 0 && resCost[0] <= 0 && resCost[1] <= 0 && resCost[2] <= 0 && hpCost <= 0) return true;
	if(player.clientHd == ClientHandle::INVALID) return true; // bots don't track resources

	bool ok = (mp <= 0 || player.mana < 0 || player.mana >= mp) &&
		(ug <= 0 || player.ultGauge < 0 || player.ultGauge >= ug) &&
		(st <= 0 || player.stamina < 0 || player.stamina >= st) &&
		(hpCost <= 0 || player.Main().hp > hpCost);
	for(int r = 0; r < 3; r++) {
		if(resCost[r] > 0 && player.res[r] >= 0 && player.res[r] < resCost[r]) ok = false;
	}
	if(resCost[0] > 0 || resCost[1] > 0 || resCost[2] > 0 || hpCost > 0) {
		LOG("Cost :: %ls skill=%d energy=%.0f/%.0f rage=%.0f/%.0f charges=%.0f/%.1f hp=%.0f", player.name.data(), skillID,
			resCost[0], player.res[0], resCost[1], player.res[1], resCost[2], player.res[2], hpCost);
	}
	LOG("Cost :: %ls skill=%d mp=%.0f/%.0f ug=%.0f/%.0f stamina=%.0f/%.0f %s", player.name.data(), skillID,
		mp, player.mana, ug, player.ultGauge, st, player.stamina, ok ? "paid" : "REFUSED");
	if(!ok) return false;

	if(mp > 0 && player.mana >= 0) { player.mana -= mp; player.tNextManaSend = Time::ZERO; }
	if(ug > 0 && player.ultGauge >= 0) { player.ultGauge -= ug; player.tNextUltSend = Time::ZERO; }
	if(st > 0 && player.stamina >= 0) { player.stamina -= st; player.tNextStaminaSend = Time::ZERO; }
	for(int r = 0; r < 3; r++) {
		if(resCost[r] > 0 && player.res[r] >= 0) { player.res[r] -= resCost[r]; player.tNextResSend = Time::ZERO; }
	}
	if(hpCost > 0) {
		ActorMaster& chara = player.Main();
		chara.hp = MAX(chara.hp - (i32)hpCost, 1);
		replication->SendHpToAll(chara.UID, chara.hp, chara.hpMax);
	}
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

f32 World::HitRadius(const Player& p) const
{
	if(p.Main().classType == ClassType::NONE) return 0;
	return (f32)GetGameXmlContent().GetMaster(p.Main().classType).character.getColliderRadius();
}

bool World::RemoteCanAffect(const RemoteInst& inst, const Player& p, bool includeOwner) const
{
	if(p.dead || p.untargetable) return false;
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

World::ActiveStatus* World::FindActiveStatus(u32 targetIndex, i32 statusID)
{
	foreach(it, activeStatuses) {
		if(it->targetIndex == targetIndex && it->statusID == statusID && it->until > localTime) return &*it;
	}
	return nullptr;
}

// ends a status now: the status update removes it and runs its _NextStatusIndex chain
void World::ConsumeStatus(u32 targetIndex, i32 statusID)
{
	ActiveStatus* st = FindActiveStatus(targetIndex, statusID);
	if(st) st->until = localTime;
}

// SKILL_TYPE_PASSIVE _Status of both masters
void World::ApplyPassives(Player& player)
{
	if(player.dead) return;
	const auto& content = GetGameXmlContent();
	foreach(c, player.characters) {
		const auto* list = content.FindPassiveStatuses((*c)->classType);
		if(!list) continue;
		for(i32 id : *list) {
			if(FindActiveStatus(player.index, id)) continue;
			ApplyStatus(player, player, id);
			ActiveStatus* st = FindActiveStatus(player.index, id);
			if(st && st->info) st->custom = st->info->customInit;
			LOG("Passive :: %ls status %d", player.name.data(), id);
		}
	}
	player.passivesApplied = true;
}

void World::ApplyAllPassives()
{
	foreach(it, players) ApplyPassives(*it);
}

// is this passive status one of the active master's passives
static bool PassiveOfActiveMaster(const World::Player& player, i32 statusID)
{
	const auto* list = GetGameXmlContent().FindPassiveStatuses(player.Main().classType);
	return list && eastl::find(list->begin(), list->end(), statusID) != list->end();
}

void World::PassiveOnSkillCast(Player& player, i32 skillID)
{
	if(GetGameXmlContent().GetSkillConsumeStamina(skillID) > 0) return; // dodges are not skills here
	eastl::fixed_vector<i32,4,false> toApply;
	foreach(it, activeStatuses) {
		if(it->targetIndex != player.index || !it->info || it->info->customMax <= 0) continue;
		if(!PassiveOfActiveMaster(player, it->statusID)) continue;
		// skills used counter: STATUS_TYPE_SKILLSTATUS applies _AuraStatusIndex on the max
		if(it->info->type == "STATUS_TYPE_SKILLSTATUS") {
			it->custom++;
			LOG("Passive :: %ls skill counter %d/%d", player.name.data(), it->custom, it->info->customMax);
			if(it->custom >= it->info->customMax) {
				it->custom = it->info->customInit;
				if(it->info->auraStatus != 0 && !toApply.full()) toApply.push_back(it->info->auraStatus);
			}
		}
	}
	foreach_const(id, toApply) {
		LOG("Passive :: %ls counter full -> status %d", player.name.data(), *id);
		ApplyStatus(player, player, *id);
	}
}

void World::PassiveOnWeaponHit(Player& attacker, Player& target, const Projectile& proj)
{
	// statuses of the weapon remote itself
	if(proj.remoteDoc != RemoteIdx::INVALID && !target.dead) {
		const Remote* remote = GetGameXmlContent().FindRemote(proj.remoteDoc);
		if(remote) {
			foreach_const(st, remote->statuses) {
				if(st->rate >= 100 || (i32)(RandUint() % 100) < st->rate) ApplyStatus(attacker, target, st->statusID);
			}
		}
	}

	eastl::fixed_vector<i32,4,false> toApply;
	foreach(it, activeStatuses) {
		if(it->targetIndex != attacker.index || !it->info) continue;
		if(!PassiveOfActiveMaster(attacker, it->statusID)) continue;
		const StatusInfo& info = *it->info;
		// every Nth weapon hit (STATUS_TYPE_HIT_PLAGUE): _HITEFFECT status on the target
		if(info.type == "STATUS_TYPE_HIT_PLAGUE" && info.customMax > 0) {
			it->custom++;
			if(it->custom >= info.customMax) {
				it->custom = info.customInit;
				if(info.hitEffectStatus && !toApply.full()) toApply.push_back(info.hitEffectStatus);
			}
		}
		// STATUS_TYPE_KROMEDE: weapon hits on stunned, bound or shocked targets apply _HITEFFECT
		else if(info.type == "STATUS_TYPE_KROMEDE" && info.hitEffectStatus) {
			if(HasStatusType(target, "STATUS_TYPE_STUN") || HasStatusType(target, "STATUS_TYPE_ROOT") || HasStatusType(target, "STATUS_TYPE_SHOCKED")) {
				if(!toApply.full()) toApply.push_back(info.hitEffectStatus);
			}
		}
		// STATUS_TYPE_MANABURN: a weapon hit on a target marked by a skill steals _AddValue mana
		else if(info.type == "STATUS_TYPE_MANABURN" && info.hitEffectStatus) {
			if(FindActiveStatus(target.index, info.hitEffectStatus)) {
				ConsumeStatus(target.index, info.hitEffectStatus);
				f32 stolen = info.hitEffectValue;
				if(target.mana >= 0) { stolen = MIN(stolen, target.mana); target.mana -= stolen; target.tNextManaSend = Time::ZERO; }
				if(attacker.mana >= 0) { attacker.mana += stolen; attacker.tNextManaSend = Time::ZERO; }
				LOG("Passive :: %ls drained %.0f mana from %ls", attacker.name.data(), stolen, target.name.data());
			}
		}
	}
	foreach_const(id, toApply) {
		LOG("Passive :: %ls weapon hit -> status %d on %ls", attacker.name.data(), *id, target.name.data());
		ApplyStatus(attacker, target, *id);
	}
}

void World::PassiveOnSkillHit(Player& attacker, Player& target)
{
	foreach_const(it, activeStatuses) {
		if(it->targetIndex != attacker.index || !it->info) continue;
		// STATUS_TYPE_MANABURN: skill hits mark the target (_HITEFFECT _StatusDocIndex)
		if(it->info->type == "STATUS_TYPE_MANABURN" && it->info->hitEffectStatus && PassiveOfActiveMaster(attacker, it->statusID)) {
			const i32 mark = it->info->hitEffectStatus;
			ApplyStatus(attacker, target, mark);
			return;
		}
	}
}

f32 World::PassiveWeaponDamageMul(Player& attacker, Player& target)
{
	f32 mul = 1.0f;
	foreach_const(it, activeStatuses) {
		if(it->targetIndex != attacker.index || !it->info) continue;
		// STATUS_TYPE_GHOSTWIDOW: weapon damage +_AddValue per debuff on the target
		if(it->info->type == "STATUS_TYPE_GHOSTWIDOW" && PassiveOfActiveMaster(attacker, it->statusID)) {
			i32 debuffs = 0;
			foreach_const(d, activeStatuses) {
				if(d->targetIndex == target.index && d->info && d->info->isDebuff && d->until > localTime) debuffs++;
			}
			mul *= 1.0f + it->info->hitEffectValue * debuffs;
		}
	}
	return mul;
}

void World::ApplyStatus(Player& caster, Player& target, i32 statusID, f32 power, f32 durationOverride)
{
	if(target.dead || target.untargetable) return;
	if(caster.team != target.team) target.statusesTaken++;
	const StatusInfo* info = GetGameXmlContent().FindStatus(statusID);
	f32 duration = durationOverride > 0 ? durationOverride : (info && info->durationSec > 0 ? info->durationSec : 1.0f);
	// passives without duration stay until consumed; clients get the data's "infinite" value (-1)
	const bool permanent = info && info->permanent && durationOverride <= 0;
	if(permanent) duration = 1000000.0f;
	const f32 sentDuration = permanent ? -1.0f : duration;
	if(info && info->casterStatus != 0 && info->casterStatus != statusID && !caster.dead && !applyingCasterStatus) {
		applyingCasterStatus = true;
		ApplyStatus(caster, caster, info->casterStatus);
		applyingCasterStatus = false;
	}

	const bool displacement = info && (info->type == "STATUS_TYPE_PUSH" || info->type == "STATUS_TYPE_FORCEBACK" ||
		info->type == "STATUS_TYPE_KNOCKDOWN" || info->type == "STATUS_TYPE_AIRBORNE" || info->type == "STATUS_TYPE_FLY" || info->type == "STATUS_TYPE_DRAGGED");
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
			// clients add one instance per SN_AddStatus: drop the previous one first, otherwise the final remove
			// leaves a copy (icon and effect) on screen
			replication->SendRemoveStatusToAll(statusID, it->targetUID, it->casterUID);
			replication->SendAddStatusToAll(statusID, it->targetUID, it->casterUID, sentDuration, pos, pos, 0);
			// stacking passive (_ICEQUEEN): full stack -> remove it and apply the burst status
			if(info && it->overlap >= info->maxOverlap && info->maxOverlap > 1) {
				foreach_const(ps, activeStatuses) {
					if(ps->targetIndex != caster.index || !ps->info || ps->info->stackStatus != statusID || ps->info->stackFullStatus == 0) continue;
					const i32 full = ps->info->stackFullStatus;
					it->until = localTime;
					LOG("Passive :: %ls full stack of %d on %ls -> %d", caster.name.data(), statusID, target.name.data(), full);
					ApplyStatus(caster, target, full);
					break;
				}
			}
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
	st.nextHealTick = localTime;
	st.healTicksDone = 0;
	st.shieldHP = 0;
	if(info && info->type == "STATUS_TYPE_SHIELD") {
		f32 dataShield = 0;
		foreach_const(m, info->mods) {
			if(m->stat == StatusMod::SHIELD_DEFENCE && !m->mul) dataShield += m->value;
		}
		// skilltt shield value when present, otherwise the old estimate
		st.shieldHP = dataShield > 0 ? dataShield : MAX(power, 100.0f) * (info->paramRatio > 0 ? info->paramRatio : 1.0f);
	}
	activeStatuses.push_back(st);

	if(info && (info->kind == StatusInfo::Kind::HARD_CC || info->kind == StatusInfo::Kind::ROOT)) {
		Time ccEnd = st.until;
		if(info->type == "STATUS_TYPE_AIRBORNE" || info->type == "STATUS_TYPE_FLY" || info->type == "STATUS_TYPE_KNOCKDOWN") {
			// lying on the ground (_LyingTime) then the stand-up animation (AniLength LIE_END)
			const auto& content = GetGameXmlContent();
			const f32 recover = info->lyingTime + content.GetLieEndTime(content.GetMaster(target.Main().classType).className.c_str());
			ccEnd = TimeAdd(ccEnd, TimeMsToTime(recover * 1000.0));
		}
		if(ccEnd > target.ccUntil) target.ccUntil = ccEnd;
	}

	const vec3 from = target.body->GetWorldPos();
	vec3 to = from;
	const f32 graphDuration = duration * (info && info->timeRatio > 0 ? info->timeRatio : 1.0f);
	if(info && displacement && info->type != "STATUS_TYPE_AIRBORNE" && info->type != "STATUS_TYPE_FLY") {
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

	if(info && info->type == "STATUS_TYPE_DISPEL" && info->dispelEffect >= 0) {
		// removes the target's dispellable buffs (0) or debuffs (1)
		foreach(it, activeStatuses) {
			if(it->targetIndex != target.index || !it->info || !it->info->canDispel || it->statusID == statusID) continue;
			if((it->info->isDebuff ? 1 : 0) != info->dispelEffect) continue;
			it->until = localTime;
			LOG("Dispel :: %ls removed status %d from %ls", caster.name.data(), it->statusID, target.name.data());
		}
	}

	LOG("Status :: %ls -> %ls status=%d type=%s duration=%.2f shield=%.0f", caster.name.data(), target.name.data(), statusID, info ? info->type.c_str() : "?", duration, st.shieldHP);
	replication->SendAddStatusToAll(statusID, st.targetUID, st.casterUID, sentDuration, from, to, graphDuration);
}

bool World::IsBlocked(const Player& p, bool attack, bool skill, bool shirk) const
{
	foreach_const(it, activeStatuses) {
		if(it->targetIndex != p.index || !it->info || localTime >= it->until) continue;
		if((attack && it->info->blockAttack) || (skill && it->info->blockSkill) || (shirk && it->info->blockShirk)) return true;
	}
	return false;
}

f32 World::StatusModMulSum(const Player& p, i32 stat) const
{
	f32 sum = 0;
	foreach_const(it, activeStatuses) {
		if(it->targetIndex != p.index || !it->info || localTime >= it->until) continue;
		foreach_const(m, it->info->mods) {
			if((i32)m->stat == stat && !m->regen && m->mul) sum += m->value * it->overlap;
		}
	}
	return sum;
}

f32 World::StatusModSum(const Player& p, i32 stat) const
{
	f32 sum = 0;
	foreach_const(it, activeStatuses) {
		if(it->targetIndex != p.index || !it->info) continue;
		foreach_const(m, it->info->mods) {
			if((i32)m->stat == stat && !m->regen && !m->mul) sum += m->value * it->overlap;
		}
	}
	return sum;
}

// bots cast their master's skills (BotBrainData _SkillIndexData) at the nearest enemy within _BotSkillMaxDistance;
// each skill waits max(its _CoolTime, bot NORMALSKILL/UGSKILL cooldown) before being used again
void World::UpdateBotSkills()
{
	const auto& content = GetGameXmlContent();
	foreach(it, players) {
		Player& bot = *it;
		if(bot.clientHd != ClientHandle::INVALID || bot.dead) continue;
		if(localTime < bot.tNextBotSkill || localTime < bot.ccUntil) continue;
		if(IsBlocked(bot, false, true, false)) continue;
		bot.tNextBotSkill = TimeAdd(localTime, TimeMsToTime(500)); // re-evaluated twice per second

		const ActorMaster& master = bot.Main();
		const auto* skills = content.FindBotSkills(100000000 + (i32)master.classType);
		if(!skills) continue;

		const vec3 pos = bot.body->GetWorldPos();
		Player* target = nullptr;
		f32 best = 1e9f;
		foreach(e, players) {
			if(e->dead || e->team == bot.team) continue;
			const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(pos));
			if(d < best) { best = d; target = &*e; }
		}
		if(!target) continue;

		// ready skills whose bot range reaches the target
		eastl::fixed_vector<i32,5,false> ready;
		for(i32 i = 0; i < 5; i++) {
			const i32 skill = (*skills)[i];
			if(skill == 0 || localTime < bot.botSkillReady[i]) continue;
			const f32 maxDist = content.GetBotSkillMaxDistance(skill);
			if(maxDist <= 0 || best > maxDist) continue;
			ready.push_back(i);
		}
		if(ready.empty()) continue;
		const i32 slot = ready[RandUint() % ready.size()];
		const i32 skill = (*skills)[slot];

		const f32 botCd = slot == 4 ? content.BotUgSkillCoolTime() : content.BotNormalSkillCoolTime();
		const f32 cd = MAX(content.GetSkillCoolTime(skill), botCd);
		bot.botSkillReady[slot] = TimeAdd(localTime, TimeMsToTime(cd * 1000.0));
		bot.tNextBotSkill = TimeAdd(localTime, TimeMsToTime(MAX(content.BotAfterSkillDelay(), 0.5f) * 1000.0));

		// face the target, then cast like a player would
		const vec3 tpos = target->body->GetWorldPos();
		const f32 yaw = atan2f(tpos.y - pos.y, tpos.x - pos.x);
		bot.input.rot.upperYaw = yaw;
		bot.input.rot.bodyYaw = yaw;
		bot.weaponAimYaw = yaw;
		const ActorUID targetUID = target->Main().UID;
		LOG("BotSkill :: %ls casts %d at %ls (dist %.0f, cd %.0f)", bot.name.data(), skill, target->name.data(), best, cd);
		PlayerCastSkill(bot, SkillID(skill), tpos, Slice<const ActorUID>(&targetUID, 1), 0);
	}
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
	f32 yaw = owner.input.rot.upperYaw;
	if(current && current != &owner) {
		const vec2 rel = vec2(current->body->GetWorldPos()) - vec2(npc.pos);
		if(glm::length(rel) > 1.0f) yaw = atan2f(rel.y, rel.x);
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
			pc.pos = npc.pos;
			pc.yaw = yaw;
			pc.skill = info->skills.empty() ? 0 : info->skills[0];
			pc.until = npc.despawnAt;
			pendingSummonCommands.push_back(pc);
		}
		else if(c->kind == SummonCommand::STATUS) {
			if(c->onCurrent) {
				if(current && !current->dead) ApplyStatus(owner, *current, c->index);
				continue;
			}
			const StatusInfo* st = GetGameXmlContent().FindStatus(c->index);
			if(!st) continue;
			const f32 duration = st->durationSec > 0 ? st->durationSec : 1.0f;
			replication->SendAddStatusToAll(c->index, npc.UID, owner.Main().UID, duration, npc.pos, npc.pos, 0);
			if(st->type == "STATUS_TYPE_SUICIDE") {
				const Time at = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
				if(npc.despawnAt == Time::ZERO || at < npc.despawnAt) npc.despawnAt = at;
			}
			LOG("Summon :: creature=%d self status %d (%s)", (i32)npc.docID, c->index, st->type.c_str());
			if(st->auraStatus != 0 && st->targetingRange > 0 && !groundAuras.full()) {
				GroundAura ga;
				ga.ownerIndex = npc.ownerIndex;
				ga.auraStatus = st->auraStatus;
				ga.range = st->targetingRange;
				ga.pos = npc.pos;
				ga.until = TimeAdd(localTime, TimeMsToTime(duration * 1000.0));
				ga.nextPulse = localTime;
				groundAuras.push_back(ga);
			}
		}
		else if(c->kind == SummonCommand::DIE) {
			npc.dieMode = c->dieMode;
		}
	}
}

void World::UpdateSummonAi(f32 delta)
{
	const auto& content = GetGameXmlContent();
	foreach(it, actorNpcList) {
		ActorNpc& npc = *it;
		if(npc.ai == SummonInfo::AI_NONE) continue;
		if(npc.despawnAt != Time::ZERO && localTime >= npc.despawnAt) continue;
		const SummonInfo* info = content.FindSummon((i32)npc.docID);
		if(!info) continue;
		Player& owner = players[npc.ownerIndex];
		vec3 goal = npc.pos;
		f32 stopDist = 0;

		if(npc.ai == SummonInfo::AI_RELAY) {
			if(owner.dead) continue;
			goal = owner.body->GetWorldPos();
			stopDist = info->aiDistance;
		}
		else {
			ActorMaster* tm = FindMasterActor(npc.chaseTarget);
			Player* target = tm ? tm->parent : nullptr;
			if(npc.stuck) {
				if(!target || target->dead) { npc.stuck = false; npc.chaseTarget = ActorUID::INVALID; continue; }
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
				npc.chaseTarget = target ? target->Main().UID : ActorUID::INVALID;
			}
			if(!target) continue;
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
		if(dist <= stopDist + 1.0f) continue;
		const f32 step = MIN(info->moveSpeed * delta, dist - stopDist);
		const vec2 dir = rel / dist;
		npc.pos = vec3(vec2(npc.pos) + dir * step, goal.z);
		npc.rot.z = atan2f(dir.y, dir.x);
		if(localTime >= npc.nextMoveSync) {
			npc.nextMoveSync = TimeAdd(localTime, TimeMsToTime(200));
			replication->SendNpcMoveToAll(npc.UID, npc.pos, goal, npc.rot.z, info->moveSpeed);
		}
	}
}

void World::TeleportPlayer(Player& p, const vec3& pos, f32 yaw)
{
	p.body->collider->setFootPosition(PxExtendedVec3(pos.x, pos.y, pos.z));
	p.body->vel = vec3(0);
	p.posHistoryCount = 0;
	p.posHistoryHead = 0;
	p.input.moveTo = pos;
	p.input.rot.upperYaw = yaw;
	p.input.rot.bodyYaw = yaw;
	p.weaponAimYaw = yaw;
}

static f32 RemoteDamageRadius(const GameXmlContent& content, RemoteIdx idx, i32 depth)
{
	const Remote* r = content.FindRemote(idx);
	if(!r || depth > 5) return 0;
	f32 radius = (r->attackMultiplier > 0 || !r->statuses.empty() || r->changeCasterAction != ActionStateID::INVALID) ? r->boundSize[0] * 0.5f : 0;
	const Remote::SpawnRules* lists[4] = { &r->onDestroy, &r->onTime, &r->onActivate, &r->onHitCharacter };
	for(const Remote::SpawnRules* l : lists) {
		for(const Remote::SpawnRule& rule : *l) radius = MAX(radius, RemoteDamageRadius(content, rule.idx, depth + 1));
	}
	return radius;
}

static f32 SkillDamageRadius(const GameXmlContent& content, ClassType cls, SkillID skillID)
{
	auto f = content.skillMap.find(skillID);
	if(f == content.skillMap.end()) return 0;
	const auto& action = content.GetSkillAction(cls, f->second.action);
	f32 radius = 0;
	bool anyRemote = false;
	foreach_const(cmd, action.commands) {
		if((cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET) && cmd->remote.idx != RemoteIdx::INVALID) {
			anyRemote = true;
			radius = MAX(radius, RemoteDamageRadius(content, cmd->remote.idx, 0));
		}
	}
	return anyRemote ? radius : -1.0f;
}

static i64 ShowcaseWallMs()
{
	return (i64)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void World::UpdateShowcase()
{
	if(showcase.comboSkill != 0 && localTime >= showcase.comboAt) {
		Player& comboCaster = players[showcase.comboCaster];
		LOG("Showcase :: combo skill=%d", showcase.comboSkill);
		if(!comboCaster.dead) PlayerCastSkill(comboCaster, SkillID(showcase.comboSkill), showcase.comboPos, Slice<const ActorUID>(&showcase.comboTarget, showcase.comboTarget != ActorUID::INVALID ? 1 : 0), 0);
		showcase.comboSkill = 0;
	}
	if(showcase.done || localTime < showcase.next) return;
	const auto& content = GetGameXmlContent();
	eastl::fixed_vector<u32,16,false> bots;
	Player* human = nullptr;
	foreach(it, players) {
		if(it->clientHd == ClientHandle::INVALID) bots.push_back(it->index);
		else if(!human) human = &*it;
	}
	if(!human || bots.size() < 2) return;

	if(!showcase.started) {
		vec3 sum[2] = { vec3(0), vec3(0) };
		i32 cnt[2] = { 0, 0 };
		f32 topZ = -1e9f;
		foreach_const(it, players) {
			sum[it->team & 1] += it->spawnPos; cnt[it->team & 1]++;
			topZ = MAX(topZ, it->spawnPos.z);
		}
		if(cnt[0] == 0 || cnt[1] == 0) return;
		const vec3 mid = (sum[0] / (f32)cnt[0] + sum[1] / (f32)cnt[1]) * 0.5f;
		showcase.mid = vec3(mid.x, mid.y, topZ + 300.0f);
		showcase.started = true;
		showcase.phase = 0;
		showcase.yaw = 0;
		showcase.dummySlot = (i32)bots.size() - 1;
		TeleportPlayer(players[bots[0]], showcase.mid, 0);
		showcase.next = TimeAdd(localTime, TimeMsToTime(2500));
		LOG("Showcase :: start wall=%lld bots=%d probe=(%.0f, %.0f, %.0f)", ShowcaseWallMs(), (i32)bots.size(), showcase.mid.x, showcase.mid.y, showcase.mid.z);
		return;
	}
	static const f32 diag = 0.70710678f;
	static const vec2 probeDirs[8] = { vec2(1, 0), vec2(-1, 0), vec2(0, 1), vec2(0, -1), vec2(diag, diag), vec2(-diag, diag), vec2(diag, -diag), vec2(-diag, -diag) };
	static const f32 probeDist[4] = { 600.0f, 1200.0f, 1800.0f, 2400.0f };
	const f32 flatLen = showcase.flatLen;
	const i32 probes = MIN((i32)bots.size(), 4);
	if(showcase.phase == 0) {
		if(showcase.candidates.empty()) showcase.candidates.push_back(vec2(showcase.mid));
		showcase.phase = 3;
		TeleportPlayer(players[bots[0]], vec3(showcase.candidates[showcase.candIdx], showcase.mid.z), 0);
		showcase.next = TimeAdd(localTime, TimeMsToTime(2500));
		return;
	}
	if(showcase.phase == 3) {
		showcase.groundZ = players[bots[0]].body->GetWorldPos().z;
		showcase.probeDir = 0;
		showcase.phase = 2;
		const vec2 c = showcase.candidates[showcase.candIdx];
		for(int k = 0; k < probes; k++) TeleportPlayer(players[bots[k]], vec3(c + probeDirs[0] * probeDist[k], showcase.mid.z), 0);
		showcase.next = TimeAdd(localTime, TimeMsToTime(2500));
		return;
	}
	if(showcase.phase == 2) {
		const vec2 c = showcase.candidates[showcase.candIdx];
		i32 flatCount = 0;
		bool broken = false;
		for(int k = 0; k < probes; k++) {
			const vec3 lp = players[bots[k]].body->GetWorldPos();
			if(fabsf(lp.z - showcase.groundZ) <= 15.0f && !broken) flatCount++;
			else broken = true;
			if(lp.z < showcase.groundZ - 50.0f && lp.z > showcase.groundZ - 400.0f && !showcase.candidates.full()) showcase.candidates.push_back(vec2(lp));
		}
		const f32 len = flatCount > 0 ? probeDist[flatCount - 1] : 0.0f;
		LOG("Showcase :: probe from (%.0f, %.0f) z=%.0f dir=(%.2f, %.2f) flat length=%.0f", c.x, c.y, showcase.groundZ, probeDirs[showcase.probeDir].x, probeDirs[showcase.probeDir].y, len);
		if(len > showcase.bestLen) {
			showcase.bestLen = len;
			showcase.bestDir = probeDirs[showcase.probeDir];
			showcase.bestCenter = c;
			showcase.bestZ = showcase.groundZ;
		}
		if(flatCount < probes) {
			if(showcase.probeDir < 7) {
				showcase.probeDir++;
				for(int k = 0; k < probes; k++) TeleportPlayer(players[bots[k]], vec3(c + probeDirs[showcase.probeDir] * probeDist[k], showcase.mid.z), 0);
				showcase.next = TimeAdd(localTime, TimeMsToTime(2500));
				return;
			}
			if(showcase.candIdx + 1 < (i32)showcase.candidates.size() && showcase.candIdx < 10) {
				showcase.candIdx++;
				TeleportPlayer(players[bots[0]], vec3(showcase.candidates[showcase.candIdx], showcase.mid.z), 0);
				showcase.phase = 3;
				showcase.next = TimeAdd(localTime, TimeMsToTime(2500));
				return;
			}
			LOG("Showcase :: no full flat line, best length=%.0f", showcase.bestLen);
		}
		showcase.dir = showcase.bestDir;
		showcase.groundZ = showcase.bestZ;
		showcase.mid = vec3(showcase.bestCenter, showcase.mid.z);
		showcase.flatLen = MAX(showcase.bestLen - 350.0f, 0.0f);
		showcase.phase = 0;
	}
	if(showcase.phase == 0) {
		const vec3 ground = vec3(0, 0, showcase.groundZ);
		showcase.anchor = vec3(vec2(showcase.mid) + showcase.dir * 350.0f, ground.z);
		showcase.phase = 1;
		foreach(it, players) {
			if(it->clientHd == ClientHandle::INVALID) TeleportPlayer(*it, it->spawnPos, 0);
		}
		const vec2 perp = vec2(-showcase.dir.y, showcase.dir.x);
		const vec3 humanPos = showcase.anchor + vec3(showcase.dir * -250.0f - perp * 250.0f, 0);
		TeleportPlayer(*human, humanPos, 0);
		replication->SendReviveToAll(human->userID, human->Main().UID, human->Sub().UID, humanPos);
		LOG("Showcase :: ground z=%.0f anchor=(%.0f, %.0f, %.0f)", ground.z, showcase.anchor.x, showcase.anchor.y, showcase.anchor.z);
		showcase.next = TimeAdd(localTime, TimeMsToTime(2000));
		return;
	}

	if(showcase.casterSlot >= (i32)bots.size()) {
		showcase.done = true;
		LOG("Showcase :: done wall=%lld", ShowcaseWallMs());
		return;
	}

	Player& caster = players[bots[showcase.casterSlot]];
	u32 dummyIndex = bots[showcase.dummySlot];
	foreach_const(b, bots) {
		if(*b != caster.index && players[*b].team != caster.team) { dummyIndex = *b; break; }
	}
	Player& dummy = players[dummyIndex];
	const vec3 casterPos = showcase.anchor;
	vec3 dummyPos = showcase.anchor + vec3(showcase.dir * 650.0f, 0);

	if(caster.dead || dummy.dead) {
		if(caster.dead) RespawnPlayer(caster);
		if(dummy.dead) RespawnPlayer(dummy);
		showcase.next = TimeAdd(localTime, TimeMsToTime(1500));
		return;
	}

	if(showcase.pendingResult) {
		showcase.pendingResult = false;
		const Player& pd = players[showcase.prevDummy];
		const i32 dealt = pd.damageTaken - showcase.lastDummyHp;
		const i32 hits = pd.hitsTaken - showcase.lastDummyHits;
		const i32 statusHits = pd.statusesTaken - showcase.lastDummyStatuses;
		bool hit = hits > 0 || statusHits > 0;
		foreach_const(it, activeStatuses) {
			if(it->targetIndex == showcase.prevDummy && it->casterIndex == showcase.prevCaster) hit = true;
		}
		const bool ok = showcase.prevExpect < 0 || (showcase.prevExpect == 1) == hit;
		LOG("Showcase :: result hit=%d expect=%d %s damage=%d hits=%d dummyZ=%.0f groundZ=%.0f", hit ? 1 : 0, showcase.prevExpect, ok ? "OK" : "MISMATCH", dealt, hits, pd.body->GetWorldPos().z, showcase.groundZ);
	}

	const ClassType cls = caster.Main().classType;
	static const char* slotNames[] = { "WEAPON", "SKILL_1", "SKILL_2", "SKILL_3", "SKILL_4", "ULTIMATE", "SHIRK", "TAG" };
	const i32 step = showcase.step;

	foreach(r, remoteInsts) {
		if(r->clientRemoteID != 0) replication->SendDestroyRemoteToAll(r->clientRemoteID);
	}
	remoteInsts.clear();
	pendingRemotes.clear();
	groundAuras.clear();
	projectileList.clear();
	pendingSummonCommands.clear();
	foreach(c, dummy.characters) { (*c)->hp = (*c)->hpMax; replication->SendHpToAll((*c)->UID, (*c)->hp, (*c)->hpMax); }
	const f32 faceCaster = atan2f(-showcase.dir.y, -showcase.dir.x);
	const f32 faceDummy = atan2f(showcase.dir.y, showcase.dir.x);
	foreach(it, players) {
		if(it->index == dummy.index || it->index == caster.index || it->clientHd != ClientHandle::INVALID) continue;
		if(glm::length(vec2(it->body->GetWorldPos()) - vec2(it->spawnPos)) > 50.0f) TeleportPlayer(*it, it->spawnPos, 0);
	}
	TeleportPlayer(dummy, dummyPos, faceCaster);
	TeleportPlayer(caster, casterPos, faceDummy);
	foreach(it, activeStatuses) {
		if(it->targetIndex == dummy.index || it->targetIndex == caster.index) {
			if(!it->info || !it->info->permanent) it->until = localTime;
		}
	}
	dummy.ccUntil = Time::ZERO;
	caster.ccUntil = Time::ZERO;

	i32 skill = 0;
	if(step >= 1 && step <= 6) {
		const auto& master = content.GetMaster(cls);
		foreach_const(sid, master.skillIDs) {
			auto f = content.skillMap.find(*sid);
			if(f == content.skillMap.end()) continue;
			const auto& sk = f->second;
			if(step == 6) {
				if(sk.type == SkillType::SHIRK) { skill = (i32)*sid; break; }
				continue;
			}
			eastl::fixed_string<char,48,false> name;
			name.sprintf("SKILL_%d_BEHAVIORSTATE", step);
			if(sk.type == SkillType::NORMAL && sk.action == ActionStateFromString(name.c_str())) { skill = (i32)*sid; break; }
		}
	}

	const f32 dummyRadius = HitRadius(dummy);
	f32 range = 0;
	vec3 castPos = dummyPos;
	const char* testName = "-";
	i32 expect = -1;
	showcase.testCount = 1;
	if(skill != 0) {
		range = content.GetSkillRange(skill);
		const f32 zone = content.GetSkillTargetRange(skill) * 0.5f;
		const f32 effect = SkillDamageRadius(content, cls, SkillID(skill));
		struct T { f32 dist; f32 side; const char* name; i32 expect; };
		eastl::fixed_vector<T,8,false> tests;
		if(range > 0) {
			tests.push_back({ MAX(range * 0.3f, 150.0f), 0, "near", 1 });
			tests.push_back({ range - 40.0f, 0, "range_in", 1 });
			tests.push_back({ range + MAX(zone, effect) + dummyRadius + 60.0f, 0, "range_out", 0 });
		}
		else {
			tests.push_back({ 300.0f, 0, "near", -1 });
		}
		if(zone > 0) {
			const f32 d = range > 0 ? range * 0.65f : 300.0f;
			tests.push_back({ d, zone - 40.0f, "zone_in", 1 });
			tests.push_back({ d, zone + dummyRadius + 60.0f, "zone_out", 0 });
		}
		showcase.testCount = (i32)tests.size();
		const T& t = tests[MIN(showcase.rangeIdx, (i32)tests.size() - 1)];
		dummyPos = showcase.anchor + vec3(showcase.dir * MIN(t.dist, flatLen), 0);
		castPos = dummyPos + vec3(vec2(-showcase.dir.y, showcase.dir.x) * t.side, 0);
		if(range > 0) {
			const vec2 rel = vec2(castPos) - vec2(casterPos);
			const f32 len = glm::length(rel);
			if(len > range) castPos = vec3(vec2(casterPos) + rel / len * range, castPos.z);
		}
		const bool support = effect == 0;
		testName = t.dist > flatLen ? "skip_beyond_flat" : (support ? "support" : t.name);
		expect = (t.dist > flatLen || support) ? -1 : t.expect;
		TeleportPlayer(dummy, dummyPos, faceCaster);
	}
	else if(step == 0) {
		const u32 wm = caster.mainCharaID & 1;
		const WeaponIndex weapon = wm == 0 ? caster.mainWeapon : caster.subWeapon;
		const WeaponFiring* wf = content.FindWeaponFiring((i32)weapon);
		const Remote* wr = wf ? content.FindRemote(wf->base) : nullptr;
		f32 wrange = wr ? (wr->maxDistance > 0 ? wr->maxDistance : wr->lifeTime * wr->maxSpeed) : 0;
		if(wf && wf->maxDistance > 0 && wr && wr->behaviorType == Remote::BehaviorType::GRAPH) wrange = wf->maxDistance;
		const Remote* bomb = wr ? content.FindRemote(wr->destroyCreateRemote) : nullptr;
		const f32 splash = bomb ? bomb->boundSize[0] * 0.5f : 0;
		range = wrange;
		struct T { f32 dist; f32 side; const char* name; i32 expect; };
		eastl::fixed_vector<T,8,false> tests;
		if(wrange > 0) {
			tests.push_back({ wrange - 40.0f, 0, "range_in", 1 });
			tests.push_back({ wrange + dummyRadius + 60.0f, 0, "range_out", 0 });
		}
		else tests.push_back({ 300.0f, 0, "near", -1 });
		if(splash > 0) {
			const f32 d = wrange > 0 ? wrange * 0.6f : 300.0f;
			tests.push_back({ d, splash - 40.0f, "splash_in", 1 });
			tests.push_back({ d, splash + dummyRadius + 60.0f, "splash_out", 0 });
		}
		showcase.testCount = (i32)tests.size();
		const T& t = tests[MIN(showcase.rangeIdx, (i32)tests.size() - 1)];
		dummyPos = showcase.anchor + vec3(showcase.dir * MIN(t.dist, flatLen), 0);
		castPos = dummyPos + vec3(vec2(-showcase.dir.y, showcase.dir.x) * t.side, 0);
		testName = t.dist > flatLen ? "skip_beyond_flat" : t.name;
		expect = t.dist > flatLen ? -1 : t.expect;
		TeleportPlayer(dummy, dummyPos, faceCaster);
	}
	{
		human->untargetable = true;
		const vec3 viewPos = vec3((vec2(casterPos) + vec2(dummyPos)) * 0.5f, showcase.anchor.z);
		if(glm::length(vec2(viewPos) - vec2(human->body->GetWorldPos())) > 50.0f) {
			TeleportPlayer(*human, viewPos, 0);
			replication->SendReviveToAll(human->userID, human->Main().UID, human->Sub().UID, viewPos);
		}
	}
	LOG("Showcase :: placement caster=%ls (%.0f, %.0f) dummy=%ls (%.0f, %.0f) player=(%.0f, %.0f) cast=(%.0f, %.0f)", caster.name.data(), casterPos.x, casterPos.y, dummy.name.data(), dummyPos.x, dummyPos.y, human->body->GetWorldPos().x, human->body->GetWorldPos().y, castPos.x, castPos.y);
	showcase.lastDummyHp = dummy.damageTaken;
	showcase.lastDummyHits = dummy.hitsTaken;
	showcase.lastDummyStatuses = dummy.statusesTaken;
	showcase.pendingResult = step <= 6 && (step == 0 || skill != 0);
	showcase.prevExpect = expect;
	showcase.prevDummy = dummy.index;
	showcase.prevCaster = caster.index;
	LOG("Showcase :: wall=%lld master=%d slot=%s skill=%d caster=%ls range=%.0f dist=%.0f test=%s expect=%d", ShowcaseWallMs(), (i32)cls, slotNames[step], skill, caster.name.data(), range, glm::length(vec2(dummyPos) - vec2(casterPos)), testName, expect);
	const ActorUID targetUID = dummy.Main().UID;
	if(step == 0) {
		const vec2 rel = vec2(castPos) - vec2(casterPos);
		const f32 len = glm::length(rel);
		const vec2 dir = len > 1.0f ? rel / len : vec2(1, 0);
		caster.weaponAimYaw = atan2f(dir.y, dir.x);
		if(ClassIsMelee(cls)) PlayerWeaponFire(caster);
		else PlayerWeaponShot(caster, casterPos, dir, len);
	}
	else if(step >= 1 && step <= 6) {
		const bool targetInRange = range <= 0 || glm::length(vec2(dummyPos) - vec2(casterPos)) <= range;
		if(skill != 0) PlayerCastSkill(caster, SkillID(skill), castPos, Slice<const ActorUID>(&targetUID, targetInRange ? 1 : 0), 0);
		const i32 combo = skill != 0 ? content.GetFirstComboSkill(skill) : 0;
		if(combo != 0) {
			showcase.comboSkill = combo;
			showcase.comboAt = TimeAdd(localTime, TimeMsToTime(700));
			showcase.comboPos = castPos;
			showcase.comboCaster = (i32)caster.index;
			showcase.comboTarget = targetInRange ? targetUID : ActorUID::INVALID;
		}
	}
	else {
		caster.input.tag = true;
	}

	if((step == 0 || (skill != 0 && step <= 6)) && showcase.rangeIdx < showcase.testCount - 1) showcase.rangeIdx++;
	else { showcase.rangeIdx = 0; showcase.step++; }
	f32 wait = step == 7 ? 2.0f : 3.5f;
	if(step >= 1 && step <= 6 && skill == 0) wait = 0.2f;
	if(showcase.step > 7) {
		showcase.step = 0;
		showcase.masterPass++;
		if(showcase.masterPass >= 2) {
			showcase.masterPass = 0;
			showcase.casterSlot++;
		}
	}
	showcase.next = TimeAdd(localTime, TimeMsToTime(wait * 1000.0));
}

void World::UpdateBotCombat()
{
	const auto& content = GetGameXmlContent();
	foreach(it, players) {
		Player& bot = *it;
		if(bot.clientHd != ClientHandle::INVALID || bot.dead) continue;
		if(localTime < bot.tNextBotShot || localTime < bot.ccUntil) continue;
		if(IsBlocked(bot, true, false, false)) continue;
		const ClassType cls = bot.Main().classType;
		if(ClassIsMelee(cls)) continue;

		const Remote* remote = content.FindRemote(RemoteIdx(1430000000 + (i32)cls * 10000 + 10));
		f32 range = ClassAttackRange(cls) * WEAPON_RANGE_SCALE;
		if(remote && remote->maxDistance > 0) range = remote->maxDistance;

		// nearest living enemy in weapon range
		const vec3 pos = bot.body->GetWorldPos();
		Player* target = nullptr;
		f32 best = range;
		foreach(e, players) {
			if(e->dead || e->team == bot.team) continue;
			const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(pos));
			if(d < best) { best = d; target = &*e; }
		}
		const u32 m = bot.mainCharaID & 1;
		const WeaponIndex weapon = m == 0 ? bot.mainWeapon : bot.subWeapon;
		const f32 delay = MAX(content.GetWeaponFireDelay((i32)weapon), 0.15f);
		bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(delay * 1000.0));
		if(!target) continue;

		if(!WeaponHeatShot(bot, localTime, replication)) continue; // overheated
		const vec2 rel = vec2(target->body->GetWorldPos()) - vec2(pos);
		const vec2 dir = glm::length(rel) > 1.0f ? glm::normalize(rel) : vec2(1, 0);
		bot.weaponAimYaw = atan2f(dir.y, dir.x);
		PlayerWeaponShot(bot, pos, dir, best);
	}
}

void World::UpdateSummons()
{
	for(auto ps = pendingSummons.begin(); ps != pendingSummons.end(); ) {
		if(localTime < ps->time) { ++ps; continue; }
		Player& owner = players[ps->ownerIndex];
		const SummonInfo* info = GetGameXmlContent().FindSummon(ps->creature);
		if(!owner.dead) {
			ActorNpc& actor = SpawnNpcActor(CreatureIndex(ps->creature), nextSummonLocalID++);
			actor.pos = ps->pos;
			actor.rot = ps->rot;
			actor.faction = owner.team == 0 ? Faction::RED : Faction::BLUE;
			actor.ownerUID = owner.Main().UID;
			actor.seed = (i32)RandUint();
			const f32 life = info && info->lifeTime > 0 ? info->lifeTime : 5.0f;
			actor.despawnAt = TimeAdd(localTime, TimeMsToTime(life * 1000.0));
			actor.ownerIndex = ps->ownerIndex;
			actor.ai = info ? (u8)info->ai : 0;
			if(info && info->ai == SummonInfo::AI_RELAY) {
				foreach(other, actorNpcList) {
					if(&*other == &actor || other->ownerIndex != ps->ownerIndex || other->ai != SummonInfo::AI_RELAY) continue;
					other->despawnAt = localTime;
				}
				if(owner.pendingChildSkill > 0 && localTime < owner.pendingChildSkillUntil) {
					LOG("Summon :: %ls relay state SKILL_%d", owner.name.data(), owner.pendingChildSkill);
					RunSummonState(actor, owner.pendingChildSkill);
				}
				owner.pendingChildSkill = 0;
			}
			if(info) {
				f32 t = 0;
				foreach_const(c, info->onSpawn) {
					if(!pendingSummonCommands.full()) {
						PendingSummonCommand pc;
						pc.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
						pc.ownerIndex = ps->ownerIndex;
						pc.kind = c->kind;
						pc.index = c->index;
						pc.pos = ps->pos;
						pc.yaw = ps->rot.z;
						pc.skill = info->skills.empty() ? 0 : info->skills[0];
						pc.until = actor.despawnAt;
						pendingSummonCommands.push_back(pc);
					}
					t += c->delay;
				}
			}
			LOG("Summon :: %ls creature=%d at (%.0f, %.0f) life=%.1fs known=%d", owner.name.data(), ps->creature, ps->pos.x, ps->pos.y, life, info != nullptr);
		}
		ps = pendingSummons.erase_unsorted(ps);
	}

	for(auto pc = pendingSummonCommands.begin(); pc != pendingSummonCommands.end(); ) {
		if(localTime < pc->time) { ++pc; continue; }
		Player& owner = players[pc->ownerIndex];
		if(pc->kind == SummonCommand::REMOTE) {
			if(!pendingRemotes.full()) {
				PendingRemote pr;
				pr.time = localTime;
				pr.idx = RemoteIdx(pc->index);
				pr.preset = (i32)ActionCommand::TargetPreset::POS_FFF; // fired from the summon's position
				pr.offsetSide = 0;
				pr.offsetFront = 0;
				pr.ownerIndex = pc->ownerIndex;
				pr.skillID = pc->skill != 0 ? SkillID(pc->skill) : SkillID::INVALID;
				pr.castID = nextCastID++;
				pr.damage = pc->skill != 0 ? (i32)SkillDamage(SkillID(pc->skill)) : 0;
				pr.castPos = pc->pos;
				pr.angle = pc->yaw;
				pr.target = ActorUID::INVALID;
				pendingRemotes.push_back(pr);
			}
		}
		else {
			const StatusInfo* st = GetGameXmlContent().FindStatus(pc->index);
			if(st && st->auraStatus != 0 && !groundAuras.full()) {
				GroundAura ga;
				ga.ownerIndex = pc->ownerIndex;
				ga.auraStatus = st->auraStatus;
				ga.range = st->targetingRange;
				ga.pos = pc->pos;
				const Time statusEnd = st->durationSec > 0 ? TimeAdd(localTime, TimeMsToTime(st->durationSec * 1000.0)) : pc->until;
				ga.until = pc->until < statusEnd ? pc->until : statusEnd;
				ga.nextPulse = localTime;
				groundAuras.push_back(ga);
				LOG("Summon :: %ls aura %d (gives %d within %.0f)", owner.name.data(), pc->index, st->auraStatus, st->targetingRange);
			}
		}
		pc = pendingSummonCommands.erase_unsorted(pc);
	}

	for(auto ga = groundAuras.begin(); ga != groundAuras.end(); ) {
		if(localTime >= ga->until) { ga = groundAuras.erase_unsorted(ga); continue; }
		if(localTime >= ga->nextPulse) {
			ga->nextPulse = TimeAdd(localTime, TimeMsToTime(500));
			const StatusInfo* child = GetGameXmlContent().FindStatus(ga->auraStatus);
			const Player& owner = players[ga->ownerIndex];
			if(child && ga->range > 0) {
				foreach(p, players) {
					if(p->dead) continue;
					const bool ally = p->team == owner.team;
					if(ally == child->isDebuff) continue;
					if(glm::length(vec2(p->body->GetWorldPos()) - vec2(ga->pos)) > ga->range + HitRadius(*p)) continue;
					if(!pendingStatuses.full()) {
						PendingStatus ps;
						ps.time = localTime;
						ps.statusID = ga->auraStatus;
						ps.casterIndex = ga->ownerIndex;
						ps.targetUID = p->Main().UID;
						// short-lived so it fades once out of the aura
						ps.duration = child->durationSec > 0 ? child->durationSec : 1.0f;
						ps.power = 0;
						pendingStatuses.push_back(ps);
					}
				}
			}
		}
		++ga;
	}

	for(auto it = actorNpcList.begin(); it != actorNpcList.end(); ) {
		if(it->despawnAt != Time::ZERO && localTime >= it->despawnAt) {
			LOG("Summon :: creature=%d despawned", (i32)it->docID);
			const SummonInfo* info = GetGameXmlContent().FindSummon((i32)it->docID);
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
						pc.pos = it->pos;
						pc.yaw = it->rot.z;
						pc.skill = info->skills.empty() ? 0 : info->skills[0];
						pc.until = localTime;
						pendingSummonCommands.push_back(pc);
					}
					t += c->delay;
				}
			}
			actorNpcMap.erase(it->UID);
			it = actorNpcList.erase(it);
		}
		else {
			++it;
		}
	}
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

		// regen modifiers (skilltt STATPROC_REGEN HEALTH): value per tick, value_2 ticks over the status duration
		if(it->info && !target.dead) {
			foreach_const(m, it->info->mods) {
				if(!m->regen || m->stat != StatusMod::HEALTH) continue;
				const f32 ticks = MAX(m->count, 1.0f);
				const f32 interval = it->info->intervalSec > 0 ? it->info->intervalSec : MAX(it->info->durationSec, 0.1f) / ticks;
				if(it->healTicksDone >= (i32)ticks || localTime < it->nextHealTick) continue;
				it->nextHealTick = TimeAdd(localTime, TimeMsToTime(interval * 1000.0));
				it->healTicksDone++;
				const i32 amount = (i32)(m->mul ? target.Main().hpMax * m->value / 100.0f : m->value);
				HealPlayer(players[it->casterIndex], target, amount * it->overlap);
			}
		}

		// aura: periodically gives its _AuraStatusIndex to players around (buffs to allies, debuffs to enemies)
		if(it->info && it->info->auraStatus != 0 && !target.dead && localTime >= it->nextAuraPulse) {
			it->nextAuraPulse = TimeAdd(localTime, TimeMsToTime(500));
			const StatusInfo* child = GetGameXmlContent().FindStatus(it->info->auraStatus);
			const f32 range = it->info->targetingRange > 0 ? it->info->targetingRange : (child ? child->targetingRange : 0);
			if(child && range > 0) {
				const i32 auraStatus = it->info->auraStatus;
				const u32 casterIndex = it->casterIndex;
				const vec2 center = vec2(target.body->GetWorldPos());
				foreach(p, players) {
					if(p->dead) continue;
					const bool ally = p->team == target.team;
					if(ally == child->isDebuff) continue;
					if(glm::length(vec2(p->body->GetWorldPos()) - center) > range + HitRadius(*p)) continue;
					if(!pendingStatuses.full()) {
						PendingStatus ps;
						ps.time = localTime;
						ps.statusID = auraStatus;
						ps.casterIndex = casterIndex;
						ps.targetUID = p->Main().UID;
						ps.duration = child->durationSec > 0 ? child->durationSec : 1.0f;
						ps.power = 0;
						pendingStatuses.push_back(ps);
					}
				}
			}
		}

		const bool shieldBroken = it->info && it->info->type == "STATUS_TYPE_SHIELD" && it->shieldHP <= 0;
		const bool timeOver = localTime >= it->until;
		if(timeOver || target.dead || shieldBroken || it->removeOnHit) {
			replication->SendRemoveStatusToAll(it->statusID, it->targetUID, it->casterUID);
			// next status chain (_NextStatusIndex / _NextStatusCondition)
			i32 next = 0;
			if(it->info && it->info->nextStatus != 0 && !target.dead) {
				const auto cond = it->info->nextCondition;
				if(cond == StatusInfo::NextCondition::ALL ||
				   (cond == StatusInfo::NextCondition::TIMEOVER && timeOver) ||
				   (cond == StatusInfo::NextCondition::REMOVE_ONHIT && it->removeOnHit)) next = it->info->nextStatus;
			}
			const u32 casterIndex = it->casterIndex;
			it = activeStatuses.erase_unsorted(it);
			if(next != 0 && !pendingStatuses.full()) {
				PendingStatus ps;
				ps.time = localTime;
				ps.statusID = next;
				ps.casterIndex = casterIndex;
				ps.targetUID = target.Main().UID;
				ps.duration = -1.0f;
				ps.power = 0;
				pendingStatuses.push_back(ps);
			}
		}
		else {
			++it;
		}
	}
}

bool World::ScheduleSkillRemotes(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 angle, i32 forceAnim, ActionStateID forcedAction)
{
	const auto& content = GetGameXmlContent();
	auto skillFound = content.skillMap.find(skillID);
	if(skillFound == content.skillMap.end()) return false;
	const auto& skill = skillFound->second;
	const auto& action = content.GetSkillAction(player.Main().classType, forcedAction != ActionStateID::INVALID ? forcedAction : skill.action);

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
		(void)consumeUG; // paid in PlayerPaySkillCost
	}

	const u32 castID = nextCastID++;
	i32 damage = (i32)SkillDamage(skillID);
	{
		// skill power granted by active statuses (skilltt STAT_TYPE_SKILLPOWER) scales with the skill's attack multiplier
		const f32 skillPower = StatusModSum(player, StatusMod::SKILLPOWER);
		if(skillPower > 0) {
			SkillNormalModel& sm = const_cast<SkillNormalModel&>(skill);
			damage += (i32)(skillPower * sm.getSkillNormalLevelByIndex(0)->getAttackMultiplier());
		}
	}
	const ActorUID currentTarget = targets.size() > 0 ? targets[0] : ActorUID::INVALID;

	bool scheduled = false;
	// animation 0 without any action and no hit-event combo: animation 1 is its continuation,
	// played once animation 0 is over (AniLength of its AnimationId)
	f32 tStart = 0;
	if(anim == 0 && forceAnim < 0 && GetGameXmlContent().FindSkillHitEventRemote((i32)skillID) == 0) {
		bool anim0Acts = false, anim1Acts = false;
		i32 anim0Id = 0;
		foreach_const(cmd, action.commands) {
			bool acts = cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET ||
				cmd->type == ActionCommand::Type::STATUS || cmd->type == ActionCommand::Type::ENTITY;
			if(cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET) {
				const Remote* r = content.FindRemote(cmd->remote.idx);
				const bool visualOnly = r && r->boundType == Remote::BoundType::E_BOUND_NONE && r->onDestroy.empty() && r->onTime.empty() && r->onActivate.empty() &&
					(r->damageGroup == Remote::DamageGroup::eNONE || r->damageGroup == Remote::DamageGroup::INVALID);
				if(visualOnly) acts = false;
			}
			if(cmd->animIndex == 0) { anim0Acts |= acts; if(!anim0Id) anim0Id = cmd->animationId; }
			if(cmd->animIndex == 1) anim1Acts |= acts;
		}
		if(!anim0Acts && anim1Acts) {
			anim = 1;
			tStart = content.GetAniSeqTime(content.GetMaster(player.Main().classType).className.c_str(), anim0Id);
		}
	}

	f32 t = tStart;
	foreach_const(cmd, action.commands) {
		if(cmd->animIndex != anim && cmd->animIndex != -1) continue; // -1: command shared by every animation
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
			// DestroyAwayTarget: the remote ends at the aimed point. Client RE: ActionBase Param2 flag 0x20 passed to
			// the remote creation; every PC skill using it is aimed with a ground cursor (SKILL_CAST_TARGET_POSITION*)
			pr.stopAtTarget = cmd->remote.stopAtTarget;
			pr.nodeName = cmd->nodeName;
			pendingRemotes.push_back(pr);
			scheduled = true;
		}
		if(cmd->type == ActionCommand::Type::ENTITY && cmd->entity.entityIndex != 0 && !pendingSummons.full()) {
			PendingSummon ps;
			ps.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			ps.creature = cmd->entity.entityIndex;
			ps.ownerIndex = player.index;
			const auto preset = cmd->entity.targetPreset;
			vec3 pos = player.body->GetWorldPos();
			if(preset == ActionCommand::TargetPreset::POS_FFF || preset == ActionCommand::TargetPreset::POS_TFF ||
			   preset == ActionCommand::TargetPreset::POS_TTT_1 || preset == ActionCommand::TargetPreset::POS_TFF_1_800_1000) {
				if(glm::length(vec2(castPos)) > 1.0f) pos = vec3(vec2(castPos), pos.z);
			}
			else if(preset == ActionCommand::TargetPreset::CURRENT_FFF && currentTarget != ActorUID::INVALID) {
				const ActorMaster* tm = FindMasterActor(currentTarget);
				if(tm) pos = tm->parent->body->GetWorldPos();
			}
			ps.pos = pos;
			ps.rot = vec3(0, 0, angle);
			pendingSummons.push_back(ps);
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

World::RemoteInst* World::SpawnRemote(u32 ownerIndex, RemoteIdx idx, const vec3& pos, const vec2& dir, ActorUID target, SkillID skillID, u32 castID, i32 damage, bool broadcast, const vec3* aimPos, u16 nodeName, bool casterCreatesLocally, bool stopAtTarget)
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
		inst.landZ = aimPos->z;
		inst.hasLandZ = true;
	}
	inst.stopAtFirstHit = false; // first-hit stop comes from _ActivateCount
	if(stopAtTarget && remote->behaviorType != Remote::BehaviorType::GRAPH && aimPos && glm::length(vec2(*aimPos)) > 1.0f && inst.speed > 0) {
		// DestroyAwayTarget: the remote ends at the aimed point
		const f32 aimDist = glm::length(vec2(*aimPos) - vec2(pos));
		inst.range = inst.range > 0 ? MIN(aimDist, inst.range) : aimDist;
	}
	inst.spawnTime = localTime;
	inst.target = target;
	inst.hitsLeft = MAX(remote->activateCount, 1);
	inst.areaDone = false;
	inst.casterLocal = casterCreatesLocally;
	inst.graphStart = pos;
	inst.graphDir = dir;
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
		// the client adds the remote's _OffsetZ to the fire position: compensate so the remote shows at the
		// height the server simulates (weapon fire height), not under the ground
		const vec3 sendPos = pos - vec3(0, 0, remote->offsetZ);
		replication->SendRemoteCreateToAll(players[ownerIndex].Main().UID, seed, idx, sendPos, sendYaw, targetPos, targetMaster ? target : ActorUID::INVALID, GetGameXmlContent().GetNodeName(nodeName), casterCreatesLocally ? players[ownerIndex].clientHd : ClientHandle::INVALID);
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
	static World::RemoteInst* Spawn(World& w, u32 ownerIndex, RemoteIdx idx, const vec3& pos, const vec2& dir, ActorUID target, SkillID skillID, u32 castID, i32 damage, bool broadcast, bool casterLocal)
	{
		return w.SpawnRemote(ownerIndex, idx, pos, dir, target, skillID, castID, damage, broadcast, nullptr, 0, casterLocal);
	}
};

static void SpawnRemoteRules(World& world, const World::RemoteInst& parent, const Remote::SpawnRules& rules, const vec3& pos, i32 excludeIndex, RemoteIdx skipFirst, bool fromDestroy = false)
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
			ActorUID childTarget = parent.target;
			if(rule->targetOwner) {
				World::Player& owner = world.GetPlayer(parent.ownerIndex);
				childTarget = owner.Main().UID;
				const vec2 rel = vec2(owner.body->GetWorldPos()) - vec2(at);
				if(glm::length(rel) > 1.0f) dir = glm::normalize(rel);
			}
			World::RemoteInst* inst = WorldRemoteSpawner::Spawn(world, parent.ownerIndex, rule->idx, at + vec3(dir * child->offsetY, 0), dir, childTarget, parent.skillID, parent.castID, parent.damage,
				// clients spawn the directed children (_CreateRemote) of a remote ending on their side themselves
				parent.clientRemoteID != 0 && !(fromDestroy && rule->kind != Remote::SPAWN_SINGLE && !rule->targetOwner), parent.casterLocal); // returns to the caster are not spawned by clients
			if(inst) {
				inst->excludeIndex = excludeIndex;
				inst->centeredBox = parent.centeredBox;
			}
		}
	}
}

void World::RemoteHit(RemoteInst& inst, Player& target)
{
	const Remote& remote = *inst.remote;
	Player& owner = players[inst.ownerIndex];
	if(!RemoteCanAffect(inst, target, true)) return;
	if(remote.activateDelay > 0 && TimeDiffSec(TimeDiff(inst.spawnTime, localTime)) < remote.activateDelay) return;
	const bool enemy = target.team != owner.team;

	const i32 amount = (i32)(inst.damage * remote.attackMultiplier);
	if(enemy && remote.memorizeTarget) owner.memorizedTarget = target.Main().UID;
	if(enemy && remote.changeCasterAction != ActionStateID::INVALID && !inst.actionChanged && !owner.dead && owner.clientHd == ClientHandle::INVALID) {
		inst.actionChanged = true;
		inst.hitsLeft = 0;
		const ActorUID targetUID = target.Main().UID;
		LOG("Remote :: %ls hit %ls -> caster action %d", owner.name.data(), target.name.data(), (i32)remote.changeCasterAction);
		PlayerCastSkill(owner, inst.skillID, target.body->GetWorldPos(), Slice<const ActorUID>(&targetUID, 1), 0, remote.changeCasterAction);
		return;
	}
	if(amount <= 0 && remote.statuses.empty() && remote.friendlyStatuses.empty() && remote.onActivate.empty() && remote.onHitCharacter.empty()) return;

	// damaging hits share one hit window per cast (a target takes a skill's damage once per window),
	// effect-only hits keep a window per remote so lingering zones keep applying their effects
	const RemoteIdx hitKey = amount > 0 ? RemoteIdx::INVALID : remote.ID;
	foreach(h, castHits) {
		if(h->castID == inst.castID && h->remoteID == hitKey && h->playerIndex == target.index && localTime < h->until) return;
	}
	if(inst.speed <= 0 && enemy) inst.hitsLeft--;
	if(!castHits.full()) {
		CastHit h;
		h.remoteID = hitKey;
		h.castID = inst.castID;
		h.playerIndex = target.index;
		h.until = TimeAdd(localTime, TimeMsToTime(MAX(remote.hitInvalidTime, 0.1f) * 1000.0));
		castHits.push_back(h);
	}

	if(amount > 0) {
		if(enemy && inst.skillID != SkillID::INVALID) PassiveOnSkillHit(owner, target);
		if(enemy) DamagePlayer(owner, target, inst.skillID, amount, nullptr, inst.clientRemoteID, remote.ID, inst.dir);
		else if(remote.damageGroup == Remote::DamageGroup::eFRIEND) HealPlayer(owner, target, amount);
	}

	inst.lastHitIndex = (i32)target.index;
	if(enemy && remote.onHitOwnerStatus != 0 && !owner.dead) ApplyStatus(owner, owner, remote.onHitOwnerStatus);
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

void World::RemoteHitAlongPath(RemoteInst& inst)
{
	const Remote& remote = *inst.remote;
	if(remote.damageGroup == Remote::DamageGroup::eNONE || remote.damageGroup == Remote::DamageGroup::INVALID) return;
	if(remote.attackMultiplier <= 0 && remote.statuses.empty()) return;
	const f32 radius = remote.boundSize[0] * 0.5f;
	foreach(it, players) {
		if(!RemoteCanAffect(inst, *it, false)) continue;
		if(it->team == players[inst.ownerIndex].team) continue; // on the way: enemies only
		if(glm::length(vec2(it->body->GetWorldPos()) - vec2(inst.pos)) > radius + HitRadius(*it)) continue;
		RemoteHit(inst, *it);
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
		if(along < start - HitRadius(*it) || along > start + length + HitRadius(*it)) continue;
		if(side > halfWidth + HitRadius(*it)) continue;
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
	const bool fanBox = remote.boundType == Remote::BoundType::E_BOUND_BOX && remote.angle > 0;
	if(remote.boundType == Remote::BoundType::E_BOUND_SPHERE || fanBox) {
		const bool cone = remote.angle > 0;
		const f32 radius = remote.boundSize[0] * 0.5f;
		foreach(it, players) {
			const vec3 pos = it->body->GetWorldPos();
			if(fabsf(pos.z - inst.pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
			const vec2 rel = LagClosestPos(*it, vec2(inst.pos)) - vec2(inst.pos);
			const f32 dist = glm::length(rel);
			if(dist > radius + HitRadius(*it)) continue;
			if(remote.angle > 0 && dist > HitRadius(*it)) {
				if(glm::dot(rel / dist, inst.dir) < cosf(MIN(remote.angle * 0.5f, 180.0f) * PI / 180.0f)) continue;
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
			const f32 back = inst.centeredBox ? length * 0.5f : 0.0f;
			const vec2 center = vec2(inst.pos) + inst.dir * (length * 0.5f - back);
			const vec2 rel = LagClosestPos(*it, center) - vec2(inst.pos) + inst.dir * back;
			const f32 along = glm::dot(rel, inst.dir);
			const f32 side = fabsf(glm::dot(rel, Perp(inst.dir)));
			if(along < -HitRadius(*it) || along > length + HitRadius(*it)) continue;
			if(side > halfWidth + HitRadius(*it)) continue;
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
		const f32 offsetY = remote ? (remote->forceTargetDir ? -remote->offsetY : remote->offsetY) : 0;
		vec3 pos = owner.body->GetWorldPos() + vec3(dir * offsetY, 158.0f);

		bool atAimedPoint = pr->atTarget;
		switch((ActionCommand::TargetPreset)pr->preset) {
			case ActionCommand::TargetPreset::POS_FFF:
			case ActionCommand::TargetPreset::POS_TFF:
			case ActionCommand::TargetPreset::POS_TTT_1:
			case ActionCommand::TargetPreset::POS_TFF_1_800_1000: {
				atAimedPoint = glm::length(vec2(pr->castPos)) > 1.0f;
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
			// presets whose T/F flags start with T (SELF_TFF, MEMORIZED_TFT, ...) are created by the caster's own client
			bool casterLocal = false;
			{
				const char* name = ActionCommand::TargetPresetToString((ActionCommand::TargetPreset)pr->preset);
				for(const char* t = name; t && *t; ) {
					const char* e = t;
					while(*e && *e != '_') e++;
					if(e - t == 3 && (t[0] == 'T' || t[0] == 'F') && (t[1] == 'T' || t[1] == 'F') && (t[2] == 'T' || t[2] == 'F')) {
						casterLocal = t[0] == 'T';
						break;
					}
					t = *e ? e + 1 : e;
				}
			}
			RemoteInst* spawned = SpawnRemote(pr->ownerIndex, pr->idx, pos, dir, pr->target, pr->skillID, pr->castID, pr->damage, true, &pr->castPos, pr->nodeName, casterLocal, pr->stopAtTarget);
			if(spawned) spawned->centeredBox = atAimedPoint;
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
				inst.pos = instOwner.body->GetWorldPos() + vec3(inst.dir * (remote.forceTargetDir ? -remote.offsetY : remote.offsetY), 158.0f);
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
					if(glm::length(vec2(it->body->GetWorldPos()) - vec2(instOwner.body->GetWorldPos())) <= radius + HitRadius(*it)) RemoteHit(inst, *it);
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
				if(dist <= step + remote.boundSize[0] * 0.5f + HitRadius(target)) {
					inst.pos = vec3(vec2(tpos), inst.pos.z);
					RemoteHit(inst, target);
					destroyed = true;
				}
				else {
					inst.dir = rel / dist;
					inst.pos += vec3(inst.dir * step, 0);
					RemoteHitAlongPath(inst);
				}
			} break;

			case Remote::BehaviorType::GRAPH: {
				const f32 step = MIN(inst.speed * delta, inst.range - inst.travelled);
				inst.travelled += step;
				if(remote.bezier && inst.range > 0) {
					// cubic bezier start -> end; control points at 1/3 and 2/3, offset by up rotated around the
					// forward axis (only its horizontal part matters here) and pushed by front
					const vec2 f = inst.graphDir;
					const vec2 side = -Perp(f); // positive rotate = right of the travel direction (client angles are clockwise; matches gameplay video)
					const vec2 p0 = vec2(inst.graphStart);
					const vec2 p3 = p0 + f * inst.range;
					vec2 c[2];
					for(int k = 0; k < 2; k++) {
						const f32 a = remote.bezierRotate[k] * PI / 180.0f;
						c[k] = p0 + f * (inst.range * (k + 1) / 3.0f + remote.bezierFront[k]) + side * (remote.bezierUp[k] * sinf(a));
					}
					const f32 u = MIN(inst.travelled / inst.range, 1.0f);
					const f32 v = 1.0f - u;
					const vec2 p = p0 * (v * v * v) + c[0] * (3 * v * v * u) + c[1] * (3 * v * u * u) + p3 * (u * u * u);
					const vec2 tangent = (c[0] - p0) * (3 * v * v) + (c[1] - c[0]) * (6 * v * u) + (p3 - c[1]) * (3 * u * u);
					if(glm::length(tangent) > 0.001f) inst.dir = glm::normalize(tangent);
					inst.pos = vec3(p, inst.pos.z);
				}
				else {
					inst.pos += vec3(inst.dir * step, 0);
				}
				RemoteHitAlongPath(inst);
				destroyed = inst.travelled >= inst.range - 0.01f;
				if(destroyed && inst.hasLandZ) inst.pos.z = inst.landZ; // landed on the ground
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
							if(!LagSegmentHits(*it, from, to, halfWidth + HitRadius(*it) + PROJECTILE_HIT_TOLERANCE, HitRadius(*it))) continue;
							RemoteHit(inst, *it);
							inst.hitsLeft--;
							if(inst.stopAtFirstHit && it->team != players[inst.ownerIndex].team) inst.hitsLeft = 0;
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
					if(remote.activateDelay <= 0 || elapsed >= remote.activateDelay) {
						inst.areaDone = true;
						RemoteApplyArea(inst);
					}
				}
				else if(remote.lifeTime > 0 && inst.hitsLeft > 0) {
					// lingering zone / mine: keeps checking contact, hits again once each target's hit window expires
					RemoteApplyArea(inst);
				}
				if(inst.hitsLeft <= 0 && remote.lifeTime > 0) {
					// activation count used up (e.g. a mine triggered by an enemy): ends now and spawns its chain
					if(inst.clientRemoteID != 0) replication->SendDestroyRemoteToAll(inst.clientRemoteID);
					inst.destroyedByHit = true;
					destroyed = true;
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

		if(destroyed && inst.hasLandZ) inst.pos.z = inst.landZ;
		// tell clients a moving remote ended here (otherwise they let it fly on, e.g. a boomerang curve past its target)
		if(destroyed && !inst.destroyedByHit && inst.clientRemoteID != 0 && inst.speed > 0) replication->SendDestroyRemoteToAll(inst.clientRemoteID);
		if(destroyed) {
			const RemoteIdx next = remote.destroyCreateRemote;
			const RemoteInst copy = inst;
			remoteInsts.erase_unsorted(remoteInsts.begin() + i);
			if(!copy.chainEnded) {
				const i32 childExclude = remote.behaviorType == Remote::BehaviorType::CHAIN ? copy.lastHitIndex : copy.excludeIndex;
				if(next != RemoteIdx::INVALID) {
					const Remote* nextRemote = GetGameXmlContent().FindRemote(next);
					const f32 nextOffset = nextRemote ? nextRemote->offsetY : 0;
					RemoteInst* child = SpawnRemote(copy.ownerIndex, next, copy.pos + vec3(copy.dir * nextOffset, 0), copy.dir, copy.target, copy.skillID, copy.castID, copy.damage, copy.clientRemoteID != 0, nullptr, 0, copy.casterLocal);
					if(child) {
						child->excludeIndex = childExclude;
						child->centeredBox = copy.centeredBox;
					}
				}
				SpawnRemoteRules(*this, copy, remote.onDestroy, copy.pos, childExclude, next, true);
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
		if(it->team == player.team || it->dead || it->untargetable) continue;
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
	if(target.dead || target.untargetable) return;
	if(HasStatusType(target, "STATUS_TYPE_GODMODE")) return;
	if(HasStatusType(target, "STATUS_TYPE_EVADE") && attacker.team != target.team) {
		LOG("Evade :: %ls evaded %ls", target.name.data(), attacker.name.data());
		return;
	}

	// close-range penalty of the remote (_DistPenaltyRate within _PenaltyApplyDistance of the caster)
	{
		const RemoteIdx doc = proj ? proj->remoteDoc : remoteDoc;
		const Remote* pr = doc != RemoteIdx::INVALID ? GetGameXmlContent().FindRemote(doc) : nullptr;
		if(pr && pr->distPenaltyRate > 0 && pr->penaltyApplyDistance > 0 &&
		   glm::length(vec2(target.body->GetWorldPos()) - vec2(attacker.body->GetWorldPos())) < pr->penaltyApplyDistance) {
			damage = (i32)(damage * (1.0f - pr->distPenaltyRate / 100.0f));
		}
	}

	// element advantage: weapon shots use the weapon bonus, skills the skill bonus, DoTs none
	u32 damageType = PACKET_DAMAGE_TYPE_NORMAL;
	if(proj || skillID != SkillID::INVALID) {
		const f32 mul = ElementMultiplier(attacker, target, !proj);
		if(mul != 1.0f) damage = (i32)(damage * mul);

		// critical hit: master crit stats (weapon or skill) + the remote's own _CriticalRate
		const ClassType ac = attacker.Main().classType;
		const RemoteIdx doc = proj ? proj->remoteDoc : remoteDoc;
		const Remote* critRemote = doc != RemoteIdx::INVALID ? GetGameXmlContent().FindRemote(doc) : nullptr;
		const f32 rate = MasterStat(ac, proj ? STAT_CRITICALRATE : STAT_SKILL_CRITICALRATE) + (critRemote ? critRemote->criticalRate : 0) +
			StatusModSum(attacker, proj ? StatusMod::CRITICALRATE : StatusMod::SKILL_CRITICALRATE);
		if(rate > 0 && (RandUint() % 10000) < (u32)(rate * 100.0f)) {
			const f32 critDamage = MasterStat(ac, proj ? STAT_CRITICALDAMAGE : STAT_SKILL_CRITICALDAMAGE);
			damage = (i32)(damage * (critDamage > 0 ? critDamage : 100.0f) / 100.0f);
			damageType = PACKET_DAMAGE_TYPE_CRITICAL;
		}
	}

	foreach(it, activeStatuses) {
		if(it->targetIndex == target.index && it->info && it->info->nextCondition == StatusInfo::NextCondition::REMOVE_ONHIT) it->removeOnHit = true;
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
	target.damageTaken += damage;
	target.hitsTaken++;
	const bool showcaseDummy = Config().DevShowcase && target.clientHd == ClientHandle::INVALID;
	chara.hp = MAX(chara.hp - damage, (showcaseDummy || HasStatusType(target, "STATUS_TYPE_NEARLY_IMMORTAL")) ? 1 : 0);

	// rage: STAT_TYPE_RAGE_ATTACK_RECOVERY_VALUE per landed hit
	if(attacker.res[1] >= 0) {
		const ClassType ac = attacker.Main().classType;
		const f32 rageMax = MasterStat(ac, RESOURCES[1].maxStat);
		if(rageMax > 0) attacker.res[1] = MIN(attacker.res[1] + MasterStat(ac, STAT_RAGE_ATTACK_RECOVERY_VALUE), rageMax);
	}

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
		replication->SendScoreToAll(attacker.userID, attacker.team, attacker.kills, teamKills, target.userID, target.deaths);
		replication->SendDeathToAll(attacker.Main().UID, chara.UID, CreatureIndex(100000000 + (i32)chara.classType), target.userID, RESPAWN_DELAY_MS);
	}
}

void World::RespawnPlayer(Player& player)
{
	player.dead = false;
	// pools back to their start values (initialised again on the next update)
	player.mana = -1.0f;
	player.benchMana = -1.0f;
	player.stamina = -1.0f;
	player.res = { -1, -1, -1 };
	player.benchRes = { -1, -1, -1 };
	player.resSent = { -1, -1, -1 };
	player.tNextManaSend = Time::ZERO;
	player.tNextStaminaSend = Time::ZERO;
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
	if(player.passivesApplied) ApplyPassives(player);
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
