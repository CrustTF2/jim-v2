#include "../github_release.hpp"
#include <cstdio>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 Json::Value release(Json::objectValue);release["tag_name"]="v2.0.1";release["draft"]=false;release["prerelease"]=false;
 Json::Value asset(Json::objectValue);asset["name"]="item_manager.exe";asset["state"]="uploaded";
 asset["browser_download_url"]="https://github.com/CrustTF2/jim-v2/releases/download/v2.0.1/item_manager.exe";
 asset["digest"]="sha256:"+std::string(64,'a');release["assets"].append(asset);GithubRelease chosen;
 check(select_github_release(release,"Version 2.0.0",chosen),"New release not selected");
 check(!select_github_release(release,"Version 2.0.1",chosen),"Equal version selected");
 check(!select_github_release(release,"Version 2.1.0",chosen),"Older version selected");
 auto copy=release;copy["prerelease"]=true;check(!select_github_release(copy,"Version 2.0.0",chosen),"Prerelease selected");
 copy=release;copy["draft"]=true;check(!select_github_release(copy,"Version 2.0.0",chosen),"Draft selected");
 copy=release;copy["assets"][0]["browser_download_url"]="https://example.com/item_manager.exe";check(!select_github_release(copy,"Version 2.0.0",chosen),"Untrusted origin selected");
 copy=release;copy["assets"][0]["digest"]="sha256:bad";check(!select_github_release(copy,"Version 2.0.0",chosen),"Invalid digest accepted");
 copy=release;copy["assets"].append(asset);check(!select_github_release(copy,"Version 2.0.0",chosen),"Duplicate asset accepted");
 copy=release;copy["tag_name"]="v2.0.1-beta";check(!select_github_release(copy,"Version 2.0.0",chosen),"Invalid version accepted");
 std::puts("GitHub release selection and version checks passed.");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
