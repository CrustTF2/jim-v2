#include "source_random.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc,char** argv) {
    try {
        if(argc!=2)return 2;
        SourceRandom random(L"D:\\Steam\\steamapps\\common\\Team Fortress 2\\tf",_strtoui64(argv[1],nullptr,10));
        for(int i=0;i<8;++i){int stage=random.next_stage();float a=random.uniform(stage,0,1),b=random.uniform(stage,-8,100);int c=random.integer(stage,0,1);std::printf("%.9g %.9g %d\n",a,b,c);}
        return 0;
    } catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
