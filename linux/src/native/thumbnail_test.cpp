#include "thumbnail_renderer.hpp"
int main(int argc,char** argv){try{
 if(argc!=8)return 2;std::wstring tf=L"D:\\Steam\\steamapps\\common\\Team Fortress 2\\tf";
 PaintDefinitions defs(tf+L"/scripts/protodefs/proto_defs.vpd");VpkArchive textures(tf+L"/tf2_textures_dir.vpk"),misc(tf+L"/tf2_misc_dir.vpk");
 std::ifstream file("outputs/ItemManager-1.0.8-Custom/thumbnail_renderer/catalog.json");Json::Value doc;file>>doc;std::map<uint32_t,std::string> catalog;
 for(auto& item:doc["weapons"])catalog[item["id"].asUInt()]=item["model"].asString();
 unsigned def=std::stoul(argv[1]),paint=std::stoul(argv[2]);float wear=std::stof(argv[3]);uint64_t seed=std::stoull(argv[4]);
 PaintGraph graph(defs,paint,catalog.at(def),wear,catalog);PaintCompositor compositor(tf,512,seed,textures,misc);auto atlas=compositor.render(graph.graph);
 NativeThumbnailRenderer renderer(tf,misc,textures);auto pixels=renderer.render(catalog.at(def),atlas,std::stoi(argv[5])!=0,1,std::stoi(argv[6]));save_thumbnail_png(argv[7],pixels);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
