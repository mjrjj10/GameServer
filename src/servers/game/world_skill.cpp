#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const i32 STAT_STAMINAREGEN = 5;
static const i32 STAT_MANAREGEN = 39;
static const i32 STAT_ULTIMATEREGEN = 42;
static const i32 STAT_TAG_HEALTHREGEN_VALUE = 23;
static const i32 STAT_TAG_STAMINAREGEN_VALUE = 25;
static const i32 STAT_TAG_HEALTHREGEN_RATIO = 27;
static const i32 STAT_TAG_STAMINAREGEN_RATIO = 29;
static const i32 STAT_TAG_MANAREGEN_VALUE = 44;
static const i32 STAT_TAG_MANAREGEN_RATIO = 47;
static const u32 STAT_REASON_REGEN = 4;
static const f64 REGEN_INTERVAL_MS = 1000.0;
static const f64 RAGE_DECAY_DELAY_MS = 6000.0;
static const f32 HIDE_DETECT_RADIUS_UNIT = 100.0f;
static const f64 SKILL_POINT_INTERVAL_MS = 35000.0;
static const f64 CHASE_UPDATE_INTERVAL_MS = 200.0;
static const f64 BATTLE_STAGE_SEC = 1.0;
static const f32 BATTLE_SPEED_RATIO_ACTIVE = 535.0f / 620.0f;
static const f32 BATTLE_SPEED_RATIO_RECENT = 578.0f / 620.0f;
static const i32 SKILL_UPGRADE_SLOT_ULTIMATE = 12;
static const f32 STANCE_EXIT_DURATION = -1.0f;
static const f32 MOVEMENT_YAW_MIN_DIST = 10.0f;
static const i32 ULTIMATE_SKILL_ID_SUFFIX = 50;

static f32 MovementYaw(const World::Player& player, f32 fallback)
{
	const vec2 moving = vec2(player.input.moveTo) - vec2(player.body->GetWorldPos());
	return glm::length(moving) > MOVEMENT_YAW_MIN_DIST ? atan2f(moving.y, moving.x) : fallback;
}

void World::UpdateUltimateGauges(f32 delta)
{
	foreach(it, players) {
		if(it->clientHd == ClientHandle::INVALID && (Config().DevShowcase || Config().DevMirror)) continue;
		const ClassType cls = it->Main().classType;
		const ClassType benchCls = it->Sub().classType;

		UpdateWeaponHeat(*it);

		if(localTime >= it->tNextSpeedRefresh) {
			it->tNextSpeedRefresh = TimeAdd(localTime, TimeMsToTime(1000));
			it->moveSpeedSent = -1.0f;
		}
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
		const f32 staminaMax = MasterStat(cls, STAT_STAMINA);
		const f32 manaMax = MasterStat(cls, STAT_MANA);
		eastl::array<f32,3> resMax;
		for(int r = 0; r < 3; r++) resMax[r] = MasterStat(cls, RESOURCES[r].maxStat);
		if(it->ultGauge < 0) { it->ultGauge = 0; it->ultDirty = true; }
		if(staminaMax > 0 && it->stamina < 0) { it->stamina = staminaMax; it->staminaDirty = true; }
		if(manaMax > 0 && it->mana < 0) { it->mana = manaMax; it->manaDirty = true; }
		for(int r = 0; r < 3; r++) {
			if(resMax[r] > 0 && it->res[r] < 0) { it->res[r] = RESOURCES[r].startFull ? resMax[r] : 0; it->resDirty = true; }
		}

		const ActorMaster& sub = it->Sub();
		const f32 benchStaminaMax = MasterStat(benchCls, STAT_STAMINA);
		const f32 benchManaMax = MasterStat(benchCls, STAT_MANA);
		eastl::array<f32,3> benchResMax;
		for(int r = 0; r < 3; r++) benchResMax[r] = MasterStat(benchCls, RESOURCES[r].maxStat);
		if(benchStaminaMax > 0 && it->benchStamina < 0) {
			it->benchStamina = benchStaminaMax;
			replication->SendStatToAll(sub.UID, STAT_STAMINA, it->benchStamina, benchStaminaMax);
		}
		if(benchManaMax > 0 && it->benchMana < 0) {
			it->benchMana = benchManaMax;
			replication->SendStatToAll(sub.UID, STAT_MANA, it->benchMana, benchManaMax);
		}
		for(int r = 0; r < 3; r++) {
			if(benchResMax[r] <= 0 || it->benchRes[r] >= 0) continue;
			it->benchRes[r] = RESOURCES[r].startFull ? benchResMax[r] : 0;
			replication->SendStatToAll(sub.UID, RESOURCES[r].maxStat, it->benchRes[r], benchResMax[r]);
		}

		const ResourceGainRule* charging = it->weaponState == WEAPON_STATE_CHARGE ? FindResourceGainRule(cls, ResourceEvent::CHARGING_PER_SEC) : nullptr;
		if(charging && charging->slot >= 0 && charging->slot < 3 && resMax[charging->slot] > 0) {
			f32& pool = it->res[charging->slot];
			pool = MIN(MAX(pool + charging->amount * delta, 0.0f), resMax[charging->slot]);
			if(localTime >= it->tNextChargeSend) {
				it->tNextChargeSend = TimeAdd(localTime, TimeMsToTime(250));
				it->resDirty = true;
			}
		}

		const f32 staminaRegen = MasterStat(cls, STAT_STAMINAREGEN) * MAX(1.0f + StatusRegenBonus(*it, StatusMod::STAMINA, true) / 100.0f, 0.0f);
		const f32 manaRegen = MasterStat(cls, STAT_MANAREGEN) + StatusRegenBonus(*it, StatusMod::MANAREGEN, false);
		const f32 rageRegen = MasterStat(cls, RESOURCES[1].regenStat) + StatusRegenBonus(*it, StatusMod::RAGEREGEN, false);
		{
			const f32 rates[3] = { staminaRegen, manaRegen, rageRegen };
			const i32 statIDs[3] = { STAT_STAMINAREGEN, STAT_MANAREGEN, RESOURCES[1].regenStat };
			const bool used[3] = { staminaMax > 0, manaMax > 0, resMax[1] > 0 };
			for(int k = 0; k < 3; k++) {
				if(!used[k]) continue;
				if(it->regenSent[k] < 0 && !it->regenResend) it->regenSent[k] = rates[k];
				if(!it->regenResend && fabsf(rates[k] - it->regenSent[k]) < 0.001f) continue;
				it->regenSent[k] = rates[k];
				replication->SendStatToAll(it->Main().UID, statIDs[k], 0.0f, rates[k]);
			}
			it->regenResend = false;
		}

		if(localTime >= it->tNextRegen) {
			it->tNextRegen = TimeAdd(localTime, TimeMsToTime(REGEN_INTERVAL_MS));
			auto regen = [&](ActorUID uid, i32 stat, f32& value, f32 max, f32 amount) {
				if(value < 0 || max <= 0 || amount == 0) return;
				const f32 next = MIN(MAX(value + amount, 0.0f), max);
				if(next == value) return;
				value = next;
				replication->SendStatToAll(uid, stat, value, max, STAT_REASON_REGEN);
			};
			if(!it->dead) {
				const ActorUID mainUID = it->Main().UID;
				regen(mainUID, (i32)StatType::STAT_TYPE_ULTIMATE, it->ultGauge, ultMax, MasterStat(cls, STAT_ULTIMATEREGEN));
				regen(mainUID, STAT_STAMINA, it->stamina, staminaMax, staminaRegen);
				regen(mainUID, STAT_MANA, it->mana, manaMax, manaRegen);
				for(int r = 0; r < 3; r++) {
					f32 amount = r == 1 ? rageRegen : MasterStat(cls, RESOURCES[r].regenStat);
					if(r == 1 && amount < 0 && localTime < TimeAdd(it->rageGainAt, TimeMsToTime(RAGE_DECAY_DELAY_MS))) amount = 0;
					regen(mainUID, RESOURCES[r].maxStat, it->res[r], resMax[r], amount);
				}

				ActorMaster& bench = it->Sub();
				if(bench.hp > 0 && bench.hp < bench.hpMax) {
					const i32 hpGain = (i32)(MasterStat(benchCls, STAT_TAG_HEALTHREGEN_VALUE) + MasterStat(benchCls, STAT_TAG_HEALTHREGEN_RATIO) * bench.hpMax / 100.0f);
					if(hpGain > 0) {
						bench.hp = MIN(bench.hp + hpGain, bench.hpMax);
						replication->SendStatToAll(bench.UID, (i32)StatType::STAT_TYPE_HEALTH, (f32)bench.hp, (f32)bench.hpMax, STAT_REASON_REGEN);
					}
				}
				regen(bench.UID, STAT_STAMINA, it->benchStamina, benchStaminaMax, MasterStat(benchCls, STAT_TAG_STAMINAREGEN_VALUE) + MasterStat(benchCls, STAT_TAG_STAMINAREGEN_RATIO) * benchStaminaMax / 100.0f);
				regen(bench.UID, STAT_MANA, it->benchMana, benchManaMax, MasterStat(benchCls, STAT_TAG_MANAREGEN_VALUE) + MasterStat(benchCls, STAT_TAG_MANAREGEN_RATIO) * benchManaMax / 100.0f);
				for(int r = 0; r < 3; r++) {
					regen(bench.UID, RESOURCES[r].maxStat, it->benchRes[r], benchResMax[r], MasterStat(benchCls, RESOURCES[r].tagRegenValueStat) + MasterStat(benchCls, RESOURCES[r].tagRegenRatioStat) * benchResMax[r] / 100.0f);
				}
			}
		}

		if(ShowcaseKeepsFullRes(*it)) {
			for(int r = 0; r < 3; r++) {
				if(resMax[r] > 0 && it->res[r] != resMax[r]) { it->res[r] = resMax[r]; it->resDirty = true; }
			}
		}

		const ActorUID mainUID = it->Main().UID;
		if(it->ultDirty) replication->SendStatToAll(mainUID, (i32)StatType::STAT_TYPE_ULTIMATE, it->ultGauge, ultMax);
		if(it->staminaDirty && staminaMax > 0) replication->SendStatToAll(mainUID, STAT_STAMINA, it->stamina, staminaMax);
		if(it->manaDirty && manaMax > 0) replication->SendStatToAll(mainUID, STAT_MANA, it->mana, manaMax);
		if(it->resDirty) {
			for(int r = 0; r < 3; r++) {
				if(resMax[r] > 0) replication->SendStatToAll(mainUID, RESOURCES[r].maxStat, it->res[r], resMax[r]);
			}
		}
		it->ultDirty = false;
		it->staminaDirty = false;
		it->manaDirty = false;
		it->resDirty = false;
	}
}

bool World::PlayerPaySkillCost(Player& player, i32 skillID)
{
	const auto& content = GetGameXmlContent();
	f32 mp = PlayerSkillConsumeMP(player, skillID);
	foreach(it, activeStatuses) {
		if(it->targetIndex != player.index || !it->info || it->info->type != StatusType::NOCOST || it->until <= localTime) continue;
		if(eastl::find(it->info->costSkills.begin(), it->info->costSkills.end(), skillID) == it->info->costSkills.end()) continue;
		mp = MAX(mp * (1.0f + it->info->costRatioMP / 100.0f), 0.0f);
		it->until = localTime;
		LOG("Passive :: %ls skill %d mana cost %.0f (status %d used)", player.name.data(), skillID, mp, it->statusID);
		break;
	}
	if(const ActiveStatus* stacking = FindPassiveOfType(player, StatusType::DOUBLEGUN)) {
		const auto& list = stacking->info->costSkills;
		if(player.skillStacks > 0 && localTime < player.skillStacksUntil && eastl::find(list.begin(), list.end(), skillID) != list.end()) mp = MAX(mp * (1.0f + stacking->info->costRatioMP * player.skillStacks / 100.0f), 0.0f);
	}
	const f32 ug = PlayerSkillConsume(player, skillID, 4);
	const f32 st = content.GetSkillConsumeStamina(skillID);
	const f32 resCost[3] = { PlayerSkillConsume(player, skillID, 0), PlayerSkillConsume(player, skillID, 1), PlayerSkillConsume(player, skillID, 2) };
	const f32 hpCost = PlayerSkillConsume(player, skillID, 3);
	if(mp <= 0 && ug <= 0 && st <= 0 && resCost[0] <= 0 && resCost[1] <= 0 && resCost[2] <= 0 && hpCost <= 0) return true;
	if(player.clientHd == ClientHandle::INVALID && (Config().DevShowcase || Config().DevMirror)) return true;

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

	if(mp > 0 && player.mana >= 0) { player.mana -= mp; player.manaDirty = true; }
	if(ug > 0 && player.ultGauge >= 0) { player.ultGauge -= ug; player.ultDirty = true; }
	if(st > 0 && player.stamina >= 0) { player.stamina -= st; player.staminaDirty = true; }
	for(int r = 0; r < 3; r++) {
		if(resCost[r] > 0 && player.res[r] >= 0) { player.res[r] -= resCost[r]; player.resDirty = true; }
	}
	if(hpCost > 0) {
		ActorMaster& chara = player.Main();
		chara.hp = MAX(chara.hp - (i32)hpCost, 1);
		replication->SendHpToAll(chara.UID, chara.hp, chara.hpMax);
	}
	return true;
}

void World::RunStanceOut(Player& player, i8 oldStance)
{
	const auto& content = GetGameXmlContent();
	const GameXmlContent::Action* action = content.FindSkillAction(player.Main().classType, ActionStateID::STANCE_OUT_BEHAVIORSTATE, oldStance);
	if(!action) return;
	foreach_const(cmd, action->commands) {
		if(cmd->type == ActionCommand::Type::STATUS && cmd->status.statusIndex != 0) {
			if(cmd->status.remove) {
				foreach(it, activeStatuses) {
					if(it->targetIndex == player.index && it->statusID == cmd->status.statusIndex && localTime < it->until) it->until = localTime;
				}
			}
			else if(!player.dead) ApplyStatus(player, player, cmd->status.statusIndex);
		}
		else if(cmd->type == ActionCommand::Type::REMOTE && cmd->remote.remove) {
			foreach(ri, remoteInsts) {
				if(ri->ownerIndex == player.index && ri->remote && ri->remote->ID == cmd->remote.idx && !ri->killNow) ri->killNow = true;
			}
		}
	}
	LOG("Stance :: %ls stance %d out", player.name.data(), (i32)oldStance);
}

void World::SetStance(ActorMaster& master, i8 stance, f32 duration, i32 weaponFireCount, bool cancelByDefeat)
{
	const i8 oldStance = master.stance;
	master.stance = stance;
	master.stanceUntil = stance != 0 && duration > 0 ? TimeAdd(localTime, TimeMsToTime((i64)(duration * 1000.0f))) : Time::ZERO;
	master.stanceShotsLeft = stance != 0 && weaponFireCount > 0 ? weaponFireCount : -1;
	master.stanceCancelByDefeat = stance != 0 && cancelByDefeat;
	if(stance == 0 && oldStance != 0) replication->SendStanceToAll(master.UID, 0, STANCE_EXIT_DURATION);
	LOG("Stance :: %ls stance=%d duration=%.1f weaponFireCount=%d", master.parent ? master.parent->name.data() : L"?", (i32)stance, duration, weaponFireCount);
	if(oldStance != 0 && stance == 0 && master.parent) RunStanceOut(*master.parent, oldStance);
}

void World::StanceWeaponUse(Player& player)
{
	ActorMaster& m = player.Main();
	if(m.stance == 0 || m.stanceShotsLeft <= 0) return;
	if(--m.stanceShotsLeft == 0) SetStance(m, 0, 0, 0, false);
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
		const GameXmlContent::SkillUpgrade* tu = LatestUpgradeWith(p, (i32)p.toggleSkill, [](const GameXmlContent::SkillUpgrade& u) { return u.toggleValue >= 0; });
		const f32 cost = tu ? tu->toggleValue : tg->value;
		if(*pool < cost) { ToggleOff(p); continue; }
		*pool -= cost;
		p.manaDirty = true;
		p.resDirty = true;
		p.staminaDirty = true;
	}
}

void World::ApplyNpcOwnerStatus(ActorUID npcUID, Player& carrier, i32 statusID)
{
	const StatusInfo* info = GetGameXmlContent().FindStatus(statusID);
	const f32 duration = info && info->durationSec > 0 ? info->durationSec : 0.0f;
	const vec3 pos = carrier.body->GetWorldPos();
	replication->SendAddStatusToAll(statusID, npcUID, carrier.Main().UID, duration, pos, pos, 0, 1, 1.0f, -1.0f, false, false);
	LOG("Status :: summon %u carries %ls status=%d duration=%.2f", (u32)npcUID, carrier.name.data(), statusID, duration);
	if(npcStatuses.full()) return;
	NpcStatus ns;
	ns.npcUID = npcUID;
	ns.statusID = statusID;
	ns.casterUID = carrier.Main().UID;
	ns.until = TimeAdd(localTime, TimeMsToTime(duration * 1000.0));
	npcStatuses.push_back(ns);
}

void World::TrackNpcSelfStatus(ActorUID npcUID, const StatusInfo& info)
{
	if(info.durationSec <= 0) return;
	const Time until = TimeAdd(localTime, TimeMsToTime(info.durationSec * 1000.0));
	foreach(ns, npcStatuses) {
		if(ns->npcUID != npcUID || ns->statusID != info.ID || ns->linked) continue;
		ns->until = until;
		return;
	}
	if(npcStatuses.full()) return;
	NpcStatus ns;
	ns.npcUID = npcUID;
	ns.statusID = info.ID;
	ns.casterUID = npcUID;
	ns.until = until;
	ns.linked = false;
	npcStatuses.push_back(ns);
}

void World::UpdateNpcStatuses()
{
	for(i32 i = 0; i < (i32)npcStatuses.size(); ) {
		const NpcStatus& ns = npcStatuses[i];
		bool linked = false;
		foreach_const(st, activeStatuses) {
			if(st->casterUID == ns.npcUID && st->targetUID == ns.casterUID && st->info && st->info->type == StatusType::TARGET && st->until > localTime) { linked = true; break; }
		}
		if((linked || !ns.linked) && localTime < ns.until && FindNpcActor(ns.npcUID)) { i++; continue; }
		replication->SendRemoveStatusToAll(ns.statusID, ns.npcUID, ns.casterUID);
		npcStatuses.erase(npcStatuses.begin() + i);
	}
}

bool World::IsCarrying(const Player& player) const
{
	foreach_const(st, activeStatuses) {
		if(st->casterIndex == player.index && st->info && st->info->type == StatusType::TARGET && st->until > localTime) return true;
	}
	return false;
}

void World::ReleaseCast(Player& player)
{
	const Player::Casting c = player.casting;
	player.casting.skillID = SkillID::INVALID;
	auto f = GetGameXmlContent().skillMap.find(c.skillID);
	if(f == GetGameXmlContent().skillMap.end() || player.dead) return;
	LOG("Cast :: %ls skill=%d release", player.name.data(), (i32)c.skillID);
	GroupTeleport(player, c.skillID, c.castPos);
	if(f->second.action == ActionStateID::INVALID) return;
	PlayerCastSkill(player, c.skillID, c.castPos, Slice<const ActorUID>(&c.target, c.target != ActorUID::INVALID ? 1 : 0), 0, f->second.action);
}

void World::DropPendingActions(Player& player)
{
	i32 dropped = 0;
	for(auto pr = pendingRemotes.begin(); pr != pendingRemotes.end(); ) {
		if(pr->ownerIndex == player.index && pr->sourceUID == ActorUID::INVALID && !pr->remove && localTime < pr->time) {
			pr = pendingRemotes.erase_unsorted(pr);
			dropped++;
		}
		else ++pr;
	}
	for(auto ps = pendingStatuses.begin(); ps != pendingStatuses.end(); ) {
		if(ps->casterIndex == player.index && ps->sourceUID == ActorUID::INVALID && !ps->remove && localTime < ps->time) {
			ps = pendingStatuses.erase_unsorted(ps);
			dropped++;
		}
		else ++ps;
	}
	if(dropped > 0) LOG("Skill :: %ls new action, %d pending commands dropped", player.name.data(), dropped);
}

void World::InterruptSkills(Player& player)
{
	i32 dropped = 0;
	for(auto pr = pendingRemotes.begin(); pr != pendingRemotes.end(); ) {
		if(pr->ownerIndex == player.index && pr->sourceUID == ActorUID::INVALID && !pr->remove && localTime < pr->time) {
			pr = pendingRemotes.erase_unsorted(pr);
			dropped++;
		}
		else ++pr;
	}
	if(dropped > 0) LOG("Skill :: %ls interrupted, %d pending remotes dropped", player.name.data(), dropped);
	if(player.casting.skillID != SkillID::INVALID) CancelCast(player, "interrupted");
}

void World::CancelCast(Player& player, const char* reason)
{
	LOG("Cast :: %ls skill=%d cancelled (%s)", player.name.data(), (i32)player.casting.skillID, reason);
	replication->SendCancelSkillToAll(player.Main().UID, player.casting.skillID);
	player.casting.skillID = SkillID::INVALID;
}

void World::UpdateCasts()
{
	foreach(it, players) {
		if(it->comboPending != SkillID::INVALID && it->comboSlot >= 0 && localTime >= it->comboSendAt) {
			replication->SendChangeSkillSlot(it->clientHd, it->Main().UID, it->comboSlot, it->comboPending, 0);
			it->comboPending = SkillID::INVALID;
		}
		if(it->comboAutoAt != Time::ZERO && localTime >= it->comboAutoAt) {
			it->comboAutoAt = Time::ZERO;
			if(it->comboSlot >= 0 && it->comboSkill != SkillID::INVALID && !it->dead) {
				const GameXmlContent::SkillInput* comboIn = GetGameXmlContent().FindSkillInput((i32)it->comboSkill);
				const bool petToCaster = it->clientHd == ClientHandle::INVALID && comboIn && comboIn->petCaster && comboIn->needsClick && IsCarrying(*it);
				const vec3 comboPos = petToCaster ? it->body->GetWorldPos() : it->comboCastPos;
				LOG("Combo :: %ls auto-cast %d at (%.0f, %.0f)", it->name.data(), (i32)it->comboSkill, comboPos.x, comboPos.y);
				PlayerCastSkill(*it, it->comboSkill, comboPos, Slice<const ActorUID>(), 0);
			}
		}
		if(it->comboSlot >= 0 && localTime >= it->comboUntil) {
			replication->SendChangeSkillSlot(it->clientHd, it->Main().UID, it->comboSlot, it->comboBase, 0);
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

bool World::ScheduleSkillRemotes(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 angle, i32 forceAnim, ActionStateID forcedAction)
{
	const auto& content = GetGameXmlContent();
	auto skillFound = content.skillMap.find(skillID);
	if(skillFound == content.skillMap.end()) return false;
	const auto& skill = skillFound->second;
	const ActionStateID actionID = forcedAction != ActionStateID::INVALID ? forcedAction : skill.action;
	if(!content.SkillActionKnown(player.Main().classType, actionID, player.Main().stance)) {
		LOG("Skill :: %ls skill=%d action %d not on active master %d, dropped", player.name.data(), (i32)skillID, (i32)actionID, (i32)player.Main().classType);
		return false;
	}
	const auto& action = content.GetSkillAction(player.Main().classType, actionID, player.Main().stance);

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
	const i32 damage = (i32)PlayerSkillDamage(player, skillID);
	const ActorUID currentTarget = targets.size() > 0 ? targets[0] : ActorUID::INVALID;
	const i32 upgradeSummon = UpgradeSummon(player, (i32)skillID);

	bool scheduled = false;
	bool teleportToChild = false;
	eastl::fixed_vector<SkillAnimPass,8,false>& animPasses = lastAnimPasses;
	animPasses.clear();
	if(anim == 0 && forceAnim < 0) {
		const bool hitEventCombo = GetGameXmlContent().FindSkillHitEventRemote((i32)skillID) != 0;
		bool regCombo = false;
		bool acts[8] = {};
		bool visuals[8] = {};
		bool moves[8] = {};
		foreach_const(cmd, action.commands) {
			if(cmd->type == ActionCommand::Type::REGCOMBO) regCombo = true;
			if((cmd->type == ActionCommand::Type::MOVE || cmd->type == ActionCommand::Type::GRAPH_MOVE_HORZ) && cmd->animIndex >= 0 && cmd->animIndex < 8) moves[cmd->animIndex] = true;
			bool a = cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET ||
				cmd->type == ActionCommand::Type::STATUS || cmd->type == ActionCommand::Type::STATUS_SKILL_TARGET || cmd->type == ActionCommand::Type::ENTITY;
			if(cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET) {
				const Remote* r = content.FindRemote(cmd->remote.idx);
				const bool visualOnly = r && r->boundType == Remote::BoundType::E_BOUND_NONE && r->onDestroy.empty() && r->onTime.empty() && r->onActivate.empty() &&
					(r->damageGroup == Remote::DamageGroup::eNONE || r->damageGroup == Remote::DamageGroup::INVALID);
				if(visualOnly) {
					a = false;
					if(cmd->animIndex >= 0 && cmd->animIndex < 8) visuals[cmd->animIndex] = true;
				}
			}
			if(cmd->animIndex >= 0 && cmd->animIndex < 8) acts[cmd->animIndex] |= a;
		}
		const char* className = content.GetMaster(player.Main().classType).className.c_str();
		i32 lastActing = -1;
		for(i32 i = 0; i < 8; i++) if(acts[i]) lastActing = i;
		if((!regCombo || !acts[0]) && (!hitEventCombo || !acts[0])) {
			f32 start = 0;
			for(i32 i = 0; i <= lastActing && (!regCombo || i <= 1); i++) {
				if((acts[i] || visuals[i] || moves[i]) && !animPasses.full()) animPasses.push_back({ i, start });
				i32 animId = 0, loop = 1;
				foreach_const(st, action.anims) if(st->index == i) { animId = st->animationId; loop = MAX(st->loop, 1); }
				start += content.GetAniSeqTime(className, animId) * (f32)loop;
				if(regCombo && acts[i]) break;
			}
		}
	}
	if(animPasses.empty()) animPasses.push_back({ anim, schedulePassOffset });
	anim = animPasses[0].index;

	const i32 passes = (i32)animPasses.size();
	for(i32 pass = 0; pass < passes; pass++) {
	const i32 curAnim = animPasses[pass].index;
	foreach_const(cmd, action.commands) {
		if(cmd->animIndex != curAnim && (cmd->animIndex != -1 || pass > 0)) continue;
		const f32 t = animPasses[pass].start + cmd->delay;
		if((cmd->type == ActionCommand::Type::REMOTE || cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET) && !pendingRemotes.full()) {
			PendingRemote pr;
			pr.casterMasterUID = player.Main().UID;
			pr.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			pr.idx = cmd->remote.idx;
			pr.costLevel = castCostLevel;
			if(castCostLevel) {
				foreach_const(rr, castCostLevel->remoteReplace) if(rr->first == (i32)pr.idx) pr.idx = RemoteIdx(rr->second);
				if(pr.idx != cmd->remote.idx) pr.sendIdx = cmd->remote.idx;
			}
			pr.preset = (i32)cmd->remote.targetPreset;
			pr.offsetSide = cmd->remote.offsetSide;
			pr.offsetFront = cmd->remote.offsetFront;
			pr.ownerIndex = player.index;
			pr.skillID = skillID;
			pr.castID = castID;
			pr.damage = castCostLevel ? (i32)(damage * castCostLevel->damageRatio) : damage;
			pr.castPos = castPos;
			pr.angle = cmd->lowerDummyDir ? MovementYaw(player, angle) : angle;
			pr.target = cmd->remote.targetPreset == ActionCommand::TargetPreset::MEMORIZED_TFT ? player.memorizedTarget : currentTarget;
			pr.atTarget = cmd->type == ActionCommand::Type::REMOTE_SKILL_TARGET;
			pr.stopAtTarget = cmd->remote.stopAtTarget;
			pr.remove = cmd->remote.remove != 0;
			pr.nodeName = cmd->nodeName;
			pr.rootDummyDir = cmd->rootDummyDir != 0;
			if(cmd->remote.removeAtAnimEnd) {
				i32 animId = 0, loop = 1;
				foreach_const(st, action.anims) if(st->index == curAnim) { animId = st->animationId; loop = MAX(st->loop, 1); }
				const f32 passEnd = animPasses[pass].start + content.GetAniSeqTime(content.GetMaster(player.Main().classType).className.c_str(), animId) * (f32)loop;
				if(passEnd > t) pr.aniEndAt = TimeAdd(localTime, TimeMsToTime(passEnd * 1000.0));
			}
			pendingRemotes.push_back(pr);
			scheduled = true;
		}
		if(cmd->type == ActionCommand::Type::TELEPORT && cmd->teleport.toLastChild && t <= 0.001f) teleportToChild = true;
		if(cmd->type == ActionCommand::Type::INTERACTION && cmd->interaction.interactionID != 0 && !pendingInteractions.full()) {
			PendingInteraction pi;
			pi.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			pi.ownerIndex = player.index;
			pi.interactionID = cmd->interaction.interactionID;
			pendingInteractions.push_back(pi);
		}
		if(cmd->type == ActionCommand::Type::ENTITY && cmd->entity.entityIndex != 0 && !pendingSummons.full()) {
			PendingSummon ps;
			ps.time = TimeAdd(localTime, TimeMsToTime(t * 1000.0));
			ps.creature = cmd->entity.entityIndex;
			if(upgradeSummon != 0 && upgradeSummon / 10 == ps.creature / 10) ps.creature = upgradeSummon;
			ps.ownerIndex = player.index;
			ps.remove = cmd->entity.remove != 0;
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
			f32 replaceDuration = -1.0f;
			if(castCostLevel) {
				foreach_const(sr, castCostLevel->statusReplace) {
					if(sr->statusID != cmd->status.statusIndex) continue;
					ps.statusID = sr->replaceID;
					if(sr->durationSec > 0) replaceDuration = sr->durationSec;
				}
			}
			ps.casterIndex = player.index;
			const auto preset = cmd->status.targetPreset;
			const bool self = preset == ActionCommand::TargetPreset::SELF_FFF || preset == ActionCommand::TargetPreset::SELF_FFF_1 ||
				preset == ActionCommand::TargetPreset::SELF_TFF || preset == ActionCommand::TargetPreset::SELF_TTF || preset == ActionCommand::TargetPreset::SELF_TTT;
			const bool lastChild = preset == ActionCommand::TargetPreset::LAST_CHILD_FFF || preset == ActionCommand::TargetPreset::LAST_CHILD_TFF || preset == ActionCommand::TargetPreset::CHILD_INDEX_FFF;
			ps.targetUID = lastChild ? ActorUID::INVALID : self ? player.Main().UID : currentTarget;
			ps.duration = replaceDuration;
			if(cmd->status.removeAtEnd) {
				i32 animId = 0, loop = 1;
				foreach_const(st, action.anims) if(st->index == curAnim) { animId = st->animationId; loop = MAX(st->loop, 1); }
				const f32 passLen = animId != 0 ? content.GetAniSeqTime(content.GetMaster(player.Main().classType).className.c_str(), animId) * (f32)loop : action.seqLength;
				ps.duration = MAX(animPasses[pass].start + passLen - t, 0.3f);
			}
			ps.power = (f32)damage;
			ps.remove = cmd->status.remove != 0;
			ps.aniEnd = cmd->status.removeAtEnd != 0;
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
					else if(moveDist > 999990.0f) moveDist = len + HitRadius(*tm->parent) + HitRadius(mover);
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

void World::GainResource(Player& player, i32 slot, f32 amount)
{
	const f32 resMax = MasterStat(player.Main().classType, RESOURCES[slot].maxStat);
	if(resMax <= 0) return;
	player.res[slot] = MIN(MAX(player.res[slot], 0.0f) + amount, resMax);
	player.resDirty = true;
	if(slot == 1 && amount > 0) player.rageGainAt = localTime;
	LOG("Resource :: %ls slot %d +%.0f -> %.0f", player.name.data(), slot, amount, player.res[slot]);
}

void World::ApplyResourceGain(Player& player, ResourceEvent event)
{
	if(const ResourceGainRule* rule = FindResourceGainRule(player.Main().classType, event)) GainResource(player, rule->slot, rule->amount);
}

void World::ApplyRemoteHitResourceGain(Player& owner, RemoteIdx remote, u32 castID, u32 targetIndex)
{
	const ResourceGainRule* rule = FindResourceGainRule(owner.Main().classType, ResourceEvent::REMOTE_HIT_ENEMY, remote);
	if(!rule) return;
	if(owner.resourceGainCast != castID) {
		owner.resourceGainCast = castID;
		owner.resourceGainTargets.clear();
	}
	if(owner.resourceGainTargets.full() || eastl::find(owner.resourceGainTargets.begin(), owner.resourceGainTargets.end(), targetIndex) != owner.resourceGainTargets.end()) return;
	owner.resourceGainTargets.push_back(targetIndex);
	GainResource(owner, rule->slot, rule->amount);
}

void World::UpdateChases()
{
	foreach(it, players) {
		if(it->chaseTarget == ActorUID::INVALID) continue;
		const ActorMaster* tm = FindMasterActor(it->chaseTarget);
		if(it->dead || !tm || tm->parent->dead || localTime >= it->chaseUntil) {
			it->chaseTarget = ActorUID::INVALID;
			continue;
		}
		if(localTime < it->chaseNext) continue;
		it->chaseNext = TimeAdd(localTime, TimeMsToTime(CHASE_UPDATE_INTERVAL_MS));
		replication->SendTargetGraphMoveToAll(it->Main().UID, it->body->GetWorldPos(), it->chaseTarget, tm->parent->body->GetWorldPos(), it->chaseAction);
	}
}

void World::MarkInBattle(Player& player)
{
	player.battleActionAt = localTime;
}

f32 World::PlayerMoveSpeed(const Player& p) const
{
	const f32 baseSpeed = const_cast<CharacterModel&>(GetGameXmlContent().GetMaster(p.Main().classType).character).getMoveSpeed();
	f32 mul = StatusModMulSum(p, StatusMod::MOVESPEED);
	if(localTime < p.firingStatUntil) mul += p.firingMoveSpeedMul;
	return baseSpeed * (p.battleStage == 2 ? BATTLE_SPEED_RATIO_ACTIVE : (p.battleStage == 1 ? BATTLE_SPEED_RATIO_RECENT : 1.0f)) * MAX(1.0f + mul / 100.0f, 0.0f);
}

void World::UpdateBattleStates()
{
	foreach(it, players) {
		if(it->Main().classType == ClassType::NONE) continue;
		u8 stage = 0;
		if(it->battleActionAt != Time::ZERO && !it->dead) {
			const f64 since = TimeDiffSec(TimeDiff(it->battleActionAt, localTime));
			stage = since < BATTLE_STAGE_SEC ? 2 : (since < 2 * BATTLE_STAGE_SEC ? 1 : 0);
		}
		if(stage == it->battleStage) continue;
		it->battleStage = stage;
		const f32 baseSpeed = const_cast<CharacterModel&>(GetGameXmlContent().GetMaster(it->Main().classType).character).getMoveSpeed();
		const f32 speed = baseSpeed * (stage == 2 ? BATTLE_SPEED_RATIO_ACTIVE : (stage == 1 ? BATTLE_SPEED_RATIO_RECENT : 1.0f));
		replication->SendBattleStateToAll(it->Main().UID, stage > 0, speed);
	}
}

void World::BreakHide(Player& player)
{
	foreach(it, activeStatuses) {
		if(it->targetIndex != player.index || !it->info || it->info->type != StatusType::HIDE || it->appliedAt >= localTime || it->until <= localTime) continue;
		it->until = localTime;
		LOG("Hide :: %ls cast ends status %d", player.name.data(), it->statusID);
	}
}

void World::UpdateHideVisibility()
{
	foreach(it, players) {
		ActorUID hidden = ActorUID::INVALID;
		if(!it->dead) {
			foreach_const(st, activeStatuses) {
				if(st->targetIndex == it->index && st->info && st->info->type == StatusType::HIDE && st->until > localTime) {
					hidden = st->targetUID;
					break;
				}
			}
		}
		eastl::fixed_vector<ActorUID,4,false> detectors;
		if(hidden != ActorUID::INVALID) {
			const vec2 at = vec2(it->body->GetWorldPos());
			const Faction mine = it->team == 0 ? Faction::RED : Faction::BLUE;
			foreach_const(npc, actorNpcList) {
				if(npc->ownerUID == ActorUID::INVALID || npc->faction == mine || localTime >= npc->despawnAt || detectors.full()) continue;
				const SummonInfo* info = GetGameXmlContent().FindSummon((i32)npc->docID);
				if(!info || info->hideDetectRadius <= 0) continue;
				if(glm::length(vec2(npc->pos) - at) <= info->hideDetectRadius * HIDE_DETECT_RADIUS_UNIT) detectors.push_back(npc->UID);
			}
		}
		const ActorUID concealed = detectors.empty() ? hidden : ActorUID::INVALID;
		if(concealed != it->hiddenUID) {
			const ActorUID was = it->hiddenUID;
			it->hiddenUID = concealed;
			ShowHiddenToEnemies(*it, was, concealed);
		}
		foreach_const(d, it->hideDetectors) {
			if(eastl::find(detectors.begin(), detectors.end(), *d) != detectors.end() && hidden == it->hideDetectedUID) continue;
			SendHideDetectToAll(*d, it->hideDetectedUID, 0);
		}
		foreach_const(d, detectors) {
			if(eastl::find(it->hideDetectors.begin(), it->hideDetectors.end(), *d) != it->hideDetectors.end() && hidden == it->hideDetectedUID) continue;
			SendHideDetectToAll(*d, hidden, 1);
			LOG("Hide :: %ls detected by summon %u", it->name.data(), (u32)*d);
		}
		it->hideDetectors = detectors;
		it->hideDetectedUID = hidden;
	}
}

void World::SendHideDetectToAll(ActorUID detectorUID, ActorUID targetUID, u8 flag)
{
	foreach(viewer, players) {
		if(viewer->clientHd == ClientHandle::INVALID) continue;
		replication->SendHideDetect(viewer->clientHd, detectorUID, targetUID, flag);
	}
}

void World::ShowHiddenToEnemies(Player& player, ActorUID was, ActorUID hidden)
{
	foreach(viewer, players) {
		if(viewer->clientHd == ClientHandle::INVALID || viewer->team == player.team) continue;
		const u32 bit = 1u << viewer->index;
		if(was != ActorUID::INVALID && (player.hiddenFrom & bit)) {
			const ActorMaster* chara = FindMasterActor(was);
			if(chara) {
				const f32 baseSpeed = const_cast<CharacterModel&>(GetGameXmlContent().GetMaster(chara->classType).character).getMoveSpeed();
				const f32 speed = baseSpeed * (player.battleStage == 2 ? BATTLE_SPEED_RATIO_ACTIVE : (player.battleStage == 1 ? BATTLE_SPEED_RATIO_RECENT : 1.0f));
				replication->SendEnterActor(viewer->clientHd, was, chara->classType, player.body->GetWorldPos(), player.input.rot.bodyYaw, player.input.rot.upperYaw, player.input.moveTo, player.battleStage > 0, speed, chara->actionState != ActionStateID::INVALID ? chara->actionState : ActionStateID::NONE_BEHAVIORSTATE, chara->hp, chara->hpMax);
			}
			player.hiddenFrom &= ~bit;
		}
		if(hidden != ActorUID::INVALID) {
			replication->SendLeaveActor(viewer->clientHd, hidden);
			player.hiddenFrom |= bit;
		}
	}
	LOG("Hide :: %ls %s from enemies", player.name.data(), hidden != ActorUID::INVALID ? "hidden" : "visible");
}

void World::StartSkillPoints()
{
	foreach(it, players) it->nextSkillPointAt = TimeAdd(localTime, TimeMsToTime(SKILL_POINT_INTERVAL_MS));
}

void World::GrantSkillPoint(Player& player)
{
	player.skillPoints++;
	if(player.clientHd != ClientHandle::INVALID) replication->SendSkillPoints(player.clientHd, player.userID, player.skillPoints);
	LOG("Upgrade :: %ls skill points %d", player.name.data(), player.skillPoints);
}

void World::UpdateSkillPoints()
{
	static const i32 BOT_UPGRADE_ORDER[] = { SKILL_UPGRADE_SLOT_ULTIMATE, 0, 1 };
	foreach(it, players) {
		if(it->nextSkillPointAt != Time::ZERO && localTime >= it->nextSkillPointAt) {
			it->nextSkillPointAt = TimeAdd(it->nextSkillPointAt, TimeMsToTime(SKILL_POINT_INTERVAL_MS));
			GrantSkillPoint(*it);
		}
		if(!botUpgrades || it->clientHd != ClientHandle::INVALID || it->skillPoints <= 0 || it->dead) continue;
		for(i32 tries = 0; tries < (i32)ARRAY_COUNT(BOT_UPGRADE_ORDER) && it->skillPoints > 0; tries++) {
			const i32 slot = BOT_UPGRADE_ORDER[it->botUpgradeTurn++ % ARRAY_COUNT(BOT_UPGRADE_ORDER)];
			if(UpgradeSkillSlot(*it, slot)) break;
		}
	}
}

static SkillID SlotSkill(const World::Player& player, u32 masterIndex, i32 slot)
{
	if(slot == SKILL_UPGRADE_SLOT_ULTIMATE) {
		foreach_const(sid, GetGameXmlContent().GetMaster(player.characters[masterIndex]->classType).skillIDs) {
			if((i32)*sid % 100 == ULTIMATE_SKILL_ID_SUFFIX) return *sid;
		}
		return SkillID::INVALID;
	}
	if(slot < 0 || slot > 1) return SkillID::INVALID;
	return player.skills[masterIndex * 2 + slot];
}

static void AcquiredProps(const World::Player& player, SkillID skill, eastl::fixed_vector<i32,8,false>& out)
{
	const auto* ups = GetGameXmlContent().FindSkillUpgrades((i32)skill);
	auto f = player.skillLevels.find((i32)skill);
	if(!ups || f == player.skillLevels.end()) return;
	for(u8 i = 0; i < f->second && i < ups->size(); i++) out.push_back((*ups)[i].propertyID);
}

bool World::UpgradeSkillSlot(Player& player, i32 slot)
{
	if(player.skillPoints <= 0) return false;
	const u32 active = player.mainCharaID;
	bool upgraded = false;
	for(u32 m = 0; m < 2; m++) {
		const SkillID skill = SlotSkill(player, m, slot);
		const auto* ups = skill != SkillID::INVALID ? GetGameXmlContent().FindSkillUpgrades((i32)skill) : nullptr;
		if(!ups) continue;
		u8& level = player.skillLevels[(i32)skill];
		if(level >= ups->size()) continue;
		level++;
		upgraded = true;
		eastl::fixed_vector<i32,8,false> props;
		AcquiredProps(player, skill, props);
		replication->SendPlayerSkillUpdateToAll(player.characters[m]->UID, skill, m == active, Slice<const i32>(props.data(), props.size()));
		LOG("Upgrade :: %ls slot %d skill %d level %d (property %d)", player.name.data(), slot, (i32)skill, (i32)level, props.back());
	}
	if(!upgraded) return false;
	player.skillPoints--;
	if(player.clientHd != ClientHandle::INVALID) {
		eastl::fixed_vector<i32,8,false> mainProps, subProps;
		const SkillID mainSkill = SlotSkill(player, active, slot);
		const SkillID subSkill = SlotSkill(player, active ^ 1, slot);
		AcquiredProps(player, mainSkill, mainProps);
		AcquiredProps(player, subSkill, subProps);
		replication->SendSkillUpgradeResult(player.clientHd, 0, player.characters[active]->UID, mainSkill, Slice<const i32>(mainProps.data(), mainProps.size()),
			player.characters[active ^ 1]->UID, subSkill, Slice<const i32>(subProps.data(), subProps.size()));
		replication->SendSkillPoints(player.clientHd, player.userID, player.skillPoints);
	}
	return true;
}

const GameXmlContent::SkillUpgrade* World::LatestUpgrade(const Player& player, i32 skillID, GameXmlContent::SkillUpgrade::Type type) const
{
	auto f = player.skillLevels.find(skillID);
	const auto* ups = GetGameXmlContent().FindSkillUpgrades(skillID);
	if(f == player.skillLevels.end() || !ups) return nullptr;
	for(i32 i = MIN((i32)f->second, (i32)ups->size()) - 1; i >= 0; i--) {
		if((*ups)[i].type == type) return &(*ups)[i];
	}
	return nullptr;
}

ActionStateID World::UpgradeAction(const Player& player, i32 skillID) const
{
	auto f = player.skillLevels.find(skillID);
	const auto* ups = GetGameXmlContent().FindSkillUpgrades(skillID);
	if(f == player.skillLevels.end() || !ups) return ActionStateID::INVALID;
	for(i32 i = MIN((i32)f->second, (i32)ups->size()) - 1; i >= 0; i--) {
		if((*ups)[i].action != ActionStateID::INVALID) return (*ups)[i].action;
	}
	return ActionStateID::INVALID;
}

const GameXmlContent::SkillUpgrade* World::LatestUpgradeWith(const Player& player, i32 skillID, bool (*has)(const GameXmlContent::SkillUpgrade&)) const
{
	auto f = player.skillLevels.find(skillID);
	const auto* ups = GetGameXmlContent().FindSkillUpgrades(skillID);
	if(f == player.skillLevels.end() || !ups) return nullptr;
	for(i32 i = MIN((i32)f->second, (i32)ups->size()) - 1; i >= 0; i--) {
		if(has((*ups)[i])) return &(*ups)[i];
	}
	return nullptr;
}

f32 World::ComboPassOffset(const Player& player, SkillID skillID) const
{
	const auto& content = GetGameXmlContent();
	for(i32 i = (i32)skillProgramList.size() - 1; i >= 0; i--) {
		const SkillProgram& pg = skillProgramList[i];
		if(pg.casterUID != player.Main().UID || pg.skillID != skillID) continue;
		const GameXmlContent::Action* action = content.FindSkillAction(player.Main().classType, pg.actionID, pg.stance);
		if(!action) return 0;
		f32 first = 0;
		foreach_const(st, action->anims) if(st->index == 0) first = content.GetAniSeqTime(content.GetMaster(player.Main().classType).className.c_str(), st->animationId) * (f32)MAX(st->loop, 1);
		return MAX(first - TimeDiffSec(TimeDiff(pg.startTime, localTime)), 0.0f);
	}
	return 0;
}

i32 World::UpgradeSummon(const Player& player, i32 skillID) const
{
	auto f = player.skillLevels.find(skillID);
	const auto* ups = GetGameXmlContent().FindSkillUpgrades(skillID);
	if(f == player.skillLevels.end() || !ups) return 0;
	for(i32 i = MIN((i32)f->second, (i32)ups->size()) - 1; i >= 0; i--) {
		if((*ups)[i].summon != 0) return (*ups)[i].summon;
	}
	return 0;
}

i32 World::ComboSkillFor(const Player& player, i32 skillID) const
{
	auto f = player.skillLevels.find(skillID);
	const auto* ups = GetGameXmlContent().FindSkillUpgrades(skillID);
	if(f != player.skillLevels.end() && ups) {
		for(i32 i = MIN((i32)f->second, (i32)ups->size()) - 1; i >= 0; i--) {
			if((*ups)[i].nextCombo != 0) return (*ups)[i].nextCombo;
		}
	}
	return GetGameXmlContent().GetFirstComboSkill(skillID);
}

f32 World::PlayerSkillDamage(Player& player, SkillID skillID)
{
	const f32 statusPower = StatusModSum(player, StatusMod::SKILLPOWER);
	if(const GameXmlContent::SkillUpgrade* up = LatestUpgrade(player, (i32)skillID, GameXmlContent::SkillUpgrade::Type::DAMAGE)) {
		if(up->baseDamage >= 0) return MAX(up->baseDamage + up->attackMultiplier * (MasterStat(player.Main().classType, STAT_SKILLPOWER) + statusPower), 0.0f);
	}
	f32 damage = SkillDamage(skillID);
	auto found = GetGameXmlContent().skillMap.find(skillID);
	if(statusPower > 0 && found != GetGameXmlContent().skillMap.end()) {
		SkillNormalModel& sm = const_cast<SkillNormalModel&>(found->second);
		damage += statusPower * sm.getSkillNormalLevelByIndex(0)->getAttackMultiplier();
	}
	return damage;
}

f32 World::PlayerSkillConsume(const Player& player, i32 skillID, i32 index) const
{
	const auto* ups = GetGameXmlContent().FindSkillUpgrades(skillID);
	auto f = player.skillLevels.find(skillID);
	if(ups && f != player.skillLevels.end()) {
		for(i32 i = MIN((i32)f->second, (i32)ups->size()) - 1; i >= 0; i--) {
			if((*ups)[i].consume[index] >= 0) return (*ups)[i].consume[index];
		}
	}
	return index == 4 ? GetGameXmlContent().GetSkillConsumeUG(skillID) : GetGameXmlContent().GetSkillConsume(skillID, index);
}

f32 World::PlayerSkillConsumeMP(const Player& player, i32 skillID) const
{
	const auto* ups = GetGameXmlContent().FindSkillUpgrades(skillID);
	auto f = player.skillLevels.find(skillID);
	if(ups && f != player.skillLevels.end()) {
		for(i32 i = MIN((i32)f->second, (i32)ups->size()) - 1; i >= 0; i--) {
			if((*ups)[i].consumeMP >= 0) return (*ups)[i].consumeMP;
		}
	}
	return GetGameXmlContent().GetSkillConsumeMP(skillID);
}

static i32 ReplaceKey(const eastl::pair<i32,i32>& e) { return e.first; }
static i32 ReplaceKey(const GameXmlContent::StatusReplace& e) { return e.statusID; }
static i32 ReplaceKey(const eastl::pair<i32,const Remote*>& e) { return e.first; }

template<typename List>
static void MergeReplace(List& into, const List& from)
{
	foreach_const(r, from) {
		auto it = eastl::find_if(into.begin(), into.end(), [&](const auto& e) { return ReplaceKey(e) == ReplaceKey(*r); });
		if(it != into.end()) *it = *r;
		else if(!into.full()) into.push_back(*r);
	}
}

const GameXmlContent::CostLevel* World::UpgradeCastMods(Player& player, SkillID skillID, const GameXmlContent::CostLevel* costLevel)
{
	auto f = player.skillLevels.find((i32)skillID);
	const auto* ups = GetGameXmlContent().FindSkillUpgrades((i32)skillID);
	if(f == player.skillLevels.end() || f->second == 0 || !ups) return costLevel;
	const auto* levels = costLevel ? GetGameXmlContent().FindSkillCostLevels((i32)skillID) : nullptr;
	const i32 costIndex = levels ? (i32)(costLevel - levels->data()) : -1;
	GameXmlContent::CostLevel& mods = player.castMods[((u32)skillID << 3) | (u32)(costIndex + 1)];
	mods = costLevel ? *costLevel : GameXmlContent::CostLevel();
	for(u8 i = 0; i < f->second && i < ups->size(); i++) {
		const GameXmlContent::SkillUpgrade& up = (*ups)[i];
		MergeReplace(mods.remoteReplace, up.remoteReplace);
		MergeReplace(mods.statusReplace, up.statusReplace);
		MergeReplace(mods.remoteResize, up.remoteResize);
	}
	const GameXmlContent::SkillUpgrade* dmg = LatestUpgrade(player, (i32)skillID, GameXmlContent::SkillUpgrade::Type::DAMAGE);
	if(dmg && costIndex >= 0 && costIndex < (i32)dmg->costLevelDamage.size() && dmg->baseDamage > 0 && dmg->costLevelDamage[costIndex].first >= 0) {
		const f32 skillPower = MasterStat(player.Main().classType, STAT_SKILLPOWER) + StatusModSum(player, StatusMod::SKILLPOWER);
		const f32 normal = dmg->baseDamage + dmg->attackMultiplier * skillPower;
		const f32 empowered = dmg->costLevelDamage[costIndex].first + dmg->costLevelDamage[costIndex].second * skillPower;
		if(normal > 0) mods.damageRatio = empowered / normal;
	}
	return &mods;
}
