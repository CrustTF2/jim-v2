#include "steam_paths.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

namespace LinuxSteam {
namespace fs=std::filesystem;
namespace {
bool file(const fs::path& p){std::error_code e;return fs::is_regular_file(p,e);}
bool directory(const fs::path& p){std::error_code e;return fs::is_directory(p,e);}
std::string unescape(const std::string& encoded){
    std::string result;
    for(size_t i=0;i<encoded.size();++i){
        if(encoded[i]=='\\' && i+1<encoded.size() && (encoded[i+1]=='\\'||encoded[i+1]=='"'))++i;
        result+=encoded[i];
    }
    return result;
}
void add_library_paths(const fs::path& root,std::vector<fs::path>& libraries){
    libraries.push_back(root);
    auto file_path=root/"steamapps/libraryfolders.vdf";
    std::error_code e;auto size=fs::file_size(file_path,e);
    if(e||size>4u*1024u*1024u)return;
    std::ifstream stream(file_path);if(!stream)return;
    std::ostringstream data;data<<stream.rdbuf();const auto text=data.str();
    const std::regex path_pattern(R"REGEX("path"\s*"((?:\\.|[^"\\])*)")REGEX",std::regex::icase);
    for(std::sregex_iterator it(text.begin(),text.end(),path_pattern),end;it!=end;++it)
        libraries.emplace_back(unescape((*it)[1].str()));
}
}

Paths discover_paths(const std::string& home,const std::string& tf_override,
                     const std::string& client_override,const std::string& api_override){
    Paths out;
    if(home.empty() && (tf_override.empty()||client_override.empty()||api_override.empty()))return out;
    const fs::path base(home);
    const std::vector<fs::path> roots={base/".steam/steam",base/".steam/root",base/".local/share/Steam",
        base/".var/app/com.valvesoftware.Steam/.local/share/Steam"};
    if(!client_override.empty()){
        if(file(client_override))out.client=client_override;
    }else{
        std::vector<fs::path> candidates={base/".steam/sdk64/steamclient.so"};
        for(const auto& root:roots)candidates.push_back(root/"linux64/steamclient.so");
        for(const auto& candidate:candidates)if(file(candidate)){out.client=candidate.string();break;}
    }
    if(!tf_override.empty()){
        if(directory(tf_override))out.tf=tf_override;
    }else{
        std::vector<fs::path> libraries;
        for(const auto& root:roots)add_library_paths(root,libraries);
        for(const auto& root:libraries){auto candidate=root/"steamapps/common/Team Fortress 2/tf";
            if(directory(candidate)){out.tf=candidate.string();break;}}
    }
    if(!api_override.empty()){
        if(file(api_override))out.api=api_override;
    }else if(!out.tf.empty()){
        const fs::path tf(out.tf), game=tf.parent_path();
        const std::vector<fs::path> candidates={game/"bin/linux64/libsteam_api.so",game/"bin/linux64/steam_api.so",
            tf/"bin/linux64/libsteam_api.so",tf/"bin/linux64/steam_api.so"};
        for(const auto& candidate:candidates)if(file(candidate)){out.api=candidate.string();break;}
    }
    return out;
}
}
