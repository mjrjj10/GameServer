#pragma once
#include <common/base.h>
#include <common/protocol.h>
#include <mxm/core.h>

enum class ResourceEvent: u8
{
	CHARGED_SHOT,
	CHARGING_PER_SEC,
	REMOTE_HIT_ENEMY,
	SELF_STATUS,
};

struct ResourceGainRule
{
	ClassType master;
	ResourceEvent event;
	i32 slot;
	f32 amount;
	RemoteIdx remote;
	i32 status = 0;
};

static const ResourceGainRule RESOURCE_GAIN_RULES[] = {
	{ ClassType::SHUTTLE, ResourceEvent::CHARGED_SHOT, 1, 20.0f, RemoteIdx::INVALID },
	{ ClassType::SHUTTLE, ResourceEvent::CHARGING_PER_SEC, 1, 5.0f, RemoteIdx::INVALID },
	{ ClassType::SHUTTLE, ResourceEvent::REMOTE_HIT_ENEMY, 1, 22.0f, RemoteIdx(1440410000) },
	{ ClassType::JINSEOYEON, ResourceEvent::SELF_STATUS, 2, 1.0f, RemoteIdx::INVALID, 1230120011 },
};

static inline const ResourceGainRule* FindResourceGainRule(ClassType master, ResourceEvent event, RemoteIdx remote = RemoteIdx::INVALID, i32 status = 0)
{
	for(const ResourceGainRule& rule : RESOURCE_GAIN_RULES) {
		if(rule.master == master && rule.event == event && (rule.remote == RemoteIdx::INVALID || rule.remote == remote) && rule.status == status) return &rule;
	}
	return nullptr;
}

struct ResourceStartEmpty
{
	ClassType master;
	i32 slot;
};

static const ResourceStartEmpty MEASURED_RESOURCE_START_EMPTY[] = {
	{ ClassType::JINSEOYEON, 2 },
};

static inline bool ResourceStartsFull(ClassType master, i32 slot, bool startFull)
{
	for(const ResourceStartEmpty& rule : MEASURED_RESOURCE_START_EMPTY) {
		if(rule.master == master && rule.slot == slot) return false;
	}
	return startFull;
}

static const ClassType TESTER_RAGE_FROM_WEAPON_ONLY[] = {
	ClassType::EFREET,
};

static inline bool RageFromWeaponOnly(ClassType master)
{
	for(ClassType c : TESTER_RAGE_FROM_WEAPON_ONLY) if(c == master) return true;
	return false;
}

struct MeasuredStatusStat
{
	i32 status;
	StatusMod::Stat stat;
	f32 value;
};

static const MeasuredStatusStat MEASURED_STATUS_STATS[] = {
	{ 1230110012, StatusMod::REMOTESPEED, 100.0f },
};

struct CarryLiftRule
{
	i32 status;
	f32 lift;
};

static const CarryLiftRule MEASURED_CARRY_LIFTS[] = {
	{ 1230400053, 450.0f },
};

struct RepeatHitDecayRule
{
	RemoteIdx remote;
	f32 stepPerHit;
	f32 floor;
	f32 resetSec = 0;
	bool perCaster = false;
};

static const RepeatHitDecayRule MEASURED_REPEAT_HIT_DECAY[] = {
	{ RemoteIdx(1440010042), 0.1f, 0.2f },
	{ RemoteIdx(1440010037), 0.1f, 0.2f },
	{ RemoteIdx(1440010047), 0.1f, 0.2f },
	{ RemoteIdx(1440110052), 0.1f, 0.2f },
	{ RemoteIdx(1440110053), 0.1f, 0.2f },
	{ RemoteIdx(1440110054), 0.1f, 0.2f },
	{ RemoteIdx(1430200012), 0.2f, 0.4f, 1.0f, true },
	{ RemoteIdx(1430200015), 0.2f, 0.4f, 1.0f, true },
	{ RemoteIdx(1440040042), 0.8f, 0.2f, 1.0f },
	{ RemoteIdx(1440040044), 0.8f, 0.2f, 1.0f },
};

static inline const RepeatHitDecayRule* FindRepeatHitDecay(RemoteIdx remote)
{
	for(const RepeatHitDecayRule& r : MEASURED_REPEAT_HIT_DECAY) if(r.remote == remote) return &r;
	return nullptr;
}

static const f32 MEASURED_RADIATE_REPEAT_HIT_RATIO = 0.5f;

static const RemoteIdx MEASURED_REFLECTORS[] = {
	RemoteIdx(1440050050),
};

static inline bool IsMeasuredReflector(RemoteIdx remote)
{
	for(RemoteIdx r : MEASURED_REFLECTORS) if(r == remote) return true;
	return false;
}

struct FireNodeRule
{
	RemoteIdx remote;
	f32 forward;
	f32 side;
	f32 up;
};

static const FireNodeRule FIRE_NODE_RULES[] = {
	{ RemoteIdx(1440010030), 56.0f, -19.0f, 133.0f },
	{ RemoteIdx(1440010050), 126.0f, -34.0f, 165.0f },
	{ RemoteIdx(1440010060), 107.0f, 5.0f, 153.0f },
	{ RemoteIdx(1440010061), 107.0f, 5.0f, 153.0f },
	{ RemoteIdx(1440010062), 107.0f, 5.0f, 153.0f },
	{ RemoteIdx(1440010063), 107.0f, 5.0f, 153.0f },
	{ RemoteIdx(1440010064), 107.0f, 5.0f, 153.0f },
	{ RemoteIdx(1440020010), 37.0f, -12.0f, 289.0f },
	{ RemoteIdx(1440040020), 73.0f, 2.0f, -2.0f },
	{ RemoteIdx(1440040021), 0.0f, 0.0f, 98.0f },
	{ RemoteIdx(1440040023), 0.0f, 0.0f, 99.0f },
	{ RemoteIdx(1440050050), 201.0f, -1.0f, 212.0f },
	{ RemoteIdx(1440060020), 181.0f, -9.0f, 152.0f },
	{ RemoteIdx(1440060040), 215.0f, -1.0f, 127.0f },
	{ RemoteIdx(1440060050), 211.0f, -7.0f, 152.0f },
	{ RemoteIdx(1440060051), 177.0f, 0.0f, 152.0f },
	{ RemoteIdx(1440060052), 211.0f, -7.0f, 152.0f },
	{ RemoteIdx(1440060053), 177.0f, 0.0f, 152.0f },
	{ RemoteIdx(1440080020), 70.0f, 0.0f, 147.0f },
	{ RemoteIdx(1440090000), 0.0f, 0.0f, 248.0f },
	{ RemoteIdx(1440090050), 45.0f, -87.0f, 252.0f },
	{ RemoteIdx(1440090052), 45.0f, 81.0f, 252.0f },
	{ RemoteIdx(1440110010), 105.0f, -16.0f, 173.0f },
	{ RemoteIdx(1440110020), 43.0f, -4.0f, 155.0f },
	{ RemoteIdx(1440110030), 115.0f, -18.0f, 194.0f },
	{ RemoteIdx(1440110031), 111.0f, 16.0f, 202.0f },
	{ RemoteIdx(1440110040), 120.0f, 0.0f, 156.0f },
	{ RemoteIdx(1440120010), 95.0f, 0.0f, 152.0f },
	{ RemoteIdx(1440120022), 194.0f, 2.0f, 166.0f },
	{ RemoteIdx(1440120023), 21.0f, 61.0f, 203.0f },
	{ RemoteIdx(1440130010), 43.0f, -23.0f, 209.0f },
	{ RemoteIdx(1440140010), 199.0f, -1.0f, 137.0f },
	{ RemoteIdx(1440140050), 88.0f, -14.0f, 191.0f },
	{ RemoteIdx(1440150020), 30.0f, 4.0f, 155.0f },
	{ RemoteIdx(1440160020), 121.0f, -1.0f, 211.0f },
	{ RemoteIdx(1440170010), 45.0f, 10.0f, 171.0f },
	{ RemoteIdx(1440170030), 85.0f, 2.0f, 165.0f },
	{ RemoteIdx(1440180010), 105.0f, -9.0f, 128.0f },
	{ RemoteIdx(1440180042), 116.0f, -1.0f, 49.0f },
	{ RemoteIdx(1440190010), 0.0f, -57.0f, 170.0f },
	{ RemoteIdx(1440190040), 90.0f, 1.0f, 182.0f },
	{ RemoteIdx(1440190050), 73.0f, -75.0f, 243.0f },
	{ RemoteIdx(1440220030), 108.0f, 0.0f, 98.0f },
	{ RemoteIdx(1440220040), 100.0f, 0.0f, 157.0f },
	{ RemoteIdx(1440220041), 78.0f, -4.0f, 94.0f },
	{ RemoteIdx(1440240011), 73.0f, 7.0f, 188.0f },
	{ RemoteIdx(1440240012), 70.0f, 7.0f, 190.0f },
	{ RemoteIdx(1440250020), 103.0f, 4.0f, 192.0f },
	{ RemoteIdx(1440250040), 149.0f, 0.0f, 201.0f },
	{ RemoteIdx(1440250041), 166.0f, 0.0f, 201.0f },
	{ RemoteIdx(1440260010), 76.0f, 4.0f, 88.0f },
	{ RemoteIdx(1440260030), 76.0f, 4.0f, 88.0f },
	{ RemoteIdx(1440280080), 51.0f, 2.0f, 222.0f },
	{ RemoteIdx(1440290020), 0.0f, -6.0f, 104.0f },
	{ RemoteIdx(1440290052), -40.0f, -103.0f, 271.0f },
	{ RemoteIdx(1440310022), 71.0f, -1.0f, 168.0f },
	{ RemoteIdx(1440320010), 133.0f, -2.0f, 191.0f },
	{ RemoteIdx(1440320020), 138.0f, -5.0f, 172.0f },
	{ RemoteIdx(1440320021), 133.0f, 32.0f, 198.0f },
	{ RemoteIdx(1440320022), 137.0f, 17.0f, 287.0f },
	{ RemoteIdx(1440320030), 127.0f, -19.0f, 152.0f },
	{ RemoteIdx(1440320031), 138.0f, 16.0f, 174.0f },
	{ RemoteIdx(1440320032), 98.0f, 13.0f, 181.0f },
	{ RemoteIdx(1440330030), 39.0f, 1.0f, 168.0f },
	{ RemoteIdx(1440330032), 37.0f, 1.0f, 169.0f },
	{ RemoteIdx(1440330034), 42.0f, 2.0f, 168.0f },
	{ RemoteIdx(1440330040), 90.0f, 1.0f, 139.0f },
	{ RemoteIdx(1440330044), 90.0f, 2.0f, 139.0f },
	{ RemoteIdx(1440330048), 90.0f, 1.0f, 139.0f },
	{ RemoteIdx(1440330050), 50.0f, -1.0f, 132.0f },
	{ RemoteIdx(1440350010), 114.0f, 3.0f, 160.0f },
	{ RemoteIdx(1440350050), 80.0f, 13.0f, 127.0f },
	{ RemoteIdx(1440370010), 90.0f, -5.0f, 160.0f },
	{ RemoteIdx(1440370011), 90.0f, -5.0f, 160.0f },
	{ RemoteIdx(1440370012), 167.0f, 0.0f, 117.0f },
	{ RemoteIdx(1440370030), 36.0f, 7.0f, 128.0f },
	{ RemoteIdx(1440370050), 95.0f, 8.0f, 131.0f },
};

static const FireNodeRule MEASURED_HIT_TARGET_FIRE_NODES[] = {
	{ RemoteIdx(1440180040), 70.0f, 0.0f, 109.0f },
};

static inline const FireNodeRule* FindHitTargetFireNode(RemoteIdx remote)
{
	for(const FireNodeRule& rule : MEASURED_HIT_TARGET_FIRE_NODES) if(rule.remote == remote) return &rule;
	return nullptr;
}

static inline const FireNodeRule* FindFireNodeRule(RemoteIdx remote)
{
	for(const FireNodeRule& rule : FIRE_NODE_RULES) {
		if(rule.remote == remote) return &rule;
	}
	return nullptr;
}

struct SummonFireRule
{
	RemoteIdx remote;
	f32 towardOwner;
	f32 up;
};

static const SummonFireRule SUMMON_FIRE_RULES[] = {
	{ RemoteIdx(1440220010), 643.0f, 635.0f },
};

static inline const SummonFireRule* FindSummonFireRule(RemoteIdx remote)
{
	for(const SummonFireRule& rule : SUMMON_FIRE_RULES) {
		if(rule.remote == remote) return &rule;
	}
	return nullptr;
}

struct SummonAttackRule
{
	const char* summonClass;
	f32 attackRange;
	f64 cooldownMs;
	f64 firstDelayMs;
	bool followOwner;
};

static const SummonAttackRule SUMMON_ATTACK_RULES[] = {
	{ "PC_MissileTurret", 1000.0f, 420.0, 400.0, false },
	{ "SOULMASTER_GIANTSOUL", 400.0f, 2000.0, 1500.0, false },
	{ "SLIME_MINI", 1000.0f, 2200.0, 140.0, true },
};

static inline const SummonAttackRule* FindSummonAttackRule(const char* summonClass)
{
	for(const SummonAttackRule& rule : SUMMON_ATTACK_RULES) {
		if(EA::StdC::Strcmp(rule.summonClass, summonClass) == 0) return &rule;
	}
	return nullptr;
}

struct HitRecoverRule
{
	i32 status;
	f32 hpRatio;
	f32 npcHpRatio;
	f64 cooldownMs;
	f32 stamina;
	f32 npcStamina;
};

static const HitRecoverRule HIT_RECOVER_RULES[] = {
	{ 1230070005, 0.08f, 0.04f, 1000.0, 0.0f, 0.0f },
	{ 1230010010, 0.0f, 0.0f, 0.0, 5.0f, 2.0f },
};

static inline const HitRecoverRule* FindHitRecoverRule(i32 status)
{
	for(const HitRecoverRule& rule : HIT_RECOVER_RULES) {
		if(rule.status == status) return &rule;
	}
	return nullptr;
}

struct MeasuredRoom { CreatureIndex creature; f32 radius; };
static const MeasuredRoom MEASURED_BOSS_ROOMS[] = { { CreatureIndex(105001200), 1429.0f } };

static inline const MeasuredRoom* FindMeasuredRoom(CreatureIndex doc)
{
	for(const MeasuredRoom& r : MEASURED_BOSS_ROOMS) if(r.creature == doc) return &r;
	return nullptr;
}

struct MeasuredLeap { i32 skill; f32 durationSec; f32 endRise; };
static const MeasuredLeap MEASURED_LEAPS[] = { { 189000020, 1.5f, 350.0f } };

static inline const MeasuredLeap* FindMeasuredLeap(SkillID skill)
{
	for(const MeasuredLeap& l : MEASURED_LEAPS) if(l.skill == (i32)skill) return &l;
	return nullptr;
}

struct FogExcludeBox { i32 map; f32 minX, minY, minZ, maxX, maxY, maxZ; };
static const FogExcludeBox MEASURED_FOG_EXCLUDE_BOXES[] = {
	{ 160000050, 45692.0f, 5328.0f, -167.0f, 48578.0f, 8336.0f, 3408.0f },
};

struct TurretCannonPair { CreatureIndex tower; CreatureIndex cannon; ActionStateID allowed; };
static const TurretCannonPair MEASURED_TURRET_CANNONS[] = { { CreatureIndex(105000308), CreatureIndex(105000309), ActionStateID::SKILL_4_BEHAVIORSTATE } };
static const f32 MEASURED_TURRET_CANNON_OFFSET = 5.0f;

static const CreatureIndex MEASURED_SPECIAL_FIRST_CREATURES[] = { CreatureIndex(105001200) };

static inline bool IsMeasuredSpecialFirst(CreatureIndex doc)
{
	for(CreatureIndex c : MEASURED_SPECIAL_FIRST_CREATURES) if(c == doc) return true;
	return false;
}

static const i32 MEASURED_RAGE_ON_STATUS[] = { 1230330003 };
static const i32 MEASURED_RAGE_BUNDLE_STATUS_FIRST = 1230330010;
static const i32 MEASURED_RAGE_BUNDLE_STATUS_LAST = 1230330049;
static const f64 MEASURED_RAGE_BUNDLE_ONCE_MS = 100.0;

static inline bool IsMeasuredRageOnStatus(i32 status)
{
	for(i32 s : MEASURED_RAGE_ON_STATUS) if(s == status) return true;
	return false;
}

static inline bool IsMeasuredRageBundleStatus(i32 status)
{
	return status >= MEASURED_RAGE_BUNDLE_STATUS_FIRST && status <= MEASURED_RAGE_BUNDLE_STATUS_LAST;
}

static const i32 MEASURED_BOT_SKILL_FIRST_UPGRADES = 6;

static const SkillID TESTER_BURST_BACK_AFTER_THROUGH[] = { SkillID(180430020) };

struct SummonDieStandRule
{
	i32 creature;
	f32 sec;
};

static const SummonDieStandRule MEASURED_SUMMON_DIE_STANDS[] = {
	{ 100033841, 2.49f },
	{ 100033842, 2.49f },
	{ 100033400, 2.49f },
	{ 100033401, 2.49f },
	{ 100033402, 2.49f },
};

static inline f32 SummonDieStandSec(i32 creature, f32 fallback)
{
	for(const SummonDieStandRule& r : MEASURED_SUMMON_DIE_STANDS) if(r.creature == creature) return r.sec;
	return fallback;
}

struct TesterAllyHealRule
{
	i32 status;
	f32 hpRatio;
};

static const TesterAllyHealRule TESTER_ALLY_HEALS[] = {
	{ 1200070917, 0.33f },
};

static inline f32 TesterAllyHealRatio(i32 status)
{
	for(const TesterAllyHealRule& r : TESTER_ALLY_HEALS) if(r.status == status) return r.hpRatio;
	return 0.0f;
}

struct TesterRemoteFuse
{
	RemoteIdx remote;
	f32 fuseSec;
	i32 damage;
	i32 stunStatus;
	f32 stunSec;
	RemoteIdx visual;
};

static const TesterRemoteFuse TESTER_REMOTE_FUSES[] = {
	{ RemoteIdx(1440430060), 0.5f, 100, 1230010012, 0.5f, RemoteIdx(1440020021) },
};

static inline const TesterRemoteFuse* FindTesterRemoteFuse(RemoteIdx remote)
{
	for(const TesterRemoteFuse& r : TESTER_REMOTE_FUSES) if(r.remote == remote) return &r;
	return nullptr;
}

static const RemoteIdx TESTER_TAG_OUT_STAYS_REMOTES[] = { RemoteIdx(1440430041) };

static inline bool StaysWhereTargetTaggedOut(RemoteIdx remote)
{
	for(RemoteIdx r : TESTER_TAG_OUT_STAYS_REMOTES) if(r == remote) return true;
	return false;
}

static inline bool BurstsBackAfterThrough(SkillID skill)
{
	for(SkillID s : TESTER_BURST_BACK_AFTER_THROUGH) if(s == skill) return true;
	return false;
}

struct SkillMoveLiftRule
{
	SkillID skill;
	f32 lift;
};

static const SkillMoveLiftRule MEASURED_SKILL_MOVE_LIFTS[] = {
	{ SkillID(180080020), 350.0f },
	{ SkillID(180120030), 350.0f },
	{ SkillID(180140030), 322.0f },
	{ SkillID(180180020), 330.0f },
	{ SkillID(180220030), 297.0f },
	{ SkillID(180280030), 294.0f },
	{ SkillID(180350020), 300.0f },
	{ SkillID(189000020), 350.0f },
};

static inline f32 SkillMoveLift(SkillID skill)
{
	for(const SkillMoveLiftRule& r : MEASURED_SKILL_MOVE_LIFTS) if(r.skill == skill) return r.lift;
	return 0.0f;
}

struct PvpRewardRule
{
	i32 minTeamSize;
	u32 winGoldPerMin;
	u32 winExpPerMin;
	u32 loseGoldPerMin;
	u32 loseExpPerMin;
};

static const PvpRewardRule MEASURED_PVP_REWARDS[] = {
	{ 5, 15, 100, 10, 70 },
	{ 0, 12, 80, 8, 56 },
};

static inline const PvpRewardRule& FindPvpRewardRule(i32 teamSize)
{
	for(const PvpRewardRule& rule : MEASURED_PVP_REWARDS) {
		if(teamSize >= rule.minTeamSize) return rule;
	}
	ASSERT(0);
	return MEASURED_PVP_REWARDS[0];
}

struct ChargeHoldRule
{
	SkillID skill;
	f32 holdSec;
};

static const ChargeHoldRule MEASURED_CHARGE_HOLDS[] = {
	{ SkillID(180140030), 0.67f },
	{ SkillID(180320050), 0.15f },
	{ SkillID(180060050), 1.44f },
	{ SkillID(180070030), 0.16f },
	{ SkillID(180150040), 1.75f },
	{ SkillID(180280050), 0.16f },
	{ SkillID(180370040), 0.71f },
};

static inline const ChargeHoldRule* FindChargeHoldRule(SkillID skill)
{
	for(const ChargeHoldRule& rule : MEASURED_CHARGE_HOLDS) {
		if(rule.skill == skill) return &rule;
	}
	return nullptr;
}

struct SummonGroupLimitRule
{
	ClassType master;
	i32 creatureMin;
	i32 creatureMax;
	i32 limit;
};

static const i32 MEASURED_ONE_HIT_SUMMONS[] = { 105000453 };

static inline bool OneHitSummon(i32 creature)
{
	for(i32 c : MEASURED_ONE_HIT_SUMMONS) if(c == creature) return true;
	return false;
}

static const SummonGroupLimitRule MEASURED_SUMMON_GROUP_LIMITS[] = {
	{ ClassType::MECHANIC, 100033800, 100033839, 3 },
};

struct SummonChaseSpeedRule
{
	i32 creature;
	f32 farSpeed;
	f32 nearSpeed;
	f32 nearDist;
	f32 spawnSec;
};

static const SummonChaseSpeedRule MEASURED_SUMMON_CHASE_SPEEDS[] = {
	{ 100033841, 600.0f, 750.0f, 450.0f, 0.58f },
	{ 100033842, 600.0f, 750.0f, 450.0f, 0.58f },
};

static inline const SummonChaseSpeedRule* FindSummonChaseSpeedRule(i32 creature)
{
	for(const SummonChaseSpeedRule& rule : MEASURED_SUMMON_CHASE_SPEEDS) {
		if(rule.creature == creature) return &rule;
	}
	return nullptr;
}

struct StatusDrainRule
{
	i32 status;
	f32 fraction;
};

static const StatusDrainRule STATUS_DRAIN_RULES[] = {
	{ 1230410020, 0.30f },
};

static inline const StatusDrainRule* FindStatusDrainRule(i32 status)
{
	for(const StatusDrainRule& rule : STATUS_DRAIN_RULES) {
		if(rule.status == status) return &rule;
	}
	return nullptr;
}

struct StatusStaminaRule
{
	i32 status;
	f32 amount;
};

static const StatusStaminaRule MEASURED_STATUS_STAMINA_RULES[] = {
	{ 1230090002, 15.0f },
	{ 1230090003, 30.0f },
};

static inline const StatusStaminaRule* FindStatusStaminaRule(i32 status)
{
	for(const StatusStaminaRule& rule : MEASURED_STATUS_STAMINA_RULES) {
		if(rule.status == status) return &rule;
	}
	return nullptr;
}

struct PushToEdgeRule
{
	i32 status;
	RemoteIdx remote;
};

static const PushToEdgeRule PUSH_TO_EDGE_RULES[] = {
	{ 1230410030, RemoteIdx(1440410030) },
};

static inline const PushToEdgeRule* FindPushToEdgeRule(i32 status)
{
	for(const PushToEdgeRule& rule : PUSH_TO_EDGE_RULES) {
		if(rule.status == status) return &rule;
	}
	return nullptr;
}

static const RemoteIdx CHAINED_ZONE_REMOTES[] = {
	RemoteIdx(1440410010),
};

static inline bool IsChainedZoneRemote(RemoteIdx remote)
{
	for(RemoteIdx r : CHAINED_ZONE_REMOTES) {
		if(r == remote) return true;
	}
	return false;
}

enum class PassiveTrigger: u8
{
	SKILL_USE,
	SKILL_HIT,
};

enum class PassiveTarget: u8
{
	SELF,
	HIT_TARGET,
	LOWEST_HP_ALLY_IN_STATUS_RANGE,
};

struct PassiveStatusRule
{
	ClassType master;
	PassiveTrigger trigger;
	PassiveTarget target;
	i32 status;
	i32 cooldownStatus;
	i32 heal;
};

static const PassiveStatusRule PASSIVE_STATUS_RULES[] = {
	{ ClassType::ANDROB, PassiveTrigger::SKILL_USE, PassiveTarget::LOWEST_HP_ALLY_IN_STATUS_RANGE, 1230300001, 0, 250 },
	{ ClassType::PHOTOG, PassiveTrigger::SKILL_HIT, PassiveTarget::SELF, 1230150010, 0, 0 },
	{ ClassType::SHADOWHUNTER, PassiveTrigger::SKILL_HIT, PassiveTarget::SELF, 1230240000, 1230240001, 0 },
};

enum class WeaponBonus: u8
{
	TARGET_HAS_OWN_STATUS,
	EVERY_NTH_HIT,
	LAST_OF_CHAIN,
};

struct WeaponBonusRule
{
	ClassType master;
	WeaponBonus condition;
	i32 count;
	i32 status;
	f32 mul;
};

struct LowHpFireRateRule
{
	ClassType master;
	f32 fireDelayScale;
};

struct LowHpStatRule
{
	i32 status;
	StatusMod::Stat stat;
	f32 value;
};

static const LowHpStatRule TOOLTIP_LOW_HP_STATS[] = {
	{ 1230310000, StatusMod::DEFENCE, 25.0f },
	{ 1230310000, StatusMod::ATTACK, 10.0f },
};

static const LowHpFireRateRule MEASURED_LOW_HP_FIRE_RATES[] = {
	{ ClassType::ATTACKER, 0.843f },
};

static const WeaponBonusRule WEAPON_BONUS_RULES[] = {
	{ ClassType::SHUGOTRADER, WeaponBonus::EVERY_NTH_HIT, 6, 1230330000, 1.80f },
	{ ClassType::RYTLOCK, WeaponBonus::LAST_OF_CHAIN, 3, 1230250002, 1.0f },
};
