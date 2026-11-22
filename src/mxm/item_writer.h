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
