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

static const ClassType SERVER_CREATED_REMOTE_MASTERS[] = {
	ClassType::SHUTTLE,
	ClassType::GATLING,
};

static inline bool ServerCreatesRemotes(ClassType master)
{
	for(ClassType c : SERVER_CREATED_REMOTE_MASTERS) {
		if(c == master) return true;
	}
	return false;
}
