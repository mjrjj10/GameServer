#pragma once
#include <common/base.h>
#include <common/vector_math.h>
#include <mxm/game_content.h>

static const i32 STAT_STAMINA = 2;
static const i32 STAT_MOVESPEED = 14;
static const i32 STAT_MANA = 35;
struct ResourceDef { i32 maxStat; i32 regenStat; bool startFull; };
static const ResourceDef RESOURCES[3] = {
	{ 17, 41, true },
	{ 36, 40, false },
	{ 56, 57, true },
};
static const i32 STAT_ULTIMATE = 37;
static const f32 WEAPON_HIT_TOLERANCE = 50.0f;
static const f32 OBSTRUCT_JUMPABLE_HEIGHT = 120.0f;
static const f32 OBSTRUCT_SIDE = 50.0f;
static const i32 CLASS_NUM_SHUTTLE = 41;

static inline f32 MasterStat(ClassType classType, i32 statType)
{
	const auto& master = GetGameXmlContent().GetMaster(classType);
	foreach_const(st, master.baseStats) {
		if(st->type == statType) return st->value;
	}
	return 0;
}

static inline vec2 Perp(const vec2& v)
{
	return vec2(-v.y, v.x);
}
