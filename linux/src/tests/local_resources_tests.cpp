#include "../http_resource_loader.hpp"
#include <string/constant_string.hpp>
#include <cstdio>
int main(){
 const char* paths[]={
  "fonts/tf2build.ttf","img/manager/gear.png","img/manager/sort.png","img/manager/delete.png",
  "img/backpack/unknown_item.png",
  "img/backpack/player/items/crafting/backpack_jewel_paint_splatter.png",
  "img/backpack/player/items/crafting/backpack_jewel_paint_splatter_blu.png",
  "img/backpack/player/items/crafting/backpack_jewel_paint_splatter_red.png",
  "img/backpack/player/items/crafting/paintcan_paintcolor.png",
  "img/backpack/player/items/crafting/teampaint_blu_mask.png",
  "img/backpack/player/items/crafting/teampaint_red_mask.png"
 };
 HttpResourceLoader loader(nullptr,nullptr);
 for(auto path:paths){JUTIL::ConstantString name(path);if(!loader.get_resource(&name,&name)){std::fprintf(stderr,"Missing bundled resource: %s\n",path);return 1;}}
 JUTIL::ConstantString missing("missing-resource-test.png");if(loader.get_resource(&missing,&missing)){std::fprintf(stderr,"Missing file was accepted\n");return 1;}
 std::puts("Bundled interface resources verified without a network downloader.");return 0;
}
