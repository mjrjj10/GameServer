#include "config.h"
#include "world.h"
#include <mxm/game_content.h>
#include "world_common.h"

static const f32 COMBAT_ATTACK_STAT = 0.0f;
static const i32 STAT_HEALTH = 0;
static const i32 STAT_STAMINAREGEN = 5;
static const i32 STAT_CRITICALRATE = 9;
static const i32 STAT_CRITICALDAMAGE = 10;
static const i32 STAT_RAGE_ATTACK_RECOVERY_VALUE = 51;
static const i32 STAT_MANAREGEN = 39;
static const i32 STAT_ULTIMATEREGEN = 42;
static const i32 STAT_SKILL_CRITICALRATE = 63;
static const i32 STAT_SKILL_CRITICALDAMAGE = 64;
static const u32 PACKET_DAMAGE_TYPE_NORMAL = 0;
static const u32 PACKET_DAMAGE_TYPE_CRITICAL = 2;
static const i32 STAT_ELEMENT_DAMAGE_BONUS = 54;
static const i32 STAT_ELEMENT_SKILL_DAMAGE_BONUS = 55;


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
	if(ElementBeats(ea, et)) return 1.0f + MasterStat(ac, stat) / 100.0f;
	if(ElementBeats(et, ea)) return 1.0f - MasterStat(target.Main().classType, stat) / 100.0f;
	return 1.0f;
}

static const i32 WEAPON_HIT_DAMAGE = 120;
static const i32 STAT_DEFENCE = 7;
static const i32 STAT_DEFENCE_PENETRATION = 20;
static const i32 STAT_SKILL_PENETRATION = 21;
static const i32 STAT_SKILLDAMAGE_REDUCE_RATIO = 58;
static const i32 STAT_TOUGHNESS = 65;
static const f32 DEFENCE_SCALE = 100.0f;
static const f32 PENETRATION_CAP = 50.0f;
static const f32 SKILLDAMAGE_REDUCE_CAP = 80.0f;
static const f64 WEAPON_HIT_INTERVAL_MS = 80.0;
static const f32 WEAPON_PROJECTILE_SPEED = 4000.0f;
static const f32 WEAPON_MELEE_MAX_RANGE = 600.0f;
static const f32 WEAPON_MELEE_HALF_ANGLE = 1.05f;
static const f32 WEAPON_NODE_MAX_FORWARD = 250.0f;
static const f32 WEAPON_RANGE_SCALE = 1.1f;
static const f32 WEAPON_TARGET_RADIUS = 110.0f;
static const f32 PROJECTILE_TARGET_RADIUS = 70.0f;
static const f32 PROJECTILE_HIT_TOLERANCE = 20.0f;
static const i32 CLASS_NUM_LILU = 22;
static const i32 CLASS_NUM_ANDROB = 30;
static const i32 CLASS_NUM_ATTACKER = 31;
static const i32 CLASS_NUM_BOXER = 32;
static const i32 CLASS_NUM_SHUGOTRADER = 33;
static const f32 TOOLTIP_DEFENDER_CC_IMMUNE_SEC = 1.0f;
static const f32 TOOLTIP_DEATHKNIGHT_REVIVE_HP = 0.30f;
static const i32 TOOLTIP_LILU_EVERY_N = 4;
static const f32 TOOLTIP_SNIPER_CRIT_PER_MARK = 33.0f;
static const f32 COMBO_SLOT_WINDOW_SEC = 5.0f;
static const i32 SKILL_SLOT_ENUM_FIRST = 2;
static const f32 OBSTRUCT_LOW_HEIGHT = 30.0f;
static const f64 SELF_DRAG_LINK_SEC = 0.6;
static const i32 ULTIMATE_SKILL_SUFFIX = 50;
static const f32 REPEAT_HIT_REDUCTION = 0.1f;
static const f32 REPEAT_HIT_REDUCTION_CAP = 0.4f;
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
static const i32 RELAY_STEALTH_STATUS = 1230210030;
static const i32 SKILL_SLOT_ENUM_UG = 6;
static const i32 TOOLTIP_ANDROB_HEAL = 250;
static const f32 TOOLTIP_ANDROB_RANGE = 800.0f;
static const f32 TOOLTIP_ATTACKER_WEAPON_MUL = 1.10f;
static const f32 TOOLTIP_BOXER_MARK_SEC = 1.5f;
static const f32 TOOLTIP_BOXER_WEAPON_MUL = 1.15f;
static const i32 TOOLTIP_SHUGO_EVERY_N = 6;
static const f32 TOOLTIP_SHUGO_WEAPON_MUL = 1.80f;
static const i32 CLASS_NUM_ELECTRO = 4;
static const i32 CLASS_NUM_PHOTOG = 15;
static const i32 CLASS_NUM_EFREET = 23;
static const i32 CLASS_NUM_SHADOWHUNTER = 24;
static const i32 CLASS_NUM_RYTLOCK = 25;
static const i32 CLASS_NUM_CATTHECAT = 26;
static const i32 CLASS_NUM_ANDROA = 29;
static const i32 STATUS_ELECTRO_PASSIVE_BUFF_MPREGEN = 1230040011;
static const i32 STATUS_PHOTOG_PASSIVE_MOVESPEED = 1230150010;
static const i32 STATUS_EFREET_PASSIVE_DOT_FIRE = 1230230001;
static const i32 STATUS_SHADOWHUNTER_HIDE = 1230240000;
static const i32 STATUS_SHADOWHUNTER_COOLDOWN = 1230240001;
static const i32 STATUS_RYTLOCK_ATTACK_BLEEDING = 1230250002;
static const i32 STATUS_THECAT_PASSIVE_FIREDELAY1 = 1230260000;
static const i32 STATUS_ANDROIDA_PASSIVE_ATTACKSPEED = 1230290001;
static const i32 STATUS_SHUGOTRADER_PASSIVE_REMOTE = 1230330000;
static const i32 TOOLTIP_RYTLOCK_CHAIN = 3;
static const i32 WEAPON_STATE_CHARGE = 7;
static const i32 REMOTE_SHUTTLE_DODGE = 1440410000;
static const f32 TOOLTIP_SHUTTLE_CHARGED_FUEL = 20.0f;
static const f32 TOOLTIP_SHUTTLE_CHARGING_FUEL_SEC = 5.0f;
static const f32 TOOLTIP_SHUTTLE_DODGE_FUEL = 22.0f;
static const f32 TOOLTIP_SHUTTLE_TELEPORT_SEC = 3.0f;
static const i32 STATUS_SHUTTLE_TELEPORT_RESIST = 1230410053;
static const i32 STATUS_SHUTTLE_EMP_DRAIN = 1230410020;
static const i32 STATUS_SHUTTLE_SHOCKWAVE = 1230410030;
static const i32 REMOTE_SHUTTLE_SHOCKWAVE = 1440410030;
static const f32 TOOLTIP_SHUTTLE_EMP_DRAIN = 0.30f;
static const f32 TELEPORT_SPREAD = 80.0f;
static const i32 INTERACTION_SHUTTLE_BOARD = 900000071;
static const i32 INTERACTION_NOT_OCCUPIED = -1;
static const u32 PLAYER_FACTION_BASE = 3;
static const i32 DYNAMIC_SHUTTLE_BOARD_POINT = 110041070;
static const i32 BOARD_POINT_LOCAL_BASE = 41000;
static const f32 TOOLTIP_SHUTTLE_BOARD_WINDOW_SEC = 3.0f;
static const i32 STATUS_SHUTTLE_BOARD_END = 1230410047;
static const f32 RIDE_SYNC_SEC = 0.1f;
static const i32 FIRST_PORTED_CLASS = CLASS_NUM_SHUTTLE;
static const f32 TOOLTIP_RYTLOCK_THIRD_MUL = 1.20f;
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

		if(!p.dead && p.dashDone < p.dashDist) {
			const f32 total = (f32)TimeDiffSec(TimeDiff(p.dashStartAt, p.dashUntil));
			const f32 frac = total > 0 ? MIN((f32)TimeDiffSec(TimeDiff(p.dashStartAt, localTime)) / total, 1.0f) : 1.0f;
			const f32 target = p.dashCurve ? p.dashDist * p.dashCurve->Eval(frac) : p.dashDist * frac * frac;
			const f32 ahead = glm::dot(vec2(p.dashOrigin) + p.dashDir * target - vec2(body.GetWorldPos()), p.dashDir);
			if(p.dashCurve ? fabsf(ahead) > 0.01f : ahead > 0) physics.Move(&body, vec3(p.dashDir * ahead, 0), (f32)tdelta);
			p.dashDone = frac >= 1.0f ? p.dashDist : (p.dashCurve ? MIN(fabsf(target), p.dashDist - 0.01f) : target);
			p.input.moveTo = body.GetWorldPos();
		}

		if(!p.dead && p.clientHd == ClientHandle::INVALID && p.dragElapsed < p.dragDuration) {
			const f32 dt = MIN((f32)tdelta, p.dragDuration - p.dragElapsed);
			const f32 accel = (p.dragMax - p.dragMin) / p.dragDuration;
			const f32 t0 = p.dragElapsed;
			const f32 t1 = t0 + dt;
			const f32 step = (t1 * t1 - t0 * t0) * accel * 0.5f + p.dragMin * dt;
			const vec2 rel = p.dragCenter - vec2(body.GetWorldPos());
			const f32 dist = glm::length(rel);
			p.dragElapsed = t1;
			if(dist > 0.001f) {
				if(dist * dist < step * step) {
					physics.Move(&body, vec3(rel, 0), (f32)tdelta);
					p.dragElapsed = p.dragDuration;
				}
				else physics.Move(&body, vec3(rel / dist * step, 0), (f32)tdelta);
			}
			p.input.moveTo = body.GetWorldPos();
		}

		if(!p.dead && p.clientHd == ClientHandle::INVALID && localTime < p.ccUntil) {
			const ActiveStatus* fear = nullptr;
			foreach_const(st, activeStatuses) {
				if(st->targetIndex == p.index && st->info && localTime < st->until && st->info->type == "STATUS_TYPE_FEAR") { fear = &*st; break; }
			}
			if(localTime < p.pushUntil) {
				p.input.moveTo = p.pushTarget;
				p.input.speed = p.pushSpeed;
			}
			else if(fear) {
				if(localTime >= p.fearNextTurn) {
					const f32 a = (f32)(RandUint() % 3600) * PI / 1800.0f;
					p.fearDir = vec2(cosf(a), sinf(a));
					p.fearNextTurn = TimeAdd(localTime, TimeMsToTime(MAX(fear->info->intervalSec, 0.1f) * 1000.0));
				}
				p.input.moveTo = body.GetWorldPos() + vec3(p.fearDir * 500.0f, 0);
				p.input.speed = MasterStat(p.Main().classType, STAT_MOVESPEED);
				const f32 yaw = atan2f(p.fearDir.y, p.fearDir.x);
				p.input.rot.upperYaw = yaw;
				p.input.rot.bodyYaw = yaw;
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
			if(p.clientHd != ClientHandle::INVALID && p.casting.skillID != SkillID::INVALID && p.casting.type == GameXmlContent::SkillCast::LIMIT) ReleaseCast(p);
			else if(WeaponIsMelee(p) && localTime > p.tNextWeaponHit) {
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
			const vec2 shotDir = glm::length(it->pendingShotDir) < 0.1f ? vec2(cosf(it->weaponAimYaw), sinf(it->weaponAimYaw)) : it->pendingShotDir;
			PlayerWeaponShot(*it, it->pendingShotOrigin, shotDir, it->pendingShotDistance);
		}
		if(it->pendingAim && TimeDiffSec(TimeDiff(it->pendingAimTime, localTime)) > 0.15) {
			it->pendingAim = false;
		}
	}

	UpdateProjectiles((f32)tdelta);
	UpdateRemotes((f32)tdelta);
	UpdateUltimateGauges((f32)tdelta);
	UpdateStatuses();
	UpdateToggles();
	UpdateSummons();
	UpdateSummonAi((f32)tdelta);
	if(!frozen) UpdatePassives();
	if(!frozen) UpdateStances();
	if(!frozen) UpdateCasts();
	if(!frozen) UpdateRides();
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
			rch.skillMoving = localTime < player.dashUntil;

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
	main.hpMax = (i32)MasterStat(main.classType, STAT_HEALTH);
	main.hp = main.hpMax;
	main.seed = (i32)RandUint();




	sub.parent = &player;
	sub.classType = player.subClass;
	sub.skinIndex = player.subSkin;
	sub.hpMax = (i32)MasterStat(sub.classType, STAT_HEALTH);
	sub.hp = sub.hpMax;
	ASSERT(main.hpMax > 0 && sub.hpMax > 0);
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

static f32 SkillMoveDuration(const GameXmlContent& content, ClassType cls, const GameXmlContent::Action& action, i32 animIndex)
{
	f32 animLen = 0;
	foreach_const(st, action.anims) {
		if(st->index != animIndex) continue;
		animLen = content.GetAniSeqTime(content.GetMaster(cls).className.c_str(), st->animationId) * (f32)MAX(st->loop, 1);
		if(animLen > 0) break;
	}
	f32 attachedLife = 0;
	foreach_const(cmd, action.commands) {
		if(cmd->type != ActionCommand::Type::REMOTE || (cmd->animIndex != animIndex && cmd->animIndex != -1)) continue;
		const Remote* r = content.FindRemote(cmd->remote.idx);
		if(!r || !r->attachFireObject || r->lifeTime <= 0) continue;
		const f32 life = (cmd->remote.removeAtAnimEnd && animLen > 0) ? MIN(r->lifeTime, animLen) : r->lifeTime;
		attachedLife = MAX(attachedLife, life);
	}
	if(attachedLife > 0) return attachedLife;
	if(animLen > 0) return animLen;
	return action.seqLength;
}

void World::PlayerCastSkill(Player& player, SkillID skillIn, const vec3& castPosIn, Slice<const ActorUID> targets, f32 clientTime, ActionStateID forcedAction)
{
	SkillID skillID = skillIn;
	if(player.clientHd != ClientHandle::INVALID && forcedAction == ActionStateID::INVALID && player.comboSlot >= 0 && skillIn == player.comboBase && localTime < player.comboUntil && player.comboSkill != SkillID::INVALID) {
		LOG("Combo :: %ls recast %d -> combo %d", player.name.data(), (i32)skillIn, (i32)player.comboSkill);
		skillID = player.comboSkill;
	}
	vec3 castPos = castPosIn;
	if(player.clientHd != ClientHandle::INVALID && forcedAction == ActionStateID::INVALID && player.comboSlot >= 0 && skillID == player.comboSkill && glm::length(vec2(player.comboCastPos)) > 1.0f) {
		const GameXmlContent::SkillInput* comboIn = GetGameXmlContent().FindSkillInput((i32)skillID);
		if(comboIn && comboIn->selfPosition) castPos = player.comboCastPos;
	}
	if(player.clientHd == ClientHandle::INVALID && forcedAction == ActionStateID::INVALID && glm::length(vec2(castPos)) > 1.0f) {
		const GameXmlContent::SkillInput* in = GetGameXmlContent().FindSkillInput((i32)skillID);
		const vec2 from = vec2(player.body->GetWorldPos());
		const vec2 rel = vec2(castPos) - from;
		const f32 len = glm::length(rel);
		if(in && len > 1.0f) {
			f32 clamped = len;
			if(in->minDistance > 0) clamped = MAX(clamped, in->minDistance);
			if(in->maxDistance > 0) clamped = MIN(clamped, in->maxDistance);
			if(clamped != len) castPos = vec3(from + rel / len * clamped, castPos.z);
		}
	}
	if(frozen) return;
	if(player.Main().classType == ClassType::SLIME && targets.size() > 0) {
		const GameXmlContent::SkillInput* pickIn = GetGameXmlContent().FindSkillInput((i32)skillID);
		const ActorMaster* picked = pickIn && pickIn->pickedEntity ? FindMasterActor(targets[0]) : nullptr;
		if(picked && picked->parent != &player && picked->classType == ClassType::SLIME) {
			LOG("Skill :: %ls skill=%d refused: cannot cling to another Bumaro (ERROR_TYPE_SKILL_CANT_USE_SAME_MASTER)", player.name.data(), (i32)skillID);
			return;
		}
	}
	eastl::fixed_vector<ActorUID,10,false> allEnemyTargets;
	if(const GameXmlContent::SkillInput* selIn = GetGameXmlContent().FindSkillInput((i32)skillID)) {
		if(selIn->allEnemies) {
			foreach_const(e, players) if(e->team != player.team && !e->dead) allEnemyTargets.push_back(e->Main().UID);
			targets = Slice<const ActorUID>(allEnemyTargets.data(), allEnemyTargets.size());
		}
	}
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
	if(Config().DevMirror && !followUp && player.clientHd != ClientHandle::INVALID && player.index == showcase.prevCaster && !showcase.castSeen) {
		showcase.castSeen = true;
		showcase.castAt = localTime;
	}
	if(!followUp && content.FindSkillToggle((i32)skillID)) {
		if(player.toggleSkill == skillID) {
			ToggleOff(player);
			return;
		}
		ToggleOn(player, skillID, castPos);
	}
	if(!followUp && player.casting.skillID == skillID) {
		ReleaseCast(player);
		return;
	}
	const GameXmlContent::SkillCast* cast = followUp ? nullptr : content.FindSkillCast((i32)skillID);
	castCostLevel = nullptr;
	if(!followUp) {
		if(const auto* levels = content.FindSkillCostLevels((i32)skillID)) {
			for(const GameXmlContent::CostLevel& lvl : *levels) {
				bool affordable = lvl.consume[0] > 0 || lvl.consume[1] > 0 || lvl.consume[2] > 0;
				for(i32 r = 0; r < 3; r++) if(lvl.consume[r] > 0 && player.res[r] < lvl.consume[r]) affordable = false;
				if(affordable) castCostLevel = &lvl;
			}
		}
	}
	const ActionStateID actionState = followUp ? forcedAction : castCostLevel && castCostLevel->action != ActionStateID::INVALID ? castCostLevel->action : (cast ? cast->action : skill.action);

	if(!followUp) {
		const bool shirk = GetGameXmlContent().GetSkillConsumeStamina((i32)skillID) > 0;
		if(IsBlocked(player, false, !shirk, shirk)) {
			LOG("Blocked :: %ls skill=%d (input blocked by a status)", player.name.data(), (i32)skillID);
			return;
		}
		if(player.clientHd == ClientHandle::INVALID) {
			const GameXmlContent::SkillInput* obIn = content.FindSkillInput((i32)skillID);
			if(obIn && (obIn->obstructMove != 0 || obIn->obstructSight)) {
				vec3 to = castPos;
				if(targets.size() > 0) { if(const ActorMaster* tm = FindMasterActor(targets[0])) to = tm->parent->body->GetWorldPos(); }
				const vec3 from = player.body->GetWorldPos();
				if(glm::length(vec2(to)) > 1.0f && glm::length(vec2(to) - vec2(from)) > 1.0f) {
					const f32 height = obIn->obstructMove == 1 ? OBSTRUCT_JUMPABLE_HEIGHT : OBSTRUCT_LOW_HEIGHT;
					const vec2 along = glm::normalize(vec2(to) - vec2(from));
					const vec2 side = vec2(-along.y, along.x);
					bool blocked = false;
					for(f32 off : { 0.0f, -OBSTRUCT_SIDE, OBSTRUCT_SIDE }) {
						f32 hitDist = 0;
						const bool hit = physics.RaycastStatic(vec3(vec2(from) + side * off, from.z + height), vec3(vec2(to) + side * off, from.z + height), &hitDist);
						if(hit) blocked = true;
					}
					if(blocked) {
						LOG("Blocked :: %ls skill=%d obstacle between caster and target", player.name.data(), (i32)skillID);
						return;
					}
				}
			}
		}
		if(!PlayerPaySkillCost(player, (i32)skillID)) return;
		if(castCostLevel) {
			for(i32 r = 0; r < 3; r++) {
				if(castCostLevel->consume[r] > 0 && player.res[r] >= 0) { player.res[r] -= castCostLevel->consume[r]; player.tNextResSend = Time::ZERO; }
			}
			LOG("Cost :: %ls skill=%d empowered level (consumed rp=%.0f bb=%.0f, %d remotes / %d statuses replaced, damage x%.3f)", player.name.data(), (i32)skillID,
				castCostLevel->consume[1], castCostLevel->consume[2], (i32)castCostLevel->remoteReplace.size(), (i32)castCostLevel->statusReplace.size(), castCostLevel->damageRatio);
		}
		PassiveOnSkillCast(player, (i32)skillID);
		if(const ActiveStatus* gx = FindPassiveOfType(player, "STATUS_TYPE_GX09")) {
			if(gx->info->ParamInt("_ShuttleSkillIndex") == (i64)skillID) {
				player.boardOpenUntil = TimeAdd(localTime, TimeMsToTime(TOOLTIP_SHUTTLE_BOARD_WINDOW_SEC * 1000.0));
				RemoveBoardPoint(player);
				static i32 boardPointLocal = BOARD_POINT_LOCAL_BASE;
				ActorDynamic& point = SpawnDynamic(CreatureIndex(DYNAMIC_SHUTTLE_BOARD_POINT), boardPointLocal++);
				point.pos = player.body->GetWorldPos();
				point.rot = vec3(0, 0, player.input.rot.upperYaw);
				player.boardPointUID = point.UID;
				player.boardPointAnnounceAt = TimeAdd(localTime, TimeMsToTime(200));
				LOG("Interaction :: %ls opens boarding for %.0fs", player.name.data(), TOOLTIP_SHUTTLE_BOARD_WINDOW_SEC);
			}
		}
	}

	f32 angle = player.input.rot.upperYaw;
	if(skill.type == SkillType::SHIRK) {
		const vec2 moving = vec2(player.input.moveTo) - vec2(player.body->GetWorldPos());
		if(glm::length(moving) > 10.0f) angle = atan2f(moving.y, moving.x);
	}
	const vec2 dir = vec2(cosf(angle), sinf(angle));

	Replication::SkillCast rpCast;
	rpCast.clientHd = player.clientHd;
	rpCast.casterUID = player.Main().UID;
	rpCast.skillID = skillID;
	rpCast.castPos = castPos;
	rpCast.actionID = actionState;
	rpCast.sendAction = cast != nullptr;

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
	prog.stance = player.Main().stance;
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
	vec2 gotoDir = vec2(0);

	const auto& action = content.GetSkillAction(player.Main().classType, actionState, player.Main().stance);

	foreach_const(cmd, action.commands) {
		switch(cmd->type) {
			case ActionCommand::Type::GRAPH_MOVE_HORZ: {
				distance = cmd->graphMoveHorz.distance;
				moveDuration = SkillMoveDuration(content, player.Main().classType, action, cmd->animIndex);
			} break;

			case ActionCommand::Type::MOVE: {
				switch(cmd->move.preset) {
					case ActionCommand::MovePreset::WARP: {
						distance = (f32)cmd->move.param2;
						moveDuration = 0.01f; // warping
					} break;
					case ActionCommand::MovePreset::GOTO_CURRENT_TARGET:
					case ActionCommand::MovePreset::GOTO_CURRENT_TARGET_CHASE:
					case ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH:
					case ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH_CHASE: {
						const ActorMaster* tm = targets.size() > 0 ? FindMasterActor(targets[0]) : nullptr;
						if(tm && distance == 0) {
							const vec2 rel = vec2(tm->parent->body->GetWorldPos()) - vec2(player.body->GetWorldPos());
							const f32 len = glm::length(rel);
							if(len > 1.0f) {
								const bool through = cmd->move.preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH || cmd->move.preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH_CHASE;
								gotoDir = rel / len;
								distance = through ? len : MAX(len - HitRadius(*tm->parent) - HitRadius(player), 0.0f);
								moveDuration = action.seqLength;
							}
						}
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
	rpExec.casterClientHd = player.clientHd;
	rpExec.castStart = cast != nullptr;
	rpExec.startPos = player.body->GetWorldPos();
	rpExec.moveDir = dir;
	rpExec.rot = { angle, 0, angle };
	rpExec.speed = player.input.speed; // FIXME: should not come from input

	if(distance > 0 && glm::length(gotoDir) < 0.5f && glm::length(vec2(castPos)) > 1.0f) {
		foreach_const(cmd, action.commands) {
			if(cmd->type == ActionCommand::Type::MOVE && (cmd->move.preset == ActionCommand::MovePreset::TARGET_POS || cmd->move.preset == ActionCommand::MovePreset::TARGET_POS_LEAP)) {
				const vec2 rel = vec2(castPos) - vec2(player.body->GetWorldPos());
				const f32 len = glm::length(rel);
				distance = len;
				if(len > 1.0f) gotoDir = rel / len;
			}
		}
	}
	if(distance != 0) {
		const vec2 moveDir = glm::length(gotoDir) > 0.5f ? gotoDir : dir;
		const vec3 endPos = physics.FindMovePos(player.body, vec3(moveDir * distance, 0), moveDuration);
		rpExec.moveDir = moveDir;
		rpExec.endPos = endPos;
	}

	replication->FramePushSkillExec(rpExec);

	const bool castRelease = followUp && forcedAction == skill.action;
	if(castRelease && player.lastComboSkill == skillID) player.lastComboSkill = SkillID::INVALID;
	const bool remotesScheduled = ScheduleSkillRemotes(player, skillID, castPos, targets, angle, followUp && !castRelease ? 0 : -1, cast ? cast->action : forcedAction);
	castCostLevel = nullptr;
	if(cast) {
		const bool bot = player.clientHd == ClientHandle::INVALID;
		f32 wait = cast->time;
		if(bot && cast->type != GameXmlContent::SkillCast::DELAY) wait = MIN(wait, action.seqLength);
		player.casting.skillID = skillID;
		player.casting.castPos = castPos;
		player.casting.target = targets.size() > 0 ? targets[0] : ActorUID::INVALID;
		player.casting.startPos = vec2(player.body->GetWorldPos());
		player.casting.executeAt = TimeAdd(localTime, TimeMsToTime(wait * 1000.0));
		player.casting.type = (u8)cast->type;
		player.casting.interruptByMove = cast->interruptByMove;
		LOG("Cast :: %ls skill=%d start action=%d type=%d wait=%.2f", player.name.data(), (i32)skillID, (i32)cast->action, (i32)cast->type, wait);
		if(const ActiveStatus* gx = FindPassiveOfType(player, "STATUS_TYPE_GX09")) {
			if(gx->info->ParamInt("_MassiveTeleportIndex") == (i64)skillID) {
				player.massTeleportAt = TimeAdd(localTime, TimeMsToTime(TOOLTIP_SHUTTLE_TELEPORT_SEC * 1000.0));
				player.massTeleportPos = castPos;
			}
		}
	}
	skillProgramList.back().passes = lastAnimPasses;
	bool effectCommands = false;
	foreach_const(cmd, action.commands) {
		if(cmd->type == ActionCommand::Type::STATUS || cmd->type == ActionCommand::Type::STATUS_SKILL_TARGET || cmd->type == ActionCommand::Type::ENTITY) effectCommands = true;
	}
	if(!remotesScheduled && !followUp && !effectCommands && !cast) {
		PlayerSkillDamage(player, skillID, castPos, targets, angle);
	}

	if(!followUp && player.clientHd != ClientHandle::INVALID) {
		if(player.comboSlot >= 0 && skill.type == SkillType::COMBO) {
			replication->SendChangeSkillSlot(player.clientHd, player.Main().UID, player.comboSlot, player.comboBase, 0);
			ComboBaseCooldown(player);
			LOG("Combo :: %ls slot %d back to %d", player.name.data(), player.comboSlot, (i32)player.comboBase);
			player.comboSlot = -1;
		}
		const i32 combo = content.GetFirstComboSkill((i32)skillID);
		if(combo != 0) {
			const i32 base = (i32)player.mainCharaID * 2;
			i32 slot = -1;
			const i32 order = ((i32)skillID / 10) % 10;
			if((player.skills[base] == skillID || player.skills[base + 1] == skillID) && order >= 1 && order <= 4) slot = SKILL_SLOT_ENUM_FIRST + order - 1;
			else if(order == 5 && (i32)skillID % 10 == 0) slot = SKILL_SLOT_ENUM_UG;
			if(slot >= 0) {
				const GameXmlContent::SkillInput* baseIn = content.FindSkillInput((i32)skillID);
				player.comboPending = SkillID(combo);
				player.comboSkill = SkillID(combo);
				player.comboSendAt = TimeAdd(localTime, TimeMsToTime(50.0));
				player.comboResetCooltime = baseIn ? baseIn->coolTimeID : 0;
				player.comboSlot = slot;
				player.comboBase = skillID;
				player.comboCastPos = castPos;
				player.comboUntil = TimeAdd(localTime, TimeMsToTime(COMBO_SLOT_WINDOW_SEC * 1000.0));
				LOG("Combo :: %ls slot %d -> %d", player.name.data(), slot, combo);
				const i32 autoMs = content.GetComboAutoCastMs(combo);
				player.comboAutoAt = autoMs > 0 ? TimeAdd(localTime, TimeMsToTime((f64)autoMs)) : Time::ZERO;
			}
		}
	}
	if(!followUp) {
		if(const GameXmlContent::SkillStance* st = content.FindSkillStance((i32)skillID)) SetStance(player.Main(), st->stance, st->duration, st->weaponFireCount, st->cancelByDefeat);
	}

	const i32 child = content.GetChildSkill(player.Main().classType, actionState);
	if(child > 0 && content.IsChildOnExisting(player.Main().classType, actionState)) {
		ChildSkillOnExisting(player, child, castPos);
	}
	else if(child > 0) {
		player.pendingChildSkill = child;
		player.pendingChildSkillUntil = TimeAdd(localTime, TimeMsToTime(2000));
		player.pendingChildPos = castPos;
		player.pendingChildTarget = targets.size() > 0 ? targets[0] : ActorUID::INVALID;
	}
}

static void BotWeaponAnimation(const World::Player& player, Replication* replication)
{
	if(player.clientHd != ClientHandle::INVALID) return;
	const WeaponIndex weapon = (player.mainCharaID & 1) == 0 ? player.mainWeapon : player.subWeapon;
	replication->RelayWeaponState(ClientHandle::INVALID, player.Main().UID, (i32)weapon, 2, player.chargeLevel, player.firingCombo);
}

void World::PlayerWeaponFire(Player& player)
{
	BotWeaponAnimation(player, replication);
	const f32 range = ClassAttackRange(player.Main().classType) * WEAPON_RANGE_SCALE;
	const vec2 dir = vec2(cosf(player.weaponAimYaw), sinf(player.weaponAimYaw));
	const vec3 origin = player.body->GetWorldPos();

	if(Config().DevMirror && player.clientHd != ClientHandle::INVALID && player.index == showcase.prevCaster && !showcase.castSeen) {
		showcase.castSeen = true;
		showcase.castAt = localTime;
	}
	if(player.clientHd != ClientHandle::INVALID && player.casting.skillID != SkillID::INVALID && player.casting.type == GameXmlContent::SkillCast::LIMIT) {
		ReleaseCast(player);
		return;
	}
	if(WeaponIsMelee(player)) {
		const i8 swingStance = player.Main().stance;
		StanceWeaponUse(player);
		const auto& content = GetGameXmlContent();
		const u32 m = player.mainCharaID & 1;
		const WeaponIndex weapon = m == 0 ? player.mainWeapon : player.subWeapon;
		const WeaponFiring* wf = content.FindWeaponFiring((i32)weapon, swingStance);
		RemoteIdx swing = wf ? wf->base : RemoteIdx::INVALID;
		if(wf && player.firingCombo > 0 && player.firingCombo <= wf->combo.size()) swing = wf->combo[player.firingCombo - 1];
		const Remote* swingRemote = content.FindRemote(swing);
		if(swingRemote && swingRemote->boundType != Remote::BoundType::E_BOUND_NONE) {
			LOG("Shot :: %ls melee swing remote %d (stance %d)", player.name.data(), (i32)swing, (i32)swingStance);
			SpawnRemote(player.index, swing, origin + vec3(dir * swingRemote->offsetY, 0), dir, ActorUID::INVALID, SkillID::INVALID, nextCastID++, (i32)WeaponHitDamage(player, nullptr), true, nullptr, 0, player.clientHd != ClientHandle::INVALID);
			return;
		}
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
			DamagePlayer(player, *it, SkillID::INVALID, (i32)WeaponHitDamage(player, nullptr));
		}
	}
}

void World::PlayerWeaponShot(Player& player, const vec3& origin, const vec2& dir, f32 distance)
{
	if(player.dead) return;
	const ClassType classType = player.Main().classType;
	if(WeaponIsMelee(player)) return;
	BotWeaponAnimation(player, replication);
	if((i32)classType == CLASS_NUM_SHUTTLE && player.chargeLevel > 0) AddFuel(player, TOOLTIP_SHUTTLE_CHARGED_FUEL);
	if(player.clientHd != ClientHandle::INVALID && player.casting.skillID != SkillID::INVALID && player.casting.type == GameXmlContent::SkillCast::LIMIT) {
		ReleaseCast(player);
		return;
	}
	if(Config().DevMirror && player.clientHd != ClientHandle::INVALID && player.index == showcase.prevCaster && !showcase.castSeen) {
		showcase.castSeen = true;
		showcase.castAt = localTime;
	}
	const i8 shotStance = player.Main().stance;
	StanceWeaponUse(player);
	if(projectileList.full()) return;

	const auto& content = GetGameXmlContent();
	const Remote* remote = content.FindRemote(RemoteIdx(1430000000 + (i32)classType * 10000 + 10));

	f32 aimMax = WEAPON_SHOT_MAX_DISTANCE;
	f32 aimMin = 0;
	bool spawnAtAim = false;
	{
		const u32 m = player.mainCharaID & 1;
		const WeaponIndex weapon = m == 0 ? player.mainWeapon : player.subWeapon;
		const WeaponFiring* wf = content.FindWeaponFiring((i32)weapon, shotStance);
		if(wf) {
			if(wf->maxDistance > 0) aimMax = wf->maxDistance;
			aimMin = wf->minDistance;
			spawnAtAim = wf->spawnAtAim;
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
	if(spawnAtAim && remote) {
		f32 reach = MAX(MIN(distance > 1.0f ? distance : ClassAttackRange(classType) * WEAPON_RANGE_SCALE, aimMax), aimMin);
		if(player.pendingShotRawSize > 0) {
			static i32 aimDumps = 0;
			if(aimDumps++ < 12) {
				eastl::fixed_string<char,700,true> hex;
				for(i32 k = 0; k < player.pendingShotRawSize && k < 300; k++) hex.append_sprintf("%02x", player.pendingShotRaw[k]);
				LOG("Shot :: %ls aim-spawn packet origin=(%.0f, %.0f, %.0f) dist=%.0f raw=%s", player.name.data(), origin.x, origin.y, origin.z, distance, hex.c_str());
			}
		}
		const vec3 aimPos = vec3(vec2(origin) + dir * reach, player.body->GetWorldPos().z);
		replication->SendAttackActionToOthers(player.clientHd, player.Main().UID, WorldYawToMxmYaw(atan2f(dir.y, dir.x)));
		const bool bot = player.clientHd == ClientHandle::INVALID;
		if(!bot && player.pendingShotRawSize > 0) {
			replication->RelayRemoteCreate(player.clientHd, player.pendingShotRaw.data(), player.pendingShotRawSize, aimPos, remote->lifeTime, remote->ID);
			player.pendingShotRawSize = 0;
		}
		SpawnRemote(player.index, remote->ID, aimPos, dir, ActorUID::INVALID, SkillID::INVALID, nextCastID++, (i32)WeaponHitDamage(player, nullptr), bot);
		LOG("Shot :: %ls class=%d remote=%d at aimed point (%.0f, %.0f) reach=%.0f", player.name.data(), (i32)classType, (i32)remote->ID, aimPos.x, aimPos.y, reach);
		return;
	}

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

	proj.range = graph ? (reliable ? MIN(distance, aimMax) : MIN(classRange, aimMax)) : remoteRange;
	proj.graph = graph;
	proj.ignoreWalls = !remote || remote->ignoreGround;
	proj.wallZ = player.body->GetWorldPos().z + BodyMidHeight(player);
	proj.halfWidth = remote ? remote->boundSize[0] * 0.5f : 20.0f;
	proj.hitsLeft = remote ? MAX(remote->activateCount, 1) : 1;
	proj.forwardReach = (remote && remote->boundType == Remote::BoundType::E_BOUND_BOX) ? MAX(remote->offsetY + remote->boundSize[1] * 0.5f, 0.0f) : 0.0f;
	proj.aoeRadius = bomb ? bomb->boundSize[0] * 0.5f : 0;
	if(remote && remote->behaviorType == Remote::BehaviorType::HOMING && remote->angularVel > 0) {
		proj.angularVel = remote->angularVel;
		proj.homingLength = remote->homingLength;
		proj.homingHalfCos = (remote->homingAngle > 0 && remote->homingAngle < 360) ? cosf(remote->homingAngle * 0.5f * PI / 180.0f) : -1.0f;
		proj.homingTime = remote->homingTime;
		proj.homingTimer = remote->homingTime;
		proj.homingLockOn = remote->homingLockOn;
	}

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
		const u32 seed = nextRemoteSeed;
		nextRemoteSeed += 2;
		proj.remoteID = seed + 1;
		const vec3 firePos = proj.pos + vec3(0, 0, 158.0f);
		const vec3 target = firePos + vec3(proj.dir * proj.range, 0);
		replication->SendRemoteCreateToAll(player.Main().UID, seed, remote->ID, firePos, atan2f(proj.dir.y, proj.dir.x), target, ActorUID::INVALID, L"Attack_Fire_Dummy");
	}
	proj.damage = (i32)WeaponHitDamage(player, remote);
	LOG("Shot :: %ls class=%d remote=%d graph=%d reliable=%d dist=%.0f range=%.0f speed=%.0f width=%.0f hits=%d aoe=%.0f", player.name.data(), (i32)classType, remote ? (i32)remote->ID : -1, graph, reliable, distance, proj.range, proj.speed, proj.halfWidth, proj.hitsLeft, proj.aoeRadius);
	projectileList.push_back(proj);
}

void World::Explode(Player& owner, const vec3& center, f32 radius, i32 damage)
{
	LOG("Explosion :: at (%.0f, %.0f) radius=%.0f", center.x, center.y, radius);
	i32 mastersHit = 0;
	foreach(it, players) {
		if(it->team == owner.team || it->dead || it->untargetable) continue;
		const vec3 pos = it->body->GetWorldPos();
		if(fabsf(pos.z - center.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
		if(LagDistToPoint(*it, vec2(center)) > radius + HitRadius(*it) + WEAPON_HIT_TOLERANCE) continue;
		DamagePlayer(owner, *it, SkillID::INVALID, damage);
		mastersHit++;
	}
	if(mastersHit >= 2 && owner.mana >= 0) {
		if(const ActiveStatus* st = FindPassiveOfType(owner, "STATUS_TYPE_ARTILLERY")) {
			owner.mana = MIN(owner.mana + st->info->Param("_AddValuePC"), MasterStat(owner.Main().classType, STAT_MANA));
			owner.tNextManaSend = Time::ZERO;
			LOG("Passive :: %ls hit %d masters -> mana %.0f", owner.name.data(), mastersHit, owner.mana);
		}
	}
}

void World::SteerProjectile(Projectile& proj, f32 delta)
{
	const Player& owner = players[proj.ownerIndex];
	const auto targetable = [&](const Player& p) { return p.team != owner.team && !p.dead && !p.untargetable; };
	if(proj.homingTarget >= 0 && !targetable(players[proj.homingTarget])) proj.homingTarget = -1;
	proj.homingTimer += delta;
	if(proj.homingTimer >= proj.homingTime && !(proj.homingLockOn && proj.homingTarget >= 0)) {
		proj.homingTimer = 0;
		f32 bestDist = proj.homingLength;
		i32 found = -1;
		foreach_const(it, players) {
			if(!targetable(*it)) continue;
			const vec2 rel = vec2(it->body->GetWorldPos() - proj.pos);
			const f32 d = glm::length(rel);
			if(d > bestDist) continue;
			if(d > 1.0f && glm::dot(rel / d, proj.dir) < proj.homingHalfCos) continue;
			bestDist = d;
			found = (i32)it->index;
		}
		if(found >= 0 && found != proj.homingTarget) LOG("Shot :: homing projectile locked %ls at %.0f", players[found].name.data(), bestDist);
		proj.homingTarget = found;
	}
	if(proj.homingTarget < 0) return;
	const vec2 rel = vec2(players[proj.homingTarget].body->GetWorldPos() - proj.pos);
	if(glm::length(rel) < 1.0f) return;
	f32 turn = atan2f(rel.y, rel.x) - atan2f(proj.dir.y, proj.dir.x);
	while(turn > PI) turn -= 2 * PI;
	while(turn < -PI) turn += 2 * PI;
	const f32 maxTurn = proj.angularVel * delta;
	turn = MAX(MIN(turn, maxTurn), -maxTurn);
	const f32 c = cosf(turn);
	const f32 sn = sinf(turn);
	proj.dir = vec2(proj.dir.x * c - proj.dir.y * sn, proj.dir.x * sn + proj.dir.y * c);
}

void World::UpdateProjectiles(f32 delta)
{
	for(auto proj = projectileList.begin(); proj != projectileList.end(); ) {
		Player& owner = players[proj->ownerIndex];
		if(proj->angularVel > 0) SteerProjectile(*proj, delta);
		f32 step = MIN(proj->speed * delta + proj->pendingAdvance, proj->range - proj->travelled);
		proj->pendingAdvance = 0;
		bool hitWall = false;
		if(!proj->graph && !proj->ignoreWalls && step > 0) {
			f32 wallDist = 0;
			const vec2 a = vec2(proj->pos);
			if(physics.RaycastStatic(vec3(a, proj->wallZ), vec3(a + proj->dir * step, proj->wallZ), &wallDist, (i32)players[proj->ownerIndex].team)) {
				step = wallDist;
				hitWall = true;
			}
		}
		const bool lastStep = hitWall || proj->travelled + step >= proj->range - 0.01f;
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
				if(!LagSegmentHits(*it, from, to, proj->halfWidth + HitRadius(*it) + PROJECTILE_HIT_TOLERANCE, lastStep ? proj->forwardReach + HitRadius(*it) : HitRadius(*it))) continue;

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
			destroyed = hitWall || proj->travelled >= proj->range - 0.01f;
			if(hitWall) LOG("Shot :: projectile stopped by a wall after %.0f", proj->travelled);
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
			else if(proj->graph && proj->hitsLeft > 0) {
				Explode(owner, proj->pos, proj->halfWidth, proj->damage);
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
	const f32 ttBase = content.GetSkillBaseDamage((i32)skillID);
	const f32 base = ttBase > 0 ? ttBase : level.getBaseDamage();
	const f32 damage = base + COMBAT_ATTACK_STAT * level.getAttackMultiplier();
	return damage > 0 ? damage : (f32)SKILL_FALLBACK_DAMAGE;
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

static const WeaponHeatInfo* ActiveWeaponHeat(const World::Player& player)
{
	const u32 m = player.mainCharaID & 1;
	const WeaponIndex weapon = m == 0 ? player.mainWeapon : player.subWeapon;
	return GetGameXmlContent().FindWeaponHeat((i32)weapon);
}

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
			const f32 chargingFuel = r == 1 && (i32)cls == CLASS_NUM_SHUTTLE && it->weaponState == WEAPON_STATE_CHARGE ? TOOLTIP_SHUTTLE_CHARGING_FUEL_SEC : 0.0f;
			it->res[r] = MIN(MAX(it->res[r] + (MasterStat(cls, RESOURCES[r].regenStat) + chargingFuel) * delta, 0.0f), resMax);
			if(Config().DevMirror && Config().DevShowcaseFullRes && it->clientHd != ClientHandle::INVALID && it->index == showcase.prevCaster && !showcase.castSeen) it->res[r] = resMax;
			if(fabsf(it->res[r] - it->resSent[r]) >= 0.01f) resChanged = true;
		}
		if(localTime >= it->tNextResRefresh) {
			it->tNextResRefresh = TimeAdd(localTime, TimeMsToTime(1000));
			resChanged = true;
			it->moveSpeedSent = -1.0f;
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
	const auto& content = GetGameXmlContent();
	f32 mp = content.GetSkillConsumeMP(skillID);
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
	if(player.clientHd == ClientHandle::INVALID) return true;

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

static const f32 STATUS_PUSH_BASE_DISTANCE = 500.0f;
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
	if(healer.index != target.index && (i32)healer.Main().classType == CLASS_NUM_ELECTRO && FindPassiveOfType(healer, "STATUS_TYPE_ELECTRO")) ApplyStatus(healer, healer, STATUS_ELECTRO_PASSIVE_BUFF_MPREGEN);
	replication->SendHpToAll(chara.UID, chara.hp, chara.hpMax);
}

f32 World::HitRadius(const Player& p) const
{
	if(p.Main().classType == ClassType::NONE) return 0;
	return (f32)GetGameXmlContent().GetMaster(p.Main().classType).character.getColliderRadius();
}

bool World::WeaponIsMelee(const Player& p) const
{
	const auto& content = GetGameXmlContent();
	const WeaponIndex weapon = (p.mainCharaID & 1) == 0 ? p.mainWeapon : p.subWeapon;
	const WeaponFiring* wf = content.FindWeaponFiring((i32)weapon, p.Main().stance);
	const Remote* wr = wf ? content.FindRemote(wf->base) : nullptr;
	if(wr && wr->maxSpeed > 0) return false;
	return ClassIsMelee(p.Main().classType);
}

f32 World::BodyMidHeight(const Player& p) const
{
	if(p.Main().classType == ClassType::NONE) return 0;
	return (f32)GetGameXmlContent().GetMaster(p.Main().classType).character.getColliderHeight() * 0.5f;
}

bool World::RemoteCanAffect(const RemoteInst& inst, const Player& p, bool includeOwner) const
{
	if(p.dead || p.untargetable) return false;
	if(inst.remote->monsterOnly) return false;
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

void World::ConsumeStatus(u32 targetIndex, i32 statusID)
{
	ActiveStatus* st = FindActiveStatus(targetIndex, statusID);
	if(st) st->until = localTime;
}

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

static bool PassiveOfActiveMaster(const World::Player& player, i32 statusID)
{
	const auto* list = GetGameXmlContent().FindPassiveStatuses(player.Main().classType);
	return list && eastl::find(list->begin(), list->end(), statusID) != list->end();
}


const World::ActiveStatus* World::FindPassiveOfType(const Player& p, const char* type) const
{
	foreach_const(it, activeStatuses) {
		if(it->targetIndex != p.index || !it->info || it->info->type != type) continue;
		if(PassiveOfActiveMaster(p, it->statusID)) return &*it;
	}
	return nullptr;
}

bool World::PassiveBlocksCC(Player& target, const StatusInfo& info, f32* duration)
{
	if(info.kind != StatusInfo::Kind::HARD_CC && info.kind != StatusInfo::Kind::ROOT) return false;
	if(Config().DevShowcase && target.index == showcase.prevDummy) return false;
	if(localTime < target.ccImmuneUntil) {
		LOG("Passive :: %ls immune to status %d", target.name.data(), info.ID);
		return true;
	}
	if(const ActiveStatus* st = FindPassiveOfType(target, "STATUS_TYPE_UNSHAKABLE")) {
		if(target.cleanseCharges < 0) target.cleanseCharges = st->info->customInit;
		if(target.cleanseCharges > 0) {
			target.cleanseCharges--;
			if(target.cleanseRefillAt == Time::ZERO) target.cleanseRefillAt = TimeAdd(localTime, TimeMsToTime(st->info->Param("_RefillDuration")));
			LOG("Passive :: %ls blocked status %d (charges left %d)", target.name.data(), info.ID, target.cleanseCharges);
			return true;
		}
		target.cleanseCharges = MIN(target.cleanseCharges + 1, st->info->customMax);
	}
	const f32 toughness = MasterStat(target.Main().classType, STAT_TOUGHNESS);
	if(toughness > 0 && duration) *duration *= MAX(1.0f - toughness / 100.0f, 0.0f);
	return false;
}

bool World::PassiveOnLethal(Player& target)
{
	const ActiveStatus* st = FindPassiveOfType(target, "STATUS_TYPE_ACTIVATE_UNDEAD");
	if(!st || localTime < target.reviveReadyAt) return false;
	const i32 undead = (i32)st->info->ParamInt("_UndeadStatusIndex");
	const StatusInfo* undeadInfo = GetGameXmlContent().FindStatus(undead);
	target.Main().hp = 1;
	target.reviveReadyAt = TimeAdd(localTime, TimeMsToTime(st->info->Param("_DelayMS")));
	target.reviveAt = TimeAdd(localTime, TimeMsToTime((undeadInfo ? undeadInfo->durationSec : 0.0f) * 1000.0));
	target.reviveRatio = TOOLTIP_DEATHKNIGHT_REVIVE_HP;
	ApplyStatus(target, target, undead);
	LOG("Passive :: %ls cheated death (undead status %d)", target.name.data(), undead);
	return true;
}

void World::PassiveOnDamaged(Player& target)
{
	if(target.dead) return;
	if(Config().DevShowcase && target.index == showcase.prevDummy) return;
	const ActiveStatus* st = FindPassiveOfType(target, "STATUS_TYPE_HPTRIGGER");
	if(!st) return;
	const ActorMaster& chara = target.Main();
	if(chara.hp * 100 >= chara.hpMax * st->info->Param("_CheckVal")) return;
	const i32 buff = (i32)st->info->ParamInt("_BuffStatus");
	const i32 creature = (i32)st->info->ParamInt("_SummonCreature");
	if(creature == 0 || buff == 0 || FindActiveStatus(target.index, buff) || pendingSummons.full()) return;
	ApplyStatus(target, target, buff);
	PendingSummon ps;
	ps.time = localTime;
	ps.creature = creature;
	ps.ownerIndex = target.index;
	ps.pos = target.body->GetWorldPos();
	ps.rot = vec3(0, 0, target.input.rot.upperYaw);
	pendingSummons.push_back(ps);
	LOG("Passive :: %ls low health -> summon %d", target.name.data(), creature);
}

void World::SetStance(ActorMaster& master, i8 stance, f32 duration, i32 weaponFireCount, bool cancelByDefeat)
{
	master.stance = stance;
	master.stanceUntil = stance != 0 && duration > 0 ? TimeAdd(localTime, TimeMsToTime((i64)(duration * 1000.0f))) : Time::ZERO;
	master.stanceShotsLeft = stance != 0 && weaponFireCount > 0 ? weaponFireCount : -1;
	master.stanceCancelByDefeat = stance != 0 && cancelByDefeat;
	replication->SendStanceToAll(master.UID, stance, stance != 0 ? duration : 0.0f);
	LOG("Stance :: %ls stance=%d duration=%.1f weaponFireCount=%d", master.parent ? master.parent->name.data() : L"?", (i32)stance, duration, weaponFireCount);
}

void World::StanceWeaponUse(Player& player)
{
	ActorMaster& m = player.Main();
	if(m.stance == 0 || m.stanceShotsLeft <= 0) return;
	if(--m.stanceShotsLeft == 0) SetStance(m, 0, 0, 0, false);
}

f32 World::WeaponHitDamage(const Player& player, const Remote* remote) const
{
	const u32 m = player.mainCharaID & 1;
	const WeaponIndex weapon = m == 0 ? player.mainWeapon : player.subWeapon;
	const f32 base = Config().DamageFormula ? GetGameXmlContent().GetWeaponAttack((i32)weapon) : 0.0f;
	const f32 attack = base > 0 ? base : (f32)WEAPON_HIT_DAMAGE;
	const f32 mult = Config().DamageFormula && remote && remote->attackMultiplier > 0 ? remote->attackMultiplier : 1.0f;
	return attack * mult * MAX(1.0f + StatusModMulSum(player, StatusMod::ATTACK) / 100.0f, 0.0f);
}

void World::PulseAura(u32 casterIndex, const StatusInfo& aura, const vec2& center, f32 range, i32 skipDebuffIndex)
{
	const u8 casterTeam = players[casterIndex].team;
	foreach_const(id, aura.auraStatuses) {
		const StatusInfo* child = GetGameXmlContent().FindStatus(*id);
		if(!child) continue;
		foreach(p, players) {
			if(p->dead) continue;
			const bool casterAlly = p->team == casterTeam;
			if(child->isDebuff ? casterAlly : (!casterAlly && !aura.auraAll)) continue;
			if(child->isDebuff && (i32)p->index == skipDebuffIndex) continue;
			if(glm::length(vec2(p->body->GetWorldPos()) - center) > range + HitRadius(*p)) continue;
			if(pendingStatuses.full()) return;
			PendingStatus ps;
			ps.time = localTime;
			ps.statusID = *id;
			ps.casterIndex = casterIndex;
			ps.targetUID = p->Main().UID;
			ps.duration = child->durationSec > 0 ? child->durationSec : 1.0f;
			ps.power = 0;
			pendingStatuses.push_back(ps);
		}
	}
}

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

void World::ToggleOn(Player& player, SkillID skillID, const vec3& castPos)
{
	const GameXmlContent::SkillToggle* tg = GetGameXmlContent().FindSkillToggle((i32)skillID);
	if(!tg) return;
	if(player.toggleSkill != SkillID::INVALID) ToggleOff(player);
	player.toggleSkill = skillID;
	player.toggleCastID = nextCastID++;
	player.toggleNext = TimeAdd(localTime, TimeMsToTime(tg->interval * 1000.0));
	const vec3 pos = player.body->GetWorldPos();
	const f32 yaw = player.input.rot.upperYaw;
	foreach_const(id, tg->remotes) {
		SpawnRemote(player.index, RemoteIdx(*id), pos, vec2(cosf(yaw), sinf(yaw)), ActorUID::INVALID, skillID, player.toggleCastID, (i32)SkillDamage(skillID), true, nullptr, 0, player.clientHd != ClientHandle::INVALID);
	}
	foreach_const(id, tg->statuses) ApplyStatus(player, player, *id);
	LOG("Toggle :: %ls skill=%d on (%d remotes, %d statuses)", player.name.data(), (i32)skillID, (i32)tg->remotes.size(), (i32)tg->statuses.size());
	auto sk = GetGameXmlContent().skillMap.find(skillID);
	replication->SendToggleSkill(player.Main().UID, player.clientHd, skillID, true, sk != GetGameXmlContent().skillMap.end() ? sk->second.action : ActionStateID::INVALID);
}

void World::ToggleOff(Player& player)
{
	if(player.toggleSkill == SkillID::INVALID) return;
	const GameXmlContent::SkillToggle* tg = GetGameXmlContent().FindSkillToggle((i32)player.toggleSkill);
	for(int i = 0; i < (i32)remoteInsts.size(); ) {
		RemoteInst& inst = remoteInsts[i];
		if(inst.ownerIndex == player.index && inst.castID == player.toggleCastID) {
			if(inst.clientRemoteID != 0) replication->SendDestroyRemoteToAll(inst.clientRemoteID);
			remoteInsts.erase_unsorted(remoteInsts.begin() + i);
		}
		else i++;
	}
	if(tg) {
		foreach(it, activeStatuses) {
			if(it->targetIndex != player.index) continue;
			if(eastl::find(tg->statuses.begin(), tg->statuses.end(), it->statusID) != tg->statuses.end()) it->until = localTime;
		}
	}
	LOG("Toggle :: %ls skill=%d off", player.name.data(), (i32)player.toggleSkill);
	replication->SendToggleSkill(player.Main().UID, player.clientHd, player.toggleSkill, false, ActionStateID::INVALID);
	player.toggleSkill = SkillID::INVALID;
}

void World::UpdateToggles()
{
	foreach(it, players) {
		Player& p = *it;
		if(p.toggleSkill == SkillID::INVALID) continue;
		if(p.dead) { ToggleOff(p); continue; }
		const GameXmlContent::SkillToggle* tg = GetGameXmlContent().FindSkillToggle((i32)p.toggleSkill);
		if(!tg || tg->interval <= 0 || localTime < p.toggleNext) continue;
		p.toggleNext = TimeAdd(p.toggleNext, TimeMsToTime(tg->interval * 1000.0));
		f32* pool = nullptr;
		if(tg->consume == GameXmlContent::SkillToggle::MANA) pool = &p.mana;
		else if(tg->consume == GameXmlContent::SkillToggle::RAGE) pool = &p.res[1];
		else if(tg->consume == GameXmlContent::SkillToggle::STAMINA) pool = &p.stamina;
		if(!pool || *pool < 0) continue;
		if(*pool < tg->value) { ToggleOff(p); continue; }
		*pool -= tg->value;
		p.tNextManaSend = Time::ZERO;
		p.tNextResSend = Time::ZERO;
		p.tNextStaminaSend = Time::ZERO;
	}
}

void World::ComboBaseCooldown(Player& player)
{
	const GameXmlContent::SkillInput* in = GetGameXmlContent().FindSkillInput((i32)player.comboBase);
	if(!in || in->coolTimeID == 0 || in->coolTime <= 0) return;
	const i32 id = in->coolTimeID;
	const u32 ms = (u32)(in->coolTime * 1000.0f);
	replication->SendCooltimeReset(player.clientHd, player.Main().UID, Slice<const i32>(&id, 1), ms, ms);
}

void World::ReleaseCast(Player& player)
{
	const Player::Casting c = player.casting;
	player.casting.skillID = SkillID::INVALID;
	auto f = GetGameXmlContent().skillMap.find(c.skillID);
	if(f == GetGameXmlContent().skillMap.end() || player.dead) return;
	LOG("Cast :: %ls skill=%d release", player.name.data(), (i32)c.skillID);
	if(f->second.action == ActionStateID::INVALID) return;
	PlayerCastSkill(player, c.skillID, c.castPos, Slice<const ActorUID>(&c.target, c.target != ActorUID::INVALID ? 1 : 0), 0, f->second.action);
}

void World::CancelCast(Player& player, const char* reason)
{
	LOG("Cast :: %ls skill=%d cancelled (%s)", player.name.data(), (i32)player.casting.skillID, reason);
	replication->SendCancelSkillToAll(player.Main().UID, player.casting.skillID);
	player.casting.skillID = SkillID::INVALID;
	player.massTeleportAt = Time::ZERO;
}

void World::UpdateCasts()
{
	foreach(it, players) {
		if(it->comboPending != SkillID::INVALID && it->comboSlot >= 0 && localTime >= it->comboSendAt) {
			replication->SendChangeSkillSlot(it->clientHd, it->Main().UID, it->comboSlot, it->comboPending, it->comboResetCooltime);
			if(it->comboResetCooltime != 0) replication->SendCooltimeReset(it->clientHd, it->Main().UID, Slice<const i32>(&it->comboResetCooltime, 1));
			it->comboPending = SkillID::INVALID;
		}
		if(it->massTeleportAt != Time::ZERO && localTime >= it->massTeleportAt) {
			it->massTeleportAt = Time::ZERO;
			const ActiveStatus* gx = FindPassiveOfType(*it, "STATUS_TYPE_GX09");
			if(gx && !it->dead) MassTeleport(*it, *gx->info);
		}
		if(it->comboAutoAt != Time::ZERO && localTime >= it->comboAutoAt) {
			it->comboAutoAt = Time::ZERO;
			if(it->comboSlot >= 0 && it->comboSkill != SkillID::INVALID && !it->dead) {
				LOG("Combo :: %ls auto-cast %d", it->name.data(), (i32)it->comboSkill);
				PlayerCastSkill(*it, it->comboSkill, it->comboCastPos, Slice<const ActorUID>(), 0);
			}
		}
		if(it->comboSlot >= 0 && localTime >= it->comboUntil) {
			replication->SendChangeSkillSlot(it->clientHd, it->Main().UID, it->comboSlot, it->comboBase, 0);
			ComboBaseCooldown(*it);
			LOG("Combo :: %ls slot %d window ended", it->name.data(), it->comboSlot);
			it->comboSlot = -1;
		}
		if(it->casting.skillID == SkillID::INVALID || !it->body) continue;
		if(it->dead) { CancelCast(*it, "dead"); continue; }
		const bool bot = it->clientHd == ClientHandle::INVALID;
		if(localTime < it->casting.executeAt) continue;
		if(!bot && it->casting.type == GameXmlContent::SkillCast::LIMIT) CancelCast(*it, "window ended");
		else ReleaseCast(*it);
	}
}

void World::UpdateStances()
{
	foreach(it, players) {
		if(!it->body) continue;
		ActorMaster& m = it->Main();
		if(m.stance == 0) continue;
		if((m.stanceUntil != Time::ZERO && localTime >= m.stanceUntil) || (it->dead && m.stanceCancelByDefeat)) SetStance(m, 0, 0, 0, false);
	}
}

void World::UpdatePassives()
{
	foreach(it, players) {
		if(it->reviveAt != Time::ZERO && localTime >= it->reviveAt) {
			it->reviveAt = Time::ZERO;
			if(!it->dead) {
				ActorMaster& chara = it->Main();
				chara.hp = MAX(chara.hp, (i32)(chara.hpMax * it->reviveRatio));
				replication->SendHpToAll(chara.UID, chara.hp, chara.hpMax);
				LOG("Passive :: %ls revived at %d hp", it->name.data(), chara.hp);
			}
		}
		if(it->cleanseRefillAt != Time::ZERO && localTime >= it->cleanseRefillAt) {
			const ActiveStatus* st = FindPassiveOfType(*it, "STATUS_TYPE_UNSHAKABLE");
			it->cleanseRefillAt = Time::ZERO;
			if(st && it->cleanseCharges < st->info->customMax) {
				it->cleanseCharges++;
				if(it->cleanseCharges < st->info->customMax) it->cleanseRefillAt = TimeAdd(localTime, TimeMsToTime(st->info->Param("_RefillDuration")));
			}
		}
	}
}

void World::PassiveOnSkillCast(Player& player, i32 skillID)
{
	if(GetGameXmlContent().GetSkillConsumeStamina(skillID) > 0) return;
	eastl::fixed_vector<i32,4,false> toApply;
	foreach(it, activeStatuses) {
		if(it->targetIndex != player.index || !it->info || it->info->customMax <= 0) continue;
		if(!PassiveOfActiveMaster(player, it->statusID)) continue;
		if(it->info->type == "STATUS_TYPE_SKILLSTATUS") {
			it->custom++;
			LOG("Passive :: %ls skill counter %d/%d", player.name.data(), it->custom, it->info->customMax);
			if(it->custom >= it->info->customMax) {
				it->custom = it->info->customInit;
				foreach_const(id, it->info->auraStatuses) if(!toApply.full()) toApply.push_back(*id);
			}
		}
	}
	foreach_const(id, toApply) {
		LOG("Passive :: %ls counter full -> status %d", player.name.data(), *id);
		ApplyStatus(player, player, *id);
	}

	if(const ActiveStatus* st = FindPassiveOfType(player, "STATUS_TYPE_SKILLSTAMINA")) {
		ActiveStatus& counter = const_cast<ActiveStatus&>(*st);
		counter.custom++;
		if(st->info->customMax > 0 && counter.custom >= st->info->customMax) {
			counter.custom = 0;
			const f32 staminaMax = MasterStat(player.Main().classType, STAT_STAMINA);
			if(staminaMax > 0) {
				player.stamina = staminaMax;
				player.tNextStaminaSend = Time::ZERO;
			}
			LOG("Passive :: %ls stamina restored after %d skills", player.name.data(), st->info->customMax);
		}
	}
	if(const ActiveStatus* st = FindPassiveOfType(player, "STATUS_TYPE_DEFENDER")) {
		if(st->info->ListHas("DATA", skillID)) {
			HealPlayer(player, player, (i32)(player.Main().hpMax * st->info->Param("_RecoveryFactorHP") / 100.0f));
			player.ccImmuneUntil = TimeAdd(localTime, TimeMsToTime(TOOLTIP_DEFENDER_CC_IMMUNE_SEC * 1000.0));
		}
	}
	if((i32)player.Main().classType == CLASS_NUM_CATTHECAT) ApplyStatus(player, player, STATUS_THECAT_PASSIVE_FIREDELAY1);
	if((i32)player.Main().classType == CLASS_NUM_ANDROA) ApplyStatus(player, player, STATUS_ANDROIDA_PASSIVE_ATTACKSPEED);
	if((i32)player.Main().classType == CLASS_NUM_ANDROB) {
		Player* lowest = nullptr;
		foreach(it, players) {
			if(it->team != player.team || it->dead) continue;
			if(glm::length(vec2(it->body->GetWorldPos()) - vec2(player.body->GetWorldPos())) > TOOLTIP_ANDROB_RANGE) continue;
			const f32 ratio = (f32)it->Main().hp / MAX(it->Main().hpMax, 1);
			if(!lowest || ratio < (f32)lowest->Main().hp / MAX(lowest->Main().hpMax, 1)) lowest = &*it;
		}
		if(lowest) HealPlayer(player, *lowest, TOOLTIP_ANDROB_HEAL);
	}
}

void World::PassiveOnWeaponHit(Player& attacker, Player& target, const Projectile& proj)
{
	if(proj.remoteDoc != RemoteIdx::INVALID && !target.dead) {
		const Remote* remote = GetGameXmlContent().FindRemote(proj.remoteDoc);
		if(remote) {
			foreach_const(st, remote->statuses) {
				if(st->rate >= 100 || (i32)(RandUint() % 100) < st->rate) ApplyStatus(attacker, target, st->statusID);
			}
		}
	}

	if(const ActiveStatus* sniper = FindPassiveOfType(attacker, "STATUS_TYPE_SNIPER")) {
		const i32 mark = GetGameXmlContent().FindSkillStatusOfType(sniper->info->skillIndex, "STATUS_TYPE_STACKBOMB");
		if(mark != 0 && !target.dead && target.team != attacker.team) {
			foreach(st, activeStatuses) {
				if(st->statusID == mark && st->casterIndex == attacker.index && st->targetIndex != target.index) st->until = localTime;
			}
			ApplyStatus(attacker, target, mark);
		}
	}

	eastl::fixed_vector<i32,4,false> toApply;
	foreach(it, activeStatuses) {
		if(it->targetIndex != attacker.index || !it->info) continue;
		if(!PassiveOfActiveMaster(attacker, it->statusID)) continue;
		const StatusInfo& info = *it->info;
		if(info.type == "STATUS_TYPE_HIT_PLAGUE" && info.customMax > 0) {
			it->custom++;
			if(it->custom >= info.customMax) {
				it->custom = info.customInit;
				if(info.hitEffectStatus && !toApply.full()) toApply.push_back(info.hitEffectStatus);
			}
		}
		else if(info.type == "STATUS_TYPE_KROMEDE" && info.hitEffectStatus) {
			if(HasStatusType(target, "STATUS_TYPE_STUN") || HasStatusType(target, "STATUS_TYPE_ROOT") || HasStatusType(target, "STATUS_TYPE_SHOCKED")) {
				if(!toApply.full()) toApply.push_back(info.hitEffectStatus);
			}
		}
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

	if((i32)attacker.Main().classType == CLASS_NUM_RYTLOCK && !target.dead) {
		attacker.weaponHitCounter = attacker.weaponHitCounter % TOOLTIP_RYTLOCK_CHAIN + 1;
		if(attacker.weaponHitCounter < TOOLTIP_RYTLOCK_CHAIN) ApplyStatus(attacker, target, STATUS_RYTLOCK_ATTACK_BLEEDING);
	}
	if((i32)attacker.Main().classType == CLASS_NUM_LILU && !target.dead) {
		attacker.weaponHitCounter++;
		if(attacker.weaponHitCounter >= TOOLTIP_LILU_EVERY_N) {
			attacker.weaponHitCounter = 0;
			LOG("Passive :: %ls extra weapon strike on %ls", attacker.name.data(), target.name.data());
			DamagePlayer(attacker, target, SkillID::INVALID, proj.damage);
		}
	}
}

void World::PassiveOnSkillHit(Player& attacker, Player& target)
{
	const i32 skillCls = (i32)attacker.Main().classType;
	if(skillCls == CLASS_NUM_BOXER) {
		target.skillMarkBy = attacker.index;
		target.skillMarkUntil = TimeAdd(localTime, TimeMsToTime(TOOLTIP_BOXER_MARK_SEC * 1000.0));
	}
	if(skillCls == CLASS_NUM_PHOTOG) ApplyStatus(attacker, attacker, STATUS_PHOTOG_PASSIVE_MOVESPEED);
	if(skillCls == CLASS_NUM_EFREET) ApplyStatus(attacker, target, STATUS_EFREET_PASSIVE_DOT_FIRE);
	if(skillCls == CLASS_NUM_SHADOWHUNTER && !FindActiveStatus(attacker.index, STATUS_SHADOWHUNTER_COOLDOWN)) {
		ApplyStatus(attacker, attacker, STATUS_SHADOWHUNTER_HIDE);
		ApplyStatus(attacker, attacker, STATUS_SHADOWHUNTER_COOLDOWN);
	}
	foreach_const(it, activeStatuses) {
		if(it->targetIndex != attacker.index || !it->info) continue;
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
		if(it->info->type == "STATUS_TYPE_GHOSTWIDOW" && PassiveOfActiveMaster(attacker, it->statusID)) {
			i32 debuffs = 0;
			foreach_const(d, activeStatuses) {
				if(d->targetIndex == target.index && d->info && d->info->isDebuff && d->until > localTime) debuffs++;
			}
			mul *= 1.0f + it->info->hitEffectValue * debuffs;
		}
	}
	const i32 cls = (i32)attacker.Main().classType;
	if(cls == CLASS_NUM_ATTACKER) {
		const ActiveStatus* st = FindPassiveOfType(attacker, "STATUS_TYPE_HPTRIGGER");
		if(st && attacker.Main().hp * 100 < attacker.Main().hpMax * st->info->Param("_CheckVal")) mul *= TOOLTIP_ATTACKER_WEAPON_MUL;
	}
	if(cls == CLASS_NUM_BOXER && target.skillMarkBy == attacker.index && localTime < target.skillMarkUntil) mul *= TOOLTIP_BOXER_WEAPON_MUL;
	if(cls == CLASS_NUM_SHUGOTRADER) {
		attacker.weaponHitCounter++;
		if(attacker.weaponHitCounter >= TOOLTIP_SHUGO_EVERY_N) {
			attacker.weaponHitCounter = 0;
			mul *= TOOLTIP_SHUGO_WEAPON_MUL;
			ApplyStatus(attacker, target, STATUS_SHUGOTRADER_PASSIVE_REMOTE);
		}
	}
	if(cls == CLASS_NUM_RYTLOCK && attacker.weaponHitCounter + 1 == TOOLTIP_RYTLOCK_CHAIN) mul *= TOOLTIP_RYTLOCK_THIRD_MUL;
	return mul;
}

void World::ApplyStatus(Player& caster, Player& target, i32 statusID, f32 power, f32 durationOverride)
{
	if(target.dead || target.untargetable) return;
	if(caster.team != target.team) target.statusesTaken++;
	const StatusInfo* info = GetGameXmlContent().FindStatus(statusID);
	f32 duration = durationOverride > 0 ? durationOverride : (info && info->durationSec > 0 ? info->durationSec : 1.0f);
	if(info && caster.team != target.team && PassiveBlocksCC(target, *info, &duration)) return;
	if(Config().DevShowcase && target.index == showcase.prevDummy && caster.team != target.team && !showcase.statusesSeen.full() &&
	   eastl::find(showcase.statusesSeen.begin(), showcase.statusesSeen.end(), statusID) == showcase.statusesSeen.end()) showcase.statusesSeen.push_back(statusID);
	const bool permanent = info && info->permanent && durationOverride <= 0;
	if(permanent) duration = 1000000.0f;
	const f32 sentDuration = permanent ? -1.0f : duration;
	if(info && info->casterStatus != 0 && info->casterStatus != statusID && !caster.dead && !applyingCasterStatus) {
		applyingCasterStatus = true;
		ApplyStatus(caster, caster, info->casterStatus);
		applyingCasterStatus = false;
	}
	if(statusID == STATUS_SHUTTLE_EMP_DRAIN && caster.team != target.team) {
		const f32 manaBefore = target.mana, energyBefore = target.res[0];
		target.mana = MAX(target.mana, 0.0f) * (1.0f - TOOLTIP_SHUTTLE_EMP_DRAIN);
		if(target.res[0] > 0) target.res[0] *= 1.0f - TOOLTIP_SHUTTLE_EMP_DRAIN;
		target.tNextManaSend = Time::ZERO;
		target.tNextResSend = Time::ZERO;
		LOG("Status :: %ls drained by %d (mana %.0f -> %.0f, energy %.0f -> %.0f)", target.name.data(), statusID, manaBefore, target.mana, energyBefore, target.res[0]);
	}
	else if(info && info->staminaBurn && info->type == "STATUS_TYPE_HEAL" && target.stamina > 0) {
		LOG("Status :: %ls stamina burned by %d (%.0f -> 0)", target.name.data(), statusID, target.stamina);
		target.stamina = 0;
		target.tNextStaminaSend = Time::ZERO;
	}

	const bool dragForce = info && info->type == "STATUS_TYPE_DRAGGEDBYFORCE";
	const bool displacement = info && (info->type == "STATUS_TYPE_PUSH" || info->type == "STATUS_TYPE_FORCEBACK" ||
		info->type == "STATUS_TYPE_KNOCKDOWN" || info->type == "STATUS_TYPE_AIRBORNE" || info->type == "STATUS_TYPE_FLY" || info->type == "STATUS_TYPE_DRAGGED" || dragForce);
	if(displacement && HasStatusType(target, "STATUS_TYPE_SUPERARMOR")) {
		LOG("Status :: %ls resisted %d (superarmor)", target.name.data(), statusID);
		return;
	}

	foreach(it, activeStatuses) {
		if(it->statusID == statusID && it->targetIndex == target.index) {
			const Time until = TimeAdd(localTime, TimeMsToTime(duration * 1000.0));
			if(until > it->until) it->until = until;
			const bool stacked = info && it->overlap < info->maxOverlap;
			if(stacked) it->overlap++;
			LOG("Status :: %ls -> %ls status=%d refreshed overlap=%d", caster.name.data(), target.name.data(), statusID, it->overlap);
			const vec3 pos = target.body->GetWorldPos();
			if(!stacked) replication->SendRemoveStatusToAll(statusID, it->targetUID, it->casterUID);
			replication->SendAddStatusToAll(statusID, it->targetUID, it->casterUID, sentDuration, pos, pos, 0, it->overlap);
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
	st.casterUID = statusCasterOverride != ActorUID::INVALID ? statusCasterOverride : caster.Main().UID;
	st.damageAtStart = target.damageTaken;
	st.until = TimeAdd(localTime, TimeMsToTime(MAX(duration, 0.1f) * 1000.0));
	st.appliedAt = localTime;
	st.nextTick = TimeAdd(localTime, TimeMsToTime((info && info->intervalSec > 0 ? info->intervalSec : 1.0f) * 1000.0));
	st.overlap = 1;
	st.nextHealTick = localTime;
	st.healTicksDone = 0;
	st.shieldHP = 0;
	if(info && (info->type == "STATUS_TYPE_SHIELD" || info->type == "STATUS_TYPE_SHIELDBOMB")) {
		f32 dataShield = 0;
		foreach_const(m, info->mods) {
			if(m->stat == StatusMod::SHIELD_DEFENCE && !m->mul) dataShield += m->value;
		}
		st.shieldHP = dataShield > 0 ? dataShield : MAX(power, 100.0f) * (info->paramRatio > 0 ? info->paramRatio : 1.0f);
	}
	activeStatuses.push_back(st);

	if(info && info->type == "STATUS_TYPE_LINK" && &caster != &target) {
		if(info->linkCasterStatus != 0) ApplyStatus(caster, caster, info->linkCasterStatus, power);
		if(info->linkTargetStatus != 0) ApplyStatus(caster, target, info->linkTargetStatus, power);
	}

	if(info && (info->kind == StatusInfo::Kind::HARD_CC || info->kind == StatusInfo::Kind::ROOT)) {
		Time ccEnd = st.until;
		if(info->type == "STATUS_TYPE_AIRBORNE" || info->type == "STATUS_TYPE_FLY" || info->type == "STATUS_TYPE_KNOCKDOWN") {
			const auto& content = GetGameXmlContent();
			const f32 recover = info->lyingTime + content.GetLieEndTime(content.GetMaster(target.Main().classType).className.c_str());
			ccEnd = TimeAdd(ccEnd, TimeMsToTime(recover * 1000.0));
		}
		if(ccEnd > target.ccUntil) target.ccUntil = ccEnd;
	}

	const bool chainedPush = target.clientHd == ClientHandle::INVALID ? localTime < target.pushUntil : localTime < target.clientPushUntil;
	const vec3 from = chainedPush ? vec3(vec2(target.pushTarget), target.body->GetWorldPos().z) : target.body->GetWorldPos();
	vec3 to = from;
	vec3 forceCenter = from;
	const f32 graphDuration = duration * (info && info->timeRatio > 0 ? info->timeRatio : 1.0f);
	const bool flyMove = info && info->type == "STATUS_TYPE_FLY" && info->distanceRatio != 0;
	if(info && displacement && info->type != "STATUS_TYPE_AIRBORNE" && (info->type != "STATUS_TYPE_FLY" || flyMove)) {
		vec2 dir = vec2(from - caster.body->GetWorldPos());
		const f32 len = glm::length(dir);
		dir = len > 1.0f ? dir / len : vec2(cosf(caster.input.rot.upperYaw), sinf(caster.input.rot.upperYaw));
		if(info->type == "STATUS_TYPE_FLY" && statusFromBehind) dir = -dir;
		if(dragForce) {
			forceCenter = statusForceCenterSet ? statusForceCenter : caster.body->GetWorldPos();
			const vec2 rel = vec2(forceCenter) - vec2(from);
			const f32 centerDist = glm::length(rel);
			const f32 travel = (info->dragForceMin + info->dragForceMax) * 0.5f * duration;
			dir = centerDist > 0.001f ? rel / centerDist : vec2(0);
			to = from + vec3(dir * (travel > centerDist ? centerDist : travel), 0);
		}
		else if(info->type == "STATUS_TYPE_DRAGGED" && &caster == &target) {
			const bool hooked = caster.lastHitTargetIndex >= 0 && caster.lastHitTargetIndex != (i32)caster.index && TimeDiffSec(TimeDiff(caster.lastHitAt, localTime)) <= SELF_DRAG_LINK_SEC;
			if(hooked) {
				const Player& hookedPlayer = players[caster.lastHitTargetIndex];
				const vec3 hp = hookedPlayer.body->GetWorldPos();
				const vec2 rel = vec2(hp - from);
				const f32 rl = glm::length(rel);
				if(rl > 1.0f) dir = rel / rl;
				to = hp - vec3(dir * (info->dragOffset > 0 ? info->dragOffset : HitRadius(hookedPlayer) + HitRadius(target)), 0);
			}
			else to = from;
		}
		else if(info->type == "STATUS_TYPE_DRAGGED") {
			to = caster.body->GetWorldPos() + vec3(dir * (info->dragOffset > 0 ? info->dragOffset : 200.0f), 0);
		}
		else {
			f32 dist = info->type == "STATUS_TYPE_FORCEBACK" && info->distance > 0 ? info->distance :
				STATUS_PUSH_BASE_DISTANCE * (info->distanceRatio != 0 ? info->distanceRatio : 1.0f);
			if(statusID == STATUS_SHUTTLE_SHOCKWAVE) {
				const Remote* wave = GetGameXmlContent().FindRemote(RemoteIdx(REMOTE_SHUTTLE_SHOCKWAVE));
				dist = wave ? MAX(wave->boundSize[0] * 0.5f - len, 0.0f) : dist;
			}
			to = from + vec3(dir * dist, 0);
		}
		LOG("Push :: %ls -> %ls status=%d sep=%.0f dir=(%.2f, %.2f) to=(%.0f, %.0f)", caster.name.data(), target.name.data(), statusID, len, dir.x, dir.y, to.x, to.y);
		if(dragForce && target.clientHd == ClientHandle::INVALID) {
			target.dragCenter = vec2(forceCenter);
			target.dragMin = info->dragForceMin;
			target.dragMax = info->dragForceMax;
			target.dragDuration = MAX(duration, 0.001f);
			target.dragElapsed = 0;
			const Time dragEnd = TimeAdd(localTime, TimeMsToTime(duration * 1000.0));
			if(dragEnd > target.ccUntil) target.ccUntil = dragEnd;
		}
		else if(target.clientHd == ClientHandle::INVALID && graphDuration > 0 && &caster == &target) {
			const vec2 rel = vec2(to - from);
			const f32 rl = glm::length(rel);
			if(rl > 1.0f) {
				target.dashDir = rel / rl;
				target.dashSpeed = rl / graphDuration;
				target.dashStartAt = localTime;
				target.dashOrigin = target.body->GetWorldPos();
				target.dashDist = rl;
				target.dashDone = 0;
				target.dashCurve = nullptr;
				target.dashUntil = TimeAdd(localTime, TimeMsToTime(graphDuration * 1000.0));
			}
		}
		else if(target.clientHd == ClientHandle::INVALID && graphDuration > 0) {
			const Time prevUntil = chainedPush ? target.pushUntil : localTime;
			const Time newUntil = TimeAdd(localTime, TimeMsToTime(graphDuration * 1000.0));
			target.pushUntil = newUntil > prevUntil ? newUntil : prevUntil;
			target.pushTarget = to;
			target.pushSpeed = glm::length(vec2(to - target.body->GetWorldPos())) / MAX((f32)TimeDiffSec(TimeDiff(localTime, target.pushUntil)), 0.05f);
			if(target.pushUntil > target.ccUntil) target.ccUntil = target.pushUntil;
		}
		else if(target.clientHd != ClientHandle::INVALID && graphDuration > 0) {
			target.clientPushUntil = TimeAdd(localTime, TimeMsToTime(graphDuration * 1000.0));
			target.pushTarget = to;
			const Time followUntil = TimeAdd(localTime, TimeMsToTime((graphDuration + 2.0f) * 1000.0));
			if(followUntil > target.ccUntil) target.ccUntil = followUntil;
		}
	}

	if(info && info->type == "STATUS_TYPE_DISPEL" && info->dispelEffect >= 0) {
		foreach(it, activeStatuses) {
			if(it->targetIndex != target.index || !it->info || !it->info->canDispel || it->statusID == statusID) continue;
			if((it->info->isDebuff ? 1 : 0) != info->dispelEffect) continue;
			it->until = localTime;
			LOG("Dispel :: %ls removed status %d from %ls", caster.name.data(), it->statusID, target.name.data());
		}
	}

	LOG("Status :: %ls -> %ls status=%d type=%s duration=%.2f shield=%.0f", caster.name.data(), target.name.data(), statusID, info ? info->type.c_str() : "?", duration, st.shieldHP);
	if(info && info->type == "STATUS_TYPE_HIDE" && Config().DevHideDetectFlag >= 0) {
		foreach(viewer, players) {
			if(viewer->clientHd == ClientHandle::INVALID || viewer->team == target.team) continue;
			replication->SendHideDetect(viewer->clientHd, viewer->Main().UID, target.Main().UID, (u8)Config().DevHideDetectFlag);
			LOG("Hide :: sent HideDetect flag=%d about %ls to %ls", Config().DevHideDetectFlag, target.name.data(), viewer->name.data());
		}
	}
	replication->SendAddStatusToAll(statusID, st.targetUID, st.casterUID, sentDuration, from, dragForce ? forceCenter : to, graphDuration);
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

void World::UpdateBotSkills()
{
	const auto& content = GetGameXmlContent();
	foreach(it, players) {
		Player& bot = *it;
		if(bot.clientHd != ClientHandle::INVALID || bot.dead) continue;
		if(localTime < bot.tNextBotSkill || localTime < bot.ccUntil) continue;
		if(IsBlocked(bot, false, true, false)) continue;
		bot.tNextBotSkill = TimeAdd(localTime, TimeMsToTime(500));

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
	else if(npc.skillTarget != ActorUID::INVALID) {
		ActorMaster* m = FindMasterActor(npc.skillTarget);
		if(m && !m->parent->dead) current = m->parent;
	}
	f32 yaw = owner.input.rot.upperYaw;
	if(current && current != &owner) {
		const vec2 rel = vec2(current->body->GetWorldPos()) - vec2(npc.pos);
		if(glm::length(rel) > 1.0f) yaw = atan2f(rel.y, rel.x);
	}
	{
		eastl::fixed_string<char,48,false> name;
		name.sprintf("SKILL_%d_BEHAVIORSTATE", state);
		const ActionStateID action = ActionStateFromString(name.c_str());
		const vec3 targetPos = current ? current->body->GetWorldPos() : npc.pos;
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
			npc.motionTargetUID = current ? current->Main().UID : ActorUID::INVALID;
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
				ga.auraStatus = st->ID;
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
	if(dist > 1.0f) replication->SendNpcMoveToAll(npc->UID, npc->pos, goal, atan2f(goal.y - npc->pos.y, goal.x - npc->pos.x), travel > 0 ? dist / travel : dist);
	foreach(st, activeStatuses) {
		if(st->casterIndex != owner.index || !st->info || st->info->type != "STATUS_TYPE_TARGET" || localTime >= st->until) continue;
		Player& carried = players[st->targetIndex];
		const Time drop = TimeAdd(localTime, TimeMsToTime(travel * 1000.0));
		foreach(with, activeStatuses) {
			if(with->targetIndex == st->targetIndex && with->casterIndex == owner.index && with->appliedAt == st->appliedAt && with->until > drop) with->until = drop;
		}
		if(carried.clientHd == ClientHandle::INVALID && !carried.dead) {
			carried.pushTarget = vec3(vec2(pos), carried.body->GetWorldPos().z);
			carried.pushUntil = drop;
			carried.pushSpeed = glm::length(vec2(pos) - vec2(carried.body->GetWorldPos())) / MAX(travel, 0.05f);
			if(drop > carried.ccUntil) carried.ccUntil = drop;
		}
		LOG("Summon :: %ls carried to (%.0f, %.0f)", carried.name.data(), pos.x, pos.y);
	}
	npc->pos = goal;
	npc->aimPos = pos;
	RunSummonState(*npc, child);
}

void World::RelayWarp(ActorNpc& npc, const vec3& to)
{
	LOG("Summon :: relay robot warps from (%.0f, %.0f) to (%.0f, %.0f)", npc.pos.x, npc.pos.y, to.x, to.y);
	replication->SendNpcBehaviorToAll(npc.UID, ActionStateID::EXTRAMOTION_2_BEHAVIORSTATE, npc.pos, npc.rot.z, to, ActorUID::INVALID);
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

void World::UpdateSummonAi(f32 delta)
{
	const auto& content = GetGameXmlContent();
	foreach(it, actorNpcList) {
		ActorNpc& npc = *it;
		if(npc.pendingMotion != ActionStateID::INVALID && localTime >= npc.motionAt) {
			replication->SendNpcBehaviorToAll(npc.UID, npc.pendingMotion, npc.pos, npc.motionYaw, npc.motionTarget, npc.motionTargetUID);
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
	p.dashDone = p.dashDist;
	p.dashUntil = Time::ZERO;
	p.input.moveTo = pos;
	p.input.rot.upperYaw = yaw;
	p.input.rot.bodyYaw = yaw;
	p.weaponAimYaw = yaw;
}

void World::MovePlayer(Player& p, const vec3& pos, f32 yaw, bool snap)
{
	TeleportPlayer(p, pos, yaw);
	if(p.clientHd == ClientHandle::INVALID) return;
	replication->SendSelfTeleport(p.clientHd, p.Main().UID, pos, yaw, snap ? &p.lastClientPos : nullptr);
}

static f32 WeaponNodeForward(const GameXmlContent& content, ClassType cls, SkillID skillID, const Remote& remote, u16 nodeName)
{
	if(nodeName == 0 || remote.maxSpeed > 0 || remote.attachFireObject || remote.boundType != Remote::BoundType::E_BOUND_BOX) return 0;
	auto f = content.skillMap.find(skillID);
	if(f == content.skillMap.end()) return 0;
	SkillNormalModel& skill = const_cast<SkillNormalModel&>(f->second);
	const f32 indicator = skill.getSkillNormalLevelByIndex(0)->getSkillRangeLengthY();
	if(indicator <= 0) return 0;
	const auto& action = content.GetSkillAction(cls, skill.action);
	foreach_const(cmd, action.commands) {
		if(cmd->type == ActionCommand::Type::GRAPH_MOVE_HORZ && cmd->graphMoveHorz.distance > 0) return 0;
		if(cmd->type == ActionCommand::Type::MOVE && cmd->move.preset != ActionCommand::MovePreset::MOVE_NONE) return 0;
	}
	const f32 forward = indicator - (remote.offsetY + remote.boundSize[1] * 0.5f);
	return forward > 0 && forward <= WEAPON_NODE_MAX_FORWARD ? forward : 0;
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
		if(WeaponIsMelee(bot)) continue;

		const Remote* remote = content.FindRemote(RemoteIdx(1430000000 + (i32)cls * 10000 + 10));
		f32 range = ClassAttackRange(cls) * WEAPON_RANGE_SCALE;
		if(remote && remote->maxDistance > 0) range = remote->maxDistance;

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
		const f32 stanceDelay = content.GetWeaponStanceFireDelay((i32)weapon, bot.Main().stance);
		const f32 delay = MAX(stanceDelay > 0 ? stanceDelay : content.GetWeaponFireDelay((i32)weapon), 0.15f);
		bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(delay * 1000.0));
		if(!target) continue;

		if(!WeaponHeatShot(bot, localTime, replication)) continue;
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
				actor.nextPassive = TimeAdd(localTime, TimeMsToTime(RELAY_PASSIVE_SEC * 1000.0));
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
				pr.preset = (i32)ActionCommand::TargetPreset::POS_FFF;
				const Remote* summonRemote = GetGameXmlContent().FindRemote(RemoteIdx(pc->index));
				const bool aimedGraph = summonRemote && summonRemote->behaviorType == Remote::BehaviorType::GRAPH && glm::length(vec2(pc->aim)) > 1.0f;
				pr.offsetSide = 0;
				pr.offsetFront = 0;
				pr.ownerIndex = pc->ownerIndex;
				pr.skillID = pc->skill != 0 ? SkillID(pc->skill) : SkillID::INVALID;
				pr.castID = nextCastID++;
				pr.damage = pc->skill != 0 ? (i32)SkillDamage(SkillID(pc->skill)) : 0;
				pr.castPos = aimedGraph ? pc->aim : pc->pos;
				pr.angle = aimedGraph ? atan2f(pc->aim.y - pc->pos.y, pc->aim.x - pc->pos.x) : pc->yaw;
				if(aimedGraph) pr.preset = (i32)ActionCommand::TargetPreset::SELF_FFF;
				pr.target = pc->target;
				pr.sourceUID = pc->source;
				pendingRemotes.push_back(pr);
			}
		}
		else {
			const StatusInfo* st = GetGameXmlContent().FindStatus(pc->index);
			if(st && st->auraStatus != 0 && !groundAuras.full()) {
				GroundAura ga;
				ga.ownerIndex = pc->ownerIndex;
				ga.auraStatus = st->ID;
				ga.range = st->targetingRange;
				ga.pos = pc->pos;
				const Time statusEnd = st->durationSec > 0 ? TimeAdd(localTime, TimeMsToTime(st->durationSec * 1000.0)) : pc->until;
				ga.until = pc->until < statusEnd ? pc->until : statusEnd;
				ga.nextPulse = localTime;
				groundAuras.push_back(ga);
				LOG("Summon :: %ls aura %d (gives %d statuses within %.0f%s)", owner.name.data(), pc->index, (i32)st->auraStatuses.size(), st->targetingRange, st->auraAll ? ", everyone" : "");
			}
		}
		pc = pendingSummonCommands.erase_unsorted(pc);
	}

	for(auto ga = groundAuras.begin(); ga != groundAuras.end(); ) {
		if(localTime >= ga->until) { ga = groundAuras.erase_unsorted(ga); continue; }
		if(localTime >= ga->nextPulse) {
			ga->nextPulse = TimeAdd(localTime, TimeMsToTime(500));
			const StatusInfo* aura = GetGameXmlContent().FindStatus(ga->auraStatus);
			if(aura && ga->range > 0) PulseAura(ga->ownerIndex, *aura, vec2(ga->pos), ga->range, -1);
		}
		++ga;
	}

	for(auto it = actorNpcList.begin(); it != actorNpcList.end(); ) {
		if(it->despawnAt != Time::ZERO && localTime >= it->despawnAt) {
			LOG("Summon :: creature=%d despawned", (i32)it->docID);
			physics.RemoveWall(it->wall);
			it->wall = nullptr;
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
		if(targetMaster && ps->remove) {
			foreach(it, activeStatuses) {
				if(it->targetIndex != targetMaster->parent->index || it->statusID != ps->statusID || localTime >= it->until) continue;
				it->until = localTime;
				LOG("Status :: %ls status=%d removed by action", targetMaster->parent->name.data(), ps->statusID);
			}
		}
		else if(!caster.dead && targetMaster) {
			ApplyStatus(caster, *targetMaster->parent, ps->statusID, ps->power, ps->duration);
		}
		ps = pendingStatuses.erase_unsorted(ps);
	}

	for(auto it = activeStatuses.begin(); it != activeStatuses.end(); ) {
		Player& target = players[it->targetIndex];
		if(it->info && it->info->dotDamage > 0 && it->info->intervalSec > 0) {
			while(localTime >= it->nextTick && it->nextTick <= it->until && !target.dead) {
				it->nextTick = TimeAdd(it->nextTick, TimeMsToTime(it->info->intervalSec * 1000.0));
				const i32 hpBefore = target.Main().hp;
				DamagePlayer(players[it->casterIndex], target, SkillID::INVALID, (i32)(it->info->dotDamage * it->overlap));
				Player& drainer = players[it->casterIndex];
				const i32 drained = hpBefore - target.Main().hp;
				if(it->info->drainMultiplier > 0 && drained > 0 && !drainer.dead) {
					ActorMaster& dm = drainer.Main();
					dm.hp = MIN(dm.hp + (i32)(drained * it->info->drainMultiplier), dm.hpMax);
					replication->SendHpToAll(dm.UID, dm.hp, dm.hpMax);
					LOG("Status :: %ls drains %d from %ls", drainer.name.data(), (i32)(drained * it->info->drainMultiplier), target.name.data());
				}
			}
		}

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

		if(it->info && it->info->auraStatus != 0 && !target.dead && localTime >= it->nextAuraPulse) {
			it->nextAuraPulse = TimeAdd(localTime, TimeMsToTime(500));
			const StatusInfo* child = GetGameXmlContent().FindStatus(it->info->auraStatus);
			const f32 range = it->info->targetingRange > 0 ? it->info->targetingRange : (child ? child->targetingRange : 0);
			if(child && range > 0) PulseAura(it->casterIndex, *it->info, vec2(target.body->GetWorldPos()), range, it->info->type == "STATUS_TYPE_DEFEAT_CASTER_AURA" ? (i32)target.index : -1);
		}

		if(it->info && it->info->type == "STATUS_TYPE_LINK" && localTime < it->until && it->casterIndex != it->targetIndex) {
			const GameXmlContent::SkillInput* in = GetGameXmlContent().FindSkillInput(it->info->skillIndex);
			const Player& linker = players[it->casterIndex];
			if(in && in->maxDistance > 0 && (linker.dead || glm::length(vec2(linker.body->GetWorldPos()) - vec2(target.body->GetWorldPos())) > in->maxDistance)) {
				LOG("Status :: link %d between %ls and %ls snapped", it->statusID, linker.name.data(), target.name.data());
				const i32 skillIndex = it->info->skillIndex;
				const u32 casterIdx = it->casterIndex;
				foreach(other, activeStatuses) {
					if(other->targetIndex == target.index && other->casterIndex == casterIdx && other->info && other->info->skillIndex == skillIndex) other->until = localTime;
					if(other->targetIndex == casterIdx && other->casterIndex == casterIdx && other->statusID == it->info->linkCasterStatus) other->until = localTime;
				}
			}
		}
		const bool shieldBomb = it->info && it->info->type == "STATUS_TYPE_SHIELDBOMB";
		const bool shieldBroken = it->info && ((it->info->type == "STATUS_TYPE_SHIELD" && it->shieldHP <= 0) || (shieldBomb && it->shieldHP <= 0 && it->absorbed > 0));
		const bool timeOver = localTime >= it->until;
		if(timeOver || target.dead || shieldBroken || it->removeOnHit) {
			if(timeOver && !target.dead && it->info && it->info->type == "STATUS_TYPE_NEARLY_IMMORTAL" && it->info->paramRatio > 0) {
				const i32 taken = MAX(target.damageTaken - it->damageAtStart, 0);
				const i32 extra = (i32)MIN(taken * it->info->Param("_RecoveryFactorHP") / 100.0f, it->info->Param("_MaxRecoverHP"));
				const i32 heal = (i32)(target.Main().hpMax * it->info->paramRatio) + extra;
				LOG("Status :: %ls undying %d ends: heal %d (+%d from %d taken)", target.name.data(), it->statusID, heal, extra, taken);
				HealPlayer(players[it->casterIndex], target, heal);
			}
			if(shieldBomb && !target.dead && it->info->targetingRange > 0) {
				const i32 boom = (i32)(it->info->explodeDamage + it->absorbed * it->info->paramRatio);
				const SkillID bombSkill = SkillID(it->info->skillIndex);
				Player& owner = players[it->casterIndex];
				LOG("Status :: %ls shield bomb %d explodes (absorbed %.0f) for %d within %.0f", target.name.data(), it->statusID, it->absorbed, boom, it->info->targetingRange);
				foreach(e, players) {
					if(e->dead || e->team == target.team) continue;
					if(glm::length(vec2(e->body->GetWorldPos()) - vec2(target.body->GetWorldPos())) > it->info->targetingRange + HitRadius(*e)) continue;
					DamagePlayer(owner, *e, bombSkill, boom);
				}
			}
			for(i32 k = 0; k < MAX(it->overlap, 1); k++) replication->SendRemoveStatusToAll(it->statusID, it->targetUID, it->casterUID);
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
	const auto& action = content.GetSkillAction(player.Main().classType, forcedAction != ActionStateID::INVALID ? forcedAction : skill.action, player.Main().stance);

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
		(void)consumeUG;
	}

	const u32 castID = nextCastID++;
	i32 damage = (i32)SkillDamage(skillID);
	{
		const f32 skillPower = StatusModSum(player, StatusMod::SKILLPOWER);
		if(skillPower > 0) {
			SkillNormalModel& sm = const_cast<SkillNormalModel&>(skill);
			damage += (i32)(skillPower * sm.getSkillNormalLevelByIndex(0)->getAttackMultiplier());
		}
	}
	const ActorUID currentTarget = targets.size() > 0 ? targets[0] : ActorUID::INVALID;

	bool scheduled = false;
	bool teleportToChild = false;
	eastl::fixed_vector<SkillAnimPass,8,false>& animPasses = lastAnimPasses;
	animPasses.clear();
	if(anim == 0 && forceAnim < 0) {
		const bool hitEventCombo = GetGameXmlContent().FindSkillHitEventRemote((i32)skillID) != 0;
		bool regCombo = false;
		bool acts[8] = {};
		foreach_const(cmd, action.commands) {
			if(cmd->type == ActionCommand::Type::REGCOMBO) regCombo = true;
			bool a = cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET ||
				cmd->type == ActionCommand::Type::STATUS || cmd->type == ActionCommand::Type::STATUS_SKILL_TARGET || cmd->type == ActionCommand::Type::ENTITY;
			if(cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET) {
				const Remote* r = content.FindRemote(cmd->remote.idx);
				const bool visualOnly = r && r->boundType == Remote::BoundType::E_BOUND_NONE && r->onDestroy.empty() && r->onTime.empty() && r->onActivate.empty() &&
					(r->damageGroup == Remote::DamageGroup::eNONE || r->damageGroup == Remote::DamageGroup::INVALID);
				if(visualOnly) a = false;
			}
			if(cmd->animIndex >= 0 && cmd->animIndex < 8) acts[cmd->animIndex] |= a;
		}
		const char* className = content.GetMaster(player.Main().classType).className.c_str();
		i32 lastActing = -1;
		for(i32 i = 0; i < 8; i++) if(acts[i]) lastActing = i;
		if((!regCombo || !acts[0]) && (!hitEventCombo || !acts[0])) {
			f32 start = 0;
			for(i32 i = 0; i <= lastActing && (!regCombo || i <= 1); i++) {
				if(acts[i] && !animPasses.full()) animPasses.push_back({ i, start });
				i32 animId = 0, loop = 1;
				foreach_const(st, action.anims) if(st->index == i) { animId = st->animationId; loop = MAX(st->loop, 1); }
				start += content.GetAniSeqTime(className, animId) * (f32)loop;
				if(regCombo && acts[i]) break;
			}
		}
	}
	if(animPasses.empty()) animPasses.push_back({ anim, 0.0f });
	anim = animPasses[0].index;

	const i32 passes = (i32)animPasses.size();
	for(i32 pass = 0; pass < passes; pass++) {
	const i32 curAnim = animPasses[pass].index;
	foreach_const(cmd, action.commands) {
		if(cmd->animIndex != curAnim && (cmd->animIndex != -1 || pass > 0)) continue;
		const f32 t = animPasses[pass].start + cmd->delay;
		if((cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET) && !pendingRemotes.full()) {
			PendingRemote pr;
			pr.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			pr.idx = cmd->remote.idx;
			pr.costLevel = castCostLevel;
			if(castCostLevel) {
				foreach_const(rr, castCostLevel->remoteReplace) if(rr->first == (i32)pr.idx) pr.idx = RemoteIdx(rr->second);
			}
			pr.preset = (i32)cmd->remote.targetPreset;
			pr.offsetSide = cmd->remote.offsetSide;
			pr.offsetFront = cmd->remote.offsetFront;
			pr.ownerIndex = player.index;
			pr.skillID = skillID;
			pr.castID = castID;
			pr.damage = castCostLevel ? (i32)(damage * castCostLevel->damageRatio) : damage;
			pr.castPos = castPos;
			pr.angle = angle;
			pr.target = cmd->remote.targetPreset == ActionCommand::TargetPreset::MEMORIZED_TFT ? player.memorizedTarget : currentTarget;
			pr.atTarget = cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET;
			pr.stopAtTarget = cmd->remote.stopAtTarget;
			pr.remove = cmd->remote.remove != 0;
			pr.nodeName = cmd->nodeName;
			pendingRemotes.push_back(pr);
			scheduled = true;
		}
		if(cmd->type == ActionCommand::Type::TELEPORT && cmd->teleport.toLastChild && t <= 0.001f) teleportToChild = true;
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
			ps.pos = pos;
			ps.rot = vec3(0, 0, angle);
			if(preset == ActionCommand::TargetPreset::CURRENT_FFF) ps.target = currentTarget;
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
			const bool lastChild = preset == ActionCommand::TargetPreset::LAST_CHILD_FFF || preset == ActionCommand::TargetPreset::LAST_CHILD_TFF || preset == ActionCommand::TargetPreset::CHILD_INDEX_FFF;
			ps.targetUID = lastChild ? ActorUID::INVALID : self ? player.Main().UID : currentTarget;
			ps.duration = cmd->status.removeAtEnd ? MAX(action.seqLength - t, 0.3f) : -1.0f;
			ps.power = (f32)damage;
			ps.remove = cmd->status.remove != 0;
			if(cmd->type == ActionCommand::Type::STATUS_SKILL_TARGET && targets.size() > 1) {
				foreach_const(tg, targets) {
					if(pendingStatuses.full()) break;
					ps.targetUID = *tg;
					pendingStatuses.push_back(ps);
				}
			}
			else if(ps.targetUID != ActorUID::INVALID) pendingStatuses.push_back(ps);
		}
	}
	}

	if(teleportToChild) {
		const ActorNpc* child = FindNpcActor(player.lastChildUID);
		if(child && localTime < child->despawnAt) {
			const vec3 to = vec3(vec2(child->pos), player.body->GetWorldPos().z);
			LOG("Teleport :: %ls to its last entity %d at (%.0f, %.0f)", player.name.data(), (i32)child->docID, to.x, to.y);
			TeleportPlayer(player, to, player.input.rot.upperYaw);
		}
		else LOG("Teleport :: %ls has no live entity to teleport to", player.name.data());
	}
	LOG("Skill :: %ls skill=%d anim=%d%s scheduled=%d damage=%d castPos=(%.0f, %.0f)", player.name.data(), (i32)skillID, anim, passes > 1 ? "+1" : "", scheduled, damage, castPos.x, castPos.y);
	return scheduled;
}

World::RemoteInst* World::SpawnRemote(u32 ownerIndex, RemoteIdx idx, const vec3& pos, const vec2& dir, ActorUID target, SkillID skillID, u32 castID, i32 damage, bool broadcast, const vec3* aimPos, u16 nodeName, bool casterCreatesLocally, bool stopAtTarget)
{
	const Remote* remote = GetGameXmlContent().FindRemote(idx);
	if(!remote || remoteInsts.full()) return nullptr;

	RemoteInst inst;
	inst.instID = nextRemoteInstID++;
	inst.remote = remote;
	inst.ownerIndex = ownerIndex;
	inst.skillID = skillID;
	inst.castID = castID;
	inst.damage = damage;
	inst.pos = pos;
	inst.dir = dir;
	inst.travelled = 0;
	inst.wallZ = players[ownerIndex].body->GetWorldPos().z + BodyMidHeight(players[ownerIndex]);
	inst.speed = remote->maxSpeed;
	inst.range = remote->maxDistance;
	if(remote->behaviorType == Remote::BehaviorType::GRAPH && remote->fixedGraphLength > 0) inst.range = remote->fixedGraphLength;
	if(inst.range <= 0 && remote->lifeTime > 0 && inst.speed > 0) inst.range = remote->lifeTime * inst.speed;
	if(remote->behaviorType == Remote::BehaviorType::GRAPH && remote->fixedGraphLength <= 0 && aimPos && glm::length(vec2(*aimPos)) > 1.0f) {
		const f32 aimDist = glm::length(vec2(*aimPos) - vec2(pos));
		inst.range = inst.range > 0 ? MIN(aimDist, inst.range) : aimDist;
		inst.landZ = aimPos->z;
		inst.hasLandZ = true;
	}
	inst.stopAtFirstHit = false;
	if(stopAtTarget && remote->behaviorType != Remote::BehaviorType::GRAPH && aimPos && glm::length(vec2(*aimPos)) > 1.0f && inst.speed > 0) {
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
		const bool plainStaticBox = inst.speed <= 0 && remote->boundType == Remote::BoundType::E_BOUND_BOX && remote->angle <= 0;
		const bool chained = remote->destroyCreateRemote != RemoteIdx::INVALID || !remote->onDestroy.empty() || !remote->onTime.empty() || !remote->onActivate.empty();
		const bool flip = plainStaticBox && (chained || nodeName == 0);
		const f32 sendYaw = atan2f(dir.y, dir.x) + (flip ? PI : 0.0f);
		vec3 sendPos = pos - vec3(0, 0, remote->offsetZ);
		const bool atAimedPoint = aimPos && glm::length(vec2(*aimPos)) > 1.0f && glm::length(vec2(pos) - vec2(*aimPos)) < 1.0f;
		if(plainStaticBox && !flip && !atAimedPoint) sendPos -= vec3(dir * (remote->forceTargetDir ? -remote->offsetY : remote->offsetY), 0);
		else if(plainStaticBox && flip && !atAimedPoint) sendPos += vec3(dir * (remote->forceTargetDir ? -remote->offsetY : remote->offsetY), 0);
		replication->SendRemoteCreateToAll(players[ownerIndex].Main().UID, seed, idx, sendPos, sendYaw, targetPos, targetMaster ? target : ActorUID::INVALID, Config().DevNoFireNode ? GetGameXmlContent().GetNodeName(0) : GetGameXmlContent().GetNodeName(nodeName), casterCreatesLocally ? players[ownerIndex].clientHd : ClientHandle::INVALID);
	}

	remoteInsts.push_back(inst);

	LOG("Remote :: spawn %d at (%.0f, %.0f) dir=(%.2f, %.2f) speed=%.0f range=%.0f wall=%lld", (i32)idx, pos.x, pos.y, dir.x, dir.y, inst.speed, inst.range, ShowcaseWallMs());
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

static RemoteIdx CostReplacedRemote(const GameXmlContent::CostLevel* level, RemoteIdx idx)
{
	if(level) foreach_const(rr, level->remoteReplace) if(rr->first == (i32)idx) return RemoteIdx(rr->second);
	return idx;
}

static void SpawnRemoteRules(World& world, const World::RemoteInst& parent, const Remote::SpawnRules& rules, const vec3& pos, i32 excludeIndex, RemoteIdx skipFirst, bool fromDestroy = false)
{
	bool skipped = false;
	foreach_const(rule, rules) {
		if(!skipped && rule->creature <= 0 && rule->kind == Remote::SPAWN_SINGLE && rule->idx == skipFirst) {
			skipped = true;
			continue;
		}
		if(rule->creature > 0) {
			world.SpawnCreatureFromRemote(parent, rule->creature, pos);
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
			World::RemoteInst* inst = WorldRemoteSpawner::Spawn(world, parent.ownerIndex, CostReplacedRemote(parent.costLevel, rule->idx), at + vec3(dir * child->offsetY, 0), dir, childTarget, parent.skillID, parent.castID, parent.damage,
				parent.clientRemoteID != 0 && !(fromDestroy && rule->kind != Remote::SPAWN_SINGLE && !rule->targetOwner), parent.casterLocal);
			if(inst) {
				inst->excludeIndex = excludeIndex;
				inst->costLevel = parent.costLevel;
				inst->centeredBox = parent.centeredBox;
			}
		}
	}
}

void World::MassTeleport(Player& gx, const StatusInfo& passive)
{
	const f32 radius = passive.Param("_MassiveTeleportRadius");
	const i32 effect = (i32)passive.ParamInt("_MassiveTeleportStatus");
	const vec3 dest = gx.body->GetWorldPos();
	i32 moved = 0;
	foreach(it, players) {
		if(it->index == gx.index || it->dead || it->team != gx.team) continue;
		if(glm::length(vec2(it->body->GetWorldPos()) - vec2(gx.massTeleportPos)) > radius) continue;
		if(FindActiveStatus(it->index, STATUS_SHUTTLE_TELEPORT_RESIST)) continue;
		const f32 a = moved * PI * 2.0f / 5.0f;
		if(effect != 0) ApplyStatus(gx, *it, effect);
		MovePlayer(*it, dest + vec3(cosf(a) * TELEPORT_SPREAD, sinf(a) * TELEPORT_SPREAD, 0), it->input.rot.upperYaw, true);
		moved++;
	}
	LOG("Passive :: %ls teleporter moved %d allies from (%.0f, %.0f) radius %.0f", gx.name.data(), moved, gx.massTeleportPos.x, gx.massTeleportPos.y, radius);
}

void World::RemoveBoardPoint(Player& gx)
{
	if(gx.boardPointUID == ActorUID::INVALID) return;
	auto found = actorDynamicMap.find(gx.boardPointUID);
	if(found != actorDynamicMap.end()) {
		actorDynamicList.erase(found->second);
		actorDynamicMap.erase(found);
	}
	gx.boardPointUID = ActorUID::INVALID;
}

void World::PlayerInteract(Player& player, ActorUID targetUID)
{
	Player* owner = nullptr;
	foreach(it, players) if(it->boardPointUID == targetUID && targetUID != ActorUID::INVALID) owner = &*it;
	if(!owner) {
		ActorMaster* target = FindMasterActor(targetUID);
		owner = target ? target->parent : nullptr;
	}
	if(!owner || player.dead || player.boardedOn >= 0) return;
	Player& gx = *owner;
	if(gx.index == player.index || gx.team != player.team || gx.dead || localTime >= gx.boardOpenUntil) return;
	const ActiveStatus* passive = FindPassiveOfType(gx, "STATUS_TYPE_GX09");
	if(!passive) return;
	const f32 reach = GetGameXmlContent().GetInteractionActiveDistance(INTERACTION_SHUTTLE_BOARD);
	const f32 dist = glm::length(vec2(gx.body->GetWorldPos()) - vec2(player.body->GetWorldPos()));
	if(reach > 0 && dist > reach + HitRadius(gx)) {
		LOG("Interaction :: %ls too far from %ls to board (%.0f > %.0f)", player.name.data(), gx.name.data(), dist, reach);
		return;
	}
	i32 applied = 0;
	foreach_const(p, passive->info->params) {
		if(p->name != "DATA") continue;
		ApplyStatus(gx, player, (i32)p->value);
		applied++;
	}
	LOG("Interaction :: %ls boarding statuses %d (params %d)", player.name.data(), applied, (i32)passive->info->params.size());
	player.boardedOn = (i32)gx.index;
	player.untargetable = true;
	replication->SendInteractionExecuteToAll(player.Main().UID, gx.Main().UID, (u32)INTERACTION_SHUTTLE_BOARD, 0);
	LOG("Interaction :: %ls boards %ls", player.name.data(), gx.name.data());
}

void World::Unboard(Player& passenger)
{
	if(passenger.boardedOn < 0) return;
	Player& gx = players[passenger.boardedOn];
	passenger.boardedOn = -1;
	passenger.untargetable = false;
	if(const ActiveStatus* passive = FindPassiveOfType(gx, "STATUS_TYPE_GX09")) {
		foreach_const(p, passive->info->params) {
			if(p->name != "DATA") continue;
			const i32 id = (i32)p->value;
			const StatusInfo* info = GetGameXmlContent().FindStatus(id);
			foreach(st, activeStatuses) {
				if(st->targetIndex == passenger.index && st->statusID == id) st->until = localTime;
				if(info && st->targetIndex == gx.index && st->statusID == info->casterStatus) st->until = localTime;
			}
		}
	}
	if(!passenger.dead) {
		MovePlayer(passenger, gx.body->GetWorldPos() + vec3(TELEPORT_SPREAD, 0, 0), passenger.input.rot.upperYaw, true);
	}
	LOG("Interaction :: %ls leaves %ls", passenger.name.data(), gx.name.data());
}

void World::UpdateRides()
{
	foreach(it, players) {
		if(it->boardOpenUntil != Time::ZERO && localTime >= it->boardOpenUntil) {
			it->boardOpenUntil = Time::ZERO;
			RemoveBoardPoint(*it);
		}
		if(it->boardPointAnnounceAt != Time::ZERO && localTime >= it->boardPointAnnounceAt) {
			it->boardPointAnnounceAt = Time::ZERO;
			replication->SendInteractionStatusToAll(it->boardPointUID, 1u << (PLAYER_FACTION_BASE + it->team), INTERACTION_SHUTTLE_BOARD, INTERACTION_NOT_OCCUPIED, it->clientHd);
		}
		if(it->boardedOn < 0) continue;
		Player& gx = players[it->boardedOn];
		const bool shuttle = (i32)gx.Main().classType == CLASS_NUM_SHUTTLE;
		if(it->dead || gx.dead || !shuttle || gx.Main().stance == 0) {
			Unboard(*it);
			continue;
		}
		TeleportPlayer(*it, gx.body->GetWorldPos(), gx.input.rot.upperYaw);
		if(it->clientHd != ClientHandle::INVALID && localTime >= it->nextRideSync) {
			it->nextRideSync = TimeAdd(localTime, TimeMsToTime(RIDE_SYNC_SEC * 1000.0));
			replication->SendSelfMove(it->clientHd, it->Main().UID, gx.body->GetWorldPos(), gx.movement.moveDir, gx.movement.moveSpeed, gx.input.rot.upperYaw);
		}
	}
}

void World::AddFuel(Player& player, f32 amount)
{
	const f32 resMax = MasterStat(player.Main().classType, RESOURCES[1].maxStat);
	if(resMax <= 0) return;
	player.res[1] = MIN(MAX(player.res[1], 0.0f) + amount, resMax);
	player.tNextResSend = Time::ZERO;
	LOG("Passive :: %ls fuel +%.0f -> %.0f", player.name.data(), amount, player.res[1]);
}

void World::RemoteHit(RemoteInst& inst, Player& target)
{
	const Remote& remote = *inst.remote;
	Player& owner = players[inst.ownerIndex];
	if(!RemoteCanAffect(inst, target, true)) return;
	if(remote.activateDelay > 0 && TimeDiffSec(TimeDiff(inst.spawnTime, localTime)) < remote.activateDelay) return;
	const bool enemy = target.team != owner.team;
	if(enemy && (i32)remote.ID == REMOTE_SHUTTLE_DODGE && (i32)owner.Main().classType == CLASS_NUM_SHUTTLE) {
		if(owner.fuelDodgeCast != inst.castID) {
			owner.fuelDodgeCast = inst.castID;
			owner.fuelDodgeTargets.clear();
		}
		if(eastl::find(owner.fuelDodgeTargets.begin(), owner.fuelDodgeTargets.end(), target.index) == owner.fuelDodgeTargets.end() && !owner.fuelDodgeTargets.full()) {
			owner.fuelDodgeTargets.push_back(target.index);
			AddFuel(owner, TOOLTIP_SHUTTLE_DODGE_FUEL);
		}
	}

	const i32 amount = (i32)(inst.damage * remote.attackMultiplier);
	if(enemy && remote.memorizeTarget) owner.memorizedTarget = target.Main().UID;
	if(enemy && remote.changeCasterAction != ActionStateID::INVALID && !inst.actionChanged && !owner.dead) {
		inst.actionChanged = true;
		const ActorUID targetUID = target.Main().UID;
		LOG("Remote :: %ls hit %ls -> caster action %d", owner.name.data(), target.name.data(), (i32)remote.changeCasterAction);
		PlayerCastSkill(owner, inst.skillID, target.body->GetWorldPos(), Slice<const ActorUID>(&targetUID, 1), 0, remote.changeCasterAction);
	}
	if(amount <= 0 && remote.statuses.empty() && remote.friendlyStatuses.empty() && remote.onActivate.empty() && remote.onHitCharacter.empty()) return;

	const bool sharedZone = inst.speed <= 0 && remote.lifeTime > remote.hitInvalidTime && inst.castID != 0;
	foreach(h, castHits) {
		if(h->playerIndex != target.index || localTime >= h->until) continue;
		if(h->instID == inst.instID) return;
		if(sharedZone && h->castID == inst.castID && h->remoteID == remote.ID) return;
	}
	if(inst.speed <= 0 && enemy) inst.hitsLeft--;
	if(!castHits.full()) {
		CastHit h;
		h.instID = inst.instID;
		h.remoteID = remote.ID;
		h.castID = inst.castID;
		h.playerIndex = target.index;
		h.until = TimeAdd(localTime, TimeMsToTime(MAX(remote.hitInvalidTime, 0.1f) * 1000.0));
		castHits.push_back(h);
	}

	if(amount > 0) {
		if(enemy && inst.skillID != SkillID::INVALID) PassiveOnSkillHit(owner, target);
		i32 stackBonus = 0;
		if(enemy && (i32)inst.skillID % 100 == ULTIMATE_SKILL_SUFFIX) {
			foreach_const(st, activeStatuses) {
				if(st->targetIndex == target.index && st->casterIndex == owner.index && st->info && st->info->additionalDamageByCaster > 0 && localTime < st->until)
					stackBonus += (i32)(st->info->additionalDamageByCaster * MAX(st->overlap, 1));
			}
			if(stackBonus > 0) LOG("Passive :: %ls stack bonus +%d on %ls", owner.name.data(), stackBonus, target.name.data());
		}
		i32 dealt = amount + stackBonus;
		if(enemy && GetGameXmlContent().IsRepeatHitReduced((i32)inst.skillID)) {
			RepeatHit* rh = nullptr;
			foreach(r, repeatHits) if(r->castID == inst.castID && r->targetIndex == target.index) rh = &*r;
			if(!rh) {
				repeatHits.push_back({ inst.castID, target.index, 0, localTime });
				rh = &repeatHits.back();
			}
			dealt = (i32)(dealt * MAX(1.0f - REPEAT_HIT_REDUCTION * rh->count, 1.0f - REPEAT_HIT_REDUCTION_CAP));
			rh->count++;
			rh->at = localTime;
		}
		if(enemy) DamagePlayer(owner, target, inst.skillID, dealt, nullptr, inst.clientRemoteID, remote.ID, inst.dir);
		else if(remote.damageGroup == Remote::DamageGroup::eFRIEND) HealPlayer(owner, target, amount);
	}

	inst.lastHitIndex = (i32)target.index;
	statusFromBehind = remote.offsetY < 0;
	statusForceCenter = inst.pos;
	statusForceCenterSet = true;
	if(enemy && remote.onHitOwnerStatus != 0 && !owner.dead) ApplyStatus(owner, owner, remote.onHitOwnerStatus);
	if(!target.dead) {
		const bool friendGroup = remote.damageGroup == Remote::DamageGroup::eFRIEND;
		if(enemy || friendGroup) {
			foreach_const(st, remote.statuses) {
				i32 statusID = st->statusID;
				if(inst.costLevel) {
					foreach_const(sr, inst.costLevel->statusReplace) if(sr->first == statusID) statusID = sr->second;
				}
				if(st->rate >= 100 || (i32)(RandUint() % 100) < st->rate) { statusCasterOverride = inst.sourceUID; ApplyStatus(owner, target, statusID, (f32)inst.damage); statusCasterOverride = ActorUID::INVALID; }
			}
		}
		if(!enemy) {
			foreach_const(st, remote.friendlyStatuses) {
				if(st->rate >= 100 || (i32)(RandUint() % 100) < st->rate) { statusCasterOverride = inst.sourceUID; ApplyStatus(owner, target, st->statusID, (f32)inst.damage); statusCasterOverride = ActorUID::INVALID; }
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
			castCostLevel = inst.costLevel;
			ScheduleSkillRemotes(owner, inst.skillID, target.body->GetWorldPos(), Slice<const ActorUID>(&targetUID, 1), atan2f(rel.y, rel.x), 1);
			castCostLevel = nullptr;
		}
	}
	statusFromBehind = false;
	statusForceCenterSet = false;
}

void World::RemoteHitAlongPath(RemoteInst& inst)
{
	const Remote& remote = *inst.remote;
	if(remote.damageGroup == Remote::DamageGroup::eNONE || remote.damageGroup == Remote::DamageGroup::INVALID) return;
	if(remote.attackMultiplier <= 0 && remote.statuses.empty()) return;
	const f32 radius = remote.boundSize[0] * 0.5f;
	foreach(it, players) {
		if(!RemoteCanAffect(inst, *it, false)) continue;
		if(it->team == players[inst.ownerIndex].team) continue;
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
			const f32 back = length * 0.5f;
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
		if(pr->remove) {
			bool ended = false;
			foreach(ri, remoteInsts) {
				if(ri->ownerIndex == pr->ownerIndex && ri->remote && ri->remote->ID == pr->idx && !ri->killNow) { ri->killNow = true; ended = true; }
			}
			if(ended) LOG("Remote :: %ls ends live remote %d (action Remove)", owner.name.data(), (i32)pr->idx);
			pr = pendingRemotes.erase_unsorted(pr);
			continue;
		}
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
			case ActionCommand::TargetPreset::CURRENT_FFF: {
				const ActorMaster* targetMaster = pr->target != ActorUID::INVALID ? FindMasterActor(pr->target) : nullptr;
				if(targetMaster && (!remote || remote->maxSpeed <= 0)) pos = targetMaster->parent->body->GetWorldPos();
			} break;
			case ActionCommand::TargetPreset::LAST_CHILD_FFF:
			case ActionCommand::TargetPreset::LAST_CHILD_TFF: {
				const ActorNpc* child = FindNpcActor(owner.lastChildUID);
				if(child && localTime < child->despawnAt) pos = vec3(vec2(child->pos), owner.body->GetWorldPos().z);
			} break;
			default: {
				if(pr->offsetFront != 0 || pr->offsetSide != 0) pos = vec3(vec2(owner.body->GetWorldPos()) + dir * pr->offsetFront + Perp(dir) * pr->offsetSide, owner.body->GetWorldPos().z);
			} break;
		}
		if(pr->atTarget) {
			const ActorMaster* targetMaster = pr->target != ActorUID::INVALID ? FindMasterActor(pr->target) : nullptr;
			if(targetMaster) pos = targetMaster->parent->body->GetWorldPos();
			else if(glm::length(vec2(pr->castPos)) > 1.0f) pos = vec3(vec2(pr->castPos), owner.body->GetWorldPos().z);
		}

		const bool portedZone = remote && (i32)owner.Main().classType >= FIRST_PORTED_CLASS && remote->boundType == Remote::BoundType::E_BOUND_BOX &&
			remote->maxSpeed <= 0 && remote->lifeTime > remote->hitInvalidTime;
		if(portedZone) {
			const f32 length = (f32)remote->boundSize[1];
			foreach_const(ri, remoteInsts) {
				if(ri->castID != pr->castID || !ri->remote || ri->remote->ID != pr->idx || ri->killNow) continue;
				const f32 ahead = glm::dot(vec2(pos) - vec2(ri->pos), dir);
				if(ahead >= 0 && ahead < length) pos = vec3(vec2(ri->pos) + dir * length, pos.z);
			}
		}
		if(!owner.dead) {
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
			if((i32)players[pr->ownerIndex].Main().classType >= FIRST_PORTED_CLASS) casterLocal = false;
			RemoteInst* spawned = SpawnRemote(pr->ownerIndex, pr->idx, pos, dir, pr->target, pr->skillID, pr->castID, pr->damage, true, &pr->castPos, pr->nodeName, casterLocal, pr->stopAtTarget);
			if(spawned) {
				spawned->sourceUID = pr->sourceUID;
				spawned->costLevel = pr->costLevel;
				spawned->centeredBox = atAimedPoint;
				const f32 nodeForward = atAimedPoint ? 0 : WeaponNodeForward(GetGameXmlContent(), owner.Main().classType, pr->skillID, *spawned->remote, pr->nodeName);
				if(nodeForward > 0) {
					spawned->pos += vec3(dir * nodeForward, 0);
					LOG("Remote :: %d weapon node forward %.0f (skill indicator)", (i32)pr->idx, nodeForward);
				}
			}
		}
		pr = pendingRemotes.erase_unsorted(pr);
	}

	for(int i = 0; i < (i32)remoteInsts.size(); ) {
		RemoteInst& inst = remoteInsts[i];
		const Remote& remote = *inst.remote;
		const f32 elapsed = (f32)TimeDiffSec(TimeDiff(inst.spawnTime, localTime));
		bool destroyed = inst.killNow;
		Player& instOwner = players[inst.ownerIndex];

		if(remote.attachTargetObject && inst.target != ActorUID::INVALID) {
			const ActorMaster* targetMaster = FindMasterActor(inst.target);
			if(!targetMaster || targetMaster->parent->dead) destroyed = true;
			else inst.pos = vec3(vec2(targetMaster->parent->body->GetWorldPos()), inst.pos.z);
		}
		if(remote.behaviorType == Remote::BehaviorType::CASTER_TARGET && !instOwner.dead) {
			const vec2 ownerPos = vec2(instOwner.body->GetWorldPos());
			if(inst.followDist < 0) inst.followDist = glm::length(vec2(inst.pos) - ownerPos);
			vec2 desired;
			const ActorMaster* tm = inst.target != ActorUID::INVALID ? FindMasterActor(inst.target) : nullptr;
			if(instOwner.clientHd == ClientHandle::INVALID && tm && !tm->parent->dead) desired = vec2(tm->parent->body->GetWorldPos());
			else {
				const f32 yaw = instOwner.input.rot.upperYaw;
				desired = ownerPos + vec2(cosf(yaw), sinf(yaw)) * inst.followDist;
			}
			const vec2 rel = desired - vec2(inst.pos);
			const f32 len = glm::length(rel);
			const f32 step = (remote.maxSpeed > 0 ? remote.maxSpeed : 600.0f) * delta;
			if(len > 1.0f) inst.pos += vec3(rel / len * MIN(step, len), 0);
			if(remote.boundType != Remote::BoundType::E_BOUND_NONE && remote.damageGroup != Remote::DamageGroup::eNONE) RemoteApplyArea(inst);
		}
		const bool attached = remote.attachFireObject || remote.behaviorType == Remote::BehaviorType::ORBIT ||
			remote.behaviorType == Remote::BehaviorType::BEAM;
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
			if(remote.timeChangeSpeed > 0) {
				inst.speed = remote.timeChangeSpeed;
				if(remote.maxDistance <= 0 && remote.lifeTime > 0) inst.range = inst.travelled + MAX(remote.lifeTime - elapsed, 0.0f) * inst.speed;
			}
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
			case Remote::BehaviorType::CASTER_TARGET: break;
			case Remote::BehaviorType::TARGET:
			case Remote::BehaviorType::FOLLOW_TARGET:
			case Remote::BehaviorType::HOMING: {
				if(remote.behaviorType == Remote::BehaviorType::TARGET && remote.attachTargetObject && inst.target != ActorUID::INVALID) break;
				if(inst.target == ActorUID::INVALID && remote.behaviorType == Remote::BehaviorType::HOMING && remote.homingLength > 0) {
					f32 bestDist = remote.homingLength;
					const f32 halfCos = cosf(remote.homingAngle * 0.5f * PI / 180.0f);
					foreach(it, players) {
						if(!RemoteCanAffect(inst, *it, false)) continue;
						const vec2 rel = vec2(it->body->GetWorldPos() - inst.pos);
						const f32 d = glm::length(rel);
						if(d > bestDist) continue;
						if(d > 1.0f && remote.homingAngle > 0 && remote.homingAngle < 360 && glm::dot(rel / d, inst.dir) < halfCos) continue;
						bestDist = d;
						inst.target = it->Main().UID;
					}
					if(inst.target != ActorUID::INVALID) LOG("Remote :: %d homing acquired a target at %.0f", (i32)remote.ID, bestDist);
				}
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
					const vec2 f = inst.graphDir;
					const vec2 side = -Perp(f);
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
				if(destroyed && inst.hasLandZ) inst.pos.z = inst.landZ;
			} break;

			default: {
				if(inst.speed > 0) {
					f32 step = inst.range > 0 ? MIN(inst.speed * delta, inst.range - inst.travelled) : inst.speed * delta;
					bool hitWall = false;
					if(!remote.ignoreGround && !remote.attachFireObject && step > 0) {
						f32 wallDist = 0;
						const vec2 a = vec2(inst.pos);
						if(physics.RaycastStatic(vec3(a, inst.wallZ), vec3(a + inst.dir * step, inst.wallZ), &wallDist, (i32)players[inst.ownerIndex].team)) {
							step = wallDist;
							hitWall = true;
						}
					}
					const vec2 from = vec2(inst.pos);
					const vec2 to = from + inst.dir * step;
					const f32 halfWidth = remote.boundSize[0] * 0.5f;
					const f32 boxFront = remote.boundType == Remote::BoundType::E_BOUND_BOX ? MAX(remote.offsetY + remote.boundSize[1] * 0.5f, 0.0f) : 0.0f;
					if(remote.damageGroup != Remote::DamageGroup::eNONE && remote.damageGroup != Remote::DamageGroup::INVALID) {
						foreach(it, players) {
							if(!RemoteCanAffect(inst, *it, false)) continue;
							const vec3 pos = it->body->GetWorldPos();
							if(fabsf(pos.z - inst.pos.z) > WEAPON_PROJECTILE_HIT_HEIGHT) continue;
							if(!LagSegmentHits(*it, from, to, halfWidth + HitRadius(*it) + PROJECTILE_HIT_TOLERANCE, boxFront + HitRadius(*it))) continue;
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
						if(hitWall || (inst.range > 0 && inst.travelled >= inst.range - 0.01f)) destroyed = true;
						if(hitWall) LOG("Remote :: %d stopped by a wall after %.0f", (i32)remote.ID, inst.travelled);
					}
				}
				else if(!inst.areaDone) {
					if(remote.activateDelay <= 0 || elapsed >= remote.activateDelay) {
						inst.areaDone = true;
						RemoteApplyArea(inst);
					}
				}
				else if(remote.lifeTime > 0 && inst.hitsLeft > 0) {
					RemoteApplyArea(inst);
				}
				if(inst.hitsLeft <= 0 && remote.lifeTime > 0) {
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
			const vec2 nrel = nearestPlayer ? vec2(nearestPlayer->body->GetWorldPos()) - vec2(inst.pos) : vec2(0);
			LOG("SkillRemoteEnd :: %d travelled=%.0f hitsLeft=%d nearest=%ls dist=%.0f along=%.0f side=%.0f dz=%.0f", (i32)remote.ID, inst.travelled, inst.hitsLeft, nearestPlayer ? nearestPlayer->name.data() : L"-", nearest,
				glm::dot(nrel, inst.dir), inst.dir.x * nrel.y - inst.dir.y * nrel.x, nearestPlayer ? nearestPlayer->body->GetWorldPos().z - inst.pos.z : 0.0f);
		}

		if(destroyed && inst.hasLandZ) inst.pos.z = inst.landZ;
		if(destroyed && !inst.destroyedByHit && inst.clientRemoteID != 0 && (inst.speed > 0 || inst.killNow)) replication->SendDestroyRemoteToAll(inst.clientRemoteID);
		if(destroyed) {
			const RemoteIdx next = remote.destroyCreateRemote;
			const RemoteInst copy = inst;
			remoteInsts.erase_unsorted(remoteInsts.begin() + i);
			if(!copy.chainEnded) {
				const i32 childExclude = remote.behaviorType == Remote::BehaviorType::CHAIN ? copy.lastHitIndex : copy.excludeIndex;
				if(next != RemoteIdx::INVALID) {
					const Remote* nextRemote = GetGameXmlContent().FindRemote(next);
					const f32 nextOffset = nextRemote ? nextRemote->offsetY : 0;
					RemoteInst* child = SpawnRemote(copy.ownerIndex, CostReplacedRemote(copy.costLevel, next), copy.pos + vec3(copy.dir * nextOffset, 0), copy.dir, copy.target, copy.skillID, copy.castID, copy.damage, copy.clientRemoteID != 0, nullptr, 0, copy.casterLocal);
					if(child) {
						child->excludeIndex = childExclude;
						child->costLevel = copy.costLevel;
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

	for(auto r = repeatHits.begin(); r != repeatHits.end(); ) {
		if(TimeDiffSec(TimeDiff(r->at, localTime)) > 10.0) r = repeatHits.erase_unsorted(r);
		else ++r;
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
	if(attacker.index != target.index) {
		attacker.lastHitTargetIndex = (i32)target.index;
		attacker.lastHitAt = localTime;
	}
	if(HasStatusType(target, "STATUS_TYPE_GODMODE")) return;
	const i32 damageIn = damage;
	f32 elementMul = 1.0f;
	if(HasStatusType(target, "STATUS_TYPE_EVADE") && attacker.team != target.team) {
		LOG("Evade :: %ls evaded %ls", target.name.data(), attacker.name.data());
		return;
	}

	{
		const RemoteIdx doc = proj ? proj->remoteDoc : remoteDoc;
		const Remote* pr = doc != RemoteIdx::INVALID ? GetGameXmlContent().FindRemote(doc) : nullptr;
		if(pr && pr->distPenaltyRate > 0 && pr->penaltyApplyDistance > 0 &&
		   glm::length(vec2(target.body->GetWorldPos()) - vec2(attacker.body->GetWorldPos())) < pr->penaltyApplyDistance) {
			damage = (i32)(damage * (1.0f - pr->distPenaltyRate / 100.0f));
		}
	}

	if(Config().DamageFormula && attacker.index != target.index) {
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
			if(st->info->type == "STATUS_TYPE_WEAK_ON_CRITICAL" && attacker.team != target.team) bonus = 100.0f;
			if(proj && st->info->type == "STATUS_TYPE_STACKBOMB" && st->casterIndex == attacker.index && FindPassiveOfType(attacker, "STATUS_TYPE_SNIPER")) bonus += TOOLTIP_SNIPER_CRIT_PER_MARK * st->overlap;
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
		if(damage <= 0) break;
		if(it->targetIndex != target.index || it->shieldHP <= 0) continue;
		const f32 absorbed = MIN(it->shieldHP, (f32)damage);
		it->shieldHP -= absorbed;
		it->absorbed += absorbed;
		damage -= (i32)absorbed;
	}
	if(damage <= 0) return;

	ActorMaster& chara = target.Main();
	if(Config().DevShowcase && target.index == showcase.prevDummy && showcase.firstHitMs < 0) {
		showcase.firstHitMs = (i64)(TimeDiffSec(TimeDiff(showcase.castAt, localTime)) * 1000.0);
		showcase.firstHitDamage = damage;
	}
	target.damageTaken += damage;
	target.hitsTaken++;
	const bool showcaseDummy = Config().DevShowcase && (target.clientHd == ClientHandle::INVALID || Config().DevMirror) && Config().DevGxTest != GX_DEATH_TEST_MODE;
	chara.hp = MAX(chara.hp - damage, (showcaseDummy || HasStatusType(target, "STATUS_TYPE_NEARLY_IMMORTAL") || HasStatusType(target, "STATUS_TYPE_UNDEAD")) ? 1 : 0);
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
		if(rageMax > 0) attacker.res[1] = MIN(attacker.res[1] + MasterStat(ac, STAT_RAGE_ATTACK_RECOVERY_VALUE), rageMax);
	}

	LOG("Damage :: %ls -> %ls skill=%d damage=%d%s hp=%d/%d in=%d remote=%u element=%d", attacker.name.data(), target.name.data(), (i32)skillID, damage, damageType == PACKET_DAMAGE_TYPE_CRITICAL ? " CRIT" : "", chara.hp, chara.hpMax,
		damageIn, (u32)(proj ? proj->remoteDoc : remoteDoc), (i32)(elementMul * 100.0f));
	if(proj && proj->remoteID != 0) {
		replication->SendRemoteDamageToAll(proj->remoteID, proj->remoteDoc, attacker.Main().UID, chara.UID, target.body->GetWorldPos(), proj->dir, damage, chara.hp, chara.hpMax, damageType);
	}
	else if(remoteID != 0) {
		replication->SendRemoteDamageToAll(remoteID, remoteDoc, attacker.Main().UID, chara.UID, target.body->GetWorldPos(), remoteDir, damage, chara.hp, chara.hpMax, damageType);
	}
	else {
		replication->SendDamageToAll(attacker.Main().UID, chara.UID, skillID, damage, chara.hp, chara.hpMax, damageType);
	}

	if(chara.hp > 0) PassiveOnDamaged(target);
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

void World::RunSkillProgramCommand(SkillProgram& prog, ActorMaster* caster, i32 cmdIndex, const vec2& dir)
{
	const auto& content = GetGameXmlContent();
	const auto& action = content.GetSkillAction(caster->classType, prog.actionID, prog.stance);
	const auto& cmd = action.commands[cmdIndex];
	f32 distance = 0;
	f32 moveDuration = 0;
	const GameXmlContent::MoveCurve* curve = nullptr;

	switch(cmd.type) {
		case ActionCommand::Type::STATE_BLOCK: {
			caster->parent->body->lockedMoveUntil = TimeAddSec(localTime, cmd.delay);
		} break
			;
		case ActionCommand::Type::GRAPH_MOVE_HORZ: {
			distance = cmd.graphMoveHorz.distance;
			moveDuration = SkillMoveDuration(content, caster->classType, action, cmd.animIndex);
			foreach_const(st, action.anims) {
				if(st->index != cmd.animIndex) continue;
				curve = content.GetMoveCurve(content.GetMaster(caster->classType).className.c_str(), st->animationId);
				if(!curve) continue;
				const f32 animLen = content.GetAniSeqTime(content.GetMaster(caster->classType).className.c_str(), st->animationId) * (f32)MAX(st->loop, 1);
				const f32 curveTime = curve->time > 0 ? curve->time : animLen;
				if(curveTime > 0) moveDuration = curveTime;
				else curve = nullptr;
				break;
			}
		} break;

		case ActionCommand::Type::MOVE: {
			switch(cmd.move.preset) {
				case ActionCommand::MovePreset::WARP: {
					distance = (f32)cmd.move.param2;
					moveDuration = 0.01f;
				} break;
				case ActionCommand::MovePreset::TARGET_POS:
				case ActionCommand::MovePreset::TARGET_POS_LEAP:
				case ActionCommand::MovePreset::GOTO_CURRENT_TARGET:
				case ActionCommand::MovePreset::GOTO_CURRENT_TARGET_CHASE:
				case ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH:
				case ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH_CHASE: {
					bool graphMove = false;
					foreach_const(c2, action.commands) if(c2->type == ActionCommand::Type::GRAPH_MOVE_HORZ) graphMove = true;
					if(!graphMove) {
						distance = 999999.0f;
						moveDuration = action.seqLength;
					}
				} break;
			}
		} break;
	}

	if(distance != 0) {
		Player& mover = *caster->parent;
		const vec2 from = vec2(mover.body->GetWorldPos());
		vec2 moveDir = distance < 0 ? -dir : dir;
		f32 moveDist = fabsf(distance);
		foreach_const(c2, action.commands) {
			if(c2->type != ActionCommand::Type::MOVE) continue;
			const auto preset = c2->move.preset;
			if((preset == ActionCommand::MovePreset::TARGET_POS || preset == ActionCommand::MovePreset::TARGET_POS_LEAP) && glm::length(vec2(prog.castPos)) > 1.0f) {
				moveDist = glm::length(vec2(prog.castPos) - from);
			}
			const bool gotoTarget = preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET || preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET_CHASE ||
				preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH || preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH_CHASE;
			if(gotoTarget && !prog.targetList.empty()) {
				const ActorMaster* tm = FindMasterActor(prog.targetList[0]);
				if(tm) {
					const vec2 rel = vec2(tm->parent->body->GetWorldPos()) - from;
					const f32 len = glm::length(rel);
					if(len > 1.0f) moveDir = rel / len;
					const bool through = preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH || preset == ActionCommand::MovePreset::GOTO_CURRENT_TARGET_THROUGH_CHASE;
					if(!through) moveDist = MIN(moveDist, MAX(len - HitRadius(*tm->parent) - HitRadius(mover), 0.0f));
					else if(moveDist > 999990.0f) moveDist = len;
				}
			}
		}
		if(moveDist > 999990.0f) moveDist = 0;
		mover.input.moveTo = mover.body->GetWorldPos();
		mover.body->vel = vec3(0);
		if(moveDuration <= 0.02f) physics.Move(mover.body, vec3(moveDir * moveDist, 0), moveDuration);
		else {
			mover.dashDir = moveDir;
			mover.dashSpeed = moveDist / moveDuration;
			mover.dashStartAt = localTime;
			mover.dashOrigin = mover.body->GetWorldPos();
			mover.dashDist = moveDist;
			mover.dashDone = 0;
			mover.dashCurve = curve;
			mover.dashUntil = TimeAdd(localTime, TimeMsToTime(moveDuration * 1000.0));
			if(curve) LOG("Dash :: %ls follows move graph over %.2fs for %.0f", mover.name.data(), moveDuration, moveDist);
		}
	}
}

void World::ExecuteSkillProgram(SkillProgram& prog)
{
	ActorMaster* caster = FindMasterActor(prog.casterUID);
	if(!caster) {
		prog.Finish();
		return;
	}
	const vec2 dir = vec2(cosf(prog.castAngle), sinf(prog.castAngle));
	const auto& action = GetGameXmlContent().GetSkillAction(caster->classType, prog.actionID, prog.stance);
	const f32 elapsed = (f32)TimeDiffSec(TimeDiff(prog.startTime, localTime));
	bool pending = false;
	for(i32 i = 0; i < (i32)action.commands.size() && i < 64; i++) {
		if(prog.done & (1ull << i)) continue;
		const auto& cmd = action.commands[i];
		f32 start = -1.0f;
		for(const SkillAnimPass& pass : prog.passes) {
			if(pass.index == cmd.animIndex || (cmd.animIndex == -1 && start < 0)) start = pass.start;
		}
		if(prog.passes.empty() && (cmd.animIndex == 0 || cmd.animIndex == -1)) start = 0;
		if(start < 0) {
			prog.done |= 1ull << i;
			continue;
		}
		if(elapsed < start + (cmd.type == ActionCommand::Type::STATE_BLOCK ? 0.0f : cmd.delay)) {
			pending = true;
			continue;
		}
		prog.done |= 1ull << i;
		RunSkillProgramCommand(prog, caster, i, dir);
	}
	if(!pending) prog.Finish();
}
