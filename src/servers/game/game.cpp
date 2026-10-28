#include "game.h"

#include <EAStdC/EAString.h>
#include <EAStdC/EAScanf.h>
#include <common/utils.h>

#include <mxm/game_content.h>
#include "config.h"
#include "world_master_rules.h"
#include <mxm/account_progress.h>

static const f32 STAGE_TIME_LIMIT_FALLBACK_SEC = 300.0f;
static const f64 RETURN_TO_LOBBY_DELAY_SEC = 20.0;
static const u16 HUB_LISTEN_PORT = 11900;

const CreatureIndex CI_DOOR = CreatureIndex(110040546);
const CreatureIndex CI_WALL = CreatureIndex(110042602);

static const f64 LEVEL_START_BEFORE_GAME_MS = 5000.0;
static const f64 PRE_GAME_MIN_MS = 10000.0;

void Game::Init(Server* server_, const In::MQ_CreateGame& gameInfo, const eastl::array<ClientHandle, MAX_PLAYERS>& playerClientHdList)
{
	replication.Init(server_);
	ASSERT(gameInfo.gameType != GameType::INVALID);
	ASSERT(gameInfo.areaIndex != AreaIndex(0));
	ASSERT(gameInfo.stageIndex != StageIndex(0));
	ASSERT(gameInfo.mapIndex != MapIndex(0));
	replication.inGameID = (i32)(u64)gameInfo.sortieUID;
	replication.gameType = gameInfo.gameType;
	replication.areaIndex = gameInfo.areaIndex;
	replication.stageIndex = gameInfo.stageIndex;
	replication.mapIndex = gameInfo.mapIndex;
	if(Config().DevForcedMap != 0) {
		replication.mapIndex = (MapIndex)Config().DevForcedMap;
	}
	replication.canEscape = gameInfo.canEscape;
	replication.isTrespass = 0;
	replication.surrenderAbleTime = gameInfo.surrenderAbleTime;
	replication.supportKitIndex = -1;
	world.Init(&replication);
	if(const GameXmlContent::StageMaps* stage = GetGameXmlContent().FindStageMaps(gameInfo.stageIndex)) {
		const bool pve = stage->gameMode == GameModeType::GAME_MODE_STAGE || stage->gameMode == GameModeType::GAME_MODE_DEFENCE;
		world.hostileMonsters = pve;
		if(pve) world.difficultyBit = stage->difficultyBit;
		if(pve) world.monsterStatScale = GetGameXmlContent().DifficultyMonsterStat(stage->difficultyBit);
		if(stage->gameMode == GameModeType::GAME_MODE_STAGE) {
			foreach_const(m, stage->maps) if(!zoneMaps.full()) zoneMaps.push_back(*m);
			if(stage->bossMap != MapIndex(0) && !zoneMaps.full()) zoneMaps.push_back(stage->bossMap);
			LOG("Stage :: %d zone(s), difficulty bit %d", (i32)zoneMaps.size(), (i32)world.difficultyBit);
		}
	}

	LoadMap();

	const GameXmlContent& xml = GetGameXmlContent();

	// create players
	i32 spawnPointIndex[2] = { 0 };

	for(int pi = 0; pi < gameInfo.playerCount; pi++) {
		const In::MQ_CreateGame::Player& p = gameInfo.players[pi];
		if(p.team != 0 && p.team != 1) continue; // skip spectators

		World::PlayerDescription desc;
		desc.userID = UserID(pi+1);
		desc.clientHd = playerClientHdList[pi];
		desc.name.assign(p.name.data, p.name.len);
		desc.guildTag.assign(p.guildTag.data, p.guildTag.len);
		desc.team = p.team;
		desc.masters = p.masters;
		desc.skins = p.skins;
		desc.skills = p.skills;
		desc.stageSkills = { SkillID::INVALID, SkillID::INVALID };
		if(const GameXmlContent::StageMaps* stage = xml.FindStageMaps(gameInfo.stageIndex)) {
			desc.stageSkills[0] = stage->stageSkill;
			if(!stage->stageSkillCandidates.empty()) desc.stageSkills[1] = stage->stageSkillCandidates[0];
			foreach_const(c, stage->stageSkillCandidates) if(*c == p.stageSkill) desc.stageSkills[1] = p.stageSkill;
		}
		desc.weapons = p.weapons;
		desc.weaponGrades = p.weaponGrades;
		desc.masterGearNo = p.masterGearNo;
		desc.characterType = p.characterType;

		const auto& master0 = xml.GetMaster(p.masters[0]);
		const auto& master1 = xml.GetMaster(p.masters[1]);

		desc.colliderSize[0] = {
			(u16)master0.character.getColliderRadius(),
			(u16)master0.character.getColliderHeight(),
		};
		desc.colliderSize[1] = {
			(u16)master1.character.getColliderRadius(),
			(u16)master1.character.getColliderHeight(),
		};

		const auto& spawnPoints = mapSpawnPoints[p.team].empty() ? mapSpawnPoints[p.team ^ 1] : mapSpawnPoints[p.team];
		const SpawnPoint fallback = { vec3(0), vec3(0) };
		const SpawnPoint& spawnPoint = spawnPoints.empty() ? fallback : spawnPoints[spawnPointIndex[p.team]++ % spawnPoints.size()];
		if(spawnPoints.empty()) WARN("Spawn :: map %d has no spawn point", (i32)replication.mapIndex);

		LOG("Spawn :: %ls team=%d masters=(%d, %d) skins=(%d, %d) weapons=(%d, %d)", desc.name.data(), (i32)desc.team, (i32)desc.masters[0], (i32)desc.masters[1], (i32)desc.skins[0], (i32)desc.skins[1], (i32)desc.weapons[0], (i32)desc.weapons[1]);
		World::Player& worldPlayer = world.CreatePlayer(desc, spawnPoint.pos, RotationHumanoid{0.f, 0.f, MxmYawToWorldYaw(spawnPoint.rot.z)});

		playerList.emplace_back(desc.clientHd, p.accountUID, desc.name, worldPlayer.index);
		if(desc.clientHd != ClientHandle::INVALID) {
			playerMap.emplace(desc.clientHd, --playerList.end());

			replication.OnPlayerConnect(desc.clientHd, worldPlayer.index);
			replication.PlayerRegisterMasterActor(desc.clientHd, worldPlayer.Main().UID, worldPlayer.mainClass);
			replication.PlayerRegisterMasterActor(desc.clientHd, worldPlayer.Sub().UID, worldPlayer.subClass);
		}
		else {
			botList.emplace_back(worldPlayer.index);
		}
	}

	dbgGameUID = Dbg::PushNewGame("PVP_DeathMatch");
}

void Game::Cleanup()
{
	world.Cleanup();
}

void Game::Update(Time localTime_)
{
	ProfileFunction();
	localTime = localTime_;
	Dbg::PushNewFrame(dbgGameUID);

	// update clone
	if(clone) {
		clone->input = world.GetPlayer(0).input; // TODO: hardcoded hack
		// TODO: maybe don't replicate cloned players, just their master?
	}

	if(lego) {
		enum Step {
			Move = 0,
			Stop = 1,
		};

		f64 localTimeSec = TimeDiffSec(localTime);
		u32 step = ((u64)localTimeSec % 5) == 0;

		if(legoLastStep == Step::Stop && step == Step::Move) {
			f32 a = Randf01() * 2 * PI;
			legoDir = vec2(cosf(a), sinf(a));
			legoAngle = (legoAngle+1) % 4;
		}

		if(step == Step::Move) { // move
			f32 a = legoAngle * PI/2;
			lego->input.rot.upperYaw = a;
			lego->input.rot.bodyYaw = a;
			lego->input.moveTo = lego->body->GetWorldPos() + vec3(legoDir * 1000.f, 0);
			lego->input.speed = 626;
		}
		else { // stop
			lego->input.moveTo = vec3(0);
			lego->input.speed = 0;
		}

		legoLastStep = step;
	}

	switch(phase) {
		case Phase::WaitingForReady: {
			if(phaseTime < localTime) {
				phase = Phase::PreGame;
				phaseTime = TimeAdd(localTime, TimeMsToTime(MAX(PRE_GAME_MIN_MS, LevelStartLeadMs())));

				foreach_const(p, playerList) {
					replication.SendPreGameLevelEvents(p->clientHd);
				}
			}
		} break;

		case Phase::PreGame: {
			if(TimeAdd(localTime, TimeMsToTime(LevelStartLeadMs())) >= phaseTime) world.StartLevelEvents();
			if(phaseTime < localTime) {
				phase = Phase::Game;

				// TODO: delete when inactive
				for(auto it = world.actorDynamicList.begin(); it != world.actorDynamicList.end();) {
					if(it->docID == CI_DOOR) { // door
						it->action = ActionStateID::DYNAMIC_OPEN;

						//world.actorDynamicMap.erase(it->UID);
						//it = world.actorDynamicList.erase(it);
						++it;
					}
					else {
						++it;
					}
				}

				foreach_const(p, playerList) {
					replication.SendGameStart(p->clientHd);
				}
				tGameStarted = localTime;
				world.combatLocked = false;
				if(!Config().DevShowcase) world.gameStartedAt = localTime;
				world.ApplyAllPassives();
				world.StartSkillPoints();
				foreach_const(p, world.players) world.SendTagCooltimeToTeam(*p, 0);
				world.StartTimeRecord();
				world.StartIngameLevels();
			}
		} break;

		case Phase::Game: {
			if(world.portalRequested && !gameEnded) MoveToNextZone();
			if(!gameEnded && tGameStarted != Time::ZERO && world.winnerTeam < 0 && replication.titanMode) {
				const i32 scoreLimit = GetGameXmlContent().GetStageScoreLimit((i32)replication.stageIndex);
				for(i32 team = 0; team < 2 && scoreLimit > 0; team++) {
					if(world.TeamPoints(team) >= scoreLimit && world.TeamPoints(team) > world.TeamPoints(1 - team)) {
						world.winnerTeam = team;
						LOG("Game :: team %d reaches %d points, wins", team, scoreLimit);
					}
				}
			}
			if(!gameEnded && tGameStarted != Time::ZERO && world.winnerTeam >= 0) EndGame();
			if(!gameEnded && tGameStarted != Time::ZERO) {
				f32 limit = GetGameXmlContent().GetStageTimeLimit((i32)replication.stageIndex);
				if(limit <= 0) limit = STAGE_TIME_LIMIT_FALLBACK_SEC;
				if(Config().DevGameTimeSec > 0) limit = (f32)Config().DevGameTimeSec;
				if(TimeDiffSec(TimeDiff(tGameStarted, localTime)) >= limit) EndGame();
			}
			if(gameEnded && !sentBackToLobby && TimeDiffSec(TimeDiff(tGameEnded, localTime)) >= RETURN_TO_LOBBY_DELAY_SEC) {
				sentBackToLobby = true;
				foreach_const(p, playerList) {
					if(p->clientHd != ClientHandle::INVALID) OnPlayerReturnToChannel(p->clientHd);
				}
				LOG("Game :: players sent back to the lobby");
			}
#if 1
			// bot random actions
			if(!gameEnded && !Config().DevShowcase) foreach(bot, botList) {
				if(localTime > bot->tNextAction) {
					bot->tNextAction = TimeAdd(localTime, TimeMsToTime(Randf01() * 5 * 1000));
					World::Player& wpl = world.GetPlayer(bot->playerIndex);

					struct Action {
						enum Enum: i32 {
							Move = 0,
							Tag,
							Jump,
							_Count
						};
					};

					eastl::array<f32,Action::_Count> actionWeight = {0.f};
					actionWeight[Action::Move] = 8;
					actionWeight[Action::Tag] = 2;
					actionWeight[Action::Jump] = 0;

					f32 actionSpace = 0;
					foreach_const(w, actionWeight) actionSpace += *w;

					Action::Enum actionID = Action::Move;

					f32 rs = Randf01() * actionSpace;
					actionSpace = 0;
					for(int wi = 0; wi < Action::_Count; wi++) {
						if(rs >= actionSpace && rs < actionSpace + actionWeight[wi]) {
							actionID = (Action::Enum)wi;
							break;
						}
						actionSpace += actionWeight[wi];
					}

					switch(actionID) {
						case Action::Move: break;

						case Action::Tag: {
							wpl.input.tag = true;
						} break;

						case Action::Jump: {
							wpl.input.jump = true;
						} break;
					}
				}
			}
#endif

			// Dynamic walls going up and down randomly
			/*
			foreach(it, world.actorDynamicList) {
				if(it->docID == CI_WALL && TimeDiffSec(TimeDiff(it->tLastActionChange, localTime)) > 4.0) { // wall
					if(RandInt(0, 150) == 0) {
						if(it->action == ActionStateID::DYNAMIC_NORMAL_STAND || it->action == ActionStateID::DYNAMIC_CLOSE) {
							it->action = ActionStateID::DYNAMIC_OPEN;
						}
						else {
							it->action = ActionStateID::DYNAMIC_CLOSE;
						}

						it->tLastActionChange = localTime;
					}
				}
			}
			*/

		} break;
	}

	world.Update(localTime);

	foreach_const(player, world.players) {
		Dbg::PlayerMaster e;
		e.UID = (u32)player->userID;
		e.name = player->name;
		e.pos = player->body->GetWorldPos();
		e.rot = player->input.rot;
		e.moveDir = NormalizeSafe(player->input.moveTo - player->body->GetWorldPos());
		e.moveDest = player->input.moveTo;
		e.color = vec3(1, 0, 1);
		Dbg::Push(dbgGameUID, e);
	}

	foreach_const(npc, world.actorNpcList) {
		Dbg::Npc n;
		n.UID = (u32)npc->UID;
		n.pos = npc->pos;
		n.rot = {}; // TODO: fill
		Dbg::Push(dbgGameUID, n);
	}

	foreach_const(dyn, world.actorDynamicList) {
		Dbg::Dynamic n;
		n.UID = (u32)dyn->UID;
		n.docID = dyn->docID;
		n.pos = dyn->pos;
		n.rot = dyn->rot;
		Dbg::Push(dbgGameUID, n);
	}

	Dbg::PushPhysics(dbgGameUID, world.physics);

	replication.FrameEnd();
}

bool Game::LoadMap()
{
	const GameXmlContent& content = GetGameXmlContent();
	const GameXmlContent::Map* map = content.FindPvpMap(replication.mapIndex);
	if(!map) {
		WARN("LoadMap> map %d has no server data, falling back to deathmatch", (i32)replication.mapIndex);
		map = content.FindPvpMap(MapIndex::PVP_DEATHMATCH);
	}
	world.map = map;
	const GameXmlContent::TitanInfo& titan = content.titan;
	if(map->ingameLevels && !titan.forms.empty() && !titan.giants.empty()) {
		world.giantCycle = RandUint() % (u32)titan.giants.size();
		replication.titanMode = true;
		replication.titanNextIndex = (i8)world.giantCycle;
	}
	foreach_const(c, map->colliders) {
		world.physics.CreateStaticCollider(c->mesh.data(), c->pos, c->rot, c->scale);
	}

	foreach(it, map->creatures) {
		// don't spawn "spawn points"
		if(it->IsSpawnPoint()) {
			if(it->faction == Faction::RED || it->faction == Faction::BLUE) {
				mapSpawnPoints[(i32)it->faction].push_back(SpawnPoint{ it->pos, it->rot });
			}
			continue;
		}

		// spawn npc
		World::ActorNpc& actor = world.SpawnNpcActor(it->docID, it->localID);
		actor.pos = it->pos;
		actor.rot = it->rot;
		actor.home = it->pos;
		actor.faction = world.hostileMonsters && it->faction == Faction::INVALID ? Faction::MONSTER : it->faction;
		actor.spawnAnim = it->spawnAnim;
		actor.wanderDist = it->wanderDist;
		actor.tagID = it->tagID;
		actor.ownerID = it->ownerID;
		actor.dirToNearPC = it->dirToNearPC;
		actor.actionState = it->actionState;
		actor.entityType = it->entityType;
		actor.seed = (i32)RandUint();
		world.InitMonsterStats(actor);
	}

	foreach(it, map->dynamic) {
		// spawn dynamic
		auto& actor = world.SpawnDynamic(it->docID, it->localID);
		actor.pos = it->pos;
		actor.rot = it->rot;
		actor.faction = it->faction;
		actor.spawnAnim = it->spawnAnim;
		actor.wanderDist = it->wanderDist;
		actor.tagID = it->tagID;
		actor.ownerID = it->ownerID;
		actor.dirToNearPC = it->dirToNearPC;
		actor.action = it->actionState;
		actor.entityType = it->entityType;
		actor.seed = (i32)RandUint();

	}

	if(map->index != MapIndex::PVP_DEATHMATCH) return true;

	// spawn walls dynamically

	// NOTE: Found in "Data/Design/Level/PVP/PVP_DeathMatch/EVENTNODES/LEVELEVENT_SERVER.XML"
	const eastl::fixed_set<i32,12,false> configurations[] = {
		{ 179, 178, 177, 176, 175, 174, 173, 172, 171, 170 }, // 10 A type
		{ 40, 41, 45, 46, 39, 48, 37, 49, 36, 38, 51, 50 }, // 12 A type
		{ 39, 48, 45, 40, 37, 49, 38, 50 }, // 8 A type
		{ 39, 45, 46, 49, 36, 51 }, // 6 B type
		{ 40, 45, 48, 37, 38, 50 }, // 6 A type
		{ 48, 37, 41, 51 }, // 4 C type
		{ 48, 37, 45, 38 }, // 4 B type
		{ 51, 41, 48, 36 }, // 4 A type
	};

	const eastl::array<const char*, ARRAY_COUNT(configurations)> configurationName = {
		"10 A type",
		"12 A type",
		"8 A type",
		"6 B type",
		"6 A type",
		"4 C type",
		"4 B type",
		"4 A type",
	};

	const i32 chosenConf = RandInt(0, ARRAY_COUNT(configurations)-1);
	const auto& wallConf = configurations[chosenConf];
	const char* wallConfName = configurationName[chosenConf];

	LOG("LoadMap> chosen wall configuration = '%s'", wallConfName);

	foreach(it, map->areas) {
		if(wallConf.find(it->ID) != wallConf.end()) {
			auto& actor = world.SpawnDynamic(CI_WALL, it->ID);
			actor.pos = it->pos;
			actor.rot = it->rot;
			actor.collider = world.physics.CreateStaticCollider("PvP_Death_NM_Wall04_GuardrailMob", it->pos, it->rot);
		}
	}
	return true;
}

void Game::MoveToNextZone()
{
	world.portalRequested = false;
	if(zoneIndex + 1 >= (i32)zoneMaps.size()) {
		LOG("Stage :: portal without a next zone");
		return;
	}
	zoneIndex++;
	replication.mapIndex = zoneMaps[zoneIndex];
	LOG("Stage :: moving to zone %d (map %d)", zoneIndex, (i32)replication.mapIndex);
	foreach_const(p, playerList) {
		if(p->clientHd == ClientHandle::INVALID) continue;
		replication.SendGameAreaMove(p->clientHd, replication.mapIndex);
		replication.ResetClientForAreaMove(p->clientHd);
	}
	world.ClearMapContent();
	foreach(sp, mapSpawnPoints) sp->clear();
	LoadMap();
	PlacePlayersAtSpawn();
	zoneLoading = true;
}

void Game::PlacePlayersAtSpawn()
{
	i32 spawnPointIndex[2] = { 0 };
	foreach(p, world.players) {
		if(p->removed || p->team > 1) continue;
		const auto& spawnPoints = mapSpawnPoints[p->team];
		if(spawnPoints.empty()) continue;
		const SpawnPoint& sp = spawnPoints[spawnPointIndex[p->team]++ % spawnPoints.size()];
		p->spawnPos = sp.pos;
		world.TeleportPlayer(*p, sp.pos, MxmYawToWorldYaw(sp.rot.z));
	}
}

void Game::OnPlayerDisconnect(ClientHandle clientHd)
{
	Player& player = *playerMap.at(clientHd);

	// TODO: KILL MASTERS
	replication.OnPlayerDisconnect(clientHd);
}

void Game::OnPlayerReadyToLoad(ClientHandle clientHd)
{
	replication.SendLoadPvpMap(clientHd, replication.mapIndex);
}

void Game::OnPlayerGetCharacterInfo(ClientHandle clientHd, ActorUID actorUID)
{
	Player& p = *playerMap.at(clientHd);
	const World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	foreach_const(chit, player.characters) {
		const World::ActorMaster& chara = **chit;

		if(chara.UID == actorUID) {
			replication.SendCharacterInfo(clientHd, chara.UID, CreatureIndex(100000000 + (i32)chara.classType), chara.classType, chara.hp, chara.hpMax);



			return;
		}
	}

	WARN("Client sent an invalid actorUID (clientHd=%x actorUID=%u)", clientHd, (u32)actorUID);
}

void Game::OnPlayerUpdatePosition(ClientHandle clientHd, ActorUID actorUID, const vec3& pos, const vec2& dir, const RotationHumanoid& rot, f32 speed, ActionStateID action, f32 clientTime)
{
	ProfileFunction();

	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	bool found = false;
	foreach_const(chit, player.characters) {
		const World::ActorMaster& chara = **chit;
		if(chara.UID == actorUID) {
			found = true;
			break;
		}
	}

	// NOTE: the client is not aware that we spawned a new actor for them yet, we ignore this packet
	// LordSk (30/08/2020)
	if(!found) {
		WARN("Client sent an invalid actorUID (clientID=%x actorUID=%u)", clientHd, (u32)actorUID);
		return;
	}

	const f64 serverTime = TimeDiffSec(TimeRelNow());

	static f64 prevClientTime = 0;
	static f64 prevServerTime = 0;
	static vec3 prevPos = vec3(0);
	static vec3 prevBodyPos = vec3(0);
	static f64 accumulatedSpeedDiff = 0.0;

	if(dir == vec2(0)) {
		accumulatedSpeedDiff = 0.0;
	}
	else {
		f64 clientDelta = clientTime - prevClientTime;
		f64 serverDelta = serverTime - prevServerTime;
		vec3 posDelta = prevPos - pos;
		vec3 posBodyDelta = prevBodyPos - player.body->GetWorldPos();

		vec2 delta = vec2(pos - player.body->GetWorldPos());
		f64 speedClient = glm::length(posDelta) / clientDelta;
		f64 speedServer = glm::length(posBodyDelta) / serverDelta;
		if(speedClient - speedServer > 0) {
			accumulatedSpeedDiff += speedClient - speedServer;
		}

		LOG("Position diff = %f", glm::length(delta));
		LOG("Speed client = %g", speedClient);
		LOG("Speed server = %g", speedServer);
		LOG("Speed diff accumulated = %g", accumulatedSpeedDiff);
	}

	prevClientTime = clientTime;
	prevServerTime = serverTime;
	prevPos = pos;
	prevBodyPos = player.body->GetWorldPos();


	player.lastClientPos = pos;
	player.clientPosCount++;
	if(player.carriedBy >= 0) return;
	if(action != ActionStateID::INVALID && action != player.lastClientAction) {
		player.lastClientAction = action;
		player.lastClientActionSince = world.localTime;
	}
	if(world.localTime < player.dashUntil || world.localTime < player.ccUntil) {
		player.body->collider->setFootPosition(PxExtendedVec3(pos.x, pos.y, pos.z));
		player.body->vel = vec3(0);
	}

	// TODO: check for movement hacking
	if(dir.x == 0 && dir.y == 0) {
		player.input.moveTo = pos;
	}
	else {
		player.input.moveTo = pos + vec3(glm::normalize(vec2(dir)) * speed, 0);
	}
	player.input.rot = rot;
	player.input.speed = speed;
	if(Config().DevMirror) LOG("Aim :: yaw=%.4f pitch=%.4f", rot.upperYaw, rot.upperPitch);
}

void Game::OnPlayerUpdateRotation(ClientHandle clientHd, ActorUID actorUID, const RotationHumanoid& rot)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	bool found = false;
	foreach_const(chit, player.characters) {
		const World::ActorMaster& chara = **chit;
		if(chara.UID == actorUID) {
			found = true;
			break;
		}
	}

	// NOTE: the client is not aware that we spawned a new actor for them yet, we ignore this packet
	// LordSk (30/08/2020)
	if(!found) {
		WARN("Client sent an invalid actorUID (clientHd=%x actorUID=%u)", clientHd, (u32)actorUID);
		return;
	}

	player.input.rot = rot;
	if(Config().DevMirror) LOG("Aim :: yaw=%.4f pitch=%.4f", rot.upperYaw, rot.upperPitch);
}

void Game::OnPlayerChatMessage(ClientHandle clientHd, i32 chatType, const wchar* msg, i32 msgLen)
{
	// command
	if(ParseChatCommand(clientHd, msg, msgLen)) {
		return; // we're done here
	}

	// TODO: chat types
	// TODO: senderStaffType
	// TODO: Actual chat system

	Player& p = *playerMap.at(clientHd);
	replication.SendChatMessageToAll(p.name.data(), chatType, msg, msgLen);
}

void Game::OnPlayerChatWhisper(ClientHandle clientHd, const wchar* destNick, const wchar* msg)
{
	const Player* destPlayer = nullptr;
	foreach_const(pl, playerList) {
		if(pl->name.compare(destNick) == 0) {
			destPlayer = &*pl;
			break;
		}
	}

	if(!destPlayer || destPlayer->IsBot()) {
		replication.SendChatWhisperConfirmToClient(clientHd, destNick, msg, ErrorType::WHISPER_SEND_NOT_FOUND);
		return;
	}

	replication.SendChatWhisperConfirmToClient(clientHd, destNick, msg, ErrorType::SUCCESS);
	Player& p = *playerMap.at(clientHd);
	replication.SendChatWhisperToClient(destPlayer->clientHd, p.name.data(), msg);
}

void Game::OnPlayerSetLeaderCharacter(ClientHandle clientHd, LocalActorID characterID, SkinIndex skinIndex)
{

}

void Game::OnPlayerSyncActionState(ClientHandle clientHd, ActorUID actorUID, ActionStateID state, i32 param1, i32 param2, f32 rotate, f32 upperRotate)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	bool found = false;
	foreach_const(chit, player.characters) {
		const World::ActorMaster& chara = **chit;
		if(chara.UID == actorUID) {
			found = true;
			break;
		}
	}

	// NOTE: the client is not aware that we spawned a new actor for them yet, we ignore this packet
	// LordSk (30/08/2020)
	if(!found) {
		WARN("Client sent an invalid actorUID (clientHd=%x actorUID=%u)", clientHd, (u32)actorUID);
		return;
	}

	// TODO: check hacking
	player.input.rot.bodyYaw = rotate;
	player.input.rot.upperYaw = upperRotate;
	player.input.action = state;
	player.input.actionParam1 = param1;
	player.input.actionParam2 = param2;
}

void Game::OnPlayerLoadingComplete(ClientHandle clientHd)
{

}

void Game::OnPlayerGameMapLoaded(ClientHandle clientHd)
{

}

static const u8 MEASURED_TRAINING_EVENT_LOADOUT = 4;
static const u8 MEASURED_TRAINING_EVENT_CREATE_AI = 5;
static const u8 MEASURED_TRAINING_EVENT_REMOVE_AI = 6;
static const u8 MEASURED_TRAINING_EVENT_RESET_SKILLS = 0;
static const i32 HYP_TRAINING_BOT_SUB_OFFSET = 3;
static const wchar* MEASURED_TRAINING_BOT_NAME = L"BOT_NAME_02";

void Game::OnMasterTrainingEvent(ClientHandle clientHd, const u8* data, i32 size)
{
	const u8 eventType = data[0];
	replication.SendMasterTrainingEvent(clientHd, eventType, 0, 0, 0, 0);
	if(eventType == MEASURED_TRAINING_EVENT_LOADOUT) replication.SendMasterTrainingEvent(clientHd, eventType, 0, 1, 1, 1);
	if(eventType == MEASURED_TRAINING_EVENT_CREATE_AI && size >= 9) {
		const i32 choice = *(const i32*)(data + 5);
		const auto* classes = GetGameXmlContent().FindStageBots(replication.stageIndex);
		if(!classes || choice < 1 || choice > (i32)classes->size()) return;
		const i32 n = (i32)classes->size();
		const ClassType mainClass = (*classes)[choice - 1];
		const ClassType subClass = (*classes)[(choice - 1 + HYP_TRAINING_BOT_SUB_OFFSET) % n];
		auto pl = playerMap.find(clientHd);
		const u8 humanTeam = pl != playerMap.end() ? world.GetPlayer(pl->second->playerIndex).team : 0;
		SpawnTrainingBot(mainClass, subClass, humanTeam ^ 1);
	}
	if(eventType == MEASURED_TRAINING_EVENT_REMOVE_AI) {
		foreach_const(t, trainingBots) {
			world.RemovePlayer(world.GetPlayer(*t));
			for(auto b = botList.begin(); b != botList.end(); ++b) {
				if(b->playerIndex != *t) continue;
				botList.erase(b);
				break;
			}
		}
		trainingBots.clear();
	}
	if(eventType == MEASURED_TRAINING_EVENT_RESET_SKILLS) {
		auto pl = playerMap.find(clientHd);
		if(pl != playerMap.end()) world.ResetSkillPoints(world.GetPlayer(pl->second->playerIndex));
	}
}

void Game::OnTitanIncarnate(ClientHandle clientHd, i32 formDoc)
{
	auto pl = playerMap.find(clientHd);
	if(pl == playerMap.end()) return;
	const GameXmlContent::TitanInfo& titan = GetGameXmlContent().titan;
	for(i32 i = 0; i < (i32)titan.forms.size(); i++) {
		if(titan.forms[i].doc != formDoc) continue;
		world.TransformTitan(world.GetPlayer(pl->second->playerIndex), i, true);
		return;
	}
	WARN("Training :: unknown titan form %d", formDoc);
}

void Game::SpawnTrainingBot(ClassType mainClass, ClassType subClass, u8 team)
{
	if(world.players.full()) return;
	const GameXmlContent& xml = GetGameXmlContent();
	World::PlayerDescription desc;
	desc.userID = UserID(world.players.size() + 1);
	desc.clientHd = ClientHandle::INVALID;
	desc.name.assign(MEASURED_TRAINING_BOT_NAME);
	desc.team = team;
	const ClassType classes[2] = { mainClass, subClass };
	for(int mi = 0; mi < 2; mi++) {
		const GameXmlContent::Master& master = *xml.masterClassTypeMap.at(classes[mi]);
		desc.masters[mi] = classes[mi];
		desc.skins[mi] = SkinIndex::DEFAULT;
		desc.skills[mi * 2] = master.skillIDs[0];
		desc.skills[mi * 2 + 1] = master.skillIDs[1];
		if(!master.fairPvpWeaponIDs.empty()) desc.weapons[mi] = master.fairPvpWeaponIDs[0];
		else if(!master.defaultWeaponIDs.empty()) desc.weapons[mi] = master.defaultWeaponIDs[0];
		else desc.weapons[mi] = master.weaponIDs.size() > 1 ? master.weaponIDs[1] : master.weaponIDs[0];
		desc.weaponGrades[mi] = 0;
		desc.masterGearNo[mi] = 1;
		desc.characterType[mi] = 1;
		desc.colliderSize[mi] = { (u16)master.character.getColliderRadius(), (u16)master.character.getColliderHeight() };
	}
	desc.stageSkills = { SkillID::INVALID, SkillID::INVALID };
	const auto& spawnPoints = mapSpawnPoints[team];
	const vec3 pos = spawnPoints.empty() ? world.GetPlayer(0).spawnPos : spawnPoints[0].pos;
	const f32 yaw = spawnPoints.empty() ? 0.f : MxmYawToWorldYaw(spawnPoints[0].rot.z);
	World::Player& worldPlayer = world.CreatePlayer(desc, pos, RotationHumanoid{0.f, 0.f, yaw});
	playerList.emplace_back(desc.clientHd, AccountUID::INVALID, desc.name, worldPlayer.index);
	botList.emplace_back(worldPlayer.index);
	if(!trainingBots.full()) trainingBots.push_back(worldPlayer.index);
	LOG("Training :: bot masters=(%d, %d) team=%d", (i32)mainClass, (i32)subClass, (i32)team);
}

void Game::OnPlayerTag(ClientHandle clientHd, ActorUID actorUID)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	// TODO: cooldown
	player.input.tag = 1;
}

f64 Game::LevelStartLeadMs() const
{
	return world.map ? (f64)world.map->levelStartLead * 1000.0 : LEVEL_START_BEFORE_GAME_MS;
}

void Game::EndGame()
{
	if(world.winnerTeam >= 0) world.FireGameEndTriggers(world.winnerTeam);
	gameEnded = true;
	tGameEnded = localTime;
	world.frozen = true;
	eastl::fixed_vector<Replication::EndPlayer,10,false> list;
	i32 red = 0, blue = 0;
	for(u32 i = 0; i < world.PlayerCount(); i++) {
		World::Player& p = world.GetPlayer(i);
		Replication::EndPlayer e;
		e.playerIndex = i;
		e.userID = p.userID;
		e.clientHd = p.clientHd;
		e.name = p.name.data();
		e.team = p.team;
		e.kills = p.kills;
		e.deaths = p.deaths;
		e.masters[0] = CreatureIndex(100000000 + (i32)p.mainClass);
		e.masters[1] = CreatureIndex(100000000 + (i32)p.subClass);
		e.assists = p.assists;
		e.bestKillStreak = p.bestKillStreak;
		e.points = p.points;
		e.ingameLevel = (u8)p.ingameLevel;
		e.damageDealt = p.damageDealt;
		e.savedPieces = p.savedPieces;
		e.damageStats = p.damageStats;
		e.statueCaptures = p.statueCaptures;
		e.visionCaptures = p.visionCaptures;
		e.monsterKills = p.monsterKills;
		e.traits[0] = p.Main().traits;
		e.traits[1] = p.Sub().traits;
		list.push_back(e);
		const i32 score = world.map && world.map->ingameLevels ? p.points : p.kills;
		if(p.team == 0) red += score; else blue += score;
	}
	if(world.map && world.map->ingameLevels) { red += world.teamBonusPoints[0]; blue += world.teamBonusPoints[1]; }
	LOG("Game :: time over, red %d - blue %d", red, blue);
	const f64 playSec = TimeDiffSec(TimeDiff(tGameStarted, localTime));
	const u32 minutes = (u32)(playSec / 60.0);
	const GameXmlContent::StageMaps* stage = GetGameXmlContent().FindStageMaps(replication.stageIndex);
	const PvpRewardRule& rule = FindPvpRewardRule(stage ? stage->joinMemberMax : 0);
	foreach(e, list) {
		if(e->clientHd == ClientHandle::INVALID) continue;
		const i32 own = e->team == 0 ? red : blue;
		const i32 other = e->team == 0 ? blue : red;
		const bool won = world.winnerTeam >= 0 ? e->team == world.winnerTeam : own > other;
		e->gold = minutes * (won ? rule.winGoldPerMin : rule.loseGoldPerMin);
		e->exp = minutes * (won ? rule.winExpPerMin : rule.loseExpPerMin);
		const World::Player& p = world.GetPlayer(e->playerIndex);
		AccountGameRecord record;
		record.gold = e->gold;
		record.exp = e->exp;
		record.won = won;
		record.draw = world.winnerTeam < 0 && own == other;
		record.kills = p.kills;
		record.assists = p.assists;
		record.bestKillStreak = p.bestKillStreak;
		record.firstBlood = p.firstBlood;
		record.damage = p.damageDealt;
		record.stageIndex = (i32)replication.stageIndex;
		record.gameType = (i32)replication.gameType;
		record.classTypes = { (i32)p.mainClass, (i32)p.subClass };
		AccountGameRecordAppend(e->name, record);
		LOG("Game :: %ls reward %u gold %u exp (%u min, %s)", e->name, e->gold, e->exp, minutes, won ? "win" : (own == other ? "draw" : "loss"));
	}
	Replication::EndGot got;
	const bool titanEnd = replication.titanMode;
	if(titanEnd) {
		for(i32 team = 0; team < 2; team++) foreach_const(s, world.giantSummons[team]) got.summons[team].push_back(*s);
	}
	replication.SendGameEnd(list.data(), (i32)list.size(), red, blue, (u32)(TimeDiffSec(TimeDiff(tGameStarted, localTime)) * 1000.0), world.winnerTeam, titanEnd ? &got : nullptr);
}

void Game::OnPlayerReturnToChannel(ClientHandle clientHd)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	World::Player& player = world.GetPlayer(found->second->playerIndex);
	i32 hubIP[4] = { Config().HostIP[0], Config().HostIP[1], Config().HostIP[2], Config().HostIP[3] };
	if(hubIP[0] == 0) { hubIP[0] = 127; hubIP[3] = 1; }
	replication.SendDoConnectChannel(clientHd, player.name.data(), hubIP, HUB_LISTEN_PORT);
}

void Game::OnClientRemoteCreated(ClientHandle clientHd, u32 seed, RemoteIdx doc)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	world.BindClientRemote(world.GetPlayer(found->second->playerIndex), doc, seed);
}

void Game::OnPlayerTakeGroundItem(ClientHandle clientHd, u32 itemUID)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	World::Player& player = world.GetPlayer(found->second->playerIndex);
	if(!world.TakeGroundItem(player, itemUID)) LOG("GroundItem :: %ls take 0x%08x refused", player.name.data(), itemUID);
}

void Game::OnPlayerSkillUpgrade(ClientHandle clientHd, i32 slot)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	World::Player& player = world.GetPlayer(found->second->playerIndex);
	if(!world.UpgradeSkillSlot(player, slot)) LOG("Upgrade :: %ls slot %d refused (points %d)", player.name.data(), slot, player.skillPoints);
}

void Game::OnTraitUpgrade(ClientHandle clientHd, ActorUID characterUID, const u8* indices, i32 count)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	World::Player& player = world.GetPlayer(found->second->playerIndex);
	const u32 error = world.UpgradeTraits(player, characterUID, indices, count);
	if(error != 0) LOG("Traits :: %ls upgrade refused (error %u)", player.name.data(), error);
	replication.SendTraitAnswer(clientHd, false, error);
}

void Game::OnRequestVote(ClientHandle clientHd, u32 type)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	world.RequestVote(world.GetPlayer(found->second->playerIndex), type);
}

void Game::OnVote(ClientHandle clientHd, u32 choice)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	world.CastVote(world.GetPlayer(found->second->playerIndex), choice);
}

void Game::OnTraitReset(ClientHandle clientHd, ActorUID characterUID)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	World::Player& player = world.GetPlayer(found->second->playerIndex);
	replication.SendTraitAnswer(clientHd, true, world.ResetTraits(player, characterUID));
}

void Game::OnPlayerSyncTeleport(ClientHandle clientHd, ActorUID actorUID, const vec3& pos)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	World::Player& player = world.GetPlayer(found->second->playerIndex);
	if(player.Main().UID != actorUID) return;
	world.TeleportPlayer(player, pos, player.input.rot.bodyYaw);
	replication.SendPlayerSyncTeleportToAll(actorUID, pos);
	LOG("Teleport :: %ls confirmed at (%.0f, %.0f, %.0f)", player.name.data(), pos.x, pos.y, pos.z);
}

void Game::OnPlayerJump(ClientHandle clientHd, ActorUID actorUID, f32 rotate, f32 moveDirX, f32 moveDirY)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	player.input.jump = 1;
}

void Game::OnPlayerWeaponFire(ClientHandle clientHd, f32 aimYaw)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	player.weaponFired = true;
	player.weaponAimYaw = aimYaw;
	world.OnWeaponAimPacket(player, aimYaw);
}

void Game::OnPlayerWeaponStateRelay(ClientHandle clientHd, ActorUID ownerUID, i32 state, u8 chargeLevel, u8 firingCombo)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	if(ownerUID == ActorUID::INVALID) ownerUID = player.Main().UID;
	world.OnWeaponState(player, state, chargeLevel, firingCombo);
	if(state != WEAPON_STATE_CHARGE) return;
	const WeaponIndex weapon = world.ActiveWeapon(player);
	replication.RelayWeaponState(clientHd, ownerUID, (i32)weapon, state, player.chargeLevel, firingCombo);
}

void Game::OnPlayerInteraction(ClientHandle clientHd, ActorUID targetUID, LocalActorID selfLocal)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);
	LOG("Interaction :: %ls self=%u target uid=%u", player.name.data(), (u32)selfLocal, (u32)targetUID);
	if(targetUID != ActorUID::INVALID) world.PlayerInteract(player, targetUID);
}

void Game::OnPlayerInteractionCancel(ClientHandle clientHd)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	world.CancelMapInteraction(world.GetPlayer(found->second->playerIndex));
}

void Game::OnPlayerToggleSkill(ClientHandle clientHd, SkillID skillID, bool on)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return;
	World::Player& player = world.GetPlayer(found->second->playerIndex);
	LOG("Toggle :: %ls request skill=%d %s", player.name.data(), (i32)skillID, on ? "on" : "off");
	if(on) world.ToggleOn(player, skillID, player.body->GetWorldPos());
	else world.ToggleOff(player);
}

bool Game::IsWeaponFireObject(ClientHandle clientHd, const wchar* name, u16 len)
{
	auto found = playerMap.find(clientHd);
	if(found == playerMap.end()) return false;
	const World::Player& player = world.GetPlayer(found->second->playerIndex);
	const WeaponIndex weapon = world.ActiveWeapon(player);
	const WeaponFiring* wf = GetGameXmlContent().FindWeaponFiring((i32)weapon, player.Main().stance);
	if(wf && wf->fireDummy.empty()) return FireObjectIs(name, len, FIRE_OBJECT_SCENE_ROOT);
	if(!wf) return false;
	auto matches = [name, len](const eastl::fixed_string<char,32,false>& dummy) {
		if(dummy.size() != len) return false;
		for(u16 i = 0; i < len; i++) if((wchar)dummy[i] != name[i]) return false;
		return true;
	};
	return matches(wf->fireDummy) || (!wf->chargeDummy.empty() && matches(wf->chargeDummy));
}

void Game::OnPlayerWeaponShot(ClientHandle clientHd, const vec3& origin, const vec2& dir, f32 distance, const u8* rawPacket, i32 rawSize)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	const i32 sid = replication.server->clientHandle2IDMap.at(clientHd);
	const auto& ip = replication.server->clientInfo[sid].ip;
	const auto& host = Config().HostIP;
	const bool isHost = ip[0] == 127 || (ip[0] == host[0] && ip[1] == host[1] && ip[2] == host[2] && ip[3] == host[3]);
	player.shotLeadSec = isHost ? 0.0f : Config().ShotLeadMs / 1000.0f;

	world.OnWeaponShotPacket(player, origin, dir, distance, rawPacket, rawSize);
}

void Game::OnPlayerCastSkill(ClientHandle clientHd, ActorUID actorUID, const PlayerInputCastSkill& cast, const Cl::CQ_PlayerCastSkill::PosStruct& posInfo)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	player.input.cast = cast;
	player.input.cast.clientTime = posInfo.clientTime;

	RotationHumanoid rot = { posInfo.rot.x, posInfo.rot.y, posInfo.rot.z };
	// TODO: convert clientTime to localTime
	OnPlayerUpdatePosition(clientHd, actorUID, f2v(posInfo.pos), f2v(posInfo.moveDir), rot, posInfo.speed, ActionStateID::INVALID, 0);

	LOG("OnPlayerCastSkill :: (%f, %f, %f) targets=%d rot=(%.3f, %.3f, %.3f) moveDir=(%.2f, %.2f)", cast.pos.x, cast.pos.y, cast.pos.z, (i32)cast.targetList.size(), posInfo.rot.x, posInfo.rot.y, posInfo.rot.z, posInfo.moveDir.x, posInfo.moveDir.y);
}

void Game::OnPlayerExecuteSkill(ClientHandle clientHd, SkillID skillID, const vec3& targetPos, LocalActorID target)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	if(player.casting.skillID == skillID) {
		if(targetPos.x != 0 || targetPos.y != 0) player.casting.castPos = targetPos;
		if(target != LocalActorID::INVALID) {
			const ActorUID uid = replication.GetWorldActorUID(clientHd, target);
			if(uid != ActorUID::INVALID) player.casting.target = uid;
		}
	}
	LOG("Cast :: %ls execute request skill=%d (casting %d) pos=(%.0f, %.0f) target=%d", player.name.data(), (i32)skillID, (i32)player.casting.skillID, targetPos.x, targetPos.y, (i32)player.casting.target);
	replication.SendExecuteSkillAck(clientHd, player.Main().UID, skillID);
	if(player.casting.skillID == skillID) world.ReleaseCast(player);
}

void Game::OnPlayerCancelCast(ClientHandle clientHd, SkillID skillID)
{
	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	if(player.casting.skillID == skillID && skillID != SkillID::INVALID) world.CancelCast(player, "client");
}

void Game::OnPlayerGameIsReady(ClientHandle clientHd)
{
	const i32 READY_WAIT = 3000;

	Player& p = *playerMap.at(clientHd);
	World::Player& player = world.GetPlayer(p.playerIndex);
	ASSERT(player.clientHd == clientHd);

	if(zoneLoading) {
		zoneLoading = false;
		world.StartLevelEvents();
		LOG("Stage :: zone %d ready, level events started", zoneIndex);
	}
	if(phase == Phase::WaitingForFirstPlayer) {
		phase = Phase::WaitingForReady;
		phaseTime = TimeAdd(localTime, TimeMsToTime(READY_WAIT));
		world.combatLocked = !Config().DevShowcase;
		replication.SendGameReady(clientHd, READY_WAIT, 0, player.userID);
	}
	else {
		replication.SendGameReady(clientHd, READY_WAIT, MAX(0, READY_WAIT - (i32)TimeDurationMs(localTime, phaseTime)), player.userID);
	}
}


bool Game::ParseChatCommand(ClientHandle clientHd, const wchar* msg, const i32 len)
{
	if(!Config().DevMode) return false; // don't allow command when dev mode is not enabled

	// null terminate
	eastl::fixed_string<wchar,1024,true> msgBuff;
	msgBuff.assign(msg, len);
	msg = msgBuff.data();

	Player& p = *playerMap.at(clientHd);

	static ActorUID lastLegoActorUID = ActorUID::INVALID;

	if(msg[0] == L'!') {
		msg++;

		if(EA::StdC::Strcmp(msg, L"lego") == 0) {
			// TODO: restore?
			/*
			World::Player* playerActor = world.FindPlayer(player.actorUID); // TODO: find currently active actor
			ASSERT(playerActor);
			const vec3 pos = playerActor->Main().body->pos;

			World::Player& actor = world.SpawnPlayerMasters(-1, L"legomage15", L"MEME", (ClassType)18, SkinIndex::DEFAULT, ClassType::Lua, SkinIndex::DEFAULT, pos);
			actor.input.rot = playerActor->input.rot;

			// trigger second emote
			actor.input.actionState = ActionStateID::EMOTION_BEHAVIORSTATE;
			actor.input.actionParam1 = 2;

			lastLegoActorUID = actor.UID;

			SendDbgMsg(clientID, LFMT(L"Actor spawned at (%g, %g, %g)", pos.x, pos.y, pos.z));
			return true;
			*/
		}

		if(EA::StdC::Strcmp(msg, L"delete") == 0) {
			// TODO: restore?
			/*
			world.DestroyPlayerActor(lastLegoActorUID);

			SendDbgMsg(clientID, LFMT(L"Actor destroyed (%u)", lastLegoActorUID));
			return true;
			*/
		}

		i32 event = 0;
		if(EA::StdC::Sscanf(msg, L"e %d", &event) == 1) {
			replication.SendChatMessageToClient(clientHd, L"Dbg", EChatType::NOTICE_CHAT, LFMT(L"Event (%d)", event));
			replication.SendClientLevelEvent(clientHd, event);
			return true;
		}

		f32 tpX = 0;
		f32 tpY = 0;
		f32 tpZ = 0;
		const i32 tpArgs = EA::StdC::Sscanf(msg, L"tp %f %f %f", &tpX, &tpY, &tpZ);
		if(tpArgs >= 2) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex);
			const vec3 to(tpX, tpY, tpArgs >= 3 ? tpZ : playerActor.body->GetWorldPos().z);
			world.TeleportPlayer(playerActor, to, playerActor.input.rot.bodyYaw);
			replication.SendTeleportRequest(clientHd, playerActor.Main().UID, to);
			SendDbgMsg(clientHd, LFMT(L"Teleported to (%g, %g, %g)", to.x, to.y, to.z));
			return true;
		}

		i32 killTag = 0;
		if(EA::StdC::Sscanf(msg, L"killtag %d", &killTag) == 1) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex);
			i32 killed = 0;
			foreach(npc, world.actorNpcList) {
				if(npc->tagID != killTag || npc->hp <= 0 || npc->removeAt != Time::ZERO || npc->ownerUID != ActorUID::INVALID) continue;
				world.MonsterDie(*npc, playerActor.Main().UID, CreatureIndex(100000000 + (i32)playerActor.Main().classType));
				killed++;
			}
			SendDbgMsg(clientHd, LFMT(L"Killed %d monster(s) with tag %d", killed, killTag));
			return true;
		}

		if(EA::StdC::Strcmp(msg, L"god") == 0) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex);
			playerActor.devGod = !playerActor.devGod;
			SendDbgMsg(clientHd, playerActor.devGod ? L"God mode on" : L"God mode off");
			return true;
		}

		i32 piecesTeam = 0, piecesAdd = 0;
		if(EA::StdC::Sscanf(msg, L"pieces %d %d", &piecesTeam, &piecesAdd) == 2 && (piecesTeam == 0 || piecesTeam == 1)) {
			world.AddTeamPieces(piecesTeam, piecesAdd);
			SendDbgMsg(clientHd, LFMT(L"Team %d +%d titan pieces", piecesTeam, piecesAdd));
			return true;
		}

		i32 pointsTeam = 0, pointsAdd = 0;
		if(EA::StdC::Sscanf(msg, L"points %d %d", &pointsTeam, &pointsAdd) == 2 && (pointsTeam == 0 || pointsTeam == 1)) {
			world.AddTeamPoints(pointsTeam, pointsAdd);
			SendDbgMsg(clientHd, LFMT(L"Team %d +%d points", pointsTeam, pointsAdd));
			return true;
		}

		if(EA::StdC::Strcmp(msg, L"vision") == 0) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex);
			playerActor.devVision = !playerActor.devVision;
			SendDbgMsg(clientHd, playerActor.devVision ? L"Full vision on" : L"Full vision off");
			return true;
		}

		if(EA::StdC::Strcmp(msg, L"titan") == 0) {
			world.RequestVote(world.GetPlayer(p.playerIndex), 1);
			SendDbgMsg(clientHd, L"Titan vote requested");
			return true;
		}

		i32 itemIndex = 0;
		if(EA::StdC::Sscanf(msg, L"item %d", &itemIndex) == 1) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex);
			world.SpawnGroundItemAt(itemIndex, playerActor.body->GetWorldPos(), -1);
			SendDbgMsg(clientHd, LFMT(L"Item %d dropped", itemIndex));
			return true;
		}

		i32 hpTag = 0, hpPercent = 0;
		if(EA::StdC::Sscanf(msg, L"monhp %d %d", &hpTag, &hpPercent) == 2) {
			i32 changed = 0;
			foreach(npc, world.actorNpcList) {
				if(npc->tagID != hpTag || npc->hp <= 0) continue;
				npc->hp = MAX(npc->hpMax * hpPercent / 100, 1);
				replication.SendHpToAll(npc->UID, npc->hp, npc->hpMax);
				changed++;
			}
			SendDbgMsg(clientHd, LFMT(L"%d monster(s) with tag %d at %d%% hp", changed, hpTag, hpPercent));
			return true;
		}

		i32 provokeTag = 0;
		if(EA::StdC::Sscanf(msg, L"provoke %d", &provokeTag) == 1) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex);
			i32 provoked = 0;
			foreach(npc, world.actorNpcList) {
				if(npc->tagID != provokeTag || npc->hp <= 0 || npc->ownerUID != ActorUID::INVALID) continue;
				npc->monsterTarget = playerActor.Main().UID;
				npc->monsterReturning = false;
				provoked++;
			}
			SendDbgMsg(clientHd, LFMT(L"Provoked %d monster(s) with tag %d", provoked, provokeTag));
			return true;
		}

		i32 enemyCreature = 0;
		if(EA::StdC::Sscanf(msg, L"enemysummon %d", &enemyCreature) == 1) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex);
			foreach_const(e, world.players) {
				if(e->team == playerActor.team || world.pendingSummons.full()) continue;
				World::PendingSummon ps;
				ps.time = world.localTime;
				ps.creature = enemyCreature;
				ps.ownerIndex = e->index;
				ps.pos = playerActor.body->GetWorldPos() + vec3(150, 0, 0);
				ps.rot = vec3(0);
				world.pendingSummons.push_back(ps);
				SendDbgMsg(clientHd, LFMT(L"Creature %d summoned for %ls", enemyCreature, e->name.data()));
				break;
			}
			return true;
		}

		i32 eventSeq = 0;
		if(EA::StdC::Sscanf(msg, L"es %d", &eventSeq) == 1) {
			replication.SendChatMessageToClient(clientHd, L"Dbg", EChatType::NOTICE_CHAT, LFMT(L"Event Sequence (%d)", eventSeq));
			replication.SendClientLevelEventSeq(clientHd, eventSeq);
			return true;
		}

		// Spawn a wall
		// NOTE: Walls probably spawn on an "area point" (Area.xml)
		if(EA::StdC::Strcmp(msg, L"wall") == 0) {
			World::Player& playerActor = world.GetPlayer(p.playerIndex); // TODO: find currently active actor
			const vec3 pos = playerActor.body->GetWorldPos();

			// is lovalID used for anything?
			static i32 localID = 37;
			World::ActorDynamic& dyn = world.SpawnDynamic(CreatureIndex(110042602), localID++);
			dyn.pos = pos;

			SendDbgMsg(clientHd, LFMT(L"Actor spawned at (%g, %g, %g)", pos.x, pos.y, pos.z));
			return true;
		}

		// TODO:
		// - Spawn all walls
		// - Make them have a physical body (box)
	}

	return false;
}

void Game::SendDbgMsg(ClientHandle clientHd, const wchar* msg)
{
	replication.SendChatMessageToClient(clientHd, L"System", EChatType::NOTICE, msg);
}
