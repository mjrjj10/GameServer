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
	u32 honors = 0;
	i64 ccoin = 0;
	eastl::array<i32,3> inventoryLines = { 4, 4, 4 };
	i32 activityPoint = 0;
	u8 activityRewarded = 0;
	i64 activityPeriod = 0;
};

struct AccountItem
{
	u32 uid = 0;
	u8 invenType = 0;
	i32 slot = 0;
	i32 docIndex = 0;
	i32 count = 0;
};

struct AccountInventory
{
	u32 nextUid = 0x40000001;
	eastl::fixed_vector<AccountItem,256,true> items;
};

AccountInventory AccountInventoryLoad(const wchar* nickname);
void AccountInventorySave(const wchar* nickname, const AccountInventory& inventory);
static const i32 MEASURED_INVENTORY_FIRST_SLOT = 200;
typedef eastl::fixed_vector<AccountItem,8,false> AccountItemList;
void AccountInventoryAdd(AccountInventory* inventory, i32 docIndex, i32 count, i32 maxStack, AccountItemList* touched);

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
	u32 playSeconds = 0;
	i32 guildPoints = 0;
};

struct AccountReward
{
	i64 gold = 0;
	u32 exp = 0;
	i32 games = 0;
	u32 playSeconds = 0;
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
	eastl::fixed_vector<WideString,64,true> favorites;
	eastl::fixed_vector<WideString,64,true> blocked;
	WideString statusMessage;
};

bool AccountExists(const wchar* nickname);
AccountFriends AccountFriendsLoad(const wchar* nickname);
void AccountFriendsSave(const wchar* nickname, const AccountFriends& friends);
struct AccountStages
{
	eastl::fixed_vector<i32,512,true> cleared;
};

struct AccountSettingsBlob
{
	eastl::fixed_vector<u8,4096,false> data;
};

AccountSettingsBlob AccountSettingsLoad(const wchar* nickname, u8 settingType);
void AccountSettingsSave(const wchar* nickname, u8 settingType, const u8* data, i32 size);
AccountStages AccountStagesLoad(const wchar* nickname);
void AccountStagesSave(const wchar* nickname, const AccountStages& stages);
AccountProgress AccountProgressLoad(const wchar* nickname);
void AccountProgressSave(const wchar* nickname, const AccountProgress& progress);
void AccountGameRecordAppend(const wchar* nickname, const AccountGameRecord& record);
AccountReward AccountRewardTake(const wchar* nickname);
void AccountRewardItemAppend(const wchar* nickname, i32 docIndex, i32 count);
eastl::fixed_vector<eastl::pair<i32,i32>,64,true> AccountRewardItemsTake(const wchar* nickname);
void AccountStageCleared(const wchar* nickname, i32 stageIndex);
AccountMissions AccountMissionsLoad(const wchar* nickname);
void AccountMissionsSave(const wchar* nickname, const AccountMissions& missions);
