#include "paint_compositor.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
static void require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
static std::shared_ptr<PaintImage> image(unsigned size){
 auto value=std::make_shared<PaintImage>();value->width=value->height=size;value->pixels.resize(size_t(size)*size);return value;
}
int main(){try{
 PaintTextureCache cache;std::atomic<int> loads(0),started(0);std::atomic<bool> go(false);
 std::vector<std::thread> threads;std::vector<std::shared_ptr<PaintImage>> output(6);
 for(int i=0;i<6;++i)threads.emplace_back([&,i]{++started;while(!go.load())std::this_thread::yield();
  output[i]=cache.get("same",false,[&](){++loads;std::this_thread::sleep_for(std::chrono::milliseconds(30));return image(1);});
 });
 while(started.load()!=6)std::this_thread::yield();go=true;for(auto& t:threads)t.join();
 require(loads==1,"Concurrent requests should share one load");
 for(auto& p:output)require(p==output[0],"Concurrent requests should share the same image");
 cache.clear();int count=0;
 for(int i=0;i<16;++i)cache.get("item"+std::to_string(i),false,[&](){++count;return image(512);});
 cache.get("item0",false,[&](){++count;return image(512);});
 cache.get("item16",false,[&](){++count;return image(512);});
 cache.get("item0",false,[&](){++count;return image(512);});
 require(count==17,"A recently used image should stay cached");
 cache.get("item1",false,[&](){++count;return image(512);});
 require(count==18,"The least recently used image should be evicted");
 cache.clear();bool caught=false;
 try{cache.get("failure",false,[]()->std::shared_ptr<PaintImage>{throw std::bad_alloc();});}
 catch(const std::bad_alloc&){caught=true;}
 require(caught,"Loader failure should propagate");
 require(bool(cache.get("failure",false,[](){return image(1);})),"Failed loads should be retryable");
 cache.clear();loads=0;started=0;go=false;std::atomic<int> failures(0),successes(0);threads.clear();
 for(int i=0;i<6;++i)threads.emplace_back([&]{++started;while(!go.load())std::this_thread::yield();
  try{
   cache.get("contended_failure",false,[&](){int call=++loads;std::this_thread::sleep_for(std::chrono::milliseconds(20));if(call==1)throw std::bad_alloc();return image(1);});
   ++successes;
  }catch(const std::bad_alloc&){++failures;}
 });
 while(started.load()!=6)std::this_thread::yield();go=true;for(auto& t:threads)t.join();
 require(failures==1&&successes==5&&loads==2,"Waiting threads should recover after a loader failure");
 std::puts("Shared loading, eviction and failure-recovery checks passed.");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
