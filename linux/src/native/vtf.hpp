// Read TF2 VTF textures without Python.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

using Byte=uint8_t;

static uint32_t u32(const std::vector<Byte>& b,size_t p) {
    if (p+4>b.size()) throw std::runtime_error("Truncated VTF");
    return uint32_t(b[p]) | uint32_t(b[p+1])<<8 | uint32_t(b[p+2])<<16 | uint32_t(b[p+3])<<24;
}
static uint16_t u16(const std::vector<Byte>& b,size_t p) {
    if (p+2>b.size()) throw std::runtime_error("Truncated VTF");
    return uint16_t(b[p]) | uint16_t(b[p+1])<<8;
}
struct Color { Byte r,g,b,a; };
static Color color565(unsigned c) {
    unsigned r=(c>>11)&31,g=(c>>5)&63,b=c&31;
    return {Byte((r<<3)|(r>>2)),Byte((g<<2)|(g>>4)),Byte((b<<3)|(b>>2)),255};
}
static std::vector<Byte> decode_vtf(const std::vector<Byte>& b,unsigned& width,unsigned& height,unsigned frame=0) {
    if (b.size()<88 || std::memcmp(b.data(),"VTF\0",4) || u32(b,4)!=7 || u32(b,8)<3)
        throw std::runtime_error("Unsupported VTF header");
    width=u16(b,16);height=u16(b,18);unsigned frames=u16(b,24),fmt=u32(b,52),mips=b[56];
    if (!width || !height || uint64_t(width)*height>16000000 || !frames || frame>=frames || !mips || u16(b,63)!=1)
        throw std::runtime_error("Unsupported VTF dimensions");
    const bool block=fmt==13 || fmt==14 || fmt==15 || fmt==20;
    unsigned bpp=fmt==2?3:4;
    if (!block && fmt!=0 && fmt!=2 && fmt!=12) throw std::runtime_error("Unsupported VTF format");
    auto size=[&](unsigned x,unsigned y)->uint64_t {
        return block?uint64_t(std::max(1u,(x+3)/4))*std::max(1u,(y+3)/4)*(fmt==13||fmt==20?8:16):uint64_t(x)*y*bpp;
    };
    unsigned count=u32(b,68);size_t off=0;
    if (count>1024 || 80ull+uint64_t(count)*8>b.size()) throw std::runtime_error("Invalid VTF resource table");
    for (unsigned i=0;i<count;++i) if (u32(b,80+8*i)==0x30) off=u32(b,84+8*i);
    if (!off) throw std::runtime_error("VTF image resource missing");
    for (int level=int(mips)-1;level>0;--level) off+=size(std::max(1u,width>>level),std::max(1u,height>>level))*frames;
    off+=size(width,height)*frame;
    if (off>b.size() || size(width,height)>b.size()-off) throw std::runtime_error("Truncated VTF image");
    std::vector<Byte> out(uint64_t(width)*height*4);
    if (!block) {
        for (size_t i=0;i<size_t(width)*height;++i) {
            const Byte* p=b.data()+off+i*bpp;Byte* q=out.data()+4*i;
            q[0]=p[fmt==12?2:0];q[1]=p[1];q[2]=p[fmt==12?0:2];q[3]=bpp==4?p[3]:255;
        }
        return out;
    }
    size_t pos=off;
    for (unsigned by=0;by<(height+3)/4;++by) for (unsigned bx=0;bx<(width+3)/4;++bx) {
        Byte alpha[16];std::fill(alpha,alpha+16,255);
        if (fmt==14) {
            for (int i=0;i<16;++i) alpha[i]=Byte(((b[pos+i/2]>>(4*(i%2)))&15)*17);
            pos+=8;
        } else if (fmt==15) {
            Byte table[8]={b[pos],b[pos+1]};
            if(table[0]>table[1]) for(int i=1;i<=6;++i)table[i+1]=Byte(((7-i)*table[0]+i*table[1])/7);
            else {for(int i=1;i<=4;++i)table[i+1]=Byte(((5-i)*table[0]+i*table[1])/5);table[6]=0;table[7]=255;}
            uint64_t bits=0;for(int i=0;i<6;++i)bits|=uint64_t(b[pos+2+i])<<(8*i);
            for(int i=0;i<16;++i)alpha[i]=table[(bits>>(3*i))&7];pos+=8;
        }
        unsigned c0=u16(b,pos),c1=u16(b,pos+2);Color c[4]={color565(c0),color565(c1),{}, {}};
        if(c0>c1 || fmt==14 || fmt==15) {
            c[2]={Byte((2*c[0].r+c[1].r)/3),Byte((2*c[0].g+c[1].g)/3),Byte((2*c[0].b+c[1].b)/3),255};
            c[3]={Byte((c[0].r+2*c[1].r)/3),Byte((c[0].g+2*c[1].g)/3),Byte((c[0].b+2*c[1].b)/3),255};
        } else {
            c[2]={Byte((c[0].r+c[1].r)/2),Byte((c[0].g+c[1].g)/2),Byte((c[0].b+c[1].b)/2),255};
            c[3]={0,0,0,0};
        }
        uint32_t indices=u32(b,pos+4);pos+=8;
        for(unsigned y=0;y<4;++y) for(unsigned x=0;x<4;++x) {
            unsigned px=bx*4+x,py=by*4+y,i=y*4+x;if(px>=width||py>=height)continue;
            Color pixel=c[(indices>>(2*i))&3];Byte* q=out.data()+4*(py*width+px);
            q[0]=pixel.r;q[1]=pixel.g;q[2]=pixel.b;q[3]=Byte((unsigned(pixel.a)*alpha[i])/255);
        }
    }
    return out;
}
