#include "account_progress.h"
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <mutex>

static const char* ACCOUNT_DIR = "accounts";
static std::mutex g_accountFileMutex;

static void AccountPath(const wchar* nickname, const char* ext, char* out, i32 outSize)
{
	i32 o = snprintf(out, outSize, "%s/", ACCOUNT_DIR);
	for(const wchar* c = nickname; *c && o < outSize - 8; c++) o += snprintf(out + o, outSize - o, "%04x", (u32)(u16)*c);
	snprintf(out + o, outSize - o, ".%s", ext);
}

AccountProgress AccountProgressLoad(const wchar* nickname)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	AccountProgress progress;
	char path[512];
	AccountPath(nickname, "txt", path, sizeof(path));
	FILE* f = fopen(path, "rb");
	if(!f) return progress;
	char line[256];
	while(fgets(line, sizeof(line), f)) {
		u32 v32 = 0;
		long long v64 = 0;
		if(sscanf(line, "Level=%u", &v32) == 1) progress.level = (u16)v32;
		else if(sscanf(line, "Exp=%u", &v32) == 1) progress.levelExp = v32;
		else if(sscanf(line, "Gold=%lld", &v64) == 1) progress.gold = (i64)v64;
		else if(sscanf(line, "Wins=%u", &v32) == 1) progress.wins = v32;
		else if(sscanf(line, "Draws=%u", &v32) == 1) progress.draws = v32;
		else if(sscanf(line, "Losses=%u", &v32) == 1) progress.losses = v32;
		else if(sscanf(line, "Honors=%u", &v32) == 1) progress.honors = v32;
	}
	fclose(f);
	return progress;
}

void AccountProgressSave(const wchar* nickname, const AccountProgress& progress)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	_mkdir(ACCOUNT_DIR);
	char path[512];
	char tmp[520];
	AccountPath(nickname, "txt", path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE* f = fopen(tmp, "wb");
	ASSERT(f);
	fprintf(f, "Level=%u\nExp=%u\nGold=%lld\nWins=%u\nDraws=%u\nLosses=%u\nHonors=%u\n", (u32)progress.level, progress.levelExp, (long long)progress.gold, progress.wins, progress.draws, progress.losses, progress.honors);
	fclose(f);
	remove(path);
	rename(tmp, path);
}

void AccountGameRecordAppend(const wchar* nickname, const AccountGameRecord& record)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	_mkdir(ACCOUNT_DIR);
	char path[512];
	AccountPath(nickname, "reward", path, sizeof(path));
	FILE* f = fopen(path, "ab");
	ASSERT(f);
	fprintf(f, "%lld %u %u %d %d %d %u %lld %d %d %d %d %d %u\n", (long long)record.gold, record.exp, (u32)record.won, record.kills, record.assists, record.bestKillStreak,
		(u32)record.firstBlood, (long long)record.damage, record.titanScore, record.stageIndex, record.gameType, record.classTypes[0], record.classTypes[1], (u32)record.draw);
	fclose(f);
}

AccountReward AccountRewardTake(const wchar* nickname)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	AccountReward reward;
	char path[512];
	AccountPath(nickname, "reward", path, sizeof(path));
	FILE* f = fopen(path, "rb");
	if(!f) return reward;
	char line[512];
	while(fgets(line, sizeof(line), f)) {
		AccountGameRecord record;
		long long gold = 0, damage = 0;
		u32 won = 0, firstBlood = 0, draw = 0;
		const i32 n = sscanf(line, "%lld %u %u %d %d %d %u %lld %d %d %d %d %d %u", &gold, &record.exp, &won, &record.kills, &record.assists, &record.bestKillStreak,
			&firstBlood, &damage, &record.titanScore, &record.stageIndex, &record.gameType, &record.classTypes[0], &record.classTypes[1], &draw);
		if(n < 2) continue;
		record.gold = (i64)gold;
		record.won = (u8)won;
		record.draw = (u8)draw;
		record.firstBlood = (u8)firstBlood;
		record.damage = (i64)damage;
		reward.gold += record.gold;
		reward.exp += record.exp;
		reward.games++;
		if(n >= 13 && !reward.records.full()) reward.records.push_back(record);
	}
	fclose(f);
	remove(path);
	return reward;
}

AccountMissions AccountMissionsLoad(const wchar* nickname)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	AccountMissions missions;
	char path[512];
	AccountPath(nickname, "missions", path, sizeof(path));
	FILE* f = fopen(path, "rb");
	if(!f) return missions;
	char line[256];
	while(fgets(line, sizeof(line), f)) {
		long long period = 0, progress = 0;
		i32 id = 0, step = 0;
		u32 rewarded = 0, changed = 0;
		if(sscanf(line, "M %d %lld %lld %u", &id, &period, &progress, &rewarded) == 4) {
			if(missions.states.full()) continue;
			AccountMissionState state;
			state.ID = id;
			state.period = (i64)period;
			state.progress = (i64)progress;
			state.rewarded = (u8)rewarded;
			missions.states.push_back(state);
		}
		else if(sscanf(line, "C %d %lld %d %u", &step, &period, &id, &changed) == 4) {
			if(missions.dailyChanges.full()) continue;
			AccountDailyChange change;
			change.step = step;
			change.period = (i64)period;
			change.missionID = id;
			change.changedCount = (u16)changed;
			missions.dailyChanges.push_back(change);
		}
	}
	fclose(f);
	return missions;
}

void AccountMissionsSave(const wchar* nickname, const AccountMissions& missions)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	_mkdir(ACCOUNT_DIR);
	char path[512];
	char tmp[520];
	AccountPath(nickname, "missions", path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE* f = fopen(tmp, "wb");
	ASSERT(f);
	foreach_const(s, missions.states) fprintf(f, "M %d %lld %lld %u\n", s->ID, (long long)s->period, (long long)s->progress, (u32)s->rewarded);
	foreach_const(c, missions.dailyChanges) fprintf(f, "C %d %lld %d %u\n", c->step, (long long)c->period, c->missionID, (u32)c->changedCount);
	fclose(f);
	remove(path);
	rename(tmp, path);
}

bool AccountExists(const wchar* nickname)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	char path[512];
	AccountPath(nickname, "txt", path, sizeof(path));
	FILE* f = fopen(path, "rb");
	if(!f) return false;
	fclose(f);
	return true;
}

static bool ParseHexName(const char* hex, WideString* out)
{
	out->clear();
	for(const char* c = hex; *c && *c != '\n' && *c != '\r'; c += 4) {
		u32 v = 0;
		if(!c[1] || !c[2] || !c[3] || sscanf(c, "%4x", &v) != 1) return false;
		out->push_back((wchar)v);
	}
	return !out->empty();
}

static void WriteHexName(FILE* f, const WideString& name)
{
	foreach_const(c, name) fprintf(f, "%04x", (u32)(u16)*c);
}

AccountFriends AccountFriendsLoad(const wchar* nickname)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	AccountFriends friends;
	char path[512];
	AccountPath(nickname, "friends", path, sizeof(path));
	FILE* f = fopen(path, "rb");
	if(!f) return friends;
	char line[512];
	while(fgets(line, sizeof(line), f)) {
		WideString name;
		if(line[0] == 0 || line[1] != ' ' || !ParseHexName(line + 2, &name)) continue;
		if(line[0] == 'F' && !friends.friends.full()) friends.friends.push_back(name);
		else if(line[0] == 'R' && !friends.requests.full()) friends.requests.push_back(name);
		else if(line[0] == 'V' && !friends.favorites.full()) friends.favorites.push_back(name);
		else if(line[0] == 'B' && !friends.blocked.full()) friends.blocked.push_back(name);
		else if(line[0] == 'S') friends.statusMessage = name;
	}
	fclose(f);
	return friends;
}

void AccountFriendsSave(const wchar* nickname, const AccountFriends& friends)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	_mkdir(ACCOUNT_DIR);
	char path[512];
	char tmp[520];
	AccountPath(nickname, "friends", path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	FILE* f = fopen(tmp, "wb");
	ASSERT(f);
	foreach_const(n, friends.friends) {
		fputs("F ", f);
		WriteHexName(f, *n);
		fputc('\n', f);
	}
	foreach_const(n, friends.requests) {
		fputs("R ", f);
		WriteHexName(f, *n);
		fputc('\n', f);
	}
	foreach_const(n, friends.favorites) {
		fputs("V ", f);
		WriteHexName(f, *n);
		fputc('\n', f);
	}
	foreach_const(n, friends.blocked) {
		fputs("B ", f);
		WriteHexName(f, *n);
		fputc('\n', f);
	}
	if(!friends.statusMessage.empty()) {
		fputs("S ", f);
		WriteHexName(f, friends.statusMessage);
		fputc('\n', f);
	}
	fclose(f);
	remove(path);
	rename(tmp, path);
}
