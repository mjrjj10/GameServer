#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const f32 BOT_TITAN_MELEE_SKILL_DIST = 400.0f;

f32 World::BotTitanSkillDistance(i32 skill) const
{
	const GameXmlContent::SkillInput* in = GetGameXmlContent().FindSkillInput(skill);
	return in && in->maxDistance > 0 ? in->maxDistance : BOT_TITAN_MELEE_SKILL_DIST;
}

static const i32 BOT_STAGE_RECOVERY = 0;
static const i32 BOT_STAGE_SHIELD = 1;

static f32 HpRatio(i32 hp, i32 hpMax)
{
	return hpMax > 0 ? (f32)hp / (f32)hpMax : 1.0f;
}

bool World::BotStageSkill(Player& bot)
{
	const auto& content = GetGameXmlContent();
	const i32 skill = (i32)bot.stageSkills[1];
	if(skill <= 0 || bot.titanForm >= 0 || localTime < bot.botStageSkillReady) return false;
	const vec3 pos = bot.body->GetWorldPos();
	ActorUID target = ActorUID::INVALID;
	vec3 at = pos;
	switch(content.BotStageSkillKind(skill)) {
		case BOT_STAGE_RECOVERY: {
			bool use = HpRatio(bot.Main().hp, bot.Main().hpMax) < content.BotConstant("BOTLOGIC_CONSTANT_KEY_MY_LESS_HP_RATIO_FOR_RECOVERY_STAGE_SKILL");
			i32 hurt = 0;
			const f32 radius = content.BotConstant("BOTLOGIC_CONSTANT_KEY_FRIENDLY_CHECK_RADIUS_FOR_RECOVERY_STAGE_SKILL");
			foreach_const(p, players) {
				if(p->index == bot.index || p->team != bot.team || p->dead || glm::length(vec2(p->body->GetWorldPos()) - vec2(pos)) > radius) continue;
				if(HpRatio(p->Main().hp, p->Main().hpMax) < content.BotConstant("BOTLOGIC_CONSTANT_KEY_FRIENDLY_HP_RATIO_FOR_RECOVERY_STAGE_SKILL")) hurt++;
			}
			if(hurt >= (i32)content.BotConstant("BOTLOGIC_CONSTANT_KEY_NUM_MIN_FRIENDLY_FOR_RECOVERY_STAGE_SKILL")) use = true;
			if(!use) return false;
		} break;
		case BOT_STAGE_SHIELD: {
			static const u32 towerCenterTag = StatusTypeKey("Tower_Center");
			static const u32 towerSideTag = StatusTypeKey("Tower_Side");
			const f32 radius = content.BotConstant("BOTLOGIC_CONSTANT_KEY_TOWER_CHECK_RADIUS_FOR_SHIELD_STAGE_SKILL");
			const f32 ratio = content.BotConstant("BOTLOGIC_CONSTANT_KEY_TOWER_HP_RATIO_FOR_SHIELD_STAGE_SKILL");
			foreach_const(npc, actorNpcList) {
				if(!npc->monsterAi || (npc->monsterAi->tagKey != towerCenterTag && npc->monsterAi->tagKey != towerSideTag) || npc->hp <= 0) continue;
				if((i32)npc->faction != (i32)bot.team || glm::length(vec2(npc->pos) - vec2(pos)) > radius || HpRatio(npc->hp, npc->hpMax) >= ratio) continue;
				target = npc->UID;
				at = npc->pos;
				break;
			}
			if(target == ActorUID::INVALID) return false;
		} break;
		default: return false;
	}
	bot.botStageSkillReady = TimeAdd(localTime, TimeMsToTime(MAX(content.GetSkillCoolTime(skill), 1.0f) * 1000.0));
	LOG("BotSkill :: %ls uses stage skill %d", bot.name.data(), skill);
	PlayerCastSkill(bot, SkillID(skill), at, Slice<const ActorUID>(&target, target != ActorUID::INVALID ? 1 : 0), 0);
	return true;
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
		if(bot.casting.skillID != SkillID::INVALID || BotStageSkill(bot)) continue;

		const ActorMaster& master = bot.Main();
		const bool titan = bot.titanForm >= 0 && bot.titanForm < (i32)content.titan.forms.size();
		const auto* skills = content.FindBotSkills(titan ? content.titan.forms[bot.titanForm].doc : 100000000 + (i32)master.classType);
		if(!skills) continue;

		const vec3 pos = bot.body->GetWorldPos();
		Player* target = nullptr;
		f32 best = 1e9f;
		foreach(e, players) {
			if(e->dead || e->team == bot.team || !(e->fogSeenBy & (1u << bot.team))) continue;
			const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(pos));
			if(d < best) { best = d; target = &*e; }
		}
		if(!target) continue;

		const i32 base = (i32)bot.mainCharaID * 2;
		eastl::fixed_vector<i32,5,false> ready;
		for(i32 i = 0; i < 5; i++) {
			const i32 skill = (*skills)[i];
			if(skill == 0 || localTime < bot.botSkillReady[i]) continue;
			if(!titan && i != 4 && !Config().DevShowcase && (i32)bot.skills[base] != skill && (i32)bot.skills[base + 1] != skill) continue;
			if(titan && i == 4) continue;
			const f32 maxDist = titan ? BotTitanSkillDistance(skill) : content.GetBotSkillMaxDistance(skill);
			if(maxDist <= 0 || best > maxDist) continue;
			if(!Config().DevShowcase && !Config().DevMirror && !SkillCostAffordable(bot, skill, PlayerSkillConsumeMP(bot, skill))) continue;
			ready.push_back(i);
		}
		if(ready.empty()) continue;
		const i32 slot = ready[RandUint() % ready.size()];
		const i32 skill = (*skills)[slot];

		const f32 botCd = slot == 4 ? content.BotUgSkillCoolTime() : content.BotNormalSkillCoolTime();
		const f32 cd = MAX(content.GetSkillCoolTime(skill), botCd);
		bot.botSkillReady[slot] = TimeAdd(localTime, TimeMsToTime(cd * 1000.0));
		const GameXmlContent::SkillInput* in = content.FindSkillInput(skill);
		bot.tNextBotSkill = TimeAdd(localTime, TimeMsToTime(MAX(MAX(content.BotAfterSkillDelay(), 0.5f), in ? in->globalCoolTime : 0.0f) * 1000.0));

		const vec3 tpos = target->body->GetWorldPos();
		const f32 yaw = atan2f(tpos.y - pos.y, tpos.x - pos.x);
		bot.input.rot.upperYaw = yaw;
		bot.input.rot.bodyYaw = yaw;
		bot.weaponAimYaw = yaw;
		const ActorUID targetUID = target->Main().UID;
		bot.botAimTarget = targetUID;
		LOG("BotSkill :: %ls casts %d at %ls (dist %.0f, cd %.0f)", bot.name.data(), skill, target->name.data(), best, cd);
		PlayerCastSkill(bot, SkillID(skill), tpos, Slice<const ActorUID>(&targetUID, in && in->takesTarget ? 1 : 0), 0);
	}
}

static const f32 BOT_ENGAGE_RANGE = 2500.0f;
static const f32 BOT_MELEE_RANGE = 150.0f;
static const f32 BOT_MIN_FIRE_DELAY = 0.15f;
static const f32 BOT_RANGE_FRACTION = 0.7f;
static const f32 BOT_RANGED_MAX = 800.0f;
static const f32 BOT_STRAFE_STEP = 0.6f;
static const f32 BOT_REPATH_DIST = 150.0f;
static const f32 BOT_STUCK_DIST = 20.0f;
static const f32 BOT_ROAM_STEP = 800.0f;
static const f32 BOT_PIECE_SEARCH_RANGE = 4000.0f;
static const i32 BOT_TITAN_PIECE_ITEM = 130100904;

bool World::BotObjective(const Player& bot, vec2* goal)
{
	if(!map) return false;
	if(!titanSaveAreasBuilt) {
		titanSaveAreasBuilt = true;
		foreach_const(n, map->levelEvents) {
			if(n->kind != GameXmlContent::LevelEventNode::BOUND_TRIGGER) continue;
			const GameXmlContent::Map::Area* area = map->FindArea(n->areaID);
			if(!area) continue;
			foreach_const(o, n->outputs) {
				const GameXmlContent::LevelEventNode* cond = map->FindLevelEvent(*o);
				if(!cond || cond->kind != GameXmlContent::LevelEventNode::TEAM_CONDITION || (cond->team != Faction::RED && cond->team != Faction::BLUE)) continue;
				foreach_const(s, cond->outputs) {
					const GameXmlContent::LevelEventNode* save = map->FindLevelEvent(*s);
					if(!save || save->kind != GameXmlContent::LevelEventNode::SAVE_GOT_PIECE) continue;
					titanSaveAreas[(i32)cond->team] = vec2(area->pos);
					titanSaveAreaKnown[(i32)cond->team] = true;
				}
			}
		}
	}
	const vec2 here = vec2(bot.body->GetWorldPos());
	if(bot.titanPieces > 0 && titanSaveAreaKnown[bot.team]) {
		*goal = titanSaveAreas[bot.team];
		return true;
	}
	f32 best = BOT_PIECE_SEARCH_RANGE;
	bool found = false;
	foreach_const(g, groundItems) {
		if(g->itemIndex != BOT_TITAN_PIECE_ITEM) continue;
		const f32 d = glm::length(vec2(g->pos) - here);
		if(d < best) { best = d; *goal = vec2(g->pos); found = true; }
	}
	return found;
}
static const u32 BOT_DECISION_MIN_MS = 400;
static const u32 BOT_DECISION_SPAN_MS = 500;
static const u32 BOT_STUCK_CHECK_MS = 500;
static const f32 BOT_TOWER_MARGIN = 200.0f;

static vec2 RotateVec(const vec2& v, f32 a)
{
	return vec2(v.x * cosf(a) - v.y * sinf(a), v.x * sinf(a) + v.y * cosf(a));
}

bool World::BotDangerAt(const Player& bot, const vec2& at, vec2* towerPos, f32* towerRange) const
{
	foreach_const(npc, actorNpcList) {
		if(!npc->monsterAi || npc->monsterAi->run > 0 || npc->hp <= 0 || npc->removeAt != Time::ZERO) continue;
		if(npc->faction != Faction::RED && npc->faction != Faction::BLUE) continue;
		if(npc->faction == (Faction)bot.team) continue;
		const GameXmlContent::MonsterAttack* attack = npc->monsterAi->byTarget[GameXmlContent::MonsterAttack::MASTER];
		if(!attack) continue;
		const f32 range = attack->range + BOT_TOWER_MARGIN;
		if(glm::length(at - vec2(npc->pos)) >= range) continue;
		if(towerPos) *towerPos = vec2(npc->pos);
		if(towerRange) *towerRange = range;
		return true;
	}
	return false;
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
			if(e->dead || e->team == bot.team || e->untargetable || e->Main().classType == ClassType::NONE || !(e->fogSeenBy & (1u << bot.team))) continue;
			if(BotDangerAt(bot, vec2(e->body->GetWorldPos()), nullptr, nullptr)) continue;
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
		else if(BotObjective(bot, &goal)) {
			const vec2 d = goal - here;
			const f32 len = glm::length(d);
			if(len > BOT_ROAM_STEP) goal = here + d / len * BOT_ROAM_STEP;
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
		vec2 tower;
		f32 towerRange = 0;
		if(BotDangerAt(bot, goal, &tower, &towerRange)) {
			const vec2 away = here - tower;
			const f32 len = glm::length(away);
			goal = tower + (len > 1.0f ? away / len : vec2(1, 0)) * towerRange;
		}
		bot.input.moveTo = vec3(goal, pos.z);
		bot.input.speed = PlayerMoveSpeed(bot);
	}
}

void World::UpdateBotCombat()
{
	foreach(it, players) {
		Player& bot = *it;
		if(bot.clientHd != ClientHandle::INVALID || bot.dead) continue;
		if(localTime < bot.tNextBotShot || localTime < bot.ccUntil) continue;
		if(IsBlocked(bot, true, false, false)) continue;
		if(WeaponIsMelee(bot) && Config().DevShowcase) continue;
		if(WeaponIsMelee(bot)) {
			foreach_const(e, players) {
				if(e->dead || e->team == bot.team || !(e->fogSeenBy & (1u << bot.team))) continue;
				if(glm::length(vec2(e->body->GetWorldPos()) - vec2(bot.body->GetWorldPos())) <= BOT_MELEE_RANGE + HitRadius(*e) + HitRadius(bot)) {
					bot.weaponFired = true;
					bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(BotNextFireDelay(bot) * 1000.0));
					break;
				}
			}
			continue;
		}

		const f32 range = WeaponRange(bot);

		const vec3 pos = bot.body->GetWorldPos();
		Player* target = nullptr;
		f32 best = range;
		foreach(e, players) {
			if(e->dead || e->team == bot.team || !(e->fogSeenBy & (1u << bot.team))) continue;
			const f32 d = glm::length(vec2(e->body->GetWorldPos()) - vec2(pos));
			if(d < best) { best = d; target = &*e; }
		}
		bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(BotNextFireDelay(bot) * 1000.0));
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
		bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(BotNextFireDelay(bot) * 1000.0));
	}
}

f32 World::BotNextFireDelay(const Player& bot) const
{
	const WeaponFiring& wf = ActiveWeaponFiring(bot, bot.Main().stance);
	f32 delay = wf.fireDelay;
	if(bot.firingCombo < wf.comboDelay.size() && wf.comboDelay[bot.firingCombo] > 0 && wf.comboDelay[bot.firingCombo] <= wf.comboValid[bot.firingCombo]) delay = wf.comboDelay[bot.firingCombo];
	return MAX(delay, BOT_MIN_FIRE_DELAY) * WeaponFireDelayScale(bot);
}
