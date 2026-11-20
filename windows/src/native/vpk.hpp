// Read-only Valve VPK archive access for the native TF2 renderer.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <zlib.h>

class VpkArchive {
    struct Entry { uint32_t crc,offset,size;uint16_t index;std::vector<uint8_t> prefix; };
    std::wstring path_;std::vector<uint8_t> data_;size_t embedded_=0;
    std::unordered_map<std::string,Entry> entries_;
    static uint16_t word(const std::vector<uint8_t>& d,size_t p) {
        if(p+2>d.size())throw std::runtime_error("Truncated VPK directory");return uint16_t(d[p])|uint16_t(d[p+1])<<8;
    }
    static uint32_t dword(const std::vector<uint8_t>& d,size_t p) {
        if(p+4>d.size())throw std::runtime_error("Truncated VPK directory");return uint32_t(d[p])|uint32_t(d[p+1])<<8|uint32_t(d[p+2])<<16|uint32_t(d[p+3])<<24;
    }
    static std::string lower(std::string s) {for(char& c:s){if(c=='\\')c='/';else if(c>='A'&&c<='Z')c+=('a'-'A');}return s;}
public:
    explicit VpkArchive(const std::wstring& path):path_(path) {
        std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("VPK archive is unavailable");
        auto n=in.tellg();if(n<12||n>1024ll*1024*1024)throw std::runtime_error("Invalid VPK size");
        data_.resize(static_cast<size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(data_.data()),n);if(!in)throw std::runtime_error("Unable to read VPK");
        if(dword(data_,0)!=0x55aa1234)throw std::runtime_error("Invalid VPK signature");
        uint32_t version=dword(data_,4),tree=dword(data_,8);if(version!=1&&version!=2)throw std::runtime_error("Unsupported VPK version");
        size_t pos=version==1?12:28;embedded_=pos+tree;if(embedded_>data_.size())throw std::runtime_error("Invalid VPK tree length");
        auto str=[&](){size_t start=pos;while(pos<embedded_ && data_[pos])++pos;if(pos>=embedded_)throw std::runtime_error("Invalid VPK string");std::string out(reinterpret_cast<const char*>(data_.data()+start),pos-start);++pos;return out;};
        for(;;){std::string ext=str();if(ext.empty())break;
            for(;;){std::string dir=str();if(dir.empty())break;
                for(;;){std::string name=str();if(name.empty())break;
                    Entry e;e.crc=dword(data_,pos);uint16_t preload=word(data_,pos+4);e.index=word(data_,pos+6);e.offset=dword(data_,pos+8);e.size=dword(data_,pos+12);
                    if(word(data_,pos+16)!=0xffff || pos+18ull+preload>embedded_)throw std::runtime_error("Invalid VPK entry");pos+=18;
                    e.prefix.assign(data_.begin()+pos,data_.begin()+pos+preload);pos+=preload;
                    entries_.emplace(lower((dir==" "?"":dir+"/")+name+"."+ext),std::move(e));
                }
            }
        }
        if(pos!=embedded_)throw std::runtime_error("VPK directory length mismatch");
    }
    bool contains(const std::string& key)const{return entries_.find(lower(key))!=entries_.end();}
    std::vector<uint8_t> read(const std::string& key)const {
        auto found=entries_.find(lower(key));if(found==entries_.end())throw std::runtime_error("VPK entry not found: "+key);
        const Entry& e=found->second;std::vector<uint8_t> result=e.prefix;size_t before=result.size();
        if(uint64_t(before)+e.size>512ull*1024*1024)throw std::runtime_error("VPK entry too large");result.resize(before+e.size);
        if(e.index==0x7fff) {
            if(uint64_t(embedded_)+e.offset+e.size>data_.size())throw std::runtime_error("Truncated embedded VPK entry");
            std::copy(data_.begin()+embedded_+e.offset,data_.begin()+embedded_+e.offset+e.size,result.begin()+before);
        } else {
            auto path=path_;auto suffix=path.rfind(L"_dir.vpk");if(suffix==std::wstring::npos)throw std::runtime_error("Invalid VPK name");
            wchar_t part[32];swprintf_s(part,L"_%03u.vpk",unsigned(e.index));path.replace(suffix,std::wstring::npos,part);
            std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("VPK data file unavailable");file.seekg(e.offset);
            file.read(reinterpret_cast<char*>(result.data()+before),e.size);if(!file)throw std::runtime_error("Truncated VPK data");
        }
        if(::crc32(0,result.data(),static_cast<uInt>(result.size()))!=e.crc)throw std::runtime_error("VPK checksum mismatch: "+key);
        return result;
    }
};
