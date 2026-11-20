#pragma once
#include "inventory_model.hpp"
#include "../item_presentation.hpp"

namespace LinuxSteam {
struct PresentedItem {
    ItemPresentation presentation;
    std::string display_name;
    bool renamed = false;
};

// The fallback comes from Steam's localized schema, as in the Windows app.
PresentedItem present_item(const InventoryItem& item,const ItemPresentationSchema& schema,
                           const std::string& fallback_name);
}
