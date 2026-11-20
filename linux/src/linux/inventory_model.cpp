#include "inventory_model.hpp"
#include "gc_wire.hpp"
#include <limits>
#include <utility>

namespace LinuxSteam {
namespace {
constexpr size_t max_items=100000, max_text=1024*1024;

bool decode_attribute(GCWire::Slice input,InventoryAttribute& attribute){
    size_t pos=0;
    while(pos<input.size){
        GCWire::Field f;
        if(!GCWire::next(input,pos,f))return false;
        if(f.number==1 && f.wire==0){
            if(f.value>std::numeric_limits<uint32_t>::max())return false;
            attribute.definition=static_cast<uint32_t>(f.value);
        }else if(f.number==3 && f.wire==2){
            if(f.bytes.size>max_text)return false;
            attribute.value_bytes.assign(f.bytes.data,f.bytes.data+f.bytes.size);
        }
    }
    return true;
}

bool decode_equipped(GCWire::Slice input,EquippedState& equipped){
    size_t pos=0;
    while(pos<input.size){
        GCWire::Field f;
        if(!GCWire::next(input,pos,f))return false;
        if((f.number==1||f.number==2)&&f.wire==0){
            if(f.value>std::numeric_limits<uint32_t>::max())return false;
            if(f.number==1)equipped.player_class=static_cast<uint32_t>(f.value);
            else equipped.slot=static_cast<uint32_t>(f.value);
        }
    }
    return true;
}

bool decode_item(GCWire::Slice input,InventoryItem& item) {
    size_t pos=0;bool has_id=false;
    while(pos<input.size){
        GCWire::Field f;
        if(!GCWire::next(input,pos,f))return false;
        if(f.wire==0){
            if(f.number==1){item.id=f.value;has_id=true;continue;}
            if(f.number>=3 && f.number<=9){
                if(f.value>std::numeric_limits<uint32_t>::max())return false;
                auto v=static_cast<uint32_t>(f.value);
                switch(f.number){case 3:item.inventory=v;break;case 4:item.definition=v;break;
                    case 5:item.quantity=v;break;case 6:item.level=v;break;case 7:item.quality=v;break;
                    case 8:item.flags=v;break;case 9:item.origin=v;break;}
            }
        }else if(f.wire==2 && (f.number==10||f.number==11)){
            if(f.bytes.size>max_text)return false;
            std::string value(reinterpret_cast<const char*>(f.bytes.data),f.bytes.size);
            if(f.number==10)item.custom_name=std::move(value);
            else item.custom_description=std::move(value);
        }else if(f.wire==2 && f.number==12){
            if(item.attributes.size()>=10000)return false;
            InventoryAttribute attribute;
            if(!decode_attribute(f.bytes,attribute))return false;
            item.attributes.push_back(std::move(attribute));
        }else if(f.wire==2 && f.number==18){
            if(item.equipped.size()>=10000)return false;
            EquippedState equipped;
            if(!decode_equipped(f.bytes,equipped))return false;
            item.equipped.push_back(equipped);
        }
    }
    if(!has_id || !item.id)return false;
    item.protobuf.assign(input.data,input.data+input.size);
    return true;
}

bool decode_group(GCWire::Slice group,std::vector<InventoryItem>& items) {
    size_t pos=0;uint64_t type=0;bool has_type=false;
    std::vector<GCWire::Slice> objects;
    while(pos<group.size){
        GCWire::Field f;
        if(!GCWire::next(group,pos,f))return false;
        if(f.number==1&&f.wire==0){type=f.value;has_type=true;}
        if(f.number==2&&f.wire==2){
            if(objects.size()>=max_items)return false;
            objects.push_back(f.bytes);
        }
    }
    if(!has_type || type!=1)return true;
    if(objects.size()>max_items-items.size())return false;
    for(const auto& object:objects){
        InventoryItem item;
        if(!decode_item(object,item))return false;
        items.push_back(std::move(item));
    }
    return true;
}
}

bool parse_inventory_snapshot(const std::vector<uint8_t>& payload,
                              std::vector<InventoryItem>& output,std::string& error){
    GCWire::Slice data{payload.data(),payload.size()};size_t pos=0;
    std::vector<InventoryItem> pending;bool has_owner=false;
    while(pos<data.size){
        GCWire::Field f;
        if(!GCWire::next(data,pos,f)){error="Invalid inventory message";return false;}
        if(f.number==1&&f.wire==1)has_owner=true;
        if(f.number==2&&f.wire==2 && !decode_group(f.bytes,pending)){
            error="Invalid inventory item data";return false;
        }
    }
    if(!has_owner){error="Inventory message has no owner";return false;}
    output.swap(pending);error.clear();return true;
}
}
