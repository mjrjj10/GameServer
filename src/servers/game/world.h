#pragma once
#include <common/base.h>
#include <common/network.h>
#include <common/vector_math.h>
#include <mxm/core.h>

#include <EASTL/array.h>
#include <EASTL/fixed_list.h>
#include <EASTL/fixed_vector.h>

#include "replication.h"
#include "physics.h"

struct StatusInfo;

struct ColliderSize
{
	u16 radius;
	u16 height;
};

struct PlayerInputCastSkill
{
	SkillID skillID = SkillID::INVALID;
	vec3 pos;
	eastl::fixed_vector<ActorUID,10,false> targetList;
	f32 clientTime = 0.f;
};

struct Remote;

struct World
{
	friend struct WorldRemoteSpawner;

	struct Player;
	struct ActorMaster;

	typedef ListItT<ActorMaster> ActorMasterHandle;

	struct PlayerDescription
	{
		UserID userID;
		ClientHandle clientHd;
		WideString name;
		WideString guildTag;
		u8 team;

		eastl::array<ClassType,2> masters;
		eastl::array<SkinIndex,2> skins;
		eastl::array<ColliderSize,2> colliderSize;
		eastl::array<SkillID,4> skills;
		eastl::array<WeaponIndex,2> weapons;
		eastl::array<i32,2> weaponGrades;
		eastl::array<u8,2> masterGearNo;
		eastl::array<i32,2> characterType;
	};

	struct Player
	{
		struct Input
		{
			vec3 moveTo;
			f32 speed;
			RotationHumanoid rot;

			u8 tag: 1;
			u8 jump: 1;

			ActionStateID action;
			i32 actionParam1; // TODO: investigate these
			i32 actionParam2;

			PlayerInputCastSkill cast;
		};

		const u32 index;
		const UserID userID;
		const ClientHandle clientHd;
		const WideString name;
		const WideString guildTag;
		const u8 team;

		const ClassType mainClass;
		const SkinIndex mainSkin;
		const ClassType subClass;
		const SkinIndex subSkin;

		const eastl::array<ColliderSize,2> colliderSize;
		const eastl::array<SkillID,4> skills;
		const WeaponIndex mainWeapon;
		const WeaponIndex subWeapon;
		const i32 mainWeaponGrade;
		const i32 subWeaponGrade;
		const u8 mainMasterGearNo;
		const u8 subMasterGearNo;
		const i32 mainCharacterType;
		const i32 subCharacterType;

		u8 level;
		u32 experience;

		eastl::array<ActorMasterHandle, PLAYER_CHARACTER_COUNT> characters;
		u8 mainCharaID = 0;

		Input input;
		PhysicsDynamicBody* body = nullptr;

		bool weaponFired = false;
		u8 firingCombo = 0;       // CQ_WeaponState: current firing combo step / charge level
		u8 chargeLevel = 0;
		eastl::array<i32,2> shotCount = { 0, 0 }; // weapon shots per master (_FIRINGCOUNT)
		bool passivesApplied = false;
		i32 damageTaken = 0;
		i32 hitsTaken = 0;
		i32 statusesTaken = 0;
		i32 pendingChildSkill = 0;
		Time pendingChildSkillUntil = Time::ZERO;
		eastl::array<Time,5> botSkillReady = { Time::ZERO, Time::ZERO, Time::ZERO, Time::ZERO, Time::ZERO };
		Time tNextBotSkill = Time::ZERO;
		f32 ultGauge = -1.0f; // initialised to the master's max on first update
		f32 stamina = -1.0f;
		f32 mana = -1.0f;
		Time tNextBotShot = Time::ZERO;
		f32 moveSpeedSent = -1.0f;
		Time tNextManaSend = Time::ZERO;
		eastl::array<f32,3> res = { -1, -1, -1 };      // energy, rage, charges (bubble)
		f32 benchMana = -1.0f;                          // tagged-out master's own pools (swapped on tag)
		eastl::array<f32,3> benchRes = { -1, -1, -1 };
		eastl::array<f32,3> resSent = { -1, -1, -1 };
		Time tNextResSend = Time::ZERO;
		Time tNextResRefresh = Time::ZERO; // periodic resend: early sends are dropped until the client is in game
		eastl::array<f32,2> heat = { 0, 0 };        // weapon overheat gauge per master
		eastl::array<Time,2> heatLastShot = { Time::ZERO, Time::ZERO };
		eastl::array<Time,2> overheatUntil = { Time::ZERO, Time::ZERO };
		i32 heatSent = -1;
		Time tNextHeatSend = Time::ZERO;
		Time tNextStaminaSend = Time::ZERO;
		f32 ultGaugeSent = -1.0f;
		Time tNextUltSend = Time::ZERO;
		f32 shotLeadSec = 0;
		f32 weaponAimYaw = 0;
		i32 kills = 0;
		eastl::array<f32,2> weaponMaxShotDistance = { 0, 0 };
		i32 deaths = 0;
		Time tNextWeaponHit = Time::ZERO;
		bool dead = false;
		Time ccUntil = Time::ZERO;
		vec3 pushTarget = vec3(0);
		f32 pushSpeed = 0;
		Time pushUntil = Time::ZERO;
		Time tRespawn = Time::ZERO;
		ActorUID memorizedTarget = ActorUID::INVALID;
		bool pendingShot = false;
		vec3 pendingShotOrigin = vec3(0);
		vec2 pendingShotDir = vec2(0);
		f32 pendingShotDistance = 0;
		eastl::array<u8,256> pendingShotRaw;
		i32 pendingShotRawSize = 0;
		Time pendingShotTime = Time::ZERO;
		bool pendingAim = false;
		f32 pendingAimYaw = 0;
		Time pendingAimTime = Time::ZERO;
		eastl::array<vec3,6> posHistory;
		u32 posHistoryCount = 0;
		u32 posHistoryHead = 0;
		SkillID lastComboSkill = SkillID::INVALID;
		Time lastComboTime = Time::ZERO;
		vec3 spawnPos = vec3(0);

		struct {
			vec2 moveDir = vec2(0);
			f32 moveSpeed = 0;
			RotationHumanoid rot;
			bool hasJumped = false;
		} movement;

		explicit Player(u32 index_, const PlayerDescription& desc):
			index(index_),
			userID(desc.userID),
			clientHd(desc.clientHd),
			name(desc.name),
			guildTag(desc.guildTag),
			team(desc.team),
			mainClass(desc.masters[0]),
			mainSkin(desc.skins[0]),
			subClass(desc.masters[1]),
			subSkin(desc.skins[1]),
			colliderSize(desc.colliderSize),
			skills(desc.skills),
			mainWeapon(desc.weapons[0]),
			subWeapon(desc.weapons[1]),
			mainWeaponGrade(desc.weaponGrades[0]),
			subWeaponGrade(desc.weaponGrades[1]),
			mainMasterGearNo(desc.masterGearNo[0]),
			subMasterGearNo(desc.masterGearNo[1]),
			mainCharacterType(desc.characterType[0]),
			subCharacterType(desc.characterType[1])
		{

		}

		inline ActorMaster& Main() const { return *characters[mainCharaID]; }
		inline ActorMaster& Sub() const { return *characters[mainCharaID ^ 1]; }
	};

	struct ActorMaster
	{
		const ActorUID UID;
		Player* parent;
		ClassType classType;
		SkinIndex skinIndex;
		i32 hp;
		i32 hpMax;
		i32 seed;
		i32 spawnAnim = -1;
		i32 ownerID = 0;
		u8 dirToNearPC = 0;
		i32 wanderDist = -1;
		i32 tagID = -1;




		ActionStateID actionState;
		i32 actionParam1;
		i32 actionParam2;

		explicit ActorMaster(ActorUID UID_): UID(UID_) {}
	};

	struct ActorNpc
	{
		const ActorUID UID;
		CreatureIndex docID;
		i32 localID;
		Faction faction;
		i32 spawnAnim = 0;
		i32 ownerID = 0;
		u8 dirToNearPC = 0;
		i32 wanderDist = -1;
		i32 tagID = -1;
		ActionStateID actionState = ActionStateID::INVALID;
		i32 seed = 0;
		i32 entityType = 1;
		ActorUID ownerUID = ActorUID::INVALID;
		Time despawnAt = Time::ZERO;   // summons: end of LifeTime
		u32 ownerIndex = 0;
		u8 ai = 0;
		ActorUID chaseTarget = ActorUID::INVALID;
		bool stuck = false;
		u8 dieMode = 0;
		Time nextMoveSync = Time::ZERO;

		vec3 pos;
		vec3 rot;

		explicit ActorNpc(ActorUID UID_): UID(UID_) {}
	};

	struct ActorDynamic
	{
		const ActorUID UID;
		CreatureIndex docID;
		i32 localID;
		Faction faction;
		ActionStateID action;
		i32 spawnAnim = 0;
		i32 ownerID = 0;
		u8 dirToNearPC = 0;
		i32 wanderDist = -1;
		i32 tagID = -1;
		i32 seed = 0;
		i32 entityType = 3;

		Time tLastActionChange;
		vec3 pos;
		vec3 rot;

		explicit ActorDynamic(ActorUID UID_): UID(UID_) {}
	};

	struct SkillProgram
	{
		SkillID skillID = SkillID::INVALID;
		ActionStateID actionID;
		vec3 castPos;
		f32 castAngle;
		ActorUID casterUID;
		eastl::fixed_vector<ActorUID,10,false> targetList;
		Time startTime;
		i32 commandID = 0;

		inline bool IsDoneExecuting() const { return skillID == SkillID::INVALID; }
		inline void Finish() { skillID = SkillID::INVALID; }
	};


	Replication* replication;

	eastl::fixed_vector<Player,10,false> players;
	eastl::fixed_list<ActorMaster,512,true> actorMasterList;
	eastl::fixed_list<ActorNpc,512,true> actorNpcList;
	eastl::fixed_list<ActorDynamic,512,true> actorDynamicList;

	typedef ListItT<ActorNpc> ActorNpcHandle;
	typedef ListItT<ActorDynamic> ActorDynamicHandle;

	// TODO: make those fixed_hash_maps
	eastl::fixed_map<ActorUID, ActorMasterHandle, 2048, true> actorMasterMap;
	eastl::fixed_map<ActorUID, ActorNpcHandle, 2048, true> actorNpcMap;
	eastl::fixed_map<ActorUID, ActorDynamicHandle, 2048, true> actorDynamicMap;

	eastl::fixed_vector<SkillProgram,40,false> skillProgramList;

	struct Projectile
	{
		u32 ownerIndex;
		vec3 pos;
		vec2 dir;
		f32 travelled;
		f32 range;
		f32 aoeRadius;
		i32 damage;
		f32 speed;
		f32 halfWidth;
		u32 remoteID = 0;
		f32 pendingAdvance = 0;
		f32 forwardReach = 70.0f;
		RemoteIdx remoteDoc = RemoteIdx::INVALID;
		f32 closest = 999999.0f;
		vec3 start;
		i32 hitsLeft;
		bool graph;
		eastl::fixed_vector<u32,16,false> hitList;
	};

	eastl::fixed_vector<Projectile,256,false> projectileList;

	struct PendingRemote
	{
		Time time;
		RemoteIdx idx;
		i32 preset;
		f32 offsetSide;
		f32 offsetFront;
		u32 ownerIndex;
		SkillID skillID;
		u32 castID;
		i32 damage;
		vec3 castPos;
		f32 angle;
		ActorUID target;
		bool atTarget = false;
		bool stopAtTarget = false;
		u16 nodeName = 0;
	};

	struct RemoteInst
	{
		const Remote* remote;
		u32 ownerIndex;
		SkillID skillID;
		u32 castID;
		i32 damage;
		vec3 pos;
		vec2 dir;
		f32 travelled;
		f32 range;
		f32 speed;
		Time spawnTime;
		ActorUID target;
		i32 hitsLeft;
		bool areaDone;
		u32 clientRemoteID = 0;
		bool destroyedByHit = false;
		Time nextTimeEvent = Time::ZERO;
		bool timeEventActive = false;
		i32 excludeIndex = -1;
		i32 lastHitIndex = -1;
		bool chainEnded = false;
		bool casterLocal = false; // the caster's client creates this remote (and its children) itself
		f32 landZ = 0;            // lobbed remotes: height of the aimed point
		vec3 graphStart = vec3(0);
		bool stopAtFirstHit = false; // DestroyAwayTarget: ends on the first enemy hit (or at the aimed point)
		vec2 graphDir = vec2(1, 0);
		bool hasLandZ = false;
	};

	struct CastHit
	{
		RemoteIdx remoteID;
		u32 castID;
		u32 playerIndex;
		Time until;
	};

	eastl::fixed_vector<PendingRemote,256,false> pendingRemotes;
	eastl::fixed_vector<RemoteInst,256,false> remoteInsts;
	eastl::fixed_vector<CastHit,512,false> castHits;

	struct ActiveStatus
	{
		i32 statusID;
		const StatusInfo* info;
		u32 targetIndex;
		u32 casterIndex;
		ActorUID targetUID;
		ActorUID casterUID;
		Time until;
		Time nextTick;
		i32 overlap;
		f32 shieldHP;
		Time nextHealTick;
		i32 healTicksDone;
		Time nextAuraPulse = Time::ZERO;
		bool removeOnHit = false; // marked when its target got hit (E_NEXT_STATUS_CONDITION_REMOVE_ONHIT)
		i32 custom = 0;           // passive counter (_InitCustomValue .. _MaxCustomValue)
	};
	eastl::fixed_vector<ActiveStatus,512,false> activeStatuses;

	struct PendingStatus
	{
		Time time;
		i32 statusID;
		u32 casterIndex;
		ActorUID targetUID;
		f32 duration;
		f32 power;
	};
	eastl::fixed_vector<PendingStatus,256,false> pendingStatuses;

	struct PendingSummon
	{
		Time time;
		i32 creature;
		u32 ownerIndex;
		vec3 pos;
		vec3 rot;
	};
	eastl::fixed_vector<PendingSummon,64,false> pendingSummons;

	// commands run by a summon (spawn / die actions), attributed to its owner
	struct PendingSummonCommand
	{
		Time time;
		u32 ownerIndex;
		i32 kind;      // SummonCommand::Kind
		i32 index;
		vec3 pos;
		f32 yaw;
		i32 skill;     // the summon's own skill (SkillComData), gives remote damage
		Time until;    // summon end (auras stop with it)
	};
	eastl::fixed_vector<PendingSummonCommand,128,false> pendingSummonCommands;

	// auras carried by a summon: give the aura status around a fixed point
	struct GroundAura
	{
		u32 ownerIndex;
		i32 auraStatus;
		f32 range;
		vec3 pos;
		Time until;
		Time nextPulse;
	};
	eastl::fixed_vector<GroundAura,64,false> groundAuras;
	i32 nextSummonLocalID = 900000;
	u32 nextCastID = 1;
	u32 nextRemoteSeed = 0x40000000;
	eastl::fixed_vector<u32,64,false> comboTriggeredCasts;

	u32 nextActorUID;
	Time localTime = Time::ZERO;

	PhysicsScene physics;

	void Init(Replication* replication_);
	void Cleanup();

	void Update(Time localTime_);
	void Replicate();

	Player& CreatePlayer(const PlayerDescription& desc, const vec3& pos, const RotationHumanoid& rot);
	ActorNpc& SpawnNpcActor(CreatureIndex docID, i32 localID);
	ActorDynamic& SpawnDynamic(CreatureIndex docID, i32 localID);

	Player& GetPlayer(u32 playerIndex);
	u32 PlayerCount() const { return (u32)players.size(); }
	bool PlayerPaySkillCost(Player& player, i32 skillID);
	void UpdateWeaponHeat(Player& player);
	void UpdateSummons();
	void UpdateBotCombat();
	void UpdateBotSkills();
	void UpdateSummonAi(f32 delta);
	void UpdateShowcase();
	void TeleportPlayer(Player& p, const vec3& pos, f32 yaw);
	struct Showcase
	{
		bool started = false;
		bool done = false;
		Time next = Time::ZERO;
		i32 casterSlot = 0;
		i32 dummySlot = -1;
		i32 masterPass = 0;
		i32 step = 0;
		i32 rangeIdx = 0;
		i32 testCount = 1;
		i32 lastDummyHp = 0;
		i32 lastDummyHits = 0;
		i32 lastDummyStatuses = 0;
		bool pendingResult = false;
		i32 prevExpect = -1;
		u32 prevDummy = 0;
		u32 prevCaster = 0;
		i32 phase = 0;
		vec3 mid = vec3(0);
		vec2 dir = vec2(1, 0);
		i32 probeDir = 0;
		f32 groundZ = 0;
		eastl::fixed_vector<vec2,24,false> candidates;
		i32 candIdx = 0;
		vec3 anchor = vec3(0);
		f32 yaw = 0;
	} showcase;
	void RunSummonState(ActorNpc& npc, i32 state);
	bool frozen = false; // game over: no more shots, skills or bot actions
	ActorMaster* FindMasterActor(ActorUID actorUID) const;
	ActorNpc* FindNpcActor(ActorUID actorUID) const;
	ActorNpc* FindNpcActorByCreatureID(CreatureIndex docID); // Warning: slow!

	void PlayerWeaponShot(Player& player, const vec3& origin, const vec2& dir, f32 distance);
	void OnWeaponShotPacket(Player& player, const vec3& origin, const vec2& dir, f32 distance, const u8* rawPacket, i32 rawSize);
	void OnWeaponAimPacket(Player& player, f32 aimYaw);

private:
	ActorUID NewActorUID();
	ActorMasterHandle MasterInvalidHandle();

	void PlayerCastSkill(Player& player, SkillID skill, const vec3& castPos, Slice<const ActorUID> targets, f32 clientTime);
	void ExecuteSkillProgram(SkillProgram& prog);

	void PlayerWeaponFire(Player& player);
	void UpdateProjectiles(f32 delta);
	void Explode(Player& owner, const vec3& center, f32 radius, i32 damage);
	void RespawnPlayer(Player& player);
	bool ScheduleSkillRemotes(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 angle, i32 forceAnim = -1);
	RemoteInst* SpawnRemote(u32 ownerIndex, RemoteIdx idx, const vec3& pos, const vec2& dir, ActorUID target, SkillID skillID, u32 castID, i32 damage, bool broadcast = false, const vec3* aimPos = nullptr, u16 nodeName = 0, bool casterCreatesLocally = false, bool stopAtTarget = false);
	void UpdateRemotes(f32 delta);
	void UpdateUltimateGauges(f32 delta);
	void ApplyStatus(Player& caster, Player& target, i32 statusID, f32 power = 0, f32 durationOverride = -1);
	ActiveStatus* FindActiveStatus(u32 targetIndex, i32 statusID);
	void ConsumeStatus(u32 targetIndex, i32 statusID);
public:
	void ApplyPassives(Player& player);
	void ApplyAllPassives();
private:
	void PassiveOnSkillCast(Player& player, i32 skillID);
	void PassiveOnWeaponHit(Player& attacker, Player& target, const Projectile& proj);
	void PassiveOnSkillHit(Player& attacker, Player& target);
	f32 PassiveWeaponDamageMul(Player& attacker, Player& target);
	bool HasStatusType(const Player& p, const char* type) const;
	f32 StatusModSum(const Player& p, i32 stat) const;
	f32 StatusModMulSum(const Player& p, i32 stat) const;
	bool IsBlocked(const Player& p, bool attack, bool skill, bool shirk) const;
	void HealPlayer(Player& healer, Player& target, i32 amount);
	bool RemoteCanAffect(const RemoteInst& inst, const Player& p, bool includeOwner) const;
	void RemoteApplyShape(RemoteInst& inst);
	void RemoteHitAlongPath(RemoteInst& inst);
	void UpdateStatuses();
	void RemoteApplyArea(RemoteInst& inst);
	f32 LagDistToSegment(const Player& p, const vec2& a, const vec2& b) const;
	bool LagSegmentHits(const Player& p, const vec2& a, const vec2& b, f32 lateral, f32 forwardReach) const;
	f32 LagDistToPoint(const Player& p, const vec2& point) const;
	vec2 LagClosestPos(const Player& p, const vec2& point) const;
	void RemoteHit(RemoteInst& inst, Player& target);
	void PlayerSkillDamage(Player& player, SkillID skillID, const vec3& castPos, Slice<const ActorUID> targets, f32 angle);
	void DamagePlayer(Player& attacker, Player& target, SkillID skillID, i32 damage, const Projectile* proj = nullptr, u32 remoteID = 0, RemoteIdx remoteDoc = RemoteIdx::INVALID, vec2 remoteDir = vec2(1, 0));
};
