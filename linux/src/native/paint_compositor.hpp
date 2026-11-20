// Native implementation of the legacy TF2 paint graph.
#pragma once
#define NOMINMAX
#include "paint_graph.hpp"
#include "source_random.hpp"
#include "vpk.hpp"
#include "vtf.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>

struct PaintPixel {float r,g,b,a;};
struct PaintImage {unsigned width=0,height=0;std::vector<PaintPixel> pixels;};
struct PaintTextureCache {
 struct Bucket {
  struct Entry {std::shared_ptr<PaintImage> image;std::list<std::string>::iterator age;};
  std::map<std::string,Entry> entries;std::list<std::string> newest;std::set<std::string> loading;size_t bytes=0;
 };
 std::mutex mutex;std::condition_variable ready;Bucket textures,linear;
 std::shared_ptr<PaintImage> get(const std::string& key,bool is_linear,const std::function<std::shared_ptr<PaintImage>()>& load) {
  Bucket& bucket=is_linear?linear:textures;
  std::unique_lock<std::mutex> lock(mutex);
  for(;;){
   auto found=bucket.entries.find(key);
   if(found!=bucket.entries.end()){
    bucket.newest.splice(bucket.newest.begin(),bucket.newest,found->second.age);
    return found->second.image;
   }
   if(!bucket.loading.count(key)){bucket.loading.insert(key);break;}
   ready.wait(lock);
  }
  lock.unlock();std::shared_ptr<PaintImage> image;
  try {image=load();}
  catch(...){lock.lock();bucket.loading.erase(key);lock.unlock();ready.notify_all();throw;}
  lock.lock();
  try {
   const size_t limit=64ull*1024*1024;
   size_t bytes=image?image->pixels.size()*sizeof(PaintPixel):0;
   if(image&&bytes<=limit){
    while(bucket.bytes+bytes>limit&&!bucket.newest.empty()){
     auto old=bucket.entries.find(bucket.newest.back());
     if(old!=bucket.entries.end()){bucket.bytes-=old->second.image->pixels.size()*sizeof(PaintPixel);bucket.entries.erase(old);}
     bucket.newest.pop_back();
    }
    bucket.newest.push_front(key);
    try{bucket.entries.emplace(key,Bucket::Entry{image,bucket.newest.begin()});}
    catch(...){bucket.newest.pop_front();throw;}
    bucket.bytes+=bytes;
   }
  } catch(...){bucket.loading.erase(key);lock.unlock();ready.notify_all();throw;}
  bucket.loading.erase(key);lock.unlock();ready.notify_all();return image;
 }
 void clear(){std::lock_guard<std::mutex> lock(mutex);textures.entries.clear();textures.newest.clear();textures.bytes=0;linear.entries.clear();linear.newest.clear();linear.bytes=0;}
};
inline float paint_linear(float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
inline float paint_srgb(float v){v=std::max(v,0.f);return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1.f/2.4f)-.055f;}
inline float paint_clamp(float x){return std::min(1.f,std::max(0.f,x));}
inline std::string paint_field(const PaintStage& s,const std::string& key,const std::string& fallback=""){
 for(auto i=s.fields.rbegin();i!=s.fields.rend();++i)if(i->first==key)return i->second;return fallback;
}
inline std::vector<float> paint_numbers(const std::string& text){
 std::istringstream in(text);std::vector<float> out;float v;while(in>>v)out.push_back(v);return out;
}
class PaintCompositor {
 struct Sticker{std::string path,spec;float weight=1;};
 struct Settings{bool flip_u=false,flip_v=false;float tu=0,tv=0,rotation=0,scale=1,black=0,offset=255,gamma=1;Sticker sticker;};
 std::wstring tf_;unsigned size_;VpkArchive& textures_;VpkArchive& misc_;SourceRandom rng_;
 std::map<const PaintStage*,Settings> settings_;
 PaintTextureCache private_cache_;PaintTextureCache& cache_;
 static std::string normalized(std::string p) {
  for(auto& c:p){if(c=='\\')c='/';else c=char(std::tolower(static_cast<unsigned char>(c)));}
  if(p.size()>4&&(p.substr(p.size()-4)==".vtf"||p.substr(p.size()-4)==".tga"))p.resize(p.size()-4);
  if(p.empty()||p[0]=='/'||p.find("..")!=std::string::npos||p.find(':')!=std::string::npos)throw std::runtime_error("Invalid texture path");
  return p;
 }
 static std::wstring widen(const std::string& p){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p.c_str(),-1,nullptr,0);if(!n)throw std::runtime_error("Invalid texture name");std::wstring o(n,L'\0');MultiByteToWideChar(CP_UTF8,0,p.c_str(),-1,&o[0],n);o.pop_back();return o;}
 std::vector<uint8_t> load(const std::string& key,bool optional=false){
  std::wstring path=tf_+L"/"+widen(key);std::ifstream file(path,std::ios::binary|std::ios::ate);
  if(file){auto n=file.tellg();if(n<0||n>512ll*1024*1024)throw std::runtime_error("Invalid texture");std::vector<uint8_t> data(size_t(n),0);file.seekg(0);file.read(reinterpret_cast<char*>(data.data()),n);if(!file)throw std::runtime_error("Could not read texture");return data;}
  if(textures_.contains(key))return textures_.read(key);if(misc_.contains(key))return misc_.read(key);
  if(optional)return {};throw std::runtime_error("Texture missing: "+key);
 }
 static float sinc(float x){if(std::abs(x)<1.e-6f)return 1.f;float v=3.14159265358979323846f*x;return std::sin(v)/v;}
 static PaintImage resize(PaintImage in,unsigned w,unsigned h,bool transparency){
  // Separable Lanczos; alpha in ordinary paint maps is a mask, not opacity.
  auto pass=[&](const PaintImage& in,unsigned ow,unsigned oh,bool horizontal){
   PaintImage out;out.width=ow;out.height=oh;out.pixels.resize(size_t(ow)*oh);
   float scale=horizontal?float(in.width)/ow:float(in.height)/oh;
   struct Weight {unsigned source;float value;};
   struct Kernel {std::vector<Weight> weights;float sum=0;};
   unsigned length=horizontal?ow:oh,source_length=horizontal?in.width:in.height;
   std::vector<Kernel> kernels(length);
   for(unsigned p=0;p<length;++p){
    float center=(p+.5f)*scale-.5f,spread=std::max(1.f,scale);
    int first=int(std::ceil(center-3*spread)),last=int(std::floor(center+3*spread));
    for(int i=first;i<=last;++i){
     float d=(center-i)/spread,weight=sinc(d)*sinc(d/3);if(weight==0)continue;
     kernels[p].weights.push_back({unsigned(std::max(0,std::min(int(source_length)-1,i))),weight});kernels[p].sum+=weight;
    }
   }
   for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x){
    const auto& kernel=kernels[horizontal?x:y];PaintPixel p{};
    for(const auto& tap:kernel.weights){
     unsigned ix=horizontal?tap.source:x,iy=horizontal?y:tap.source;float weight=tap.value;
     auto v=in.pixels[size_t(iy)*in.width+ix];p.r+=v.r*weight;p.g+=v.g*weight;p.b+=v.b*weight;p.a+=v.a*weight;
    }
    if(kernel.sum){p.r/=kernel.sum;p.g/=kernel.sum;p.b/=kernel.sum;p.a/=kernel.sum;}out.pixels[size_t(y)*ow+x]=p;
   }return out;
  };
  if(transparency)for(auto& p:in.pixels){p.r*=p.a;p.g*=p.a;p.b*=p.a;}
  if(w!=in.width)in=pass(in,w,in.height,true);
  if(h!=in.height)in=pass(in,in.width,h,false);
  for(auto& p:in.pixels){p.a=paint_clamp(p.a);if(transparency&&p.a>1e-7f){p.r/=p.a;p.g/=p.a;p.b/=p.a;}p.r=paint_clamp(p.r);p.g=paint_clamp(p.g);p.b=paint_clamp(p.b);}
  return in;
 }
 std::shared_ptr<PaintImage> texture(const std::string& path,bool transparency=false,bool optional=false){
  auto stem=normalized(path);std::string cachekey=stem+(transparency?"#alpha":"");
  return cache_.get(cachekey,false,[&]() -> std::shared_ptr<PaintImage> {
  auto bytes=load("materials/"+stem+".vtf",optional||stem.size()>=3&&stem.substr(stem.size()-3)=="_ao");
  if(bytes.empty()){
   if(optional)return nullptr;
   auto white=std::make_shared<PaintImage>();white->width=white->height=1;white->pixels.push_back({1,1,1,1});return white;
  }
  unsigned w,h;auto decoded=decode_vtf(bytes,w,h);PaintImage image;image.width=w;image.height=h;image.pixels.resize(size_t(w)*h);
  for(size_t i=0;i<image.pixels.size();++i)image.pixels[i]={decoded[i*4]/255.f,decoded[i*4+1]/255.f,decoded[i*4+2]/255.f,decoded[i*4+3]/255.f};
  std::vector<uint8_t>().swap(decoded);std::vector<uint8_t>().swap(bytes);
  if(w>size_||h>size_){float ratio=std::min(float(size_)/w,float(size_)/h);image=resize(std::move(image),std::max(1u,unsigned(w*ratio+.5f)),std::max(1u,unsigned(h*ratio+.5f)),transparency);}
  return std::make_shared<PaintImage>(std::move(image));
  });
 }
 std::shared_ptr<PaintImage> linear_texture(const std::string& path,bool transparency=false){
  auto key=normalized(path)+(transparency?"#alpha":"");
  return cache_.get(key,true,[&](){
  auto img=std::make_shared<PaintImage>(*texture(path,transparency));for(auto& p:img->pixels){p.r=paint_linear(p.r);p.g=paint_linear(p.g);p.b=paint_linear(p.b);}
  return img;
  });
 }
 float number(const PaintStage& s,const char* key,float def,int stream,bool inverse=false){
  auto v=paint_numbers(paint_field(s,key));float low=v.empty()?def:v[0],high=v.size()>1?v[1]:low;
  if(inverse){low=low?1/low:.00001f;high=high?1/high:.00001f;}return rng_.uniform(stream,low,high);
 }
 void prepare(const PaintStage& s){
  if(s.kind=="select")return;Settings q;int stream=rng_.next_stage();
  if(s.kind=="apply_sticker"){
   std::vector<Sticker> choices;
   for(auto& f:s.fields){if(f.first=="sticker_base")choices.push_back({f.second,"",1});else if(f.first=="sticker_weight"&&!choices.empty())choices.back().weight=std::stof(f.second);else if(f.first=="sticker_spec"&&!choices.empty())choices.back().spec=f.second;}
   float total=0;for(auto& c:choices){if(c.path.empty()||c.weight<0)throw std::runtime_error("Invalid sticker");total+=c.weight;}
   if(total<=0)throw std::runtime_error("Invalid sticker weights");float chosen=rng_.uniform(stream,0,total);q.sticker=choices.back();
   for(auto& c:choices){chosen-=c.weight;if(chosen<0){q.sticker=c;break;}}
  }
  if(s.kind=="texture_lookup"){
   auto flag=[&](const char* key){auto t=paint_field(s,key,"0");auto values=paint_numbers(t);bool enabled=t=="true"||t=="yes";for(auto x:values)enabled|=x!=0;return enabled&&rng_.integer(stream,0,1)!=0;};
   q.flip_u=flag("flip_u");q.flip_v=flag("flip_v");q.tu=number(s,"translate_u",0,stream);q.tv=number(s,"translate_v",0,stream);
   q.rotation=number(s,"rotation",0,stream);q.scale=number(s,"scale_uv",1,stream);
  }
  q.black=number(s,"adjust_black",0,stream);q.offset=number(s,"adjust_offset",255,stream);q.gamma=number(s,"adjust_gamma",1,stream,true);
  settings_[&s]=q;for(auto& c:s.children)prepare(c);
 }
 static int wrap(int i,int n){i%=n;return i<0?i+n:i;}
 static PaintPixel sample(const PaintImage& image,float u,float v,bool linear,bool repeat,bool raw=false){
  auto coord=[&](int i,int n){return repeat?wrap(i,n):std::max(0,std::min(n-1,i));};
  if(raw){int x=coord(int(std::floor(u*image.width)),image.width),y=coord(int(std::floor(v*image.height)),image.height);return image.pixels[size_t(y)*image.width+x];}
  float px=u*image.width-.5f,py=v*image.height-.5f;int x=int(std::floor(px)),y=int(std::floor(py));float fx=px-x,fy=py-y;
  PaintPixel result{};for(int j=0;j<2;++j)for(int i=0;i<2;++i){auto p=image.pixels[size_t(coord(y+j,image.height))*image.width+coord(x+i,image.width)];if(linear){p.r=paint_linear(p.r);p.g=paint_linear(p.g);p.b=paint_linear(p.b);}float w=(i?fx:1-fx)*(j?fy:1-fy);result.r+=w*p.r;result.g+=w*p.g;result.b+=w*p.b;result.a+=w*p.a;}return result;
 }
 static void adjust(PaintPixel& p,const Settings& s){
  if(s.black==0&&s.offset==255&&s.gamma==1)return;float black=s.black/255.f,span=std::max(1e-5f,s.offset/255.f);
  auto f=[&](float x,bool rgb){if(rgb)x=paint_srgb(x);x=std::pow(paint_clamp((x-black)/span),s.gamma);return rgb?paint_linear(x):x;};
  p.r=f(p.r,true);p.g=f(p.g,true);p.b=f(p.b,true);p.a=f(p.a,false);
 }
 PaintImage evaluate(const PaintStage& s){
  const size_t count=size_t(size_)*size_;PaintImage out;out.width=out.height=size_;out.pixels.resize(count);
  Settings q;auto found=settings_.find(&s);if(found!=settings_.end())q=found->second;
  if(s.kind=="texture_lookup"||s.kind=="select"){
   std::string path=s.kind=="select"?paint_field(s,"groups"):paint_field(s,"texture_red",paint_field(s,"texture"));
   auto img=s.kind=="select"?texture(path):linear_texture(path);std::vector<float> ids;for(auto& f:s.fields)if(f.first=="select"){auto v=paint_numbers(f.second);if(!v.empty()&&v[0])ids.push_back(std::floor(v[0]/16+.5f));}
   float a=q.rotation*3.14159265358979323846f/180.f,cs=std::cos(a),sn=std::sin(a);
   for(unsigned y=0;y<size_;++y)for(unsigned x=0;x<size_;++x){
    float u=((x+.5f)/size_+q.tu)*q.scale*(q.flip_u?-1:1),v=((y+.5f)/size_+q.tv)*q.scale*(q.flip_v?-1:1);
    auto p=sample(*img,u*cs-v*sn,u*sn+v*cs,false,true,s.kind=="select");
    if(s.kind=="select"){float value=std::floor(p.r*255/16+.5f);float hit=std::find(ids.begin(),ids.end(),value)!=ids.end()?1.f:0.f;p={hit,hit,hit,hit};}
    adjust(p,q);out.pixels[size_t(y)*size_+x]=p;
   }return out;
  }
  if(s.children.empty())throw std::runtime_error("Empty paint operation: "+s.kind);
  out=evaluate(s.children[0]);
  if(s.kind=="apply_sticker"){
   if(s.children.size()!=1)throw std::runtime_error("Invalid sticker operation");auto stem=normalized(q.sticker.path);
   if(stem!="patterns/stickers/blank"){
    auto tl=paint_numbers(paint_field(s,"dest_tl")),tr=paint_numbers(paint_field(s,"dest_tr")),bl=paint_numbers(paint_field(s,"dest_bl"));
    if(tl.size()!=2||tr.size()!=2||bl.size()!=2)throw std::runtime_error("Invalid sticker coordinates");
    float ux=tr[0]-tl[0],uy=tr[1]-tl[1],vx=bl[0]-tl[0],vy=bl[1]-tl[1],det=ux*vy-uy*vx;
    if(std::abs(det)<1e-10f)throw std::runtime_error("Invalid sticker dimensions");
    auto img=linear_texture(stem,true);auto spec=texture(q.sticker.spec.empty()?stem+"_spec":q.sticker.spec,false,true);
    for(unsigned y=0;y<size_;++y)for(unsigned x=0;x<size_;++x){float dx=(x+.5f)/size_-tl[0],dy=(y+.5f)/size_-tl[1],u=(dx*vy-dy*vx)/det,v=(dy*ux-dx*uy)/det;if(u<0||u>1||v<0||v>1)continue;
     auto o=sample(*img,u,v,false,false);float alpha=o.a;auto& p=out.pixels[size_t(y)*size_+x];float gloss=spec?sample(*spec,u,v,false,false).r:1;
     p.r=p.r*(1-alpha)+o.r*alpha;p.g=p.g*(1-alpha)+o.g*alpha;p.b=p.b*(1-alpha)+o.b*alpha;p.a=p.a*(1-alpha)+gloss*alpha;
    }
   }
  }else if(s.kind=="combine_lerp"){
   if(s.children.size()!=3)throw std::runtime_error("Invalid lerp");auto b=evaluate(s.children[1]),m=evaluate(s.children[2]);
   for(size_t i=0;i<count;++i){auto& a=out.pixels[i];auto v=b.pixels[i];float t=m.pixels[i].r;a.r=a.r*(1-t)+v.r*t;a.g=a.g*(1-t)+v.g*t;a.b=a.b*(1-t)+v.b*t;a.a=a.a*(1-t)+v.a*t;}
  }else if(s.kind=="combine_multiply"||s.kind=="combine_add"){
   for(size_t n=1;n<s.children.size();++n){auto b=evaluate(s.children[n]);for(size_t i=0;i<count;++i){auto& a=out.pixels[i];auto v=b.pixels[i];
    if(s.kind=="combine_multiply"){a.r*=v.r;a.g*=v.g;a.b*=v.b;a.a*=v.a;}else{a.r=std::min(1.f,a.r+v.r);a.g=std::min(1.f,a.g+v.g);a.b=std::min(1.f,a.b+v.b);a.a=std::min(1.f,a.a+v.a);}
   }}
  }else throw std::runtime_error("Unsupported paint operation: "+s.kind);
  for(auto& p:out.pixels)adjust(p,q);return out;
 }
public:
 PaintCompositor(const std::wstring& tf,unsigned size,uint64_t seed,VpkArchive& textures,VpkArchive& misc,PaintTextureCache* cache=nullptr):tf_(tf),size_(size),textures_(textures),misc_(misc),rng_(tf,seed),cache_(cache?*cache:private_cache_){if(size<32||size>2048)throw std::runtime_error("Invalid texture size");}
 std::vector<uint8_t> render(const PaintStage& graph){settings_.clear();prepare(graph);auto out=evaluate(graph);std::vector<uint8_t> bytes(out.pixels.size()*4);
  auto cv=[](float v){return uint8_t(paint_clamp(v)*255+.5f);};for(size_t i=0;i<out.pixels.size();++i){auto p=out.pixels[i];bytes[i*4]=cv(paint_srgb(p.r));bytes[i*4+1]=cv(paint_srgb(p.g));bytes[i*4+2]=cv(paint_srgb(p.b));bytes[i*4+3]=cv(p.a);}return bytes;
 }
};
