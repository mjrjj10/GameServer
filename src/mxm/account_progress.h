#pragma once
#include <common/base.h>

struct AccountProgress
{
	u16 level = 1;
	u32 levelExp = 0;
	i64 gold = 116472;
};

struct AccountReward
{
	i64 gold = 0;
	u32 exp = 0;
	i32 games = 0;
};

AccountProgress AccountProgressLoad(const wchar* nickname);
void AccountProgressSave(const wchar* nickname, const AccountProgress& progress);
void AccountRewardAppend(const wchar* nickname, i64 gold, u32 exp);
AccountReward AccountRewardTake(const wchar* nickname);
