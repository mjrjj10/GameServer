#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const i32 WEAPON_HIT_DAMAGE = 120;
static const f32 WEAPON_PROJECTILE_SPEED = 4000.0f;
static const f32 WEAPON_MELEE_MAX_RANGE = 600.0f;
static const f32 WEAPON_MELEE_HALF_ANGLE = 1.05f;
static const f32 PROJECTILE_TARGET_RADIUS = 70.0f;
static const f32 TOOLTIP_SHUTTLE_CHARGED_FUEL = 20.0f;
static const f32 WEAPON_SHOT_RELIABLE_DISTANCE = 100.0f;
static const f32 WEAPON_SHOT_MAX_DISTANCE = 2500.0f;

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

	ShowcaseOnCast(player);
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
	ShowcaseOnCast(player);
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
			replication->RelayWeaponShot(player.clientHd, player.pendingShotRaw.data(), player.pendingShotRawSize, aimPos, remote->ID);
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
		replication->RelayWeaponShot(player.clientHd, player.pendingShotRaw.data(), player.pendingShotRawSize, target, remote ? remote->ID : RemoteIdx::INVALID);
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

bool World::WeaponIsMelee(const Player& p) const
{
	const auto& content = GetGameXmlContent();
	const WeaponIndex weapon = (p.mainCharaID & 1) == 0 ? p.mainWeapon : p.subWeapon;
	const WeaponFiring* wf = content.FindWeaponFiring((i32)weapon, p.Main().stance);
	const Remote* wr = wf ? content.FindRemote(wf->base) : nullptr;
	if(wr && wr->maxSpeed > 0) return false;
	return ClassIsMelee(p.Main().classType);
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
