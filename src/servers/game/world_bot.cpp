#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

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
		const GameXmlContent::SkillInput* in = content.FindSkillInput(skill);
		PlayerCastSkill(bot, SkillID(skill), tpos, Slice<const ActorUID>(&targetUID, in && in->takesTarget ? 1 : 0), 0);
	}
}

static const f32 BOT_ENGAGE_RANGE = 2500.0f;
static const f32 BOT_MELEE_RANGE = 150.0f;
static const f32 BOT_RANGE_FRACTION = 0.7f;
static const f32 BOT_RANGED_MAX = 800.0f;
static const f32 BOT_STRAFE_STEP = 0.6f;
static const f32 BOT_REPATH_DIST = 150.0f;
static const f32 BOT_STUCK_DIST = 20.0f;
static const f32 BOT_ROAM_STEP = 800.0f;
static const u32 BOT_DECISION_MIN_MS = 400;
static const u32 BOT_DECISION_SPAN_MS = 500;
static const u32 BOT_STUCK_CHECK_MS = 500;

static vec2 RotateVec(const vec2& v, f32 a)
{
	return vec2(v.x * cosf(a) - v.y * sinf(a), v.x * sinf(a) + v.y * cosf(a));
}

void World::UpdateBotMovement()
{
	if(Config().DevShowcase) return;
	foreach(it, players) {
		Player& bot = *it;
		if(bot.clientHd != ClientHandle::INVALID || bot.dead || bot.Main().classType == ClassType::NONE) continue;
		if(localTime < bot.ccUntil || localTime < bot.dashUntil || bot.casting.skillID != SkillID::INVALID || AttachedToCaster(bot)) continue;
		const vec3 pos = bot.body->GetWorldPos();
		const vec2 here = vec2(pos);
		bool stuck = false;
		if(localTime >= bot.botStuckCheck) {
			stuck = bot.botStuckCheck != Time::ZERO && glm::length(here - bot.botLastPos) < BOT_STUCK_DIST && glm::length(vec2(bot.input.moveTo) - here) > BOT_REPATH_DIST;
			bot.botLastPos = here;
			bot.botStuckCheck = TimeAdd(localTime, TimeMsToTime(BOT_STUCK_CHECK_MS));
		}
		const bool arrived = glm::length(vec2(bot.input.moveTo) - here) < BOT_REPATH_DIST;
		if(!stuck && !arrived && localTime < bot.botMoveAt) continue;
		bot.botMoveAt = TimeAdd(localTime, TimeMsToTime(BOT_DECISION_MIN_MS + RandUint() % BOT_DECISION_SPAN_MS));
		if(stuck || RandUint() % 6 == 0) bot.botStrafe = -bot.botStrafe;

		const Player* target = nullptr;
		f32 best = 1e9f;
		foreach_const(e, players) {
			if(e->dead || e->team == bot.team || e->untargetable || e->Main().classType == ClassType::NONE) continue;
			const f32 d = glm::length(vec2(e->body->GetWorldPos()) - here);
			if(d < best) { best = d; target = &*e; }
		}
		vec2 goal;
		if(target && best <= BOT_ENGAGE_RANGE) {
			const vec2 tp = vec2(target->body->GetWorldPos());
			const f32 range = WeaponIsMelee(bot) ? BOT_MELEE_RANGE : MIN(WeaponRange(bot) * BOT_RANGE_FRACTION, BOT_RANGED_MAX);
			const vec2 from = here - tp;
			const f32 a = atan2f(from.y, from.x) + bot.botStrafe * BOT_STRAFE_STEP;
			goal = tp + vec2(cosf(a), sinf(a)) * range;
			const f32 face = atan2f(tp.y - here.y, tp.x - here.x);
			bot.input.rot.upperYaw = face;
			bot.input.rot.bodyYaw = face;
			bot.input.rot.upperPitch = 0;
		}
		else if(target) {
			const vec2 toward = glm::normalize(vec2(target->body->GetWorldPos()) - here);
			goal = here + RotateVec(toward, bot.botStrafe * 0.3f) * BOT_ROAM_STEP;
		}
		else {
			goal = here + vec2(cosf(bot.input.rot.bodyYaw), sinf(bot.input.rot.bodyYaw)) * BOT_ROAM_STEP;
		}
		if(stuck) {
			const vec2 d = goal - here;
			const f32 len = glm::length(d);
			goal = here + RotateVec(len > 1.0f ? d / len : vec2(1, 0), bot.botStrafe * PI * 0.6f) * BOT_ROAM_STEP;
		}
		bot.input.moveTo = vec3(goal, pos.z);
		bot.input.speed = PlayerMoveSpeed(bot);
	}
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
		if(WeaponIsMelee(bot) && Config().DevShowcase) continue;
		if(WeaponIsMelee(bot)) {
			foreach_const(e, players) {
				if(e->dead || e->team == bot.team) continue;
				if(glm::length(vec2(e->body->GetWorldPos()) - vec2(bot.body->GetWorldPos())) <= BOT_MELEE_RANGE + HitRadius(*e) + HitRadius(bot)) { bot.weaponFired = true; break; }
			}
			continue;
		}

		const f32 range = WeaponRange(bot);

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
		const f32 delay = MAX(stanceDelay > 0 ? stanceDelay : content.GetWeaponFireDelay((i32)weapon), 0.15f) * WeaponFireDelayScale(bot);
		bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(delay * 1000.0));
		vec3 aim = target ? target->body->GetWorldPos() : vec3(0);
		if(!target && !Config().DevShowcase) {
			f32 bestNpc = range;
			foreach_const(npc, actorNpcList) {
				if(!NpcHittableBy(*npc, bot)) continue;
				const f32 d = glm::length(vec2(npc->pos) - vec2(pos));
				if(d < bestNpc) { bestNpc = d; best = d; aim = npc->pos; }
			}
			if(bestNpc >= range) continue;
		}
		else if(!target) continue;

		if(!WeaponHeatShot(bot, localTime, replication, WeaponHeatScale(bot))) continue;
		const vec2 rel = vec2(aim) - vec2(pos);
		const vec2 dir = glm::length(rel) > 1.0f ? glm::normalize(rel) : vec2(1, 0);
		bot.weaponAimYaw = atan2f(dir.y, dir.x);
		PlayerWeaponShot(bot, pos, dir, best);
	}
}
