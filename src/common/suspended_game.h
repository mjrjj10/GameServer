#pragma once
#include <common/base.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <io.h>
#include <mutex>

struct SuspendedGame
{
	u64 sortieUID = 0;
	u64 gameAccountUID = 0;
	i32 stage = 0;
	i32 gameType = 0;
	i32 gameDefinition = 0;
	u8 ip[4] = {};
	u16 port = 0;
};

static const char* SUSPENDED_GAME_DIR = "accounts";
static const i64 HYP_SUSPENDED_GAME_MAX_AGE_SEC = 3600;

inline void SuspendedAccountPath(const wchar* nickname, char* out, i32 outSize)
{
	i32 o = snprintf(out, outSize, "%s/", SUSPENDED_GAME_DIR);
	for(const wchar* c = nickname; *c && o < outSize - 16; c++) o += snprintf(out + o, outSize - o, "%04x", (u32)(u16)*c);
	snprintf(out + o, outSize - o, ".suspended");
}

inline void SuspendedSortiePath(u64 sortieUID, char* out, i32 outSize)
{
	snprintf(out, outSize, "%s/sortie_%llu.txt", SUSPENDED_GAME_DIR, (unsigned long long)sortieUID);
}

inline void SuspendedAccountWrite(const wchar* nickname, u64 sortieUID, u64 gameAccountUID)
{
	_mkdir(SUSPENDED_GAME_DIR);
	char path[512];
	SuspendedAccountPath(nickname, path, sizeof(path));
	FILE* f = fopen(path, "wb");
	if(!f) return;
	fprintf(f, "%llu %llu\n", (unsigned long long)sortieUID, (unsigned long long)gameAccountUID);
	fclose(f);
}

inline void SuspendedAccountDelete(const wchar* nickname)
{
	char path[512];
	SuspendedAccountPath(nickname, path, sizeof(path));
	remove(path);
}

inline void SuspendedSortieWrite(u64 sortieUID, const char* key, const char* fmt, i64 a, i64 b, i64 c, i64 d, i64 e)
{
	static std::mutex writeMutex;
	std::lock_guard<std::mutex> lock(writeMutex);
	_mkdir(SUSPENDED_GAME_DIR);
	char path[512];
	SuspendedSortiePath(sortieUID, path, sizeof(path));
	char lines[2][128] = {};
	FILE* f = fopen(path, "rb");
	if(f) {
		char line[128];
		while(fgets(line, sizeof(line), f)) {
			if(strncmp(line, "game ", 5) == 0) snprintf(lines[0], sizeof(lines[0]), "%s", line);
			else if(strncmp(line, "server ", 7) == 0) snprintf(lines[1], sizeof(lines[1]), "%s", line);
		}
		fclose(f);
	}
	char* slot = strcmp(key, "game") == 0 ? lines[0] : lines[1];
	i32 o = snprintf(slot, 128, "%s ", key);
	o += snprintf(slot + o, 128 - o, fmt, a, b, c, d, e);
	snprintf(slot + o, 128 - o, " %lld\n", (long long)time(nullptr));
	f = fopen(path, "wb");
	if(!f) return;
	fputs(lines[0], f);
	fputs(lines[1], f);
	fclose(f);
}

inline void SuspendedSortieDelete(u64 sortieUID)
{
	char path[512];
	SuspendedSortiePath(sortieUID, path, sizeof(path));
	remove(path);
}

inline bool SuspendedGameRead(const wchar* nickname, SuspendedGame* out)
{
	char path[512];
	SuspendedAccountPath(nickname, path, sizeof(path));
	FILE* f = fopen(path, "rb");
	if(!f) return false;
	unsigned long long sortie = 0, account = 0;
	const bool ok = fscanf(f, "%llu %llu", &sortie, &account) == 2;
	fclose(f);
	if(!ok) return false;
	out->sortieUID = sortie;
	out->gameAccountUID = account;
	SuspendedSortiePath(sortie, path, sizeof(path));
	f = fopen(path, "rb");
	if(!f) return false;
	bool game = false, server = false;
	const i64 now = (i64)time(nullptr);
	char line[128];
	while(fgets(line, sizeof(line), f)) {
		long long at = 0;
		i32 v[5] = {};
		if(sscanf(line, "game %d %d %d %lld", &v[0], &v[1], &v[2], &at) == 4 && now - at <= HYP_SUSPENDED_GAME_MAX_AGE_SEC) {
			out->stage = v[0];
			out->gameType = v[1];
			out->gameDefinition = v[2];
			game = true;
		}
		else if(sscanf(line, "server %d %d %d %d %d %lld", &v[0], &v[1], &v[2], &v[3], &v[4], &at) == 6 && now - at <= HYP_SUSPENDED_GAME_MAX_AGE_SEC) {
			for(i32 i = 0; i < 4; i++) out->ip[i] = (u8)v[i];
			out->port = (u16)v[4];
			server = true;
		}
	}
	fclose(f);
	return game && server;
}

inline i32 SuspendedGamesClear()
{
	i32 removed = 0;
	const char* patterns[] = { "sortie_*.txt", "*.suspended" };
	for(const char* pattern : patterns) {
		char query[256];
		snprintf(query, sizeof(query), "%s/%s", SUSPENDED_GAME_DIR, pattern);
		_finddata_t found;
		const intptr_t h = _findfirst(query, &found);
		if(h == -1) continue;
		do {
			char path[512];
			snprintf(path, sizeof(path), "%s/%s", SUSPENDED_GAME_DIR, found.name);
			if(remove(path) == 0) removed++;
		} while(_findnext(h, &found) == 0);
		_findclose(h);
	}
	return removed;
}
