#include "paint_graph.hpp"
#include <cstdio>
#include <cstdlib>
static Json::Value convert(const PaintStage& s){
    Json::Value v(Json::objectValue);v["kind"]=s.kind;v["fields"]=Json::Value(Json::arrayValue);v["children"]=Json::Value(Json::arrayValue);
    for(const auto& f:s.fields){Json::Value pair(Json::arrayValue);pair.append(f.first);pair.append(f.second);v["fields"].append(pair);}
    for(const auto& c:s.children)v["children"].append(convert(c));return v;
}
int main(int argc,char** argv){try {
    if(argc!=4)return 2;
    PaintDefinitions p(L"D:\\Steam\\steamapps\\common\\Team Fortress 2\\tf\\scripts\\protodefs\\proto_defs.vpd");
    std::ifstream file("outputs/ItemManager-1.0.8-Custom/thumbnail_renderer/catalog.json");Json::Value doc;file>>doc;
    std::map<uint32_t,std::string> catalog;for(const auto& item:doc["weapons"])catalog[item["id"].asUInt()]=item["model"].asString();
    uint32_t def=std::strtoul(argv[1],nullptr,10),paint=std::strtoul(argv[2],nullptr,10);
    auto it=catalog.find(def);if(it==catalog.end())throw std::runtime_error("Weapon missing");
    PaintGraph graph(p,paint,it->second,float(std::atof(argv[3])),catalog);Json::StreamWriterBuilder b;b["indentation"]="";
    std::printf("%s\n",Json::writeString(b,convert(graph.graph)).c_str());return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
