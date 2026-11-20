#define NOMINMAX
#include <windows.h>
#include "thumbnail_renderer.hpp"
#include "thumbnail_batch.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <set>
#include <regex>

static bool exists(const std::wstring& path){auto a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY);}
static std::wstring widen_utf8(const std::string& p){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p.c_str(),-1,nullptr,0);if(!n)throw std::runtime_error("Invalid UTF-8 path");std::wstring s(n,L'\0');MultiByteToWideChar(CP_UTF8,0,p.c_str(),-1,&s[0],n);s.pop_back();return s;}
static std::string narrow_utf8(const std::wstring& p){int n=WideCharToMultiByte(CP_UTF8,0,p.c_str(),-1,nullptr,0,nullptr,nullptr);if(!n)throw std::runtime_error("Invalid path");std::string s(n,'\0');WideCharToMultiByte(CP_UTF8,0,p.c_str(),-1,&s[0],n,nullptr,nullptr);s.pop_back();return s;}
static std::wstring reg_string(HKEY hive,const wchar_t* sub,const wchar_t* name){
 HKEY key=nullptr;if(RegOpenKeyExW(hive,sub,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return L"";DWORD type=0,size=0;LONG status=RegQueryValueExW(key,name,nullptr,&type,nullptr,&size);
 std::wstring result;if(status==ERROR_SUCCESS&&(type==REG_SZ||type==REG_EXPAND_SZ)&&size>=2&&size<32768){result.resize(size/sizeof(wchar_t));status=RegQueryValueExW(key,name,nullptr,&type,reinterpret_cast<BYTE*>(&result[0]),&size);if(status==ERROR_SUCCESS){while(!result.empty()&&!result.back())result.pop_back();}else result.clear();}RegCloseKey(key);return result;
}
static std::wstring locate_tf(){
 std::vector<std::wstring> candidates,libraries;wchar_t value[32768];DWORD n=GetEnvironmentVariableW(L"TF2_TF_DIR",value,32768);if(n&&n<32768)candidates.emplace_back(value);
 auto a=reg_string(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath"),b=reg_string(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WOW6432Node\\Valve\\Steam",L"InstallPath");
 if(!a.empty())libraries.push_back(a);if(!b.empty()&&b!=a)libraries.push_back(b);
 const auto base=libraries;
 for(auto& steam:base){
  std::ifstream in(steam+L"/steamapps/libraryfolders.vdf",std::ios::binary);if(!in)continue;std::string file((std::istreambuf_iterator<char>(in)),{});
  std::regex pattern("\"path\"\\s*\"([^\"]+)\"",std::regex::icase);
  for(std::sregex_iterator it(file.begin(),file.end(),pattern),end;it!=end;++it){std::string s=(*it)[1];std::string decoded;for(size_t i=0;i<s.size();++i){if(s[i]=='\\'&&i+1<s.size()&&s[i+1]=='\\')++i;decoded+=s[i];}libraries.push_back(widen_utf8(decoded));}
 }
 for(auto& library:libraries)candidates.push_back(library+L"/steamapps/common/Team Fortress 2/tf");
 for(auto& tf:candidates)if(exists(tf+L"/tf2_misc_dir.vpk")&&exists(tf+L"/scripts/protodefs/proto_defs.vpd"))return tf;
 throw std::runtime_error("Team Fortress 2 installation was not found; existing item images will be used.");
}
static void progress(const std::string& message){std::ofstream out("thumbnail_renderer/progress.txt",std::ios::trunc);out<<message;}
static bool valid_key(const std::string& key){return !key.empty()&&key.size()<120&&key.find_first_not_of("0123456789abcdef-")==std::string::npos;}
static Json::Value read_json(const char* path){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error(std::string("File not found: ")+path);Json::Value doc;Json::CharReaderBuilder b;std::string errors;if(!Json::parseFromStream(b,file,&doc,&errors))throw std::runtime_error("Invalid JSON: "+errors);return doc;}
int run_thumbnail_batch(const std::atomic<bool>& cancelled){
 try{
  if(cancelled.load())return 2;
  CreateDirectoryW(L"thumbnail_renderer",nullptr);CreateDirectoryW(L"thumbnail_renderer/cache",nullptr);
  auto manifest=read_json("thumbnail_renderer/manifest.json");auto items=manifest["items"];if(!items.isArray())throw std::runtime_error("Invalid thumbnail manifest");
  std::vector<Json::Value> pending;std::set<std::string> unique;std::vector<std::string> errors;size_t cached=0;
  for(auto& item:items){std::string key=item["key"].asString();if(!valid_key(key)){errors.push_back("Item has an invalid cache key");continue;}if(!unique.insert(key).second)continue;
   if(exists(widen_utf8("thumbnail_renderer/cache/"+key+"-display.png")))++cached;else pending.push_back(item);
  }
  size_t total=unique.size()+errors.size();std::mutex mutex;size_t complete=cached+errors.size(),failures=errors.size();std::ofstream log("thumbnail_renderer/thumbnail-errors.log",std::ios::trunc);
  for(auto& error:errors)log<<error<<'\n';
  auto update=[&](bool finished){std::ostringstream s;if(finished)s<<"Thumbnails ready: "<<(total-failures)<<" / "<<total;else s<<"Preparing War Paint thumbnails... "<<complete<<" / "<<total;if(failures)s<<" ("<<failures<<" unavailable)";progress(s.str());};
  update(false);if(pending.empty()){update(true);return 0;}
  auto tf=locate_tf();PaintDefinitions defs(tf+L"/scripts/protodefs/proto_defs.vpd");VpkArchive textures(tf+L"/tf2_textures_dir.vpk"),misc(tf+L"/tf2_misc_dir.vpk");
  auto catalog_doc=read_json("thumbnail_renderer/catalog.json");std::map<uint32_t,std::string> catalog;for(auto& item:catalog_doc["weapons"])catalog[item["id"].asUInt()]=item["model"].asString();
  std::atomic<size_t> next(0);std::atomic<bool> stop_workers(false);std::vector<std::thread> workers;std::vector<Json::Value> retries;PaintTextureCache cache;
  auto run=[&](int worker,bool retry){
   try{
    NativeThumbnailRenderer renderer(tf,misc,textures);
    for(;;){if(cancelled.load()||stop_workers.load())break;size_t index=next.fetch_add(1);auto& jobs=retry?retries:pending;if(index>=jobs.size())break;auto& item=jobs[index];bool deferred=false;
     try{
      uint32_t def=item["definition"].asUInt(),paint=item["paint"].asUInt();auto m=catalog.find(def);if(m==catalog.end())throw std::runtime_error("Weapon model is not in the renderer catalog");
      float wear=item["wear"].asFloat();auto seed=std::stoull(item["seed"].asString());PaintGraph graph(defs,paint,m->second,wear,catalog);
      PaintCompositor compositor(tf,512,seed,textures,misc,&cache);auto atlas=compositor.render(graph.graph);
      if(cancelled.load())break;
      auto pixels=renderer.render(m->second,atlas,item.get("stat_clock",false).asBool(),item.get("stat_clock_scale",1).asFloat(),item.get("stat_clock_count",0).asInt());
      if(cancelled.load())break;
      std::string key=item["key"].asString(),tmp="thumbnail_renderer/cache/"+key+".worker-"+std::to_string(worker)+".tmp",dest="thumbnail_renderer/cache/"+key+"-display.png";
      save_thumbnail_png(tmp,pixels);if(!MoveFileExW(widen_utf8(tmp).c_str(),widen_utf8(dest).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not save thumbnail");
     }catch(const std::bad_alloc& e){std::lock_guard<std::mutex> guard(mutex);if(!retry){retries.push_back(item);deferred=true;}else{++failures;log<<"Item definition "<<item["definition"].asUInt()<<" / paint "<<item["paint"].asUInt()<<": "<<e.what()<<'\n';}}
     catch(const std::exception& e){std::lock_guard<std::mutex> guard(mutex);++failures;log<<"Item definition "<<item["definition"].asUInt()<<" / paint "<<item["paint"].asUInt()<<": "<<e.what()<<'\n';}
     std::lock_guard<std::mutex> guard(mutex);if(!deferred)++complete;update(false);
    }
   }catch(const std::exception& e){std::lock_guard<std::mutex> guard(mutex);log<<"Worker "<<worker<<": "<<e.what()<<'\n';}
  };
  try {
   for(size_t i=0;i<std::min<size_t>(3,pending.size());++i)workers.emplace_back(run,int(i),false);
  } catch (...) {
   stop_workers.store(true);
   for(auto& worker:workers)if(worker.joinable())worker.join();
   throw;
  }
  for(auto& worker:workers)worker.join();
  if(cancelled.load()){progress("Thumbnail preparation canceled.");return 2;}
  if(!retries.empty()){cache.clear();next=0;std::thread retry_worker(run,3,true);retry_worker.join();}
  if(cancelled.load()){progress("Thumbnail preparation canceled.");return 2;}
  if(complete<total){failures+=total-complete;log<<(total-complete)<<" thumbnails had no available worker.\n";complete=total;}
  update(true);return 0;
 }catch(const std::exception& e){std::ofstream log("thumbnail_renderer/thumbnail-errors.log",std::ios::app);log<<e.what()<<'\n';progress("Thumbnail renderer unavailable; using existing item images.");return 1;}
}
