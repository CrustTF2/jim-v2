#include "paint_definitions.hpp"
#include <cstdio>
int main(){try {
    PaintDefinitions p(L"D:\\Steam\\steamapps\\common\\Team Fortress 2\\tf\\scripts\\protodefs\\proto_defs.vpd");
    auto a=p.paints.find(427),b=p.paints.find(440);
    if(a==p.paints.end()||b==p.paints.end())return 2;
    std::printf("operations=%zu items=%zu paints=%zu paint427=%s paint440=%s\n",p.operations.size(),p.items.size(),p.paints.size(),a->second.header().name().c_str(),b->second.header().name().c_str());
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
