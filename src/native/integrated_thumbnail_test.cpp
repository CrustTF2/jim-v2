#include "thumbnail_batch.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
int main(int argc,char** argv){
 std::atomic<bool> cancelled(false);
 std::vector<char> reserved;
 if(argc==2&&std::string(argv[1])=="--cancel-before-start"){
  cancelled=true;return run_thumbnail_batch(cancelled);
 }
 if(argc==3&&std::string(argv[1])=="--reserve-mib"){
  size_t size=size_t(std::strtoul(argv[2],nullptr,10))*1024*1024;
  reserved.assign(size,1);
 }
 if(argc==3&&std::string(argv[1])=="--cancel-after-ms"){
  int delay=std::atoi(argv[2]);std::thread worker([&](){std::this_thread::sleep_for(std::chrono::milliseconds(delay));cancelled=true;});
  int result=run_thumbnail_batch(cancelled);worker.join();return result;
 }
 return run_thumbnail_batch(cancelled);
}
