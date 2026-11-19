#define NOMINMAX
#include <windows.h>
#include "studio_model.hpp"
#include <cstdio>
int main(int argc,char** argv){
    try{
        if(argc!=3)return 2;
        int n=MultiByteToWideChar(CP_UTF8,0,argv[1],-1,nullptr,0);
        std::wstring path(n,L'\0');MultiByteToWideChar(CP_UTF8,0,argv[1],-1,&path[0],n);path.pop_back();
        VpkArchive archive(path);std::string base=argv[2];
        auto model=read_studio_model(archive.read(base+".mdl"),archive.read(base+".vvd"),archive.read(base+".dx90.vtx"));
        size_t indices=0;for(auto& part:model.parts)indices+=part.indices.size();
        std::printf("vertices=%zu parts=%zu indices=%zu camera=%d bones=%zu\n",model.vertices.size(),model.parts.size(),indices,model.has_icon_camera,model.bones.size());
        for(auto& part:model.parts)std::printf("%s %zu\n",part.material.c_str(),part.indices.size());
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
