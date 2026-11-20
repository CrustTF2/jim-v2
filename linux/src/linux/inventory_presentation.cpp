#include "inventory_presentation.hpp"
#include "../protobuf/tf_gcmessages.pb.h"

namespace LinuxSteam {
PresentedItem present_item(const InventoryItem& item,const ItemPresentationSchema& schema,
                           const std::string& fallback_name){
    std::vector<PresentationAttribute> attributes;
    std::string custom_name=item.custom_name;
    for(const auto& source:item.attributes){
        PresentationAttribute attribute;
        attribute.index=source.definition;
        attribute.bytes.assign(source.value_bytes.begin(),source.value_bytes.end());
        attributes.push_back(attribute);
        if(source.definition==500 && !source.value_bytes.empty() && source.value_bytes.size()<=65536){
            CAttribute_String text;
            if(text.ParseFromArray(source.value_bytes.data(),static_cast<int>(source.value_bytes.size())) && text.has_value())
                custom_name=text.value();
        }
    }
    PresentedItem result;
    result.presentation=schema.resolve(item.definition,item.quality,attributes,fallback_name.c_str(),item.origin,item.flags);
    if(result.presentation.custom_description.empty())result.presentation.custom_description=item.custom_description;
    result.renamed=!custom_name.empty();
    result.display_name=result.renamed ? "\""+custom_name+"\"" :
        (result.presentation.name.empty()?fallback_name:result.presentation.name);
    return result;
}
}
