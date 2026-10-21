#pragma once
#include <common/base.h>
#include <common/protocol.h>
#include <mxm/core.h>

enum class ResourceEvent: u8
{
	CHARGED_SHOT,
	CHARGING_PER_SEC,
	REMOTE_HIT_ENEMY,
};

struct ResourceGainRule
{
	ClassType master;
	ResourceEvent event;
	i32 slot;
	f32 amount;
	RemoteIdx remote;
};

static const ResourceGainRule RESOURCE_GAIN_RULES[] = {
	{ ClassType::SHUTTLE, ResourceEvent::CHARGED_SHOT, 1, 20.0f, RemoteIdx::INVALID },
	{ ClassType::SHUTTLE, ResourceEvent::CHARGING_PER_SEC, 1, 5.0f, RemoteIdx::INVALID },
	{ ClassType::SHUTTLE, ResourceEvent::REMOTE_HIT_ENEMY, 1, 22.0f, RemoteIdx(1440410000) },
};

static inline const ResourceGainRule* FindResourceGainRule(ClassType master, ResourceEvent event, RemoteIdx remote = RemoteIdx::INVALID)
{
	for(const ResourceGainRule& rule : RESOURCE_GAIN_RULES) {
		if(rule.master == master && rule.event == event && (rule.remote == RemoteIdx::INVALID || rule.remote == remote)) return &rule;
	}
	return nullptr;
}

struct SummonAttackRule
{
	const char* summonClass;
	f32 attackRange;
	f64 cooldownMs;
	f64 firstDelayMs;
};

static const SummonAttackRule SUMMON_ATTACK_RULES[] = {
	{ "PC_MissileTurret", 1000.0f, 420.0, 400.0 },
	{ "SOULMASTER_GIANTSOUL", 400.0f, 2000.0, 1500.0 },
	{ "SLIME_MINI", 1000.0f, 2200.0, 140.0 },
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
	f64 cooldownMs;
};

static const HitRecoverRule HIT_RECOVER_RULES[] = {
	{ 1230070005, 0.08f, 1000.0 },
};

static inline const HitRecoverRule* FindHitRecoverRule(i32 status)
{
	for(const HitRecoverRule& rule : HIT_RECOVER_RULES) {
		if(rule.status == status) return &rule;
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
	{ ClassType::CATTHECAT, PassiveTrigger::SKILL_USE, PassiveTarget::SELF, 1230260000, 0, 0 },
	{ ClassType::ANDROA, PassiveTrigger::SKILL_USE, PassiveTarget::SELF, 1230290001, 0, 0 },
	{ ClassType::ANDROB, PassiveTrigger::SKILL_USE, PassiveTarget::LOWEST_HP_ALLY_IN_STATUS_RANGE, 1230300001, 0, 250 },
	{ ClassType::BOXER, PassiveTrigger::SKILL_HIT, PassiveTarget::HIT_TARGET, 1230320002, 0, 0 },
	{ ClassType::PHOTOG, PassiveTrigger::SKILL_HIT, PassiveTarget::SELF, 1230150010, 0, 0 },
	{ ClassType::EFREET, PassiveTrigger::SKILL_HIT, PassiveTarget::HIT_TARGET, 1230230001, 0, 0 },
	{ ClassType::SHADOWHUNTER, PassiveTrigger::SKILL_HIT, PassiveTarget::SELF, 1230240000, 1230240001, 0 },
};

enum class WeaponBonus: u8
{
	OWNER_BELOW_HP_TRIGGER,
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

static const WeaponBonusRule WEAPON_BONUS_RULES[] = {
	{ ClassType::ATTACKER, WeaponBonus::OWNER_BELOW_HP_TRIGGER, 0, 0, 1.10f },
	{ ClassType::BOXER, WeaponBonus::TARGET_HAS_OWN_STATUS, 0, 1230320002, 1.15f },
	{ ClassType::SHUGOTRADER, WeaponBonus::EVERY_NTH_HIT, 6, 1230330000, 1.80f },
	{ ClassType::RYTLOCK, WeaponBonus::LAST_OF_CHAIN, 3, 1230250002, 1.20f },
};
