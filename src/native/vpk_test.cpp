#include <windows.h>
#include "vpk.hpp"
#include <cstdio>
int main(int argc,char** argv) {
    try {
        if(argc!=3)return 2;
        int n=MultiByteToWideChar(CP_UTF8,0,argv[1],-1,nullptr,0);std::wstring path(n,L'\0');MultiByteToWideChar(CP_UTF8,0,argv[1],-1,&path[0],n);path.pop_back();
        VpkArchive archive(path);auto bytes=archive.read(argv[2]);
        std::printf("%zu %08lx\n",bytes.size(),::crc32(0,bytes.data(),static_cast<uInt>(bytes.size())));return 0;
    } catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
