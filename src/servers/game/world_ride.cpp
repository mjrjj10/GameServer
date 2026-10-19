#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const i32 STATUS_SHUTTLE_TELEPORT_RESIST = 1230410053;
static const f32 TELEPORT_SPREAD = 80.0f;
static const i32 INTERACTION_SHUTTLE_BOARD = 900000071;
static const i32 INTERACTION_NOT_OCCUPIED = -1;
static const u32 PLAYER_FACTION_BASE = 3;
static const f32 RIDE_SYNC_SEC = 0.1f;

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
	const ActiveStatus* passive = FindPassiveOfType(gx, StatusType::GX09);
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
	if(const ActiveStatus* passive = FindPassiveOfType(gx, StatusType::GX09)) {
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
		const bool shuttle = gx.Main().classType == ClassType::SHUTTLE;
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
