#pragma once
#include <common/base.h>
#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>
#include <common/utils.h>

struct AccountProgress
{
	u16 level = 1;
	u32 levelExp = 0;
	i64 gold = 116472;
	u32 wins = 0;
	u32 draws = 0;
	u32 losses = 0;
};

struct AccountGameRecord
{
	i64 gold = 0;
	u32 exp = 0;
	u8 won = 0;
	u8 draw = 0;
	i32 kills = 0;
	i32 assists = 0;
	i32 bestKillStreak = 0;
	u8 firstBlood = 0;
	i64 damage = 0;
	i32 titanScore = 0;
	i32 stageIndex = 0;
	i32 gameType = 0;
	eastl::array<i32,2> classTypes = { 0, 0 };
};

struct AccountReward
{
	i64 gold = 0;
	u32 exp = 0;
	i32 games = 0;
	eastl::fixed_vector<AccountGameRecord,16,true> records;
};

struct AccountMissionState
{
	i32 ID = 0;
	i64 period = 0;
	i64 progress = 0;
	u8 rewarded = 0;
};

struct AccountDailyChange
{
	i32 step = 0;
	i64 period = 0;
	i32 missionID = 0;
	u16 changedCount = 0;
};

struct AccountMissions
{
	eastl::fixed_vector<AccountMissionState,64,true> states;
	eastl::fixed_vector<AccountDailyChange,8,true> dailyChanges;
};

struct AccountFriends
{
	eastl::fixed_vector<WideString,64,true> friends;
	eastl::fixed_vector<WideString,32,true> requests;
};

bool AccountExists(const wchar* nickname);
AccountFriends AccountFriendsLoad(const wchar* nickname);
void AccountFriendsSave(const wchar* nickname, const AccountFriends& friends);
AccountProgress AccountProgressLoad(const wchar* nickname);
void AccountProgressSave(const wchar* nickname, const AccountProgress& progress);
void AccountGameRecordAppend(const wchar* nickname, const AccountGameRecord& record);
AccountReward AccountRewardTake(const wchar* nickname);
AccountMissions AccountMissionsLoad(const wchar* nickname);
void AccountMissionsSave(const wchar* nickname, const AccountMissions& missions);
