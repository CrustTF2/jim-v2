// Minimal read-only Source MDL/VVD/VTX mesh reader, using highest-detail LOD.
#pragma once
#include "vpk.hpp"
#include <array>
#include <cmath>
#include <cctype>
#include <cstring>
#include <map>
#include <limits>

class ModelBytes {
    const std::vector<uint8_t>& b_;
public:
    explicit ModelBytes(const std::vector<uint8_t>& b):b_(b){}
    void span(size_t p,size_t n)const{if(p>b_.size()||n>b_.size()-p)throw std::runtime_error("Truncated Source model");}
    uint8_t u8(size_t p)const{span(p,1);return b_[p];}
    uint16_t u16(size_t p)const{span(p,2);return uint16_t(b_[p])|uint16_t(b_[p+1])<<8;}
    uint32_t u32(size_t p)const{span(p,4);return uint32_t(b_[p])|uint32_t(b_[p+1])<<8|uint32_t(b_[p+2])<<16|uint32_t(b_[p+3])<<24;}
    int32_t i32(size_t p)const{return static_cast<int32_t>(u32(p));}
    float f32(size_t p)const{auto bits=u32(p);float x;std::memcpy(&x,&bits,4);return x;}
    std::string str(size_t p)const{span(p,1);size_t e=p;while(e<b_.size()&&b_[e])++e;if(e==b_.size())throw std::runtime_error("Invalid model string");return std::string(reinterpret_cast<const char*>(b_.data()+p),e-p);}
    size_t relative(size_t base,int32_t offset)const{int64_t p=int64_t(base)+offset;if(p<0||p>int64_t(b_.size()))throw std::runtime_error("Invalid model offset");return size_t(p);}
    int count(size_t p)const{int n=i32(p);if(n<0||n>1000000)throw std::runtime_error("Invalid model count");return n;}
};
struct ModelVertex{float pos[3],normal[3],uv[2];};
struct ModelPart{std::string material;std::vector<uint32_t> indices;};
struct ModelMatrix{float v[12];};
struct StudioModel {
    std::vector<ModelVertex> vertices;std::vector<ModelPart> parts;std::vector<std::string> material_dirs;
    std::map<std::string,ModelMatrix> bones;ModelMatrix icon_camera{};bool has_icon_camera=false;
};
inline ModelMatrix inverse_affine(const ModelMatrix& m) {
    const float* a=m.v;float det=a[0]*(a[5]*a[10]-a[6]*a[9])-a[1]*(a[4]*a[10]-a[6]*a[8])+a[2]*(a[4]*a[9]-a[5]*a[8]);
    if(std::abs(det)<1e-10f)throw std::runtime_error("Invalid model transform");float k=1/det;
    ModelMatrix o{{(a[5]*a[10]-a[6]*a[9])*k,(a[2]*a[9]-a[1]*a[10])*k,(a[1]*a[6]-a[2]*a[5])*k,0,
                   (a[6]*a[8]-a[4]*a[10])*k,(a[0]*a[10]-a[2]*a[8])*k,(a[2]*a[4]-a[0]*a[6])*k,0,
                   (a[4]*a[9]-a[5]*a[8])*k,(a[1]*a[8]-a[0]*a[9])*k,(a[0]*a[5]-a[1]*a[4])*k,0}};
    for(int r=0;r<3;++r)o.v[r*4+3]=-(o.v[r*4]*a[3]+o.v[r*4+1]*a[7]+o.v[r*4+2]*a[11]);return o;
}
inline ModelMatrix multiply_affine(const ModelMatrix& a,const ModelMatrix& b){
    ModelMatrix o{};for(int r=0;r<3;++r)for(int c=0;c<4;++c){o.v[4*r+c]=c==3?a.v[4*r+3]:0;for(int k=0;k<3;++k)o.v[4*r+c]+=a.v[4*r+k]*b.v[4*k+c];}return o;
}
inline StudioModel read_studio_model(const std::vector<uint8_t>& mdl,const std::vector<uint8_t>& vvd,const std::vector<uint8_t>& vtx) {
    ModelBytes m(mdl),v(vvd),x(vtx);if(m.u32(0)!=0x54534449||v.u32(0)!=0x56534449)throw std::runtime_error("Invalid Source model signature");
    if(m.u32(8)!=v.u32(8)||m.u32(8)!=x.u32(16))throw std::runtime_error("Source model checksum mismatch");
    StudioModel out;int nverts=v.count(16),vstart=v.i32(56);v.span(vstart,size_t(nverts)*48);
    std::vector<ModelVertex> raw(nverts);
    for(int i=0;i<nverts;++i){size_t p=size_t(vstart)+size_t(i)*48;for(int j=0;j<3;++j){raw[i].pos[j]=v.f32(p+16+4*j);raw[i].normal[j]=v.f32(p+28+4*j);}for(int j=0;j<2;++j)raw[i].uv[j]=v.f32(p+40+4*j);}
    int fixes=v.count(48),fixpos=v.i32(52);
    if(fixes){for(int i=0;i<fixes;++i){size_t p=size_t(fixpos)+12ull*i;int lod=v.i32(p),start=v.i32(p+4),count=v.count(p+8);if(lod<0)continue;if(start<0||uint64_t(start)+count>raw.size())throw std::runtime_error("Invalid model LOD fixup");out.vertices.insert(out.vertices.end(),raw.begin()+start,raw.begin()+start+count);}}
    else out.vertices=std::move(raw);
    std::vector<std::string> textures;int texture_count=m.count(204),texture_pos=m.i32(208);
    for(int i=0;i<texture_count;++i){size_t p=size_t(texture_pos)+64ull*i;textures.push_back(m.str(m.relative(p,m.i32(p))));}
    for(int i=0;i<m.count(212);++i){size_t p=size_t(m.i32(216))+4ull*i;out.material_dirs.push_back(m.str(m.i32(p)));}
    std::vector<ModelMatrix> bone_order;int bonepos=m.i32(160);
    for(int i=0;i<m.count(156);++i){size_t p=size_t(bonepos)+216ull*i;std::string name=m.str(m.relative(p,m.i32(p)));for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));ModelMatrix mat;for(int j=0;j<12;++j)mat.v[j]=m.f32(p+96+4*j);out.bones[name]=mat;bone_order.push_back(mat);}
    int atpos=m.i32(244);
    for(int i=0;i<m.count(240);++i){size_t p=size_t(atpos)+92ull*i;auto name=m.str(m.relative(p,m.i32(p)));for(auto& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));int bone=m.i32(p+8);if(name=="icon_camera"&&bone>=0&&size_t(bone)<bone_order.size()){ModelMatrix local;for(int j=0;j<12;++j)local.v[j]=m.f32(p+12+4*j);out.icon_camera=multiply_affine(inverse_affine(bone_order[bone]),local);out.has_icon_camera=true;}}
    int body_count=std::min(m.count(232),x.count(28));int mb=m.i32(236),xb=x.i32(32);
    for(int bi=0;bi<body_count;++bi){size_t bp=size_t(mb)+16ull*bi,xp=size_t(xb)+8ull*bi;if(!m.count(bp+4)||!x.count(xp))continue;
        size_t model=m.relative(bp,m.i32(bp+12)),xmodel=x.relative(xp,x.i32(xp+4));if(!x.count(xmodel))continue;
        size_t lod=x.relative(xmodel,x.i32(xmodel+4));int meshes=std::min(m.count(model+72),x.count(lod));int vertex_base=m.i32(model+84)/48;
        size_t mm=m.relative(model,m.i32(model+76)),xm=x.relative(lod,x.i32(lod+4));
        for(int mi=0;mi<meshes;++mi){size_t mesh=mm+116ull*mi,xmesh=xm+9ull*mi;int mat=m.i32(mesh),offset=m.i32(mesh+12);
            if(m.i32(224)>0 && mat>=0 && mat<m.i32(220))mat=m.u16(size_t(m.i32(228))+2ull*mat);
            std::string name=(mat>=0&&size_t(mat)<textures.size())?textures[mat]:"";
            int groups=x.count(xmesh);size_t groupbase=x.relative(xmesh,x.i32(xmesh+4));
            for(int gi=0;gi<groups;++gi){size_t g=groupbase+25ull*gi;int nv=x.count(g),ni=x.count(g+8),ns=x.count(g+16);size_t gv=x.relative(g,x.i32(g+4)),idx=x.relative(g,x.i32(g+12)),strips=x.relative(g,x.i32(g+20));x.span(gv,size_t(nv)*9);x.span(idx,size_t(ni)*2);
                ModelPart part;part.material=name;
                auto index=[&](int i)->uint32_t{if(i<0||i>=ni)throw std::runtime_error("Invalid strip index");int local=x.u16(idx+2ull*i);if(local>=nv)throw std::runtime_error("Invalid model vertex mapping");int64_t id=int64_t(x.u16(gv+9ull*local+4))+vertex_base+offset;if(id<0||id>=int64_t(out.vertices.size()))throw std::runtime_error("Invalid model vertex index");return uint32_t(id);};
                for(int si=0;si<ns;++si){size_t strip=strips+27ull*si;int n=x.count(strip),first=x.i32(strip+4);uint8_t flags=x.u8(strip+18);
                    if(flags&1){for(int i=0;i<n;++i)part.indices.push_back(index(first+i));}
                    else if(flags&2){for(int i=0;i+2<n;++i){auto a=index(first+i),b=index(first+i+1),c=index(first+i+2);if(a==b||b==c||a==c)continue;part.indices.push_back(i%2?b:a);part.indices.push_back(i%2?a:b);part.indices.push_back(c);}}
                    else throw std::runtime_error("Unsupported triangle strip");
                }
                if(!part.indices.empty())out.parts.push_back(std::move(part));
            }
        }
    }
    if(out.parts.empty())throw std::runtime_error("No renderable model meshes");return out;
}
