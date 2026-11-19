#define NOMINMAX
#include <windows.h>
#include "vpk.hpp"
#include "vtf.hpp"
#include <fstream>
int main(int argc,char** argv){
 try{
  if(argc!=5)return 2;
  int n=MultiByteToWideChar(CP_UTF8,0,argv[1],-1,nullptr,0);
  std::wstring path(n,L'\0');MultiByteToWideChar(CP_UTF8,0,argv[1],-1,&path[0],n);path.pop_back();
  VpkArchive archive(path);unsigned w=0,h=0;
  auto pixels=decode_vtf(archive.read(argv[2]),w,h,unsigned(std::stoul(argv[3])));
  std::ofstream output(argv[4],std::ios::binary);
  output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
  if(!output)throw std::runtime_error("Output failed");
  std::printf("%u %u\n",w,h);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
