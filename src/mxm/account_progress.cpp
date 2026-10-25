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
	fprintf(f, "Level=%u\nExp=%u\nGold=%lld\n", (u32)progress.level, progress.levelExp, (long long)progress.gold);
	fclose(f);
	remove(path);
	rename(tmp, path);
}

void AccountRewardAppend(const wchar* nickname, i64 gold, u32 exp)
{
	std::lock_guard<std::mutex> lock(g_accountFileMutex);
	_mkdir(ACCOUNT_DIR);
	char path[512];
	AccountPath(nickname, "reward", path, sizeof(path));
	FILE* f = fopen(path, "ab");
	ASSERT(f);
	fprintf(f, "%lld %u\n", (long long)gold, exp);
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
	long long gold = 0;
	u32 exp = 0;
	while(fscanf(f, "%lld %u", &gold, &exp) == 2) {
		reward.gold += (i64)gold;
		reward.exp += exp;
		reward.games++;
	}
	fclose(f);
	remove(path);
	return reward;
}
