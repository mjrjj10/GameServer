#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>
#include <chrono>

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
			if(e->dead || e->untargetable || e->team == bot.team || !(e->fogSeenBy & (1u << bot.team))) continue;
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
		const f32 cd = MAX(SkillCoolTime(bot, skill), botCd);
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

static const f32 BOT_FARM_RANGE = 4000.0f;

bool World::BotFarmTarget(const Player& bot, vec2* goal, f32 range)
{
	if(!map || !map->ingameLevels) return false;
	const vec2 here = vec2(bot.body->GetWorldPos());
	f32 bestMinion = range, bestNeutral = range;
	vec2 minion, neutral;
	foreach_const(npc, actorNpcList) {
		if(!npc->monsterAi || npc->ownerUID != ActorUID::INVALID || IsStructure(*npc) || !NpcHittableBy(*npc, bot)) continue;
		const f32 d = glm::length(vec2(npc->pos) - here);
		if(npc->faction == Faction::RED || npc->faction == Faction::BLUE) {
			if(d < bestMinion && !BotDangerAt(bot, vec2(npc->pos), nullptr, nullptr)) { bestMinion = d; minion = vec2(npc->pos); }
		}
		else if((npc->faction == Faction::ORANGE || npc->faction == Faction::MONSTER) && d < bestNeutral) { bestNeutral = d; neutral = vec2(npc->pos); }
	}
	if(bestMinion < range) { *goal = minion; return true; }
	if(bestNeutral < range) { *goal = neutral; return true; }
	return false;
}

void World::BuildTitanSaveAreas()
{
	if(titanSaveAreasBuilt || !map) return;
	titanSaveAreasBuilt = true;
	{
		foreach_const(n, map->levelEvents) {
			if(n->kind != GameXmlContent::LevelEventNode::BOUND_TRIGGER) continue;
			const GameXmlContent::Map::Area* area = map->FindArea(n->areaID);
			if(!area) continue;
			foreach_const(o, n->outputs) {
				const GameXmlContent::LevelEventNode* cond = map->FindLevelEvent(*o);
				if(!cond || cond->kind != GameXmlContent::LevelEventNode::TEAM_CONDITION || (cond->team != Faction::RED && cond->team != Faction::BLUE)) continue;
				foreach_const(s, cond->outputs) {
					const GameXmlContent::LevelEventNode* save = map->FindLevelEvent(*s);
					if(save && save->kind == GameXmlContent::LevelEventNode::STATUS_CHANGE && save->apply && save->statusIndex != 0) {
						const StatusInfo* heal = GetGameXmlContent().FindStatus(save->statusIndex);
						if(heal && heal->type == StatusType::HEAL) {
							titanHealAreas[(i32)cond->team] = vec2(area->pos);
							titanHealAreaKnown[(i32)cond->team] = true;
						}
					}
					if(!save || save->kind != GameXmlContent::LevelEventNode::SAVE_GOT_PIECE) continue;
					titanSaveAreas[(i32)cond->team] = vec2(area->pos);
					titanSaveAreaKnown[(i32)cond->team] = true;
				}
			}
		}
	}
}

bool World::BotObjective(const Player& bot, vec2* goal)
{
	if(!map) return false;
	BuildTitanSaveAreas();
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
static const f32 HYP_BOT_OCCUPY_FIGHT_RANGE = 600.0f;
static const f32 HYP_BOT_TITAN_ENGAGE_RANGE = 1500.0f;
enum BotMode: u8 { BOT_MODE_FIGHT = 0, BOT_MODE_OCCUPY, BOT_MODE_OBJECTIVE, BOT_MODE_FARM, BOT_MODE_LANE, BOT_MODE_ROAM };
static const char* const BOT_MODE_NAMES[] = { "fight", "occupy", "objective", "farm", "lane", "roam" };
static const f32 HYP_BOT_LANE_FARM_RANGE = 1500.0f;
static const f32 HYP_BOT_RETREAT_HP = 0.25f;
static const f32 HYP_BOT_RETREAT_DONE_HP = 0.9f;
static const i32 HYP_BOT_SIEGE_TANKS = 2;
static const i32 HYP_BOT_SIEGE_GIANT_WEIGHT = 3;
static const i32 BOT_LANE_MIN_POINTS = 10;
static const f32 BOT_LANE_END_DIST = 3000.0f;
static const f32 BOT_LANE_SAME_DIST = 1500.0f;
static const i32 BOT_LANE_LOOKAHEAD = 2;
static const f32 BOT_NAV_MERGE_DIST = 200.0f;
static const f32 BOT_NAV_LINK_RANGE = 1500.0f;
static const f32 BOT_NAV_REACHED = 200.0f;
static const f32 BOT_NAV_REROUTE_DIST = 500.0f;
static const f32 BOT_NAV_RAY_HEIGHT = 100.0f;
static const f32 BOT_NAV_RAY_SIDE = 40.0f;

bool World::BotSees(const vec3& from, const vec3& to)
{
	const vec2 d = vec2(to) - vec2(from);
	const f32 len = glm::length(d);
	if(len < 1.0f) return true;
	foreach_const(it, actorNpcList) {
		if(!it->bodyBlocker || !it->monsterAi) continue;
		const vec2 c = vec2(it->pos) - vec2(from);
		const f32 along = MIN(MAX(glm::dot(c, d) / (len * len), 0.0f), 1.0f);
		if(glm::length(c - d * along) < it->monsterAi->bodyBlockRadius + PLAYER_BODY_RADIUS) return false;
	}
	const vec2 side = vec2(-d.y, d.x) / len * BOT_NAV_RAY_SIDE;
	for(i32 k = -1; k <= 1; k++) {
		const vec2 off = side * (f32)k;
		if(physics.RaycastStatic(vec3(vec2(from) + off, from.z + BOT_NAV_RAY_HEIGHT), vec3(vec2(to) + off, to.z + BOT_NAV_RAY_HEIGHT), nullptr)) return false;
	}
	return true;
}

void World::BuildBotNav()
{
	botNavBuilt = true;
	if(!map) return;
	BuildTitanSaveAreas();
	foreach_const(path, map->paths) {
		i32 prev = -1;
		foreach_const(pt, path->second) {
			i32 node = -1;
			for(i32 n = 0; n < (i32)botNav.size(); n++) {
				if(glm::length(vec2(botNav[n].pos) - vec2(*pt)) < BOT_NAV_MERGE_DIST) { node = n; break; }
			}
			if(node < 0) {
				node = (i32)botNav.size();
				botNav.push_back();
				botNav.back().pos = *pt;
			}
			if(prev >= 0 && prev != node && eastl::find(botNav[prev].links.begin(), botNav[prev].links.end(), (u16)node) == botNav[prev].links.end()) {
				botNav[prev].links.push_back((u16)node);
				botNav[node].links.push_back((u16)prev);
			}
			prev = node;
		}
	}
	const auto cells = GetGameXmlContent().measuredWalkCells.find((i32)map->index);
	if(cells != GetGameXmlContent().measuredWalkCells.end()) {
		foreach_const(c, cells->second) {
			if(InsideBodyBlocker(vec2(*c), PLAYER_BODY_RADIUS)) continue;
			botNav.push_back();
			botNav.back().pos = *c;
		}
	}
	for(i32 a = 0; a < (i32)botNav.size(); a++) {
		for(i32 b = a + 1; b < (i32)botNav.size(); b++) {
			if(glm::length(vec2(botNav[a].pos) - vec2(botNav[b].pos)) > BOT_NAV_LINK_RANGE) continue;
			if(eastl::find(botNav[a].links.begin(), botNav[a].links.end(), (u16)b) != botNav[a].links.end()) continue;
			if(!BotSees(botNav[a].pos, botNav[b].pos)) continue;
			botNav[a].links.push_back((u16)b);
			botNav[b].links.push_back((u16)a);
		}
	}
	for(i32 team = 0; team < 2; team++) {
		if(!titanSaveAreaKnown[team] || !titanSaveAreaKnown[1 - team]) continue;
		foreach_const(path, map->paths) {
			const auto& pts = path->second;
			if((i32)pts.size() < BOT_LANE_MIN_POINTS) continue;
			if(glm::length(vec2(pts.front()) - titanSaveAreas[team]) > BOT_LANE_END_DIST || glm::length(vec2(pts.back()) - titanSaveAreas[1 - team]) > BOT_LANE_END_DIST) continue;
			const vec2 mid = vec2(pts[pts.size() / 2]);
			bool same = false;
			foreach_const(lane, botLanes[team]) {
				if(glm::length((*lane)[lane->size() / 2] - mid) < BOT_LANE_SAME_DIST) same = true;
			}
			if(same) continue;
			botLanes[team].push_back();
			foreach_const(pt, pts) botLanes[team].back().push_back(vec2(*pt));
		}
	}
	LOG("Bot :: nav %d nodes, lanes %d/%d", (i32)botNav.size(), (i32)botLanes[0].size(), (i32)botLanes[1].size());
}

void World::BotPlanRoute(Player& bot, const vec2& goal)
{
	bot.botRoute.clear();
	bot.botRouteGoal = goal;
	const i32 count = (i32)botNav.size();
	if(count == 0) return;
	const vec3 pos = bot.body->GetWorldPos();
	const vec3 goal3 = vec3(goal, pos.z);
	eastl::vector<f32> dist(count, 1e30f);
	eastl::vector<i32> prev(count, -1);
	eastl::vector<bool> done(count, false);
	i32 nearest = 0;
	f32 nearestDist = 1e30f;
	bool seen = false;
	for(i32 n = 0; n < count; n++) {
		const f32 d = glm::length(vec2(botNav[n].pos) - vec2(pos));
		if(d < nearestDist) { nearestDist = d; nearest = n; }
		if(d <= BOT_NAV_LINK_RANGE && BotSees(pos, botNav[n].pos)) { dist[n] = d; seen = true; }
	}
	if(!seen) dist[nearest] = nearestDist;
	for(;;) {
		i32 cur = -1;
		for(i32 n = 0; n < count; n++) {
			if(!done[n] && dist[n] < 1e29f && (cur < 0 || dist[n] < dist[cur])) cur = n;
		}
		if(cur < 0) break;
		done[cur] = true;
		foreach_const(l, botNav[cur].links) {
			const f32 d = dist[cur] + glm::length(vec2(botNav[*l].pos) - vec2(botNav[cur].pos));
			if(d < dist[*l]) { dist[*l] = d; prev[*l] = cur; }
		}
	}
	i32 last = -1;
	f32 lastCost = 1e30f;
	i32 fallback = -1;
	f32 fallbackDist = 1e30f;
	for(i32 n = 0; n < count; n++) {
		if(dist[n] >= 1e29f) continue;
		const f32 d = glm::length(vec2(botNav[n].pos) - goal);
		if(d < fallbackDist) { fallbackDist = d; fallback = n; }
		if(d > BOT_NAV_LINK_RANGE || dist[n] + d >= lastCost || !BotSees(botNav[n].pos, goal3)) continue;
		lastCost = dist[n] + d;
		last = n;
	}
	if(last < 0) last = fallback;
	eastl::fixed_vector<u16,256,true> reversed;
	for(i32 n = last; n >= 0; n = prev[n]) reversed.push_back((u16)n);
	for(i32 k = (i32)reversed.size() - 1; k >= 0 && !bot.botRoute.full(); k--) bot.botRoute.push_back(reversed[k]);
}

vec2 World::BotNavStep(Player& bot, const vec2& goal)
{
	if(!botNavBuilt) BuildBotNav();
	const vec3 pos = bot.body->GetWorldPos();
	const vec2 here = vec2(pos);
	if(botNav.empty() || BotSees(pos, vec3(goal, pos.z))) {
		bot.botRoute.clear();
		return goal;
	}
	if(bot.botRoute.empty() || glm::length(goal - bot.botRouteGoal) > BOT_NAV_REROUTE_DIST) BotPlanRoute(bot, goal);
	while(!bot.botRoute.empty() && (glm::length(vec2(botNav[bot.botRoute.front()].pos) - here) < BOT_NAV_REACHED || InsideBodyBlocker(vec2(botNav[bot.botRoute.front()].pos), PLAYER_BODY_RADIUS))) bot.botRoute.erase(bot.botRoute.begin());
	if(bot.botRoute.size() > 1 && BotSees(pos, botNav[bot.botRoute[1]].pos)) bot.botRoute.erase(bot.botRoute.begin());
	if(bot.botRoute.empty()) return goal;
	return vec2(botNav[bot.botRoute.front()].pos);
}

bool World::BotRetreat(Player& bot, vec2* goal)
{
	if(bot.devGod) {
		bot.botRetreat = false;
		return false;
	}
	BuildTitanSaveAreas();
	if(!titanSaveAreaKnown[bot.team]) return false;
	const f32 ratio = HpRatio(bot.Main().hp, bot.Main().hpMax);
	if(ratio < HYP_BOT_RETREAT_HP) bot.botRetreat = true;
	else if(ratio >= HYP_BOT_RETREAT_DONE_HP) bot.botRetreat = false;
	if(!bot.botRetreat || bot.titanForm >= 0) return false;
	*goal = titanHealAreaKnown[bot.team] ? titanHealAreas[bot.team] : vec2(bot.spawnPos);
	return true;
}

bool World::BotLaneGoal(Player& bot, vec2* goal)
{
	if(!botNavBuilt) BuildBotNav();
	const auto& lanes = botLanes[bot.team];
	if(lanes.empty()) return false;
	if(bot.botLane < 0) {
		i32 rank = 0;
		foreach_const(p, players) {
			if(p->team == bot.team && p->index < bot.index) rank++;
		}
		bot.botLane = (i8)(rank % (i32)lanes.size());
	}
	const auto& lane = lanes[bot.botLane % (i32)lanes.size()];
	const vec2 here = vec2(bot.body->GetWorldPos());
	i32 nearest = 0;
	f32 nearestDist = 1e30f;
	for(i32 k = 0; k < (i32)lane.size(); k++) {
		const f32 d = glm::length(lane[k] - here);
		if(d < nearestDist) { nearestDist = d; nearest = k; }
	}
	i32 next = MIN(nearest + BOT_LANE_LOOKAHEAD, (i32)lane.size() - 1);
	while(next < (i32)lane.size() - 1 && InsideBodyBlocker(lane[next], PLAYER_BODY_RADIUS + BOT_NAV_REACHED)) next++;
	*goal = lane[next];
	return true;
}

bool World::BotSiegeOk(const Player& bot, const ActorNpc& tower) const
{
	if(!map || !map->ingameLevels || !IsStructure(tower)) return false;
	if(bot.titanForm >= 0) return true;
	const GameXmlContent::MonsterAttack* attack = tower.monsterAi->byTarget[GameXmlContent::MonsterAttack::MINION];
	if(!attack) attack = tower.monsterAi->byTarget[GameXmlContent::MonsterAttack::MASTER];
	if(!attack || attack->range <= 0) return false;
	const auto& giants = GetGameXmlContent().titan.giants;
	i32 tanks = 0;
	foreach_const(npc, actorNpcList) {
		if(!npc->monsterAi || npc->ownerUID != ActorUID::INVALID || npc->faction != (Faction)bot.team || npc->hp <= 0 || npc->removeAt != Time::ZERO || IsStructure(*npc)) continue;
		if(glm::length(vec2(npc->pos) - vec2(tower.pos)) > attack->range) continue;
		tanks += eastl::find(giants.begin(), giants.end(), (i32)npc->docID) != giants.end() ? HYP_BOT_SIEGE_GIANT_WEIGHT : 1;
		if(tanks >= HYP_BOT_SIEGE_TANKS) return true;
	}
	return false;
}
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
		if(BotSiegeOk(bot, *npc)) continue;
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
		if(bot.interactSlot >= 0 || localTime < bot.botSkillStillUntil) continue;
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
		vec2 occupyPoint;
		ActorUID occupyObject = ActorUID::INVALID;
		f32 occupyReach = 0;
		const bool occupyGoal = OccupyTarget(bot, &occupyPoint, &occupyObject, &occupyReach);
		if(occupyGoal && glm::length(occupyPoint - here) <= occupyReach && (!target || best > HYP_BOT_OCCUPY_FIGHT_RANGE)) {
			bot.input.moveTo = pos;
			StartMapInteraction(bot, occupyObject);
			continue;
		}
		const bool titanNav = map && map->ingameLevels;
		vec2 objective;
		const bool objectiveGoal = titanNav && (BotRetreat(bot, &objective) || BotObjective(bot, &objective));
		const f32 engage = occupyGoal || objectiveGoal ? HYP_BOT_OCCUPY_FIGHT_RANGE : (titanNav ? HYP_BOT_TITAN_ENGAGE_RANGE : BOT_ENGAGE_RANGE);
		bool fighting = false;
		bool farm = false;
		u8 mode = BOT_MODE_ROAM;
		if(target && best <= engage) {
			fighting = true;
			mode = BOT_MODE_FIGHT;
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
		else if(occupyGoal) {
			const vec2 d = occupyPoint - here;
			const f32 len = glm::length(d);
			goal = len > BOT_ROAM_STEP ? here + d / len * BOT_ROAM_STEP : occupyPoint;
			mode = BOT_MODE_OCCUPY;
		}
		else if(objectiveGoal) {
			goal = objective;
			mode = BOT_MODE_OBJECTIVE;
		}
		else if(titanNav && !(farm = BotFarmTarget(bot, &goal, HYP_BOT_LANE_FARM_RANGE)) && BotLaneGoal(bot, &goal)) mode = BOT_MODE_LANE;
		else if(!titanNav && BotObjective(bot, &goal)) {
			const vec2 d = goal - here;
			const f32 len = glm::length(d);
			if(len > BOT_ROAM_STEP) goal = here + d / len * BOT_ROAM_STEP;
		}
		else if(titanNav ? farm : BotFarmTarget(bot, &goal, BOT_FARM_RANGE)) {
			const f32 range = WeaponIsMelee(bot) ? BOT_MELEE_RANGE : MIN(WeaponRange(bot) * BOT_RANGE_FRACTION, BOT_RANGED_MAX);
			const vec2 d = here - goal;
			const f32 len = glm::length(d);
			const f32 face = atan2f(goal.y - here.y, goal.x - here.x);
			bot.input.rot.upperYaw = face;
			bot.input.rot.bodyYaw = face;
			goal = goal + (len > 1.0f ? d / len : vec2(1, 0)) * range;
			mode = BOT_MODE_FARM;
		}
		else if(target) {
			const vec2 toward = glm::normalize(vec2(target->body->GetWorldPos()) - here);
			goal = here + RotateVec(toward, bot.botStrafe * 0.3f) * BOT_ROAM_STEP;
		}
		else {
			goal = here + vec2(cosf(bot.input.rot.bodyYaw), sinf(bot.input.rot.bodyYaw)) * BOT_ROAM_STEP;
		}
		if(titanNav && mode != bot.botMode) {
			LOG("BotMode :: %ls %s at (%.0f, %.0f) hp %d/%d pieces %d", bot.name.data(), BOT_MODE_NAMES[mode], here.x, here.y, bot.Main().hp, bot.Main().hpMax, bot.titanPieces);
			bot.botMode = mode;
		}
		if(titanNav && !fighting) {
			goal = BotNavStep(bot, goal);
			const vec2 d = goal - here;
			const f32 len = glm::length(d);
			if(len > BOT_ROAM_STEP) goal = here + d / len * BOT_ROAM_STEP;
		}
		if(stuck) {
			bot.botRoute.clear();
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
		BotGateBlocks(bot, here, &goal);
		BotKeepOutOfBlockers(bot, &goal);
		bot.input.moveTo = vec3(goal, pos.z);
		bot.input.speed = PlayerMoveSpeed(bot);
	}
}

void World::PilotTourEvent(const char* what)
{
	FILE* f = fopen("pilot_tour.jsonl", "ab");
	if(!f) return;
	const i64 wall = (i64)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	u32 local = 0;
	foreach_const(p, players) if(p->clientHd != ClientHandle::INVALID && !p->removed && pilotTour.npc != ActorUID::INVALID) local = (u32)replication->GetLocalActorID(p->clientHd, pilotTour.npc);
	fprintf(f, "{\"w\":%lld,\"kind\":%d,\"id\":%d,\"local\":%u,\"warped\":%d,\"what\":\"%s\"}\n", wall, (i32)pilotTour.kind, pilotTour.kind == 1 ? pilotTour.localID : pilotTour.doc, local, pilotTour.warped ? 1 : 0, what);
	fclose(f);
}

void World::PilotTourFinish(const char* result)
{
	if(pilotTour.kind) PilotTourEvent(result);
	if(pilotTour.kind == 1) pilotTourInteractions.push_back(pilotTour.localID);
	if(pilotTour.kind == 2) pilotTourNpcs.push_back(pilotTour.npc);
	if(pilotTour.kind) LOG("PilotTour :: %s %d %s after %.1f s%s", pilotTour.kind == 1 ? "interaction" : "monster", pilotTour.kind == 1 ? pilotTour.localID : pilotTour.doc, result, TimeDiffSec(TimeDiff(pilotTour.since, localTime)), pilotTour.warped ? " (warped)" : "");
	pilotTour = PilotTour();
}

bool World::PilotTourStep(Player& p, PilotDecision* out, vec2* goal)
{
	static const f64 HYP_TOUR_STOP_TIMEOUT_SEC = 150.0;
	static const f32 HYP_TOUR_INTERACT_REACH = 500.0f;
	const vec2 here = vec2(p.body->GetWorldPos());
	if(pilotTour.kind && TimeDiffSec(TimeDiff(pilotTour.since, localTime)) > HYP_TOUR_STOP_TIMEOUT_SEC) PilotTourFinish(pilotTour.kind == 2 && pilotTour.reacted ? "FAILED (reacted but not killed)" : (pilotTour.kind == 2 && pilotTour.hit ? "FAILED (hit but never targeted the player)" : "FAILED (not reached or not completed)"));
	if(pilotTour.kind == 1) {
		bool still = false;
		foreach_const(m, mapInteractions) if(m->localID == pilotTour.localID && m->active) still = true;
		if(!still) PilotTourFinish("skipped (inactive)");
	}
	if(pilotTour.kind == 2) {
		ActorNpc* npc = FindNpcActor(pilotTour.npc);
		if(!npc || npc->hp <= 0) PilotTourFinish(!pilotTour.hit ? "skipped (gone)" : (pilotTour.reacted ? "done (reacted, killed)" : "FAILED (killed but never targeted the player)"));
		else if(pilotTour.hit && !pilotTour.reacted && npc->monsterTarget == p.Main().UID) {
			pilotTour.reacted = true;
			PilotTourEvent("reacted");
		}
	}
	if(!pilotTour.kind) {
		f32 best = 1e12f;
		foreach_const(m, mapInteractions) {
			const GameXmlContent::InteractionInfo* info = GetGameXmlContent().FindInteraction(m->interactionID);
			if(!m->active || m->creature || !info || info->pvpOccupy || info->action == GameXmlContent::InteractionInfo::OCCUPY) continue;
			if(eastl::find(pilotTourInteractions.begin(), pilotTourInteractions.end(), m->localID) != pilotTourInteractions.end()) continue;
			vec3 at;
			if(!MapInteractionPos(*m, &at)) continue;
			const f32 d = glm::length(vec2(at) - here);
			if(d < best) { best = d; pilotTour = PilotTour(); pilotTour.kind = 1; pilotTour.localID = m->localID; }
		}
		foreach_const(npc, actorNpcList) {
			if(!npc->monsterAi || npc->hp <= 0 || npc->ownerUID != ActorUID::INVALID || IsStructure(*npc) || !(npc->faction == Faction::ORANGE || (npc->faction == Faction::MONSTER && !hostileMonsters)) || !NpcHittableBy(*npc, p)) continue;
			if(eastl::find(pilotTourNpcs.begin(), pilotTourNpcs.end(), npc->UID) != pilotTourNpcs.end()) continue;
			const f32 d = glm::length(vec2(npc->pos) - here);
			if(d < best) { best = d; pilotTour = PilotTour(); pilotTour.kind = 2; pilotTour.npc = npc->UID; pilotTour.doc = (i32)npc->docID; }
		}
		if(!pilotTour.kind) return false;
		pilotTour.since = localTime;
		LOG("PilotTour :: next %s %d at %.0f", pilotTour.kind == 1 ? "interaction" : "monster", pilotTour.kind == 1 ? pilotTour.localID : pilotTour.doc, best);
	}
	static const f64 HYP_TOUR_WARP_AFTER_SEC = 20.0;
	static const f32 HYP_TOUR_WARP_DIST = 700.0f;
	static const f32 HYP_TOUR_WARP_STANDOFF = 350.0f;
	static const f32 HYP_TOUR_WARP_LIFT = 30.0f;
	if(!pilotTour.warped && TimeDiffSec(TimeDiff(pilotTour.since, localTime)) > HYP_TOUR_WARP_AFTER_SEC) {
		vec3 at = vec3(0);
		bool found = false;
		if(pilotTour.kind == 1) foreach_const(m, mapInteractions) if(m->localID == pilotTour.localID && MapInteractionPos(*m, &at)) found = true;
		if(pilotTour.kind == 2) if(ActorNpc* npc = FindNpcActor(pilotTour.npc)) { at = npc->pos; found = true; }
		const vec2 rel = here - vec2(at);
		const f32 len = glm::length(rel);
		if(found && len > HYP_TOUR_WARP_DIST) {
			const vec3 dest = vec3(vec2(at) + rel / len * HYP_TOUR_WARP_STANDOFF, at.z + HYP_TOUR_WARP_LIFT);
			pilotTour.warped = true;
			WarpPlayer(p, dest, atan2f(-rel.y, -rel.x));
			LOG("PilotTour :: stuck %.0f from the stop after %.0f s, warped next to it", len, HYP_TOUR_WARP_AFTER_SEC);
			PilotTourEvent("warped");
		}
	}
	if(pilotTour.kind == 1) {
		foreach_const(m, mapInteractions) {
			vec3 at;
			if(m->localID != pilotTour.localID || !MapInteractionPos(*m, &at)) continue;
			*goal = vec2(at);
			if(glm::length(vec2(at) - here) < HYP_TOUR_INTERACT_REACH) {
				out->hasInteract = true;
				out->interact = at;
			}
		}
		return true;
	}
	ActorNpc* npc = FindNpcActor(pilotTour.npc);
	*goal = vec2(npc->pos);
	out->hasTarget = true;
	out->target = npc->pos;
	out->hp = npc->hp;
	out->hpMax = npc->hpMax;
	out->doc = (i32)npc->docID;
	return true;
}

bool World::Pilot(Player& p, PilotDecision* out)
{
	static const f32 HYP_PILOT_ENGAGE_RANGE = 1300.0f;
	if(!map || !map->ingameLevels || !p.body || p.dead || p.Main().classType == ClassType::NONE) return false;
	const vec3 pos = p.body->GetWorldPos();
	const vec2 here = vec2(pos);
	vec2 goal = here;
	const bool touring = Config().DevPilotTour && PilotTourStep(p, out, &goal);
	const bool retreat = !touring && BotRetreat(p, &goal);
	f32 best = retreat || touring ? 0.0f : HYP_PILOT_ENGAGE_RANGE;
	foreach_const(e, players) {
		if(e->dead || e->untargetable || e->team == p.team || !e->body || e->Main().classType == ClassType::NONE || !(e->fogSeenBy & (1u << p.team))) continue;
		if(BotDangerAt(p, vec2(e->body->GetWorldPos()), nullptr, nullptr)) continue;
		const f32 d = glm::length(vec2(e->body->GetWorldPos()) - here);
		if(d >= best) continue;
		best = d;
		out->hasTarget = true;
		out->master = true;
		out->target = e->body->GetWorldPos();
		out->hp = e->Main().hp;
		out->hpMax = e->Main().hpMax;
		out->doc = 100000000 + (i32)e->Main().classType;
	}
	if(!out->hasTarget) {
		foreach_const(npc, actorNpcList) {
			if(!npc->monsterAi || !NpcHittableBy(*npc, p)) continue;
			if(IsStructure(*npc) && !BotSiegeOk(p, *npc)) continue;
			if(!IsStructure(*npc) && BotDangerAt(p, vec2(npc->pos), nullptr, nullptr)) continue;
			const f32 d = glm::length(vec2(npc->pos) - here);
			if(d >= best) continue;
			best = d;
			out->hasTarget = true;
			out->target = npc->pos;
			out->hp = npc->hp;
			out->hpMax = npc->hpMax;
			out->doc = (i32)npc->docID;
		}
	}
	vec2 occupyPoint;
	ActorUID occupyObject = ActorUID::INVALID;
	f32 occupyReach = 0;
	if(touring) {}
	else if(!retreat && !out->hasTarget && OccupyTarget(p, &occupyPoint, &occupyObject, &occupyReach)) {
		out->hasInteract = true;
		out->interact = vec3(occupyPoint, pos.z);
		goal = occupyPoint;
	}
	else if(!retreat && !BotObjective(p, &goal) && !BotLaneGoal(p, &goal)) goal = here;
	out->finalGoal = vec3(goal, pos.z);
	goal = BotNavStep(p, goal);
	vec2 tower;
	f32 towerRange = 0;
	if(BotDangerAt(p, goal, &tower, &towerRange)) {
		const vec2 away = here - tower;
		const f32 len = glm::length(away);
		goal = tower + (len > 1.0f ? away / len : vec2(1, 0)) * towerRange;
		out->finalGoal = vec3(goal, pos.z);
	}
	if(BotDangerAt(p, vec2(out->finalGoal), nullptr, nullptr)) out->finalGoal = vec3(goal, pos.z);
	out->goal = vec3(goal, pos.z);
	return true;
}

void World::BuildTeamGates()
{
	static const f32 GATE_PROBE_HEIGHT = 100.0f;
	static const f32 GATE_PROBE_REACH = 1500.0f;
	if(teamGatesBuilt || !map || !map->ingameLevels) return;
	teamGatesBuilt = true;
	const auto& content = GetGameXmlContent();
	i32 built = 0;
	foreach(d, actorDynamicList) {
		if(!content.IsTeamGate((i32)d->docID) || (d->faction != Faction::RED && d->faction != Faction::BLUE)) continue;
		const vec3 at = d->pos + vec3(0, 0, GATE_PROBE_HEIGHT);
		f32 bestWidth = 1e9f;
		vec2 bestAxis = vec2(0);
		vec2 bestCenter = vec2(d->pos);
		for(i32 k = 0; k < 2; k++) {
			const f32 a = d->rot.z + (k ? PI * 0.5f : 0.0f);
			const vec2 axis = vec2(cosf(a), sinf(a));
			f32 left = 0, right = 0;
			if(!physics.RaycastStatic(at, at + vec3(axis * GATE_PROBE_REACH, 0), &left)) continue;
			if(!physics.RaycastStatic(at, at - vec3(axis * GATE_PROBE_REACH, 0), &right)) continue;
			if(left + right >= bestWidth) continue;
			bestWidth = left + right;
			bestAxis = axis;
			bestCenter = vec2(d->pos) + axis * (left - right) * 0.5f;
		}
		if(bestWidth > GATE_PROBE_REACH * 2.0f) {
			LOG("Gate :: dynamic %d (doc %d) opening not found", d->localID, (i32)d->docID);
			continue;
		}
		if(!teamGates.full()) teamGates.push_back({ d->localID, bestCenter - bestAxis * bestWidth * 0.5f, bestCenter + bestAxis * bestWidth * 0.5f, (i32)d->faction, d->action == ActionStateID::DYNAMIC_OPEN || d->action == ActionStateID::DYNAMIC_DIE });
		LOG("Gate :: dynamic %d (doc %d, team %d) wall %.0f wide at (%.0f, %.0f)", d->localID, (i32)d->docID, (i32)d->faction, bestWidth, bestCenter.x, bestCenter.y);
		built++;
	}
	LOG("Gate :: %d team gate walls", built);
}

bool World::InsideBodyBlocker(const vec2& at, f32 margin) const
{
	foreach_const(it, actorNpcList) {
		if(it->bodyBlocker && it->monsterAi && glm::length(at - vec2(it->pos)) < it->monsterAi->bodyBlockRadius + margin) return true;
	}
	return false;
}

void World::BotKeepOutOfBlockers(const Player& bot, vec2* goal) const
{
	static const f32 HYP_BOT_BLOCKER_MARGIN = 20.0f;
	foreach_const(it, actorNpcList) {
		if(!it->bodyBlocker || !it->monsterAi) continue;
		const vec2 center = vec2(it->pos);
		const f32 keep = it->monsterAi->bodyBlockRadius + bot.body->GetBoundSize().x + HYP_BOT_BLOCKER_MARGIN;
		const vec2 d = *goal - center;
		const f32 len = glm::length(d);
		if(len >= keep) continue;
		const vec2 out = len > 1.0f ? d / len : glm::normalize(vec2(bot.body->GetWorldPos()) - center + vec2(0.001f, 0));
		*goal = center + out * keep;
	}
}

bool World::BotGateBlocks(const Player& bot, const vec2& from, vec2* goal) const
{
	static const f32 GATE_STOP_DIST = 150.0f;
	foreach_const(g, teamGates) {
		if(g->open || g->team == (i32)bot.team) continue;
		const vec2 d = *goal - from;
		const vec2 s = g->b - g->a;
		const f32 denom = Vec2Cross(d, s);
		if(fabsf(denom) < 1e-3f) continue;
		const f32 t = Vec2Cross(g->a - from, s) / denom;
		const f32 u = Vec2Cross(g->a - from, d) / denom;
		if(t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) continue;
		const vec2 hit = from + d * t;
		const f32 len = glm::length(d);
		*goal = len > 1.0f ? hit - d / len * GATE_STOP_DIST : from;
		return true;
	}
	return false;
}

void World::UpdateObserver()
{
	static const f64 OBSERVER_MOVE_MS = 4000.0;
	static const f64 OBSERVER_FIGHT_RECENT_SEC = 3.0;
	static const f32 OBSERVER_DISTANCE = 450.0f;
	if(!Config().DevObserver || gameStartedAt == Time::ZERO || localTime < observerNextMove) return;
	observerNextMove = TimeAdd(localTime, TimeMsToTime(OBSERVER_MOVE_MS));
	Player* human = nullptr;
	foreach(p, players) if(p->clientHd != ClientHandle::INVALID && !p->removed && p->body) { human = &*p; break; }
	if(!human || human->dead) return;
	human->untargetable = true;
	human->devGod = true;
	const Player* best = nullptr;
	f64 bestAge = 1e9;
	foreach_const(p, players) {
		if(p->clientHd != ClientHandle::INVALID || p->dead || !p->body || p->Main().classType == ClassType::NONE) continue;
		const f64 age = p->battleActionAt == Time::ZERO ? 1e6 : TimeDiffSec(TimeDiff(p->battleActionAt, localTime));
		const f64 score = age + (p->index == (u32)observerTarget && age < OBSERVER_FIGHT_RECENT_SEC ? -OBSERVER_FIGHT_RECENT_SEC : 0.0);
		if(score < bestAge) { bestAge = score; best = &*p; }
	}
	if(!best) return;
	observerTarget = (i32)best->index;
	const vec3 at = best->body->GetWorldPos();
	const f32 yaw = best->input.rot.bodyYaw;
	const vec3 spot = at - vec3(cosf(yaw), sinf(yaw), 0) * OBSERVER_DISTANCE;
	MovePlayer(*human, vec3(spot.x, spot.y, at.z), yaw, true);
	LOG("Observer :: watches %ls at (%.0f, %.0f) last action %.1fs ago", best->name.data(), at.x, at.y, bestAge);
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
			bool swing = false;
			foreach_const(e, players) {
				if(e->dead || e->untargetable || e->team == bot.team || !(e->fogSeenBy & (1u << bot.team))) continue;
				if(glm::length(vec2(e->body->GetWorldPos()) - vec2(bot.body->GetWorldPos())) <= BOT_MELEE_RANGE + HitRadius(*e) + HitRadius(bot)) { swing = true; break; }
			}
			if(!swing && !Config().DevShowcase) {
				foreach_const(npc, actorNpcList) {
					if(!NpcHittableBy(*npc, bot)) continue;
					if(glm::length(vec2(npc->pos) - vec2(bot.body->GetWorldPos())) <= BOT_MELEE_RANGE + NpcHitRadius(*npc) + HitRadius(bot)) { swing = true; break; }
				}
			}
			if(swing) {
				bot.weaponFired = true;
				bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(BotNextFireDelay(bot) * 1000.0));
			}
			continue;
		}

		const f32 range = WeaponRange(bot);

		const vec3 pos = bot.body->GetWorldPos();
		Player* target = nullptr;
		f32 best = range;
		foreach(e, players) {
			if(e->dead || e->untargetable || e->team == bot.team || !(e->fogSeenBy & (1u << bot.team))) continue;
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

		const WeaponFiring& chargeWf = ActiveWeaponFiring(bot, bot.Main().stance);
		if(!chargeWf.charge.empty() && !chargeWf.chargeTime.empty()) {
			if(bot.weaponState != WEAPON_STATE_CHARGE) {
				bot.botChargeGoal = BotRollChargeLevel(bot, chargeWf);
				OnWeaponState(bot, WEAPON_STATE_CHARGE, 0, bot.firingCombo);
				replication->RelayWeaponState(ClientHandle::INVALID, bot.Main().UID, MEASURED_WEAPON_STATE_WEAPON_ID, WEAPON_STATE_CHARGE, 0, bot.firingCombo);
			}
			if(bot.chargeAnnounced < bot.botChargeGoal) { bot.tNextBotShot = localTime; continue; }
			OnWeaponState(bot, WEAPON_STATE_FIRE, bot.chargeAnnounced, bot.firingCombo);
		}
		if(!WeaponHeatShot(bot, localTime, replication, WeaponHeatScale(bot))) continue;
		const vec2 rel = vec2(aim) - vec2(pos);
		const vec2 dir = glm::length(rel) > 1.0f ? glm::normalize(rel) : vec2(1, 0);
		bot.weaponAimYaw = atan2f(dir.y, dir.x);
		PlayerWeaponShot(bot, pos, dir, best);
		bot.tNextBotShot = TimeAdd(localTime, TimeMsToTime(BotNextFireDelay(bot) * 1000.0));
	}
}

struct BotChargeShare { i32 classType; f32 share[4]; };
static const BotChargeShare MEASURED_BOT_CHARGE_SHARES[] = {
	{ 6, { 0.38f, 0.09f, 0.21f, 0.32f } },
	{ 20, { 0.76f, 0.24f, 0.0f, 0.0f } },
	{ 36, { 0.94f, 0.06f, 0.0f, 0.0f } },
	{ 9, { 0.99f, 0.01f, 0.0f, 0.0f } },
};

u8 World::BotRollChargeLevel(const Player& bot, const WeaponFiring& wf) const
{
	const i32 levels = (i32)MIN(wf.charge.size(), wf.chargeTime.size());
	for(const BotChargeShare& row : MEASURED_BOT_CHARGE_SHARES) {
		if(row.classType != (i32)bot.Main().classType) continue;
		f32 r = (f32)Randf01();
		for(i32 lv = 0; lv < 4; lv++) {
			r -= row.share[lv];
			if(r < 0) return (u8)MIN(lv, levels);
		}
	}
	return 0;
}

f32 World::BotNextFireDelay(const Player& bot) const
{
	const WeaponFiring& wf = ActiveWeaponFiring(bot, bot.Main().stance);
	f32 delay = wf.fireDelay;
	if(bot.firingCombo < wf.comboDelay.size() && wf.comboDelay[bot.firingCombo] > 0 && wf.comboDelay[bot.firingCombo] <= wf.comboValid[bot.firingCombo]) delay = wf.comboDelay[bot.firingCombo];
	return MAX(delay, BOT_MIN_FIRE_DELAY) * WeaponFireDelayScale(bot);
}
