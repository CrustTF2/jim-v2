#include "paint_compositor.hpp"
#include <cstdio>
int main(int argc,char** argv){try{
 if(argc!=7)return 2;
 std::wstring tf=L"D:\\Steam\\steamapps\\common\\Team Fortress 2\\tf";
 PaintDefinitions defs(tf+L"/scripts/protodefs/proto_defs.vpd");
 VpkArchive textures(tf+L"/tf2_textures_dir.vpk"),misc(tf+L"/tf2_misc_dir.vpk");
 std::ifstream file("outputs/ItemManager-1.0.8-Custom/thumbnail_renderer/catalog.json");Json::Value doc;file>>doc;
 std::map<uint32_t,std::string> catalog;for(auto& item:doc["weapons"])catalog[item["id"].asUInt()]=item["model"].asString();
 unsigned def=std::stoul(argv[1]),paint=std::stoul(argv[2]),size=std::stoul(argv[5]);float wear=std::stof(argv[3]);uint64_t seed=std::stoull(argv[4]);
 PaintGraph graph(defs,paint,catalog.at(def),wear,catalog);PaintCompositor compositor(tf,size,seed,textures,misc);auto pixels=compositor.render(graph.graph);
 std::ofstream out(argv[6],std::ios::binary);out.write(reinterpret_cast<char*>(pixels.data()),pixels.size());if(!out)throw std::runtime_error("Output failed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
