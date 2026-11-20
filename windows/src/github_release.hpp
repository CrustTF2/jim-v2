#pragma once
#include <json/json.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>

struct GithubRelease {
 std::string tag,url,sha256;
};
inline bool parse_release_version(const std::string& value,std::array<unsigned,3>& out) {
 std::string s=value;
 if(s.compare(0,8,"Version ")==0)s.erase(0,8);
 if(!s.empty()&&(s[0]=='v'||s[0]=='V'))s.erase(0,1);
 size_t pos=0;
 for(int i=0;i<3;++i){
  if(pos>=s.size()||!std::isdigit(static_cast<unsigned char>(s[pos])))return false;
  unsigned n=0;while(pos<s.size()&&std::isdigit(static_cast<unsigned char>(s[pos]))){if(n>1000000)return false;n=n*10+(s[pos++]-'0');}
  out[i]=n;if(i<2){if(pos>=s.size()||s[pos++]!='.')return false;}
 }
 return pos==s.size();
}
inline bool select_github_release(const Json::Value& root,const std::string& current,GithubRelease& out) {
 if(!root.isObject()||root.get("draft",true).asBool()||root.get("prerelease",true).asBool())return false;
 if(!root["tag_name"].isString()||!root["assets"].isArray())return false;
 std::array<unsigned,3> installed{},latest{};std::string tag=root["tag_name"].asString();
 if(!parse_release_version(current,installed)||!parse_release_version(tag,latest)||latest<=installed)return false;
 const std::string expected="https://github.com/CrustTF2/jim-v2/releases/download/"+tag+"/item_manager.exe";
 bool found=false;GithubRelease candidate;
 for(const auto& asset:root["assets"]){
  if(asset.get("name","").asString()!="item_manager.exe")continue;
  if(found||asset.get("state","").asString()!="uploaded"||asset.get("browser_download_url","").asString()!=expected)return false;
  auto digest=asset.get("digest","").asString();
  if(digest.size()!=71||digest.compare(0,7,"sha256:")!=0)return false;
  digest.erase(0,7);for(char& c:digest){if(!std::isxdigit(static_cast<unsigned char>(c)))return false;c=char(std::tolower(static_cast<unsigned char>(c)));}
  candidate={tag,expected,digest};found=true;
 }
 if(!found)return false;out=std::move(candidate);return true;
}
