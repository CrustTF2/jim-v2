// Read TF2's local protocol-based paint graph definitions.
#pragma once
#include "tf_proto_def_messages.pb.h"
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class PaintDefinitions {
    static uint32_t read32(std::ifstream& file) {
        unsigned char b[4];file.read(reinterpret_cast<char*>(b),4);if(!file)throw std::runtime_error("Truncated paint definitions");
        return uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;
    }
    template<class T> static void record(std::unordered_map<uint32_t,T>& target,const std::vector<char>& bytes) {
        T value;if(!value.ParseFromArray(bytes.data(),static_cast<int>(bytes.size())))throw std::runtime_error("Invalid paint-definition record");
        target[value.header().defindex()]=std::move(value);
    }
public:
    std::unordered_map<uint32_t,CMsgPaintKit_Operation> operations;
    std::unordered_map<uint32_t,CMsgPaintKit_ItemDefinition> items;
    std::unordered_map<uint32_t,CMsgPaintKit_Definition> paints;
    explicit PaintDefinitions(const std::wstring& path) {
        std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("TF2 paint definitions not found");
        while(in.peek()!=std::char_traits<char>::eof()) {
            uint32_t kind=read32(in),count=read32(in);if(count>1000000)throw std::runtime_error("Invalid paint record count");
            for(uint32_t i=0;i<count;++i) {
                uint32_t size=read32(in);if(size>64*1024*1024)throw std::runtime_error("Paint record too large");
                if(kind!=7&&kind!=8&&kind!=9){in.seekg(size,std::ios::cur);if(!in)throw std::runtime_error("Truncated paint record");continue;}
                std::vector<char> bytes(size);in.read(bytes.data(),size);if(!in)throw std::runtime_error("Truncated paint record");
                if(kind==7)record(operations,bytes);else if(kind==8)record(items,bytes);else record(paints,bytes);
            }
        }
    }
};
