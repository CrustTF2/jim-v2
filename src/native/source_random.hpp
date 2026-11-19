// Calls TF2's own 32-bit Source random-stream implementation without altering it.
#pragma once
#include <windows.h>
#include <cstdint>
#include <stdexcept>
#include <string>

class SourceRandom {
    using Constructor=void* (__thiscall*)(void*);
    using SetSeed=void (__thiscall*)(void*,int32_t);
    using RandomFloat=float (__thiscall*)(void*,float,float);
    using RandomInt=int (__thiscall*)(void*,int,int);
    HMODULE library_=nullptr;
    alignas(16) unsigned char data_[2][512]{};
    RandomFloat uniform_=nullptr;RandomInt integer_=nullptr;int stage_=0;
public:
    SourceRandom(const std::wstring& tf,uint64_t seed) {
        std::wstring bin=tf.substr(0,tf.find_last_of(L"\\/"))+L"\\bin";
        library_=LoadLibraryExW((bin+L"\\vstdlib.dll").c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        if(!library_)throw std::runtime_error("Unable to load TF2's random generator");
        auto construct=reinterpret_cast<Constructor>(GetProcAddress(library_,"??0CUniformRandomStream@@QAE@XZ"));
        auto setseed=reinterpret_cast<SetSeed>(GetProcAddress(library_,"?SetSeed@CUniformRandomStream@@UAEXH@Z"));
        uniform_=reinterpret_cast<RandomFloat>(GetProcAddress(library_,"?RandomFloat@CUniformRandomStream@@UAEMMM@Z"));
        integer_=reinterpret_cast<RandomInt>(GetProcAddress(library_,"?RandomInt@CUniformRandomStream@@UAEHHH@Z"));
        if(!construct||!setseed||!uniform_||!integer_){FreeLibrary(library_);library_=nullptr;throw std::runtime_error("Incompatible TF2 random generator");}
        for(unsigned parity=0;parity<2;++parity) {
            uint32_t value=0;for(unsigned i=0;i<32;++i)value|=uint32_t((seed>>(2*i+parity))&1)<<i;
            construct(data_[parity]);setseed(data_[parity],static_cast<int32_t>(value));
        }
    }
    ~SourceRandom(){if(library_)FreeLibrary(library_);}
    SourceRandom(const SourceRandom&)=delete;SourceRandom& operator=(const SourceRandom&)=delete;
    int next_stage(){int current=stage_;stage_^=1;return current;}
    float uniform(int stage,float lo,float hi){return uniform_(data_[stage],lo,hi);}
    int integer(int stage,int lo,int hi){return integer_(data_[stage],lo,hi);}
};
