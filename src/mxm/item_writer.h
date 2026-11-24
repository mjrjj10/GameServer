#pragma once
#include <mxm/account_progress.h>
#include <mxm/game_content.h>

template<typename Writer>
inline void WriteInventoryItem(Writer& packet, const AccountItem& item)
{
	packet.template Write<u32>(item.uid);
	packet.template Write<u8>(item.invenType);
	packet.template Write<i32>(item.slot);
	packet.template Write<i32>(item.docIndex);
	packet.template Write<i32>(item.count);
	const GameXmlContent::GearItem* gear = item.docIndex != 0 ? GetGameXmlContent().FindGearItem(item.docIndex) : nullptr;
	const bool withProperties = gear && gear->optionGroup > 0 && !gear->properties.empty();
	packet.template Write<i32>(item.docIndex == 0 ? 0 : (withProperties ? gear->optionGroup : -1));
	packet.template Write<u8>(0);
	packet.template Write<i64>(0);
	packet.template Write<u16>(withProperties ? (u16)gear->properties.size() : 0);
	if(withProperties) {
		for(const GameXmlContent::ItemProperty& prop : gear->properties) {
			packet.template Write<u8>(prop.type);
			packet.template Write<i32>(prop.typeDetail);
			packet.template Write<u8>(prop.valueType);
			packet.template Write<f32>(prop.value);
			packet.template Write<u8>(1);
		}
	}
}

template<typename Writer>
inline void WriteMasterGearPages(Writer& packet, const wchar* nickname)
{
	const AccountGears gears = AccountGearsLoad(nickname);
	const AccountInventory inventory = AccountInventoryLoad(nickname);
	packet.template Write<u16>((u16)gears.pages.size());
	for(i32 i = 0; i < (i32)gears.pages.size(); i++) {
		const AccountGearPage& page = gears.pages[i];
		packet.template Write<u8>((u8)(i + 1));
		packet.WriteStringObj(page.name.data());
		packet.template Write<u16>((u16)page.items.size());
		for(u32 uid : page.items) {
			i32 type = -1;
			foreach_const(it, inventory.items) {
				if(uid == 0 || it->uid != uid) continue;
				if(const GameXmlContent::GearItem* gear = GetGameXmlContent().FindGearItem(it->docIndex)) type = gear->slotType;
			}
			packet.template Write<i32>(type);
			packet.template Write<u32>(type >= 0 ? uid : 0);
		}
	}
}
