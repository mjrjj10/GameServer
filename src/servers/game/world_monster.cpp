#include <time.h>
#include "config.h"
#include "world.h"
#include "world_common.h"
#include <mxm/game_content.h>

static const f64 MONSTER_START_DELAY_MS = 90.0;
static const f64 MONSTER_SEARCH_MS = 250.0;
static const f64 MONSTER_SYNC_MS = 500.0;
static const f64 HYP_MONSTER_TURN_RESYNC_MS = 150.0;
static const f32 HYP_MONSTER_TURN_RESYNC_RAD = 0.5236f;

static f32 YawGap(f32 a, f32 b)
{
	const f32 pi = (f32)PI;
	f32 d = fmodf(a - b, 2.0f * pi);
	if(d > pi) d -= 2.0f * pi;
	if(d < -pi) d += 2.0f * pi;
	return fabsf(d);
}
static const f32 MONSTER_WAYPOINT_REACHED = 60.0f;
static const f32 MONSTER_REMOTE_MIN_LIFE_SEC = 0.05f;
static const u32 MEASURED_MONSTER_REMOTE_SEED_STEP = 4;
static const i32 MEASURED_GOT_MOB_RETURN_SUPERARMOR = 1264070082;
static const f32 MONSTER_REACH_TOLERANCE = 1.0f;
static const u8 MONSTER_ATTACK_STAT = 6;
static const u32 NPC_DEAD_NO_KILLER_DOC = 0;

static f32 MonsterAttackPower(const World::ActorNpc& npc)
{
	const GameXmlContent& content = GetGameXmlContent();
	const auto* stats = content.FindMonsterStats((i32)npc.docID);
	if(!stats) return 0;
	const auto& list = *stats;
	foreach_const(st, list) if(st->type == MONSTER_ATTACK_STAT) return (st->value + content.MonsterGrowth((i32)npc.docID, MONSTER_ATTACK_STAT) * npc.growthLevel) * npc.statScale;
	return 0;
}

static bool IsTeamFaction(Faction f)
{
	return f == Faction::RED || f == Faction::BLUE;
}

static bool FactionsOppose(Faction a, Faction b, bool hostileMonsters)
{
	if((a == Faction::RED && b == Faction::BLUE) || (a == Faction::BLUE && b == Faction::RED)) return true;
	return hostileMonsters && ((a == Faction::MONSTER && IsTeamFaction(b)) || (b == Faction::MONSTER && IsTeamFaction(a)));
}

static bool MonsterNeutral(const World::ActorNpc& npc, bool hostileMonsters)
{
	return npc.faction == Faction::ORANGE || (npc.faction == Faction::MONSTER && !hostileMonsters);
}

static GameXmlContent::MonsterAttack::Target MonsterTargetKind(const GameXmlContent::MonsterAi& ai)
{
	static const u32 minionTag = StatusTypeKey("Minion");
	static const u32 titanTag = StatusTypeKey("Titan");
	if(ai.tagKey == minionTag) return GameXmlContent::MonsterAttack::MINION;
	return ai.tagKey == titanTag ? GameXmlContent::MonsterAttack::TITAN : GameXmlContent::MonsterAttack::STRUCTURE;
}

bool World::MonsterHostile(const ActorNpc& monster, const ActorNpc& other) const
{
	if(&monster == &other || !other.monsterAi || other.hp <= 0 || other.removeAt != Time::ZERO) return false;
	if(other.despawnAt != Time::ZERO && localTime >= other.despawnAt) return false;
	return FactionsOppose(monster.faction, other.faction, hostileMonsters) && monster.monsterAi->Hates(other.monsterAi->tagKey);
}

bool World::MonsterHostile(const ActorNpc& monster, const Player& player) const
{
	static const u32 playerTag = StatusTypeKey("Player");
	if(player.dead || player.untargetable || !player.body) return false;
	if(MonsterNeutral(monster, hostileMonsters)) return monster.monsterTarget == player.Main().UID;
	return FactionsOppose(monster.faction, (Faction)player.team, hostileMonsters) && (monster.monsterAi->Hates(playerTag) || (hostileMonsters && monster.monsterAi->enemyTags.empty()));
}

static const u32 MONSTER_REMOTE_ID_BASE = 0xC0000000u;
static const u32 MONSTER_REMOTE_ID_MASK = 0x3FFFFFFFu;
static const u32 MEASURED_MONSTER_HIT_PENETRATION = 10;
static const f32 HYP_MONSTER_MELEE_HIT_SLACK = 60.0f;
static const f32 HYP_MONSTER_SHELL_HIT_RADIUS = 200.0f;
static const f32 HYP_MONSTER_TARGET_RADIUS = 55.0f;
static const f32 HYP_MONSTER_FIRE_DEFAULT_REACH = 300.0f;
static const f32 DEATH_HIT_KEEP_SEC = 1.0f;

void World::ApplyDeathHit(const DeathHit& hit)
{
	ActorNpc* helper = FindNpcActor(hit.helper);
	if(!helper) return;
	const GameXmlContent::DeathRemote& d = *hit.remote;
	const bool teamed = helper->faction == Faction::RED || helper->faction == Faction::BLUE;
	const i32 damage = (i32)(MonsterAttackPower(*helper) * d.multiplier + 0.5f);
	const i32 firstStatus = d.statuses.empty() ? 0 : d.statuses[0];
	if(d.pc) {
		foreach(p, players) {
			if(p->dead || !p->body || p->untargetable) continue;
			const bool ally = teamed && (Faction)p->team == helper->faction;
			if(ally != d.ally || (!teamed && d.ally)) continue;
			if(glm::length(vec2(p->body->GetWorldPos()) - vec2(helper->pos)) > d.radius) continue;
			if(damage > 0 && !d.ally) DamagePlayerFromNpc(*helper, *p, damage, d.remote, firstStatus, hit.remoteID);
			else if(firstStatus != 0) ApplyStatusFromNpc(*helper, *p, firstStatus);
			for(i32 s = 1; s < (i32)d.statuses.size(); s++) if(!p->dead) ApplyStatusFromNpc(*helper, *p, d.statuses[s]);
		}
	}
	if(d.npc) {
		foreach(o, actorNpcList) {
			if(&*o == helper || o->hp <= 0 || o->removeAt != Time::ZERO || !o->monsterAi || (o->faction != Faction::RED && o->faction != Faction::BLUE)) continue;
			const bool ally = teamed && o->faction == helper->faction;
			if(ally != d.ally) continue;
			if(glm::length(vec2(o->pos) - vec2(helper->pos)) > d.radius + o->monsterAi->radius) continue;
			if(damage > 0 && !d.ally) DamageNpcFromNpc(*helper, *o, damage, d.remote, hit.remoteID, firstStatus);
			else if(firstStatus != 0) ApplyStatusToNpcFromNpc(*helper, *o, firstStatus);
			for(i32 s = 1; s < (i32)d.statuses.size(); s++) if(o->hp > 0) ApplyStatusToNpcFromNpc(*helper, *o, d.statuses[s]);
		}
	}
	LOG("Monster :: creature=%d death remote %d (%s, radius %.0f, damage %d)", (i32)helper->docID, d.remote, d.ally ? "allies" : "enemies", d.radius, damage);
}

void World::CheckSupportKit(Player& player)
{
	if(!replication->pveTeams || player.supportKit <= 0 || player.supportKitCount <= 0 || player.dead || player.clientHd == ClientHandle::INVALID) return;
	if(player.supportKitReadyAt != Time::ZERO && localTime < player.supportKitReadyAt) return;
	const GameXmlContent::SupportKit* kit = GetGameXmlContent().FindSupportKit(player.supportKit);
	ActorMaster& chara = player.Main();
	if(!kit || kit->hpPercent <= 0 || chara.hp <= 0 || chara.hp * 100 >= chara.hpMax * kit->hpPercent) return;
	foreach_const(s, kit->statuses) ApplyStatus(player, player, *s);
	player.supportKitReadyAt = TimeAdd(localTime, TimeMsToTime(kit->coolTime * 1000.0));
	player.supportKitCount--;
	replication->SendActivatedSupportKit(player.clientHd, replication->GetLocalActorID(player.clientHd, chara.UID), player.supportKit, kit->coolTimeID, kit->coolTime);
	AccountInventory inventory = AccountInventoryLoad(player.name.data());
	for(auto it = inventory.items.begin(); it != inventory.items.end(); ++it) {
		if(it->docIndex != player.supportKit || it->count <= 0) continue;
		it->count--;
		replication->SendKitConsumed(player.clientHd, it->uid, it->invenType, it->slot, it->docIndex, it->count);
		if(it->count <= 0) inventory.items.erase(it);
		break;
	}
	AccountInventorySave(player.name.data(), inventory);
	LOG("SupportKit :: %ls kit %d at %d/%d hp (%d left)", player.name.data(), player.supportKit, chara.hp, chara.hpMax, player.supportKitCount);
}

bool World::LoadProgress(const Player& player)
{
	if(player.clientHd == ClientHandle::INVALID || player.index >= achieve.size()) return false;
	if(!progressLoaded[player.index]) {
		achieve[player.index] = AccountAchievementsLoad(player.name.data());
		summaries[player.index] = AccountSummariesLoad(player.name.data());
		missionSaved[player.index] = AccountMissionsLoad(player.name.data());
		missions[player.index].clear();
		MissionBuildActive(player.name.data(), AccountProgressLoad(player.name.data()).level, (i64)time(nullptr), missionSaved[player.index], &missions[player.index]);
		progressLoaded[player.index] = 1;
	}
	return true;
}

void World::AchieveLive(const Player& player, const AchieveEvent& ev)
{
	if(!LoadProgress(player)) return;
	AchieveChanges changes;
	AchieveApply(achieve[player.index], ev, &changes);
	if(changes.indices.empty() && changes.titles.empty()) return;
	progressLoaded[player.index] = 2;
	AchieveSendChanges(achieve[player.index], changes, [&](const auto& writer) { replication->SendWriter(player.clientHd, writer); });
}

void World::SummaryLive(const Player& player, i32 summary, i32 stage, u8 type, i32 data)
{
	if(!LoadProgress(player)) return;
	AccountSummaries& s = summaries[player.index];
	if(!s.Add(summary, stage, type, data, AchieveNow())) return;
	progressLoaded[player.index] = 2;
	const AccountSummaryEntry& e = s.entries.back();
	PacketWriter<Sv::SN_SummaryUpdate,64> packet;
	packet.Write<i32>(e.summary);
	packet.Write<i32>(e.stage);
	packet.Write<i32>((i32)e.type);
	packet.Write<i32>(e.data);
	packet.Write<u8>(e.received);
	packet.Write<i64>(e.date);
	replication->SendWriter(player.clientHd, packet);
	LOG("Summary :: %ls stage %d type %u data %d", player.name.data(), stage, (u32)type, data);
}

void World::SendMissionChanges(const Player& player, const MissionChanges& changed)
{
	foreach_const(i, changed) {
		const ActiveMission& m = missions[player.index][*i];
		PacketWriter<Sv::SN_MissionUpdate,32> packet;
		packet.Write<i32>(m.mission->ID);
		packet.Write<i64>(m.progress);
		packet.Write<u16>(m.changedCount);
		packet.Write<u8>(m.cleared ? 1 : 0);
		packet.Write<u8>(m.rewarded ? 1 : 0);
		replication->SendWriter(player.clientHd, packet);
	}
}

void World::MissionLive(const Player& player, i32 kills, i32 assists)
{
	if(!LoadProgress(player)) return;
	AccountGameRecord section;
	section.gameType = (i32)replication->gameType;
	section.stageIndex = (i32)replication->stageIndex;
	MissionChanges changed;
	MissionApplyCombat(kills, assists, section, &missions[player.index], &changed);
	if(changed.empty()) return;
	progressLoaded[player.index] = 2;
	SendMissionChanges(player, changed);
}

void World::FinishProgress(const Player& player, const AccountGameRecord& record)
{
	if(LoadProgress(player)) {
		AccountGameRecord rest = record;
		rest.kills = 0;
		rest.assists = 0;
		MissionChanges changed;
		MissionApplyGame(rest, &missions[player.index], &changed);
		if(!changed.empty()) {
			progressLoaded[player.index] = 2;
			SendMissionChanges(player, changed);
		}
		AchieveChanges changes;
		AchieveApplyRecord(achieve[player.index], record, &changes);
		if(!changes.indices.empty() || !changes.titles.empty()) {
			progressLoaded[player.index] = 2;
			AchieveSendChanges(achieve[player.index], changes, [&](const auto& writer) { replication->SendWriter(player.clientHd, writer); });
		}
	}
	SaveProgress(player);
}

void World::SaveProgress(const Player& player)
{
	if(player.index >= achieve.size() || progressLoaded[player.index] != 2) return;
	AccountAchievementsSave(player.name.data(), achieve[player.index]);
	AccountSummariesSave(player.name.data(), summaries[player.index]);
	MissionStore(missions[player.index], &missionSaved[player.index]);
	AccountMissionsSave(player.name.data(), missionSaved[player.index]);
	progressLoaded[player.index] = 1;
}

void World::MonsterDie(ActorNpc& npc, ActorUID killerUID, CreatureIndex killerDoc)
{
	if(npc.removeAt != Time::ZERO) return;
	npc.hp = 0;
	npc.killerUID = killerUID;
	npc.actionState = ActionStateID::DIE_BEHAVIORSTATE;
	replication->SendAiActionStateToAll(npc.UID, ActionStateID::DIE_BEHAVIORSTATE, npc.rot.z, npc.pos, false);
	replication->SendNpcDeadAckToAll(npc.UID, killerUID, killerUID != ActorUID::INVALID ? (u32)killerDoc : NPC_DEAD_NO_KILLER_DOC);
	if(npc.monsterAi && map && map->ingameLevels) {
		foreach_const(r, npc.monsterAi->dieRemotes) {
			if(const Remote* remote = GetGameXmlContent().FindRemote((RemoteIdx)*r)) SpawnMonsterRemote(npc, *remote, npc.pos, vec2(cosf(npc.rot.z), sinf(npc.rot.z)), ActorUID::INVALID, true);
		}
	}
	if(npc.monsterTarget != ActorUID::INVALID) replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
	npc.monsterTarget = ActorUID::INVALID;
	const f32 destroy = npc.monsterAi ? npc.monsterAi->destroy : 0.0f;
	npc.removeAt = TimeAdd(localTime, TimeMsToTime(MAX(destroy, 1.0f) * 1000.0));
	LOG("Monster :: creature=%d (local %d) dies at (%.0f, %.0f) t=%.1f killer=%d", (i32)npc.docID, npc.localID, npc.pos.x, npc.pos.y, GameTimeSec(), (i32)killerDoc);
	if(ActorMaster* killer = killerUID != ActorUID::INVALID ? FindMasterActor(killerUID) : nullptr) {
		if(killer->parent) {
			killer->parent->pveKills++;
			if(replication->pveTeams) {
				AchieveEvent ev;
				ev.sub = GameXmlContent::AchieveSub::PVE_KILL_MONSTER_TOTAL;
				ev.stage = (i32)replication->stageIndex;
				ev.gameType = (i32)replication->gameType;
				AchieveLive(*killer->parent, ev);
			}
		}
	}
	if(const GameXmlContent::Summary* summary = GetGameXmlContent().FindStageSummary((i32)replication->stageIndex, nullptr)) {
		foreach_const(r, summary->rares) {
			if(r->first != (i32)npc.docID) continue;
			foreach_const(p, players) SummaryLive(*p, summary->index, (i32)replication->stageIndex, (u8)SummaryEntryType::RARE_MONSTER, r->first);
		}
	}
	stageKills.push_back(eastl::make_pair((i32)npc.docID, (f32)GameTimeSec()));
	DefenceMonsterDied(npc);
	if(stageClearMonster != 0 && (i32)npc.docID == stageClearMonster && ++stageClearKills >= MAX(stageClearCount, 1)) {
		stageCleared = true;
		LOG("Stage :: clear monster %d killed", stageClearMonster);
	}
	if(npc.monsterAi && npc.monsterAi->teamBuff != 0) {
		foreach(p, players) if((Faction)p->team == npc.faction) ConsumeStatus(p->index, npc.monsterAi->teamBuff);
	}
	if(const auto* deaths = GetGameXmlContent().FindDeathRemotes((i32)npc.docID)) {
		f32 longest = 0;
		const u32 remoteID = MONSTER_REMOTE_ID_BASE | (nextMonsterRemoteID++ & MONSTER_REMOTE_ID_MASK);
		for(const auto& d : *deaths) {
			if(deathHits.full()) break;
			deathHits.push_back({ npc.UID, TimeAdd(localTime, TimeMsToTime(d.delay * 1000.0)), &d, remoteID });
			longest = MAX(longest, d.delay);
		}
		const Time keep = TimeAdd(localTime, TimeMsToTime((longest + DEATH_HIT_KEEP_SEC) * 1000.0));
		if(npc.removeAt < keep) npc.removeAt = keep;
	}
	GrantMonsterKillExp(npc, killerUID);
	GrantMonsterKillPoints(npc, killerUID);
	OnMonsterDeath(npc, killerUID);
	if(const auto* spawns = GetGameXmlContent().FindMonsterDieSpawn((i32)npc.docID)) {
		const vec3 pos = npc.pos;
		const vec3 rot = npc.rot;
		const Faction faction = npc.faction;
		const bool titan = npc.monsterAi && MonsterTargetKind(*npc.monsterAi) == GameXmlContent::MonsterAttack::TITAN;
		eastl::fixed_vector<ActorUID,4,false> helpers;
		const Faction helperFaction = titan && (faction == Faction::RED || faction == Faction::BLUE) ? (faction == Faction::RED ? Faction::BLUE : Faction::RED) : Faction::MONSTER;
		for(const i32 s : *spawns) {
			ActorNpc& helper = SpawnMonster((CreatureIndex)s, 0, pos, rot, helperFaction, -1);
			if(!helpers.full()) helpers.push_back(helper.UID);
		}
		if(titan && !helpers.empty()) StartTitanVictory(faction, pos, helpers.front(), helpers.back());
	}
}

static const f32 TITAN_VICTORY_RANGE = 1600.0f;
static const f64 TITAN_VICTORY_START_MS = 500.0;
static const i32 STATUS_TITAN_VICTORY_FLAG = 1200100045;
static const i32 STATUS_TITAN_TARGET_FLAG = 1200100047;
static const i32 STATUS_TITAN_READY_FLAG = 1200100046;

void World::StartTitanVictory(Faction deadFaction, const vec3& deadPos, ActorUID soulUID, ActorUID triggerUID)
{
	ActorNpc* victor = nullptr;
	f32 best = TITAN_VICTORY_RANGE;
	foreach(o, actorNpcList) {
		if(o->hp <= 0 || o->removeAt != Time::ZERO || !o->monsterAi || o->monsterAi->victoryStates[0] == ActionStateID::INVALID) continue;
		if(!FactionsOppose(o->faction, deadFaction, hostileMonsters) || MonsterTargetKind(*o->monsterAi) != GameXmlContent::MonsterAttack::TITAN) continue;
		const f32 d = glm::length(vec2(o->pos) - vec2(deadPos));
		if(d < best) { best = d; victor = &*o; }
	}
	if(!victor || victor->victoryStep != 0) return;
	if(const ActorNpc* trigger = FindNpcActor(triggerUID)) ApplyStatusToNpcFromNpc(*trigger, *victor, STATUS_TITAN_VICTORY_FLAG);
	if(const ActorNpc* soul = FindNpcActor(soulUID)) ApplyStatusToNpcFromNpc(*soul, *victor, STATUS_TITAN_TARGET_FLAG);
	victor->victoryPendingAt = TimeAdd(localTime, TimeMsToTime(TITAN_VICTORY_START_MS));
	LOG("Titan :: creature=%d (local %d) absorbs a dead titan", (i32)victor->docID, victor->localID);
}

void World::UpdateNpcVictory(ActorNpc& npc)
{
	const GameXmlContent::MonsterAi& ai = *npc.monsterAi;
	if(npc.victoryStep == 0) {
		if(npc.victoryPendingAt == Time::ZERO || localTime < npc.victoryPendingAt || localTime < npc.monsterHoldUntil) return;
		npc.victoryPendingAt = Time::ZERO;
		npc.victoryStep = 1;
		npc.stateEndAt = Time::ZERO;
		npc.actionState = ai.victoryStates[0];
		replication->SendMonsterStateToAll(npc.UID, ai.victoryStates[0], npc.pos, 0.0f);
		npc.victoryNext = TimeAdd(localTime, TimeMsToTime(ai.victoryTimes[0] * 1000.0));
		npc.monsterHoldUntil = TimeAdd(npc.victoryNext, TimeMsToTime(ai.victoryTimes[1] * 1000.0));
		return;
	}
	if(npc.victoryStep == 1 && localTime >= npc.victoryNext) {
		npc.victoryStep = 2;
		npc.actionState = ai.victoryStates[1];
		replication->SendMonsterStateToAll(npc.UID, ai.victoryStates[1], npc.pos, 0.0f);
		ApplyMonsterStatus(npc, STATUS_TITAN_READY_FLAG, true);
		npc.victoryNext = TimeAdd(localTime, TimeMsToTime(ai.victoryStatusDelay * 1000.0));
		return;
	}
	if(npc.victoryStep == 2 && localTime >= npc.victoryNext) {
		npc.victoryStep = 3;
		f32 healDelay = 0;
		foreach_const(st, ai.victoryStatuses) {
			ApplyMonsterStatus(npc, *st, true);
			const StatusInfo* info = GetGameXmlContent().FindStatus(*st);
			if(info && info->type == StatusType::HEAL) healDelay = MAX(info->intervalSec, info->durationSec);
		}
		if(ai.victoryHeal > 0) npc.victoryHealAt = TimeAdd(localTime, TimeMsToTime(healDelay * 1000.0));
		npc.victoryNext = TimeAdd(localTime, TimeMsToTime(MAX(ai.victoryTimes[1] - ai.victoryStatusDelay, 0.0f) * 1000.0));
		return;
	}
	if(npc.victoryStep == 3) {
		if(npc.victoryHealAt != Time::ZERO && localTime >= npc.victoryHealAt) {
			npc.victoryHealAt = Time::ZERO;
			npc.hp = MIN(npc.hp + (i32)(npc.hpMax * ai.victoryHeal), npc.hpMax);
			replication->SendHpToAll(npc.UID, npc.hp, npc.hpMax);
			LOG("Titan :: creature=%d (local %d) healed to %d/%d by absorbing", (i32)npc.docID, npc.localID, npc.hp, npc.hpMax);
		}
		if(localTime >= npc.victoryNext && npc.victoryHealAt == Time::ZERO) {
			npc.victoryStep = 0;
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, 0.0f);
		}
	}
}


void World::SendNpcHit(const ActorNpc& attacker, ActorUID targetUID, const vec3& targetPos, i32 remote, u32 remoteID, i32 damage, i32 hp, i32 hpMax)
{
	if(remote <= 0 || remoteID == 0) {
		replication->SendDamageToAll(attacker.UID, targetUID, SkillID::INVALID, damage, hp, hpMax);
		return;
	}
	const Remote* info = GetGameXmlContent().FindRemote((RemoteIdx)remote);
	const vec2 rel = vec2(targetPos) - vec2(attacker.pos);
	const f32 len = glm::length(rel);
	Replication::RemoteDamage hit;
	hit.remoteID = remoteID;
	hit.remoteDoc = (RemoteIdx)remote;
	hit.attackerUID = attacker.UID;
	hit.targetUID = targetUID;
	hit.dir = len > 1.0f ? rel / len : vec2(cosf(attacker.rot.z), sinf(attacker.rot.z));
	hit.remotePos = info && info->maxSpeed > 0 ? targetPos : attacker.pos;
	hit.targetPos = targetPos;
	hit.damage = damage;
	hit.hp = hp;
	hit.hpMax = hpMax;
	replication->SendRemoteDamageToAll(hit);
	if(hostileMonsters) replication->SendRemoteActivatedToAll(remoteID, ClientHandle::INVALID, ClientHandle::INVALID, targetUID, MEASURED_MONSTER_HIT_PENETRATION, hit.remotePos);
}

void World::ApplyMonsterAllyStatus(ActorUID casterUID, const GameXmlContent::MonsterAttack& attack)
{
	const ActorNpc* caster = FindNpcActor(casterUID);
	if(!caster || caster->hp <= 0) return;
	const vec2 center = vec2(caster->pos);
	foreach(o, actorNpcList) {
		if(&*o == caster || o->hp <= 0 || o->removeAt != Time::ZERO || !o->monsterAi || o->faction != caster->faction) continue;
		if(glm::length(vec2(o->pos) - center) > attack.allyRadius) continue;
		ApplyStatusToNpcFromNpc(*caster, *o, attack.allyStatus);
	}
	foreach(p, players) {
		if(p->dead || !p->body || (Faction)p->team != caster->faction) continue;
		if(glm::length(vec2(p->body->GetWorldPos()) - center) > attack.allyRadius) continue;
		ApplyStatusFromNpc(*caster, *p, attack.allyStatus);
	}
}

void World::MonsterAttackHit(ActorUID attackerUID, ActorUID targetUID, const GameXmlContent::MonsterAttack& attack, u32 remoteID, const vec3* aimPos)
{
	auto attackerIt = actorNpcMap.find(attackerUID);
	if(attackerIt == actorNpcMap.end()) return;
	ActorNpc& attacker = *attackerIt->second;
	const Remote* hitData = attack.hitRemote > 0 ? GetGameXmlContent().FindRemote((RemoteIdx)attack.hitRemote) : nullptr;
	const i32 damage = (i32)(MonsterAttackPower(attacker) * (hitData ? hitData->attackMultiplier : attack.multiplier) + 0.5f);
	if(damage <= 0 && attack.status <= 0) return;
	auto targetIt = actorNpcMap.find(targetUID);
	ActorMaster* master = targetIt == actorNpcMap.end() ? FindMasterActor(targetUID) : nullptr;
	if(attack.radius > 0) {
		vec3 center = attacker.pos;
		if(attack.selfCentered) center = attacker.pos;
		else if(aimPos && attack.hitRemote > 0 && GetGameXmlContent().FindRemote((RemoteIdx)attack.hitRemote) && GetGameXmlContent().FindRemote((RemoteIdx)attack.hitRemote)->bezier) center = *aimPos;
		else if(targetIt != actorNpcMap.end()) center = targetIt->second->pos;
		else if(master && master->parent) center = master->parent->body->GetWorldPos();
		foreach(p, players) {
			if(p->dead || !p->body || !MonsterHostile(attacker, *p) || glm::length(vec2(p->body->GetWorldPos()) - vec2(center)) > attack.radius + (aimPos || attack.selfCentered ? HitRadius(*p) : 0.0f)) continue;
			DamagePlayerFromNpc(attacker, *p, damage, attack.hitRemote, attack.status, remoteID);
		}
		foreach(o, actorNpcList) {
			if(&*o == &attacker || o->hp <= 0 || !MonsterHostile(attacker, *o) || glm::length(vec2(o->pos) - vec2(center)) > attack.radius) continue;
			const bool execute = attack.executeMinion && MonsterTargetKind(*o->monsterAi) == GameXmlContent::MonsterAttack::MINION;
			DamageNpcFromNpc(attacker, *o, execute ? o->hpMax : damage, attack.hitRemote, remoteID, attack.status, execute);
		}
		return;
	}
	if(aimPos && attack.radius <= 0) {
		vec3 targetPos = *aimPos;
		if(targetIt != actorNpcMap.end()) targetPos = targetIt->second->pos;
		else if(master && master->parent && master->parent->body) targetPos = master->parent->body->GetWorldPos();
		const Remote* shot = attack.hitRemote > 0 ? GetGameXmlContent().FindRemote((RemoteIdx)attack.hitRemote) : nullptr;
		bool onPath = glm::length(vec2(targetPos) - vec2(*aimPos)) <= HYP_MONSTER_SHELL_HIT_RADIUS;
		if(shot && !shot->bezier && glm::length(vec2(*aimPos) - vec2(attacker.pos)) > 1.0f) {
			const vec2 dir = glm::normalize(vec2(*aimPos) - vec2(attacker.pos));
			const vec2 rel = vec2(targetPos) - vec2(attacker.pos);
			const f32 along = glm::dot(rel, dir);
			const f32 side = fabsf(dir.x * rel.y - dir.y * rel.x);
			const f32 reach = shot->lifeTime > 0 && attack.speed > 0 ? attack.speed * shot->lifeTime : FLT_MAX;
			onPath = along >= 0 && along <= reach + HYP_MONSTER_TARGET_RADIUS && side <= shot->boundSize[0] * 0.5f + HYP_MONSTER_TARGET_RADIUS;
		}
		if(!onPath) {
			LOG("Monster :: creature=%d (local %d) projectile misses, target off the shot path", (i32)attacker.docID, attacker.localID);
			return;
		}
	}
	if(attack.speed <= 0 && attack.range > 0) {
		vec3 targetPos = attacker.pos;
		if(targetIt != actorNpcMap.end()) targetPos = targetIt->second->pos;
		else if(master && master->parent && master->parent->body) targetPos = master->parent->body->GetWorldPos();
		const vec2 rel = vec2(targetPos) - vec2(attacker.pos);
		bool inReach = glm::length(rel) <= attack.range + HYP_MONSTER_MELEE_HIT_SLACK;
		const Remote* swing = attack.remote > 0 ? GetGameXmlContent().FindRemote((RemoteIdx)attack.remote) : nullptr;
		if(swing && swing->boundType == Remote::BoundType::E_BOUND_BOX && swing->boundSize[1] > 0) {
			const vec2 dir = vec2(cosf(attacker.rot.z), sinf(attacker.rot.z));
			const f32 half = swing->boundSize[1] * 0.5f;
			auto inBox = [&](const vec2& at) {
				const vec2 r = at - vec2(attacker.pos);
				const f32 along = glm::dot(r, dir);
				const f32 side = fabsf(glm::dot(r, vec2(-dir.y, dir.x)));
				const f32 mid = swing->boundOffsetY + swing->offsetY;
				return along >= mid - half - HYP_MONSTER_TARGET_RADIUS && along <= mid + half + HYP_MONSTER_TARGET_RADIUS && side <= swing->boundSize[0] * 0.5f + HYP_MONSTER_TARGET_RADIUS;
			};
			inReach = inBox(vec2(targetPos));
			if(swing->behaviorType == Remote::BehaviorType::NONETARGET && swing->activateCount > 1 && attack.radius <= 0) {
				i32 hits = 0;
				foreach(p, players) {
					if(hits >= swing->activateCount) break;
					if(p->dead || !p->body || !MonsterHostile(attacker, *p) || !inBox(vec2(p->body->GetWorldPos()))) continue;
					DamagePlayerFromNpc(attacker, *p, damage, attack.hitRemote, attack.status, remoteID);
					hits++;
				}
				foreach(o, actorNpcList) {
					if(hits >= swing->activateCount) break;
					if(&*o == &attacker || o->hp <= 0 || !MonsterHostile(attacker, *o) || !inBox(vec2(o->pos))) continue;
					const bool execute = attack.executeMinion && o->monsterAi && MonsterTargetKind(*o->monsterAi) == GameXmlContent::MonsterAttack::MINION;
					DamageNpcFromNpc(attacker, *o, execute ? o->hpMax : damage, attack.hitRemote, remoteID, attack.status, execute);
					hits++;
				}
				return;
			}
		}
		else if(swing && swing->boundType == Remote::BoundType::E_BOUND_SPHERE && swing->boundSize[0] > 0) {
			const vec2 center = vec2(cosf(attacker.rot.z), sinf(attacker.rot.z)) * (swing->offsetY + RemoteBoundOffset(*swing));
			const f32 targetRadius = targetIt != actorNpcMap.end() ? NpcHitRadius(*targetIt->second) : (master && master->parent ? HitRadius(*master->parent) : HYP_MONSTER_TARGET_RADIUS);
			inReach = glm::length(rel - center) <= swing->boundSize[0] * 0.5f + targetRadius;
		}
		if(!inReach) {
			LOG("Monster :: creature=%d (local %d) attack misses, target out of reach", (i32)attacker.docID, attacker.localID);
			return;
		}
	}
	if(targetIt != actorNpcMap.end()) {
		ActorNpc& npcTarget = *targetIt->second;
		const bool execute = attack.executeMinion && npcTarget.monsterAi && MonsterTargetKind(*npcTarget.monsterAi) == GameXmlContent::MonsterAttack::MINION;
		DamageNpcFromNpc(attacker, npcTarget, execute ? npcTarget.hpMax : damage, attack.hitRemote, remoteID, attack.status, execute);
		return;
	}
	if(master && master->parent) DamagePlayerFromNpc(attacker, *master->parent, damage, attack.hitRemote, attack.status, remoteID);
}

void World::UpdateMonsterHits()
{
	for(i32 i = 0; i < (i32)deathHits.size(); ) {
		if(localTime < deathHits[i].at) { i++; continue; }
		const DeathHit hit = deathHits[i];
		deathHits.erase(deathHits.begin() + i);
		ApplyDeathHit(hit);
	}
	for(i32 i = 0; i < (i32)monsterFires.size(); ) {
		if(localTime < monsterFires[i].at) { i++; continue; }
		const MonsterFireEvent fire = monsterFires[i];
		monsterFires.erase(monsterFires.begin() + i);
		FireMonsterRemote(fire);
	}
	for(i32 i = 0; i < (i32)monsterChains.size(); ) {
		if(localTime < monsterChains[i].at) { i++; continue; }
		const MonsterChainEvent chain = monsterChains[i];
		monsterChains.erase(monsterChains.begin() + i);
		auto it = actorNpcMap.find(chain.npc);
		if(it == actorNpcMap.end()) continue;
		const Remote* remote = GetGameXmlContent().FindRemote(chain.remote);
		if(!remote) continue;
		const u32 seed = MONSTER_REMOTE_ID_BASE | (nextMonsterRemoteID++ & MONSTER_REMOTE_ID_MASK);
		replication->SendRemoteCreateToAll(it->second->UID, seed, chain.remote, chain.pos, it->second->rot.z, chain.pos, chain.target);
		if(remote->destroyCreateRemote != RemoteIdx::INVALID && !monsterChains.full()) monsterChains.push_back({ chain.npc, chain.target, TimeAdd(localTime, TimeMsToTime(remote->lifeTime * 1000.0)), remote->destroyCreateRemote, chain.pos });
	}
	for(i32 i = 0; i < (i32)monsterHits.size(); ) {
		if(localTime < monsterHits[i].at) { i++; continue; }
		const MonsterHit hit = monsterHits[i];
		monsterHits.erase(monsterHits.begin() + i);
		if(hit.ally) ApplyMonsterAllyStatus(hit.attacker, *hit.attack);
		else if(!hit.byRemotes) MonsterAttackHit(hit.attacker, hit.target, *hit.attack, hit.remoteID, hit.aimed ? &hit.aimPos : nullptr);
		if(!hit.ally && hit.attack->selfDie) {
			auto self = actorNpcMap.find(hit.attacker);
			if(self != actorNpcMap.end() && self->second->hp > 0) MonsterDie(*self->second, ActorUID::INVALID, CreatureIndex(0));
		}
	}
}

void World::FireMonsterRemote(const MonsterFireEvent& fire)
{
	auto it = actorNpcMap.find(fire.npc);
	if(it == actorNpcMap.end() || it->second->hp <= 0) return;
	const ActorNpc& npc = *it->second;
	const vec3 targetPos = MonsterFireTargetPos(npc, fire.target);
	replication->SendRemoteCreateToAll(npc.UID, fire.seed, (RemoteIdx)fire.fire->remote, npc.pos, npc.rot.z, vec3(vec2(targetPos), npc.pos.z), fire.target, fire.fire->node.c_str());
	const Remote* remote = GetGameXmlContent().FindRemote((RemoteIdx)fire.fire->remote);
	if(remote && fire.byRemotes) {
		const vec2 rel = vec2(targetPos) - vec2(npc.pos);
		const f32 len = glm::length(rel);
		SpawnMonsterRemote(npc, *remote, npc.pos, len > 1.0f ? rel / len : vec2(cosf(npc.rot.z), sinf(npc.rot.z)), fire.target, false);
		return;
	}
	if(remote && remote->maxSpeed > 0 && remote->destroyCreateRemote != RemoteIdx::INVALID && !monsterChains.full()) {
		const f32 travel = glm::length(vec2(targetPos) - vec2(npc.pos)) / remote->maxSpeed;
		monsterChains.push_back({ npc.UID, fire.target, TimeAdd(localTime, TimeMsToTime(MIN(travel, remote->lifeTime > 0 ? remote->lifeTime : travel) * 1000.0)), remote->destroyCreateRemote, targetPos });
	}
}

static bool RemoteHasEffect(const Remote& remote, i32 depth)
{
	if(remote.boundType != Remote::BoundType::E_BOUND_NONE && remote.damageGroup != Remote::DamageGroup::eNONE && (remote.attackMultiplier > 0 || !remote.statuses.empty() || !remote.friendlyStatuses.empty())) return true;
	if(depth <= 0) return false;
	const auto& content = GetGameXmlContent();
	foreach_const(r, remote.onDestroy) if(const Remote* child = content.FindRemote(r->idx)) if(RemoteHasEffect(*child, depth - 1)) return true;
	foreach_const(r, remote.onTime) if(const Remote* child = content.FindRemote(r->idx)) if(RemoteHasEffect(*child, depth - 1)) return true;
	return false;
}

bool World::MonsterAttackUsesRemotes(const GameXmlContent::MonsterAttack& attack) const
{
	static const i32 MONSTER_REMOTE_EFFECT_DEPTH = 3;
	if(!map || !map->ingameLevels) return false;
	const auto& content = GetGameXmlContent();
	foreach_const(f, attack.fires) if(const Remote* remote = content.FindRemote((RemoteIdx)f->remote)) if(RemoteHasEffect(*remote, MONSTER_REMOTE_EFFECT_DEPTH)) return true;
	return false;
}

void World::SpawnMonsterRemote(const ActorNpc& npc, const Remote& remote, const vec3& pos, const vec2& dir, ActorUID target, bool send)
{
	if(send) {
		const u32 seed = MONSTER_REMOTE_ID_BASE | (nextMonsterRemoteID & MONSTER_REMOTE_ID_MASK);
		nextMonsterRemoteID += MEASURED_MONSTER_REMOTE_SEED_STEP;
		replication->SendRemoteCreateToAll(npc.UID, seed, remote.ID, pos, atan2f(dir.y, dir.x), pos, target);
	}
	if(monsterRemotes.full()) return;
	MonsterRemote mr;
	mr.owner = npc.UID;
	mr.target = target;
	mr.remote = &remote;
	mr.pos = vec3(vec2(pos) + dir * remote.offsetY, pos.z);
	mr.dir = dir;
	mr.expire = TimeAdd(localTime, TimeMsToTime(MAX(remote.lifeTime, MONSTER_REMOTE_MIN_LIFE_SEC) * 1000.0));
	mr.nextTime = !remote.onTime.empty() && remote.timeEventDelay >= 0 ? TimeAdd(localTime, TimeMsToTime(MAX(remote.timeEventDelay, MONSTER_REMOTE_MIN_LIFE_SEC) * 1000.0)) : Time::ZERO;
	mr.activations = MAX(remote.activateCount, 1);
	mr.travelled = 0;
	mr.impacted = false;
	monsterRemotes.push_back(mr);
	if(remote.maxSpeed <= 0) {
		auto it = actorNpcMap.find(npc.UID);
		if(it != actorNpcMap.end()) MonsterRemoteApply(monsterRemotes.back(), *it->second);
	}
}

void World::MonsterRemoteApply(MonsterRemote& mr, ActorNpc& owner)
{
	const Remote& remote = *mr.remote;
	if(remote.boundType == Remote::BoundType::E_BOUND_NONE || remote.damageGroup == Remote::DamageGroup::eNONE || mr.activations <= 0) return;
	const i32 damage = (i32)(MonsterAttackPower(owner) * remote.attackMultiplier + 0.5f);
	const bool hitsEnemies = remote.damageGroup == Remote::DamageGroup::eENEMY || remote.damageGroup == Remote::DamageGroup::eALL;
	const bool hitsAllies = remote.damageGroup == Remote::DamageGroup::eFRIEND || remote.damageGroup == Remote::DamageGroup::eALL;
	const vec2 origin = vec2(mr.pos) + mr.dir * (remote.attachFireObject ? -RemoteBoundOffset(remote) : RemoteBoundOffset(remote));
	const bool sphere = remote.boundType == Remote::BoundType::E_BOUND_SPHERE || remote.angle > 0;
	auto inside = [&](const vec2& at, f32 r) {
		const vec2 rel = at - origin;
		const f32 dist = glm::length(rel);
		if(sphere) {
			if(dist > remote.boundSize[0] * 0.5f + r) return false;
			return remote.angle <= 0 || dist <= r || glm::dot(rel / dist, mr.dir) >= cosf(MIN(remote.angle * 0.5f, 180.0f) * PI / 180.0f);
		}
		const f32 length = remote.boundSize[1];
		const vec2 local = rel + mr.dir * (length * 0.5f);
		const f32 along = glm::dot(local, mr.dir);
		return along >= -r && along <= length + r && fabsf(glm::dot(local, vec2(-mr.dir.y, mr.dir.x))) <= remote.boundSize[0] * 0.5f + r;
	};
	auto ready = [&](ActorUID uid) {
		for(i32 i = 0; i < (i32)mr.hitUntil.size(); i++) {
			if(mr.hitUntil[i].first != uid) continue;
			if(localTime < mr.hitUntil[i].second) return false;
			mr.hitUntil.erase(mr.hitUntil.begin() + i);
			break;
		}
		if(!mr.hitUntil.full()) mr.hitUntil.push_back({ uid, TimeAdd(localTime, TimeMsToTime(MAX(remote.hitInvalidTime, MONSTER_REMOTE_MIN_LIFE_SEC) * 1000.0)) });
		return true;
	};
	auto applyStatuses = [&](const eastl::fixed_vector<Remote::AppliedStatus,4,false>& list, Player* p, ActorNpc* n) {
		foreach_const(st, list) {
			if(st->rate < 100 && (i32)(RandUint() % 100) >= st->rate) continue;
			bool active = false;
			if(p) { foreach_const(a, activeStatuses) if(a->targetIndex == p->index && a->statusID == st->statusID) { active = true; break; } }
			else { foreach_const(a, npcStatuses) if(a->npcUID == n->UID && a->statusID == st->statusID && localTime < a->until) { active = true; break; } }
			if(active) continue;
			if(p) ApplyStatusFromNpc(owner, *p, st->statusID);
			else ApplyStatusToNpcFromNpc(owner, *n, st->statusID);
		}
	};
	const u32 remoteID = MONSTER_REMOTE_ID_BASE | (nextMonsterRemoteID++ & MONSTER_REMOTE_ID_MASK);
	if(remote.vs & (1u << Remote::VS_PLAYER_CHARACTER)) {
		foreach(p, players) {
			if(mr.activations <= 0) break;
			if(p->dead || p->untargetable || p->removed || !p->body) continue;
			const bool enemy = FactionsOppose(owner.faction, (Faction)p->team, hostileMonsters) || MonsterNeutral(owner, hostileMonsters);
			const bool ally = !enemy && owner.faction == (Faction)p->team;
			if(!((enemy && hitsEnemies) || (ally && hitsAllies)) || !inside(vec2(p->body->GetWorldPos()), HitRadius(*p)) || !ready(p->Main().UID)) continue;
			if(enemy) {
				if(damage > 0) DamagePlayerFromNpc(owner, *p, damage, (i32)remote.ID, 0, remoteID);
				applyStatuses(remote.statuses, &*p, nullptr);
			}
			else applyStatuses(remote.friendlyStatuses, &*p, nullptr);
			mr.activations--;
		}
	}
	if(remote.vs & (1u << Remote::VS_NPC_MONSTER)) {
		foreach(o, actorNpcList) {
			if(mr.activations <= 0) break;
			if(&*o == &owner || o->hp <= 0 || o->removeAt != Time::ZERO || !o->monsterAi) continue;
			const bool enemy = FactionsOppose(owner.faction, o->faction, hostileMonsters);
			const bool ally = !enemy && o->faction == owner.faction;
			if(!((enemy && hitsEnemies) || (ally && hitsAllies)) || !inside(vec2(o->pos), NpcHitRadius(*o)) || !ready(o->UID)) continue;
			if(enemy) {
				if(damage > 0) DamageNpcFromNpc(owner, *o, damage, (i32)remote.ID, remoteID, 0);
				applyStatuses(remote.statuses, nullptr, &*o);
			}
			else applyStatuses(remote.friendlyStatuses, nullptr, &*o);
			mr.activations--;
		}
	}
}

void World::UpdateMonsterRemotes(f32 delta)
{
	const auto& content = GetGameXmlContent();
	for(i32 i = 0; i < (i32)monsterRemotes.size(); ) {
		auto ownerIt = actorNpcMap.find(monsterRemotes[i].owner);
		if(ownerIt == actorNpcMap.end()) { monsterRemotes.erase(monsterRemotes.begin() + i); continue; }
		ActorNpc& owner = *ownerIt->second;
		MonsterRemote& mr = monsterRemotes[i];
		const Remote& remote = *mr.remote;
		if(remote.attachFireObject) mr.pos = vec3(vec2(owner.pos) + mr.dir * remote.offsetY, owner.pos.z);
		bool expired = localTime >= mr.expire;
		if(remote.maxSpeed > 0 && !mr.impacted) {
			const vec3 goal = MonsterFireTargetPos(owner, mr.target);
			const bool homing = remote.behaviorType == Remote::BehaviorType::FOLLOW_TARGET || remote.behaviorType == Remote::BehaviorType::HOMING;
			const vec2 rel = vec2(goal) - vec2(mr.pos);
			const f32 dist = glm::length(rel);
			if(homing && dist > 1.0f) mr.dir = rel / dist;
			const f32 step = remote.maxSpeed * delta;
			if(homing && dist <= step) {
				mr.pos = vec3(vec2(goal), mr.pos.z);
				mr.impacted = true;
				expired = true;
			}
			else {
				mr.pos += vec3(mr.dir * step, 0);
				mr.travelled += step;
				if(remote.maxDistance > 0 && mr.travelled >= remote.maxDistance) expired = true;
			}
			MonsterRemoteApply(mr, owner);
		}
		else if(remote.lifeTime > MONSTER_REMOTE_MIN_LIFE_SEC) MonsterRemoteApply(mr, owner);
		if(mr.nextTime != Time::ZERO && localTime >= mr.nextTime) {
			const vec3 at = mr.pos;
			const vec2 dir = mr.dir;
			const ActorUID target = mr.target;
			mr.nextTime = remote.timeEventRecycle ? TimeAdd(mr.nextTime, TimeMsToTime(MAX(remote.timeEventDelay, MONSTER_REMOTE_MIN_LIFE_SEC) * 1000.0)) : Time::ZERO;
			const eastl::fixed_vector<Remote::SpawnRule,4,false> rules = remote.onTime;
			foreach_const(r, rules) if(const Remote* child = content.FindRemote(r->idx)) SpawnMonsterRemote(owner, *child, at, dir, target, true);
		}
		if(expired) {
			const vec3 at = monsterRemotes[i].pos;
			const vec2 dir = monsterRemotes[i].dir;
			const ActorUID target = monsterRemotes[i].target;
			const eastl::fixed_vector<Remote::SpawnRule,4,false> rules = monsterRemotes[i].remote->onDestroy;
			monsterRemotes.erase(monsterRemotes.begin() + i);
			foreach_const(r, rules) if(const Remote* child = content.FindRemote(r->idx)) SpawnMonsterRemote(owner, *child, at, dir, target, true);
			continue;
		}
		i++;
	}
}

vec3 World::MonsterFireTargetPos(const ActorNpc& npc, ActorUID target) const
{
	vec3 targetPos = npc.pos + vec3(cosf(npc.rot.z), sinf(npc.rot.z), 0.0f) * HYP_MONSTER_FIRE_DEFAULT_REACH;
	auto targetNpc = actorNpcMap.find(target);
	if(targetNpc != actorNpcMap.end()) targetPos = targetNpc->second->pos;
	else if(const ActorMaster* master = FindMasterActor(target)) if(master->parent && master->parent->body) targetPos = master->parent->body->GetWorldPos();
	return targetPos;
}

f32 World::MonsterChainTravel(const ActorNpc& npc, const GameXmlContent::MonsterAttack& attack, ActorUID target, const GameXmlContent::MonsterFire** fireOut) const
{
	static const i32 MONSTER_CHAIN_MAX_DEPTH = 4;
	const auto& content = GetGameXmlContent();
	foreach_const(f, attack.fires) {
		const Remote* remote = content.FindRemote((RemoteIdx)f->remote);
		if(!remote || remote->maxSpeed <= 0) continue;
		RemoteIdx link = remote->destroyCreateRemote;
		for(i32 depth = 0; depth < MONSTER_CHAIN_MAX_DEPTH && link != RemoteIdx::INVALID; depth++) {
			if((i32)link == attack.hitRemote) {
				*fireOut = &*f;
				const f32 travel = glm::length(vec2(MonsterFireTargetPos(npc, target)) - vec2(npc.pos)) / remote->maxSpeed;
				return remote->lifeTime > 0 ? MIN(travel, remote->lifeTime) : travel;
			}
			const Remote* next = content.FindRemote(link);
			link = next ? next->destroyCreateRemote : RemoteIdx::INVALID;
		}
	}
	return -1.0f;
}

void World::TowerCallForHelp(const Player& attacker, const vec3& victimPos, Faction victimTeam, const ActorNpc* hitTower)
{
	if(!map || !map->ingameLevels || (Faction)attacker.team == victimTeam || !attacker.body) return;
	const vec3 at = attacker.body->GetWorldPos();
	foreach(o, actorNpcList) {
		ActorNpc& tower = *o;
		if(!tower.monsterAi || tower.hp <= 0 || tower.removeAt != Time::ZERO || tower.faction != victimTeam || !IsStructure(tower)) continue;
		const GameXmlContent::MonsterAttack* attack = tower.monsterAi->byTarget[GameXmlContent::MonsterAttack::MASTER];
		if(!attack) continue;
		if(glm::length(vec2(at) - vec2(tower.pos)) > tower.monsterAi->perception) continue;
		if(tower.monsterTarget == attacker.Main().UID || FindMasterActor(tower.monsterTarget)) continue;
		tower.monsterTarget = attacker.Main().UID;
		replication->SendAiSetTargetToAll(tower.UID, tower.monsterTarget);
		LOG("Monster :: tower creature=%d (local %d) switches to %ls (%s)", (i32)tower.docID, tower.localID, attacker.name.data(), &tower == hitTower ? "hit the tower" : "hit an allied master");
	}
}

void World::UpdateMonsterDash(ActorNpc& npc)
{
	if(npc.dashEnd == Time::ZERO || localTime < npc.dashStart) return;
	static const f32 MONSTER_DASH_RAY_HEIGHT = 100.0f;
	const f32 total = (f32)TimeDiffSec(TimeDiff(npc.dashStart, npc.dashEnd));
	const f32 t = total > 0 ? MIN((f32)TimeDiffSec(TimeDiff(npc.dashStart, localTime)) / total, 1.0f) : 1.0f;
	const vec2 next = npc.dashFrom + npc.dashDir * npc.dashDist * t;
	const vec2 here = vec2(npc.pos);
	f32 hit = 0;
	const f32 radius = npc.monsterAi ? npc.monsterAi->radius : 0.0f;
	const vec2 step = next - here;
	const f32 len = glm::length(step);
	bool blocked = false;
	if(len > 1.0f && physics.RaycastStatic(vec3(here, npc.pos.z + MONSTER_DASH_RAY_HEIGHT), vec3(next + step / len * radius, npc.pos.z + MONSTER_DASH_RAY_HEIGHT), &hit)) {
		npc.pos = vec3(here + step / len * MAX(hit - radius, 0.0f), npc.pos.z);
		blocked = true;
	}
	else npc.pos = vec3(next, npc.pos.z);
	if(t >= 1.0f || blocked) {
		npc.dashEnd = Time::ZERO;
		LOG("Monster :: creature=%d (local %d) dash ends at (%.0f, %.0f)%s", (i32)npc.docID, npc.localID, npc.pos.x, npc.pos.y, blocked ? " on a wall" : "");
	}
}

void World::StartMonsterAttack(ActorNpc& npc, const GameXmlContent::MonsterAi& ai, const GameXmlContent::MonsterAttack* attack, const vec3* targetPos, f32 dist, f32 yaw, f32 run)
{
	npc.rot.z = yaw;
	npc.actionState = attack->state;
	if(ai.masterMark != 0 && attack->target == GameXmlContent::MonsterAttack::MASTER) {
		ActorMaster* marked = FindMasterActor(npc.monsterTarget);
		if(marked && marked->parent) ApplyStatusFromNpc(npc, *marked->parent, ai.masterMark);
	}
	npc.monsterSyncYaw = yaw;
	npc.monsterLastTurn = localTime;
	replication->SendNpcTurnToAll(npc.UID, yaw);
	replication->SendMonsterStateToAll(npc.UID, attack->state, npc.pos, run);
	if(attack->anim > 0) {
		npc.stateEndAt = TimeAdd(localTime, TimeMsToTime(attack->anim * 1000.0));
		if(npc.monsterHoldUntil < npc.stateEndAt) npc.monsterHoldUntil = npc.stateEndAt;
	}
	foreach_const(st, attack->selfStatuses) ApplyMonsterStatus(npc, *st, true);
	if(attack->weakTime > 0) {
		npc.monsterWeakFrom = TimeAdd(localTime, TimeMsToTime(attack->weakAt * 1000.0));
		npc.monsterWeakUntil = TimeAdd(localTime, TimeMsToTime((attack->weakAt + attack->weakTime) * 1000.0));
		npc.monsterWeakStatus = attack->weakStatus;
		npc.monsterWeakHit = false;
	}
	if(attack->evade > 0) npc.monsterEvadeUntil = TimeAdd(localTime, TimeMsToTime(attack->evade * 1000.0));
	if(attack->dashDist > 0 && !attack->dashPassAnims.empty()) {
		const auto& content = GetGameXmlContent();
		f32 start = 0;
		for(i32 p = 0; p + 1 < (i32)attack->dashPassAnims.size(); p++) start += content.GetAniSeqTime(ai.className.c_str(), attack->dashPassAnims[p]);
		const f32 pass = content.GetAniSeqTime(ai.className.c_str(), attack->dashPassAnims.back());
		const f32 duration = attack->dashTime > 0 ? attack->dashTime : pass;
		if(duration > 0) {
			npc.dashFrom = vec2(npc.pos);
			npc.dashDir = vec2(cosf(yaw), sinf(yaw));
			npc.dashDist = attack->dashDist;
			npc.dashStart = TimeAdd(localTime, TimeMsToTime(start * 1000.0));
			npc.dashEnd = TimeAdd(npc.dashStart, TimeMsToTime(duration * 1000.0));
			LOG("Monster :: creature=%d (local %d) dashes %.0f in %.2fs from %.2fs", (i32)npc.docID, npc.localID, attack->dashDist, duration, start);
		}
	}
	const f32 travel = attack->speed > 0 ? dist / attack->speed : 0.0f;
	const bool fired = hostileMonsters && !attack->fires.empty();
	const bool visual = fired || (!attack->fires.empty() && map && map->ingameLevels);
	static const u32 MEASURED_MONSTER_FIRE_SEED_STEP = 4;
	const u32 seed = MONSTER_REMOTE_ID_BASE | (nextMonsterRemoteID & MONSTER_REMOTE_ID_MASK);
	nextMonsterRemoteID += visual ? (u32)attack->fires.size() * MEASURED_MONSTER_FIRE_SEED_STEP : 1;
	const u32 remoteID = fired ? seed + 1 : seed;
	eastl::array<f32,4> passStart = {{ 0, 0, 0, 0 }};
	for(i32 p = 1; p < 4 && p - 1 < (i32)attack->passes.size(); p++) passStart[p] = passStart[p - 1] + GetGameXmlContent().GetAniSeqTime(ai.className.c_str(), attack->passes[p - 1].first) * (f32)attack->passes[p - 1].second;
	const f32 hitDelay = attack->hitAt >= 0 ? attack->hitAt : (fired ? passStart[attack->fires[0].pass] + attack->fires[0].at : attack->delay);
	const bool byRemotes = visual && MonsterAttackUsesRemotes(*attack);
	for(i32 i = 0; i < (i32)monsterRemotes.size(); ) {
		const MonsterRemote& mr = monsterRemotes[i];
		if(mr.owner == npc.UID && mr.remote->attachFireObject && !mr.remote->onTime.empty()) monsterRemotes.erase(monsterRemotes.begin() + i);
		else i++;
	}
	if(visual) {
		u32 fireSeed = seed;
		foreach_const(f, attack->fires) {
			monsterFires.push_back({ npc.UID, npc.monsterTarget, TimeAdd(localTime, TimeMsToTime((passStart[f->pass] + f->at) * 1000.0)), &*f, fireSeed, byRemotes });
			fireSeed += MEASURED_MONSTER_FIRE_SEED_STEP;
		}
	}
	f32 impact = hitDelay + travel;
	const GameXmlContent::MonsterFire* chainFire = nullptr;
	if(visual && attack->speed <= 0 && !byRemotes) {
		const f32 chainTravel = MonsterChainTravel(npc, *attack, npc.monsterTarget, &chainFire);
		if(chainFire) impact = passStart[chainFire->pass] + chainFire->at + chainTravel;
	}
	for(i32 h = 0; !attack->noDamage && h < (i32)attack->hits && !monsterHits.full(); h++) {
		monsterHits.push_back({ npc.UID, npc.monsterTarget, TimeAdd(localTime, TimeMsToTime((impact + h * attack->interval) * 1000.0)), attack, remoteID, false, targetPos ? *targetPos : npc.pos, attack->speed > 0 && targetPos != nullptr, byRemotes });
	}
	if(attack->allyStatus != 0 && !monsterHits.full()) monsterHits.push_back({ npc.UID, npc.UID, TimeAdd(localTime, TimeMsToTime(attack->allyDelay * 1000.0)), attack, 0, true });
}

bool World::UpdateMonsterScript(ActorNpc& npc, const GameXmlContent::MonsterAi& ai, const vec3* targetPos, f32 delta)
{
	if(npc.monsterScriptStart != Time::ZERO && ai.scriptPause != ActionStateID::INVALID && npc.actionState == ai.scriptPause && localTime < npc.monsterHoldUntil) npc.monsterScriptStart = TimeAdd(npc.monsterScriptStart, TimeMsToTime(delta * 1000.0));
	if(localTime < npc.monsterHoldUntil || !targetPos) return true;
	const vec2 rel = vec2(*targetPos) - vec2(npc.pos);
	const f32 dist = glm::length(rel);
	const f32 yaw = dist > 1.0f ? atan2f(rel.y, rel.x) : npc.rot.z;
	if(npc.monsterScriptStart == Time::ZERO) {
		npc.monsterScriptStart = TimeAdd(localTime, TimeMsToTime(ai.scriptStart * 1000.0));
		npc.monsterScriptStep = 0;
		LOG("Monster :: creature=%d (local %d) starts its script in %.1fs", (i32)npc.docID, npc.localID, ai.scriptStart);
	}
	if(YawGap(yaw, npc.monsterSyncYaw) > HYP_MONSTER_TURN_RESYNC_RAD && TimeDurationMs(npc.monsterLastTurn, localTime) >= HYP_MONSTER_TURN_RESYNC_MS) {
		npc.rot.z = yaw;
		npc.monsterSyncYaw = yaw;
		npc.monsterLastTurn = localTime;
		replication->SendNpcTurnToAll(npc.UID, yaw);
	}
	if(localTime < npc.monsterScriptStart) return true;
	f32 t = (f32)TimeDiffSec(TimeDiff(npc.monsterScriptStart, localTime));
	if(t >= ai.scriptPeriod) {
		npc.monsterScriptStart = TimeAdd(npc.monsterScriptStart, TimeMsToTime(ai.scriptPeriod * 1000.0));
		npc.monsterScriptStep = 0;
		t -= ai.scriptPeriod;
	}
	const f32 hpRatio = npc.hpMax > 0 ? (f32)npc.hp / (f32)npc.hpMax : 1.0f;
	while(npc.monsterScriptStep < ai.script.size() && t >= ai.script[npc.monsterScriptStep].t) {
		const GameXmlContent::MonsterAi::ScriptStep& step = ai.script[npc.monsterScriptStep++];
		if(hpRatio < step.hpMin || hpRatio >= step.hpMax) continue;
		if(step.aiTrigger >= 0) FireAITrigger(step.aiTrigger, -1);
		if(step.attacks.empty()) continue;
		u32 choice = 0;
		if(step.pick == 1) choice = RandUint() % step.attacks.size();
		else if(step.pick == 2) choice = npc.monsterScriptAlt++ % step.attacks.size();
		const GameXmlContent::MonsterAttack& attack = ai.attacks[step.attacks[choice]];
		LOG("Monster :: creature=%d (local %d) script step at %.1fs plays %s", (i32)npc.docID, npc.localID, t, ActionStateToString(attack.state));
		StartMonsterAttack(npc, ai, &attack, targetPos, dist, yaw, 0.0f);
		break;
	}
	return true;
}

bool World::UpdateMonsterAllyBuff(ActorNpc& npc, const GameXmlContent::MonsterAi& ai)
{
	if(npc.faction != Faction::RED && npc.faction != Faction::BLUE) return false;
	for(i32 s = 0; s < (i32)ai.attacks.size() && s < (i32)npc.monsterSkillReady.size(); s++) {
		const GameXmlContent::MonsterAttack& buff = ai.attacks[s];
		if(buff.target != GameXmlContent::MonsterAttack::ALLY) continue;
		if(npc.monsterSkillReady[s] == Time::ZERO) npc.monsterSkillReady[s] = TimeAdd(localTime, TimeMsToTime((buff.firstAt >= 0 ? buff.firstAt : buff.cooldown) * 1000.0));
		if(localTime < npc.monsterSkillReady[s]) continue;
		Player* ally = nullptr;
		f32 best = MAX(buff.range, ai.perception);
		foreach(p, players) {
			if(p->dead || p->untargetable || p->removed || !p->body || (Faction)p->team != npc.faction) continue;
			const f32 d = glm::length(vec2(p->body->GetWorldPos()) - vec2(npc.pos));
			if(d <= best) { best = d; ally = &*p; }
		}
		if(!ally) continue;
		npc.monsterSkillReady[s] = TimeAdd(localTime, TimeMsToTime(buff.cooldown * 1000.0));
		const vec2 rel = vec2(ally->body->GetWorldPos()) - vec2(npc.pos);
		const f32 yaw = glm::length(rel) > 1.0f ? atan2f(rel.y, rel.x) : npc.rot.z;
		npc.rot.z = yaw;
		npc.monsterSyncYaw = yaw;
		npc.monsterLastTurn = localTime;
		npc.actionState = buff.state;
		replication->SendAiSetTargetToAll(npc.UID, ally->Main().UID);
		replication->SendNpcTurnToAll(npc.UID, yaw);
		replication->SendMonsterStateToAll(npc.UID, buff.state, npc.pos, 0.0f);
		replication->SendAiSetTargetToAll(npc.UID, npc.monsterTarget);
		if(buff.anim > 0) {
			npc.stateEndAt = TimeAdd(localTime, TimeMsToTime(buff.anim * 1000.0));
			if(npc.monsterHoldUntil < npc.stateEndAt) npc.monsterHoldUntil = npc.stateEndAt;
		}
		foreach_const(st, buff.allyStatuses) ApplyStatusFromNpc(npc, *ally, *st);
		foreach_const(st, buff.selfStatuses) ApplyMonsterStatus(npc, *st, true);
		LOG("Monster :: creature=%d (local %d) buffs ally %ls with %s", (i32)npc.docID, npc.localID, ally->name.data(), ActionStateToString(buff.state));
		return true;
	}
	return false;
}

bool World::UpdateMonsterSeek(ActorNpc& npc, const GameXmlContent::MonsterAi& ai, f32 speed, f32 delta)
{
	if(localTime < npc.monsterHoldUntil) return true;
	const ActorNpc* goal = nullptr;
	f32 best = FLT_MAX;
	foreach_const(o, actorNpcList) {
		if(&*o == &npc || o->hp <= 0 || o->removeAt != Time::ZERO) continue;
		if(eastl::find(ai.seekDocs.begin(), ai.seekDocs.end(), (i32)o->docID) == ai.seekDocs.end()) continue;
		const f32 d = glm::length(vec2(o->pos) - vec2(npc.pos));
		if(d < best) { best = d; goal = &*o; }
	}
	if(!goal) return true;
	const vec2 rel = vec2(goal->pos) - vec2(npc.pos);
	const f32 yaw = best > 1.0f ? atan2f(rel.y, rel.x) : npc.rot.z;
	if(best <= ai.seekReach) {
		const GameXmlContent::MonsterAttack* boom = ai.byTarget[GameXmlContent::MonsterAttack::MASTER];
		LOG("Monster :: creature=%d (local %d) reached creature=%d", (i32)npc.docID, npc.localID, (i32)goal->docID);
		if(npc.monsterWalking) {
			npc.monsterWalking = false;
			replication->SendNpcLegStopToAll(npc.UID, npc.pos, yaw);
		}
		if(boom) StartMonsterAttack(npc, ai, boom, nullptr, 0.0f, yaw, 0.0f);
		else MonsterDie(npc, ActorUID::INVALID, CreatureIndex(0));
		return true;
	}
	if(speed <= 0) return true;
	npc.pos = vec3(vec2(npc.pos) + rel / best * MIN(speed * delta, best - ai.seekReach), npc.pos.z);
	npc.rot.z = yaw;
	if(!npc.monsterWalking || localTime >= npc.monsterNextSync) {
		npc.monsterWalking = true;
		npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
		replication->SendMonsterMoveToAll(npc.UID, npc.pos, vec3(vec2(goal->pos) - rel / best * ai.seekReach, npc.pos.z), yaw, speed);
	}
	return true;
}

void World::UpdateMonsterAi(f32 delta)
{
	UpdateMonsterHits();
	UpdateMonsterRemotes(delta);
	if(frozen) return;
	foreach(it, actorNpcList) {
		ActorNpc& npc = *it;
		if(npc.selfDestructAt != Time::ZERO && localTime >= npc.selfDestructAt && npc.hp > 0 && npc.removeAt == Time::ZERO) {
			MonsterDie(npc, ActorUID::INVALID, CreatureIndex(0));
			continue;
		}
		if(!npc.monsterAi || npc.hp <= 0 || npc.removeAt != Time::ZERO) continue;
		if(combatLocked && hostileMonsters) continue;
		if(npc.despawnAt != Time::ZERO && localTime >= npc.despawnAt) continue;
		const GameXmlContent::MonsterAi& ai = *npc.monsterAi;
		const f32 walk = ai.walk * npc.speedMul;
		const f32 run = ai.run * npc.speedMul;
		UpdateNpcDefeat(npc);
		UpdateNpcVictory(npc);
		UpdateMonsterDash(npc);
		if(!npc.monsterStarted) {
			if(localTime < TimeAdd(npc.spawnedAt, TimeMsToTime(MAX(MONSTER_START_DELAY_MS, (f64)ai.spawnTime * 1000.0)))) continue;
			npc.monsterStarted = true;
			if(map && map->ingameLevels) foreach_const(st, ai.spawnStatuses) ApplyMonsterStatus(npc, *st, true);
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			if(ai.idle != ActionStateID::INVALID) {
				npc.actionState = ai.idle;
				replication->SendMonsterStateToAll(npc.UID, ai.idle, npc.pos, 0.0f);
			}
			if(npc.path && walk > 0) {
				npc.monsterWalking = true;
				replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, walk);
			}
		}

		if(hostileMonsters && !ai.seekDocs.empty() && UpdateMonsterSeek(npc, ai, walk, delta)) continue;
		const vec3* targetPos = nullptr;
		const GameXmlContent::MonsterAttack* attack = nullptr;
		vec3 targetAt;
		bool targetLost = false;
		if(npc.monsterTarget != ActorUID::INVALID) {
			auto t = actorNpcMap.find(npc.monsterTarget);
			if(t != actorNpcMap.end() && MonsterHostile(npc, *t->second)) {
				targetAt = t->second->pos;
				targetPos = &targetAt;
				attack = ai.byTarget[MonsterTargetKind(*t->second->monsterAi)];
			}
			else if(ActorMaster* m = FindMasterActor(npc.monsterTarget)) {
				if(m->parent && MonsterHostile(npc, *m->parent)) {
					targetAt = m->parent->body->GetWorldPos();
					targetPos = &targetAt;
					attack = ai.byTarget[GameXmlContent::MonsterAttack::MASTER];
				}
			}
			if(targetPos && glm::length(vec2(*targetPos) - vec2(npc.pos)) > MAX(ai.lost, ai.perception)) targetPos = nullptr;
			if(!targetPos || !attack) {
				targetPos = nullptr;
				npc.monsterTarget = ActorUID::INVALID;
				npc.monsterNextSearch = localTime;
				targetLost = true;
			}
		}
		const bool neutral = MonsterNeutral(npc, hostileMonsters);
		const bool titanMode = map && map->ingameLevels;
		if(neutral && targetLost) {
			targetLost = false;
			if(titanMode) replication->SendBattleStateToAll(npc.UID, false, walk);
			replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
		}
		if(neutral && !titanMode && targetPos && glm::length(vec2(npc.pos) - vec2(npc.home)) > MAX(ai.lost, ai.perception)) {
			targetPos = nullptr;
			npc.monsterTarget = ActorUID::INVALID;
			npc.monsterReturning = true;
			replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
		}
		if(neutral && !targetPos) {
			const vec2 rel = vec2(npc.home) - vec2(npc.pos);
			const f32 dist = glm::length(rel);
			if(dist > MONSTER_WAYPOINT_REACHED && run > 0) {
				const f32 yaw = atan2f(rel.y, rel.x);
				const f32 back = run * ai.returnSpeedRate;
				npc.pos = vec3(vec2(npc.pos) + rel / dist * MIN(back * delta, dist), npc.pos.z);
				npc.rot.z = yaw;
				if(!npc.monsterReturning && titanMode) ApplyMonsterStatus(npc, MEASURED_GOT_MOB_RETURN_SUPERARMOR, true);
				if(!npc.monsterReturning || localTime >= npc.monsterNextSync) {
					npc.monsterReturning = true;
					npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
					replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.home, yaw, back);
				}
			}
			else if(npc.monsterReturning) {
				npc.monsterReturning = false;
				if(titanMode) ApplyMonsterStatus(npc, MEASURED_GOT_MOB_RETURN_SUPERARMOR, false);
				npc.monsterChasing = false;
				npc.hp = npc.hpMax;
				if(ai.idle != ActionStateID::INVALID) {
					npc.monsterAwake = false;
					npc.actionState = ai.idle;
					replication->SendMonsterStateToAll(npc.UID, ai.idle, npc.pos, 0.0f);
				}
				replication->SendHpToAll(npc.UID, npc.hp, npc.hpMax);
				npc.monsterSkillReady.fill(Time::ZERO);
				LOG("Monster :: neutral creature=%d (local %d) back home, healed", (i32)npc.docID, npc.localID);
				replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.pos, npc.rot.z, 0.0f);
			}
			continue;
		}
		static const u32 minionTag = StatusTypeKey("Minion");
		static const f64 MEASURED_MINION_RETARGET_MS = 1000.0;
		static const f64 MEASURED_MINION_MASTER_CHASE_SEC = 5.0;
		const bool targetIsMaster = targetPos && FindMasterActor(npc.monsterTarget) != nullptr;
		if(!targetIsMaster) npc.monsterMasterSince = Time::ZERO;
		else if(npc.monsterMasterSince == Time::ZERO) npc.monsterMasterSince = localTime;
		if(titanMode && ai.tagKey == minionTag && targetIsMaster && localTime >= npc.monsterRetargetAt) {
			npc.monsterRetargetAt = TimeAdd(localTime, TimeMsToTime(MEASURED_MINION_RETARGET_MS));
			f32 bestOther = ai.perception;
			const ActorNpc* other = nullptr;
			const GameXmlContent::MonsterAttack* otherAttack = nullptr;
			foreach_const(o, actorNpcList) {
				if(!MonsterHostile(npc, *o)) continue;
				const GameXmlContent::MonsterAttack* a = ai.byTarget[MonsterTargetKind(*o->monsterAi)];
				if(!a) continue;
				const f32 d = glm::length(vec2(o->pos) - vec2(npc.pos)) - o->monsterAi->radius;
				if(d < bestOther) { bestOther = d; other = &*o; otherAttack = a; }
			}
			const bool tooLong = TimeDiffSec(TimeDiff(npc.monsterMasterSince, localTime)) >= MEASURED_MINION_MASTER_CHASE_SEC;
			if(other || tooLong) {
				LOG("Monster :: creature=%d (local %d) releases its master target (%s)", (i32)npc.docID, npc.localID, other ? "closer minion or structure" : "chase too long");
				npc.monsterMasterSince = Time::ZERO;
				if(other) {
					npc.monsterTarget = other->UID;
					targetAt = other->pos;
					targetPos = &targetAt;
					attack = otherAttack;
					replication->SendAiSetTargetToAll(npc.UID, other->UID);
				}
				else {
					npc.monsterTarget = ActorUID::INVALID;
					targetPos = nullptr;
					npc.monsterNextSearch = TimeAdd(localTime, TimeMsToTime(MEASURED_MINION_RETARGET_MS));
					replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
				}
			}
		}
		if(!targetPos && ai.perception > 0 && localTime >= npc.monsterNextSearch) {
			npc.monsterNextSearch = TimeAdd(localTime, TimeMsToTime(MONSTER_SEARCH_MS));
			f32 best = ai.perception;
			ActorUID chosen = ActorUID::INVALID;
			const bool mastersLast = titanMode && IsStructure(npc);
			if(ai.byTarget[GameXmlContent::MonsterAttack::MASTER] && !mastersLast) {
				foreach(p, players) {
					if(!MonsterHostile(npc, *p)) continue;
					const f32 d = glm::length(vec2(p->body->GetWorldPos()) - vec2(npc.pos));
					if(d < best) { best = d; chosen = p->Main().UID; targetAt = p->body->GetWorldPos(); attack = ai.byTarget[GameXmlContent::MonsterAttack::MASTER]; }
				}
			}
			foreach_const(o, actorNpcList) {
				if(!MonsterHostile(npc, *o)) continue;
				const GameXmlContent::MonsterAttack* a = ai.byTarget[MonsterTargetKind(*o->monsterAi)];
				if(!a) continue;
				const f32 d = glm::length(vec2(o->pos) - vec2(npc.pos)) - o->monsterAi->radius;
				if(d < best) { best = d; chosen = o->UID; targetAt = o->pos; attack = a; }
			}
			if(mastersLast && chosen == ActorUID::INVALID && ai.byTarget[GameXmlContent::MonsterAttack::MASTER]) {
				foreach(p, players) {
					if(!MonsterHostile(npc, *p)) continue;
					const f32 d = glm::length(vec2(p->body->GetWorldPos()) - vec2(npc.pos));
					if(d < best) { best = d; chosen = p->Main().UID; targetAt = p->body->GetWorldPos(); attack = ai.byTarget[GameXmlContent::MonsterAttack::MASTER]; }
				}
			}
			if(chosen != ActorUID::INVALID) {
				LOG("Monster :: creature=%d (local %d) at (%.0f, %.0f) targets %u at %.0f", (i32)npc.docID, npc.localID, npc.pos.x, npc.pos.y, (u32)chosen, best);
				npc.monsterTarget = chosen;
				targetPos = &targetAt;
				targetLost = false;
				if(hostileMonsters || (map && map->ingameLevels)) replication->SendBattleStateToAll(npc.UID, true, run);
				replication->SendAiSetTargetToAll(npc.UID, chosen);
			}
		}
		if(targetLost) {
			replication->SendAiSetTargetToAll(npc.UID, ActorUID::INVALID);
			if(hostileMonsters || (map && map->ingameLevels)) replication->SendBattleStateToAll(npc.UID, false, walk);
		}

		if(targetPos && attack && ai.awake != ActionStateID::INVALID && !npc.monsterAwake) {
			npc.monsterAwake = true;
			npc.actionState = ai.awake;
			replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, 0.0f);
			replication->SendMonsterStateToAll(npc.UID, ai.awake, npc.pos, 0.0f);
			replication->SendAiSetTargetToAll(npc.UID, npc.monsterTarget);
			npc.monsterHoldUntil = TimeAdd(localTime, TimeMsToTime(ai.awakeTime * 1000.0));
			LOG("Monster :: creature=%d (local %d) wakes up", (i32)npc.docID, npc.localID);
			continue;
		}
		if(npc.monsterWeakStatus != 0 && npc.monsterWeakUntil != Time::ZERO && localTime >= npc.monsterWeakFrom) {
			ApplyMonsterStatus(npc, npc.monsterWeakStatus, true);
			npc.monsterWeakStatus = 0;
		}
		if(npc.monsterWeakUntil != Time::ZERO && localTime >= npc.monsterWeakUntil) {
			npc.monsterWeakUntil = Time::ZERO;
			if(npc.monsterWeakHit && ai.groggyChance > 0 && (f32)(RandUint() % 10000) < ai.groggyChance * 10000.0f) {
				LOG("Monster :: creature=%d (local %d) is groggy", (i32)npc.docID, npc.localID);
				npc.actionState = ActionStateID::DEFEAT_GROGGY_BEHAVIORSTATE;
				replication->SendMonsterStateToAll(npc.UID, ActionStateID::DEFEAT_GROGGY_BEHAVIORSTATE, npc.pos, 0.0f);
				foreach_const(st, ai.groggyStatuses) ApplyMonsterStatus(npc, *st, true);
				npc.monsterHoldUntil = TimeAdd(localTime, TimeMsToTime(ai.groggyTime * 1000.0));
			}
		}
		if(hostileMonsters && ai.scriptPeriod > 0 && !ai.script.empty() && UpdateMonsterScript(npc, ai, targetPos, delta)) continue;
		if(localTime < npc.monsterHoldUntil) continue;
		if(UpdateMonsterAllyBuff(npc, ai)) continue;
		if(targetPos && attack) {
			const vec2 rel = vec2(*targetPos) - vec2(npc.pos);
			const f32 dist = glm::length(rel);
			const f32 yaw = dist > 1.0f ? atan2f(rel.y, rel.x) : npc.rot.z;
			const f32 reach = attack->range;
			if(dist > reach + MONSTER_REACH_TOLERANCE) {
				if(localTime >= npc.monsterNextAttack) {
					const f32 hpNow = npc.hpMax > 0 ? (f32)npc.hp / (f32)npc.hpMax : 1.0f;
					i32 farIndex = -1;
					for(i32 s = 0; s < (i32)ai.attacks.size() && s < (i32)npc.monsterSkillReady.size(); s++) {
						const GameXmlContent::MonsterAttack& special = ai.attacks[s];
						if(!special.special || special.minRange <= 0 || special.target != attack->target) continue;
						if(hpNow < special.hpMin || hpNow > special.hpMax || dist < special.minRange || dist > special.range) continue;
						if(npc.monsterSkillReady[s] == Time::ZERO) npc.monsterSkillReady[s] = TimeAdd(localTime, TimeMsToTime((special.firstAt >= 0 ? special.firstAt : special.cooldown) * 1000.0));
						if(localTime < npc.monsterSkillReady[s]) continue;
						farIndex = s;
						break;
					}
					if(farIndex >= 0) {
						const GameXmlContent::MonsterAttack& farAttack = ai.attacks[farIndex];
						npc.monsterSkillReady[farIndex] = TimeAdd(localTime, TimeMsToTime(farAttack.cooldown * 1000.0));
						npc.monsterNextAttack = TimeAdd(localTime, TimeMsToTime(attack->cooldown * 1000.0));
						if(npc.monsterChasing || npc.monsterWalking) {
							npc.monsterChasing = false;
							if(hostileMonsters) replication->SendNpcLegStopToAll(npc.UID, npc.pos, yaw);
							else replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.pos, yaw, 0.0f);
						}
						npc.monsterWalking = false;
						LOG("Monster :: creature=%d (local %d) uses %s at range %.0f", (i32)npc.docID, npc.localID, ActionStateToString(farAttack.state), dist);
						StartMonsterAttack(npc, ai, &farAttack, targetPos, dist, yaw, run);
						continue;
					}
				}
				if(run <= 0) continue;
				npc.monsterWalking = false;
				const f32 step = MIN(run * delta, dist - reach);
				npc.pos = vec3(vec2(npc.pos) + rel / dist * step, npc.pos.z);
				npc.rot.z = yaw;
				const bool turned = YawGap(yaw, npc.monsterSyncYaw) > HYP_MONSTER_TURN_RESYNC_RAD && TimeDurationMs(npc.monsterLastTurn, localTime) >= HYP_MONSTER_TURN_RESYNC_MS;
				if(!npc.monsterChasing || localTime >= npc.monsterNextSync || turned) {
					npc.monsterChasing = true;
					npc.monsterSyncYaw = yaw;
					npc.monsterLastTurn = localTime;
					npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
					replication->SendMonsterMoveToAll(npc.UID, npc.pos, vec3(vec2(*targetPos) - rel / dist * reach, npc.pos.z), yaw, run);
				}
				continue;
			}
			if(npc.monsterChasing || npc.monsterWalking) {
				npc.monsterChasing = false;
				if(hostileMonsters) replication->SendNpcLegStopToAll(npc.UID, npc.pos, yaw);
				else replication->SendMonsterMoveToAll(npc.UID, npc.pos, npc.pos, yaw, 0.0f);
			}
			npc.monsterWalking = false;
			if(localTime < npc.monsterNextAttack) {
				if(YawGap(yaw, npc.monsterSyncYaw) > HYP_MONSTER_TURN_RESYNC_RAD && TimeDurationMs(npc.monsterLastTurn, localTime) >= HYP_MONSTER_TURN_RESYNC_MS) {
					npc.rot.z = yaw;
					npc.monsterSyncYaw = yaw;
					npc.monsterLastTurn = localTime;
					replication->SendNpcTurnToAll(npc.UID, yaw);
				}
				continue;
			}
			npc.monsterNextAttack = TimeAdd(localTime, TimeMsToTime(attack->cooldown * 1000.0));
			eastl::fixed_vector<i32,16,false> ready;
			f32 totalShare = attack->share;
			const f32 hpRatio = npc.hpMax > 0 ? (f32)npc.hp / (f32)npc.hpMax : 1.0f;
			for(i32 s = 0; s < (i32)ai.attacks.size() && s < (i32)npc.monsterSkillReady.size(); s++) {
				const GameXmlContent::MonsterAttack& special = ai.attacks[s];
				if(!special.special || special.target != attack->target || localTime < npc.monsterSkillReady[s]) continue;
				if(hpRatio < special.hpMin || hpRatio > special.hpMax) continue;
				if(npc.monsterSkillReady[s] == Time::ZERO) {
					npc.monsterSkillReady[s] = TimeAdd(localTime, TimeMsToTime((special.firstAt >= 0 ? special.firstAt : special.cooldown) * 1000.0));
					if(localTime < npc.monsterSkillReady[s]) continue;
				}
				if(dist > special.range + (reach - attack->range) || dist < special.minRange) continue;
				ready.push_back(s);
				totalShare += special.share;
			}
			f32 roll = (f32)(RandUint() % 10000) / 10000.0f * totalShare - attack->share;
			foreach_const(s, ready) {
				const GameXmlContent::MonsterAttack& special = ai.attacks[*s];
				if(roll >= special.share) { roll -= special.share; continue; }
				if(roll < 0) break;
				npc.monsterSkillReady[*s] = TimeAdd(localTime, TimeMsToTime(special.cooldown * 1000.0));
				attack = &special;
				LOG("Monster :: creature=%d (local %d) uses %s", (i32)npc.docID, npc.localID, ActionStateToString(special.state));
				break;
			}
			StartMonsterAttack(npc, ai, attack, targetPos, dist, yaw, run);
			continue;
		}

		if(!npc.path || walk <= 0 || npc.pathPoint >= (i32)npc.path->size()) continue;
		if(!npc.monsterWalking) {
			npc.monsterWalking = true;
			npc.monsterChasing = false;
			f32 best = FLT_MAX;
			for(i32 i = npc.pathPoint; i < (i32)npc.path->size(); i++) {
				const f32 d = glm::length(vec2((*npc.path)[i]) - vec2(npc.pos));
				if(d < best) { best = d; npc.pathPoint = i; }
			}
			npc.actionState = ActionStateID::NONE_BEHAVIORSTATE;
			replication->SendMonsterStateToAll(npc.UID, ActionStateID::NONE_BEHAVIORSTATE, npc.pos, walk);
		}
		const i32 pointBefore = npc.pathPoint;
		f32 budget = walk * delta;
		while(budget > 0 && npc.pathPoint < (i32)npc.path->size()) {
			const vec3& goal = (*npc.path)[npc.pathPoint];
			const vec2 rel = vec2(goal) - vec2(npc.pos);
			const f32 dist = glm::length(rel);
			if(dist <= MONSTER_WAYPOINT_REACHED) { npc.pathPoint++; continue; }
			const f32 step = MIN(budget, dist);
			npc.pos = vec3(vec2(npc.pos) + rel / dist * step, goal.z);
			npc.rot.z = atan2f(rel.y, rel.x);
			budget -= step;
		}
		if(npc.pathPoint < (i32)npc.path->size() && (npc.pathPoint != pointBefore || localTime >= npc.monsterNextSync)) {
			npc.monsterNextSync = TimeAdd(localTime, TimeMsToTime(MONSTER_SYNC_MS));
			replication->SendMonsterMoveToAll(npc.UID, npc.pos, (*npc.path)[npc.pathPoint], npc.rot.z, walk);
		}
	}
}
