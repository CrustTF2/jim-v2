#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/vtf.hpp"
#include <cassert>
#include <cstdio>

static void set32(std::vector<Byte>& bytes,size_t offset,uint32_t value) {
    for (unsigned i=0;i<4;++i) bytes[offset+i]=Byte(value>>(8*i));
}
static std::vector<Byte> image() {
    std::vector<Byte> bytes(92,0);
    std::memcpy(bytes.data(),"VTF\0",4);
    set32(bytes,4,7);set32(bytes,8,3);
    bytes[16]=1;bytes[18]=1;bytes[24]=1;bytes[56]=1;bytes[63]=1;
    set32(bytes,68,1);set32(bytes,80,0x30);set32(bytes,84,88);
    bytes[88]=1;bytes[89]=2;bytes[90]=3;bytes[91]=4;
    return bytes;
}
static bool rejected(const std::vector<Byte>& bytes,unsigned frame=0) {
    try { unsigned width=0,height=0;decode_vtf(bytes,width,height,frame);return false; }
    catch (const std::runtime_error&) { return true; }
}
int main() {
    auto valid=image();unsigned width=0,height=0;
    auto decoded=decode_vtf(valid,width,height);
    assert(width==1 && height==1 && decoded==std::vector<Byte>({1,2,3,4}));
    auto invalid=valid;invalid[56]=255;assert(rejected(invalid));
    invalid=valid;set32(invalid,84,0xffffffffu);assert(rejected(invalid));
    invalid=valid;invalid.resize(91);assert(rejected(invalid));
    invalid=valid;invalid[16]=0xff;invalid[17]=0xff;invalid[18]=0xff;invalid[19]=0xff;
    assert(rejected(invalid));
    invalid=valid;invalid[24]=0xff;invalid[25]=0xff;invalid[56]=16;assert(rejected(invalid,65534));
    std::puts("VTF input bounds checks passed.");
}
