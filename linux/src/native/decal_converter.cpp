// Standalone native Steam Cloud decal converter. Does not modify TF2 files.
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
#include "vtf.hpp"

static void check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("Windows image codec failed"); }
static std::vector<Byte> read_file(const std::string& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in)throw std::runtime_error("Decal input not found");
    auto len=in.tellg();if(len<=0 || len>8*1024*1024)throw std::runtime_error("Invalid decal input size");
    std::vector<Byte> data(static_cast<size_t>(len));in.seekg(0);in.read(reinterpret_cast<char*>(data.data()),len);
    if(!in)throw std::runtime_error("Unable to read decal");return data;
}
struct ComLifetime {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~ComLifetime() { if(SUCCEEDED(result)) CoUninitialize(); }
};
int main(int argc,char** argv) {
    if(argc!=2 || std::strlen(argv[1])!=16 || std::string(argv[1]).find_first_not_of("0123456789abcdef")!=std::string::npos)return 2;
    const std::string prefix=std::string("thumbnail_renderer/decals/")+argv[1];
    ComLifetime com;
    try {
        auto data=read_file(prefix+".raw");ComPtr<IWICImagingFactory> factory;
        check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        ComPtr<IWICBitmapSource> source;std::vector<Byte> pixels;
        if(data.size()>=4 && std::memcmp(data.data(),"VTF\0",4)==0) {
            unsigned width,height;pixels=decode_vtf(data,width,height);ComPtr<IWICBitmap> image;
            check(factory->CreateBitmapFromMemory(width,height,GUID_WICPixelFormat32bppRGBA,width*4,UINT(pixels.size()),pixels.data(),&image));
            source=image;
        } else {
            ComPtr<IStream> stream;stream.Attach(SHCreateMemStream(data.data(),UINT(data.size())));if(!stream)throw std::runtime_error("Invalid image stream");
            ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));
            ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));source=frame;
        }
        UINT width,height;check(source->GetSize(&width,&height));
        if(!width||!height||uint64_t(width)*height>16000000)throw std::runtime_error("Decal is too large");
        if(width>512||height>512) {
            float scale=512.0f/std::max(width,height);UINT w=std::max(1u,UINT(width*scale+.5f)),h=std::max(1u,UINT(height*scale+.5f));
            ComPtr<IWICBitmapScaler> scaler;check(factory->CreateBitmapScaler(&scaler));check(scaler->Initialize(source.Get(),w,h,WICBitmapInterpolationModeFant));source=scaler;width=w;height=h;
        }
        ComPtr<IWICFormatConverter> rgba;check(factory->CreateFormatConverter(&rgba));
        check(rgba->Initialize(source.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
        std::wstring tmp(prefix.begin(),prefix.end());tmp+=L".tmp";
        ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromFilename(tmp.c_str(),GENERIC_WRITE));
        ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
        ComPtr<IWICBitmapFrameEncode> output;check(encoder->CreateNewFrame(&output,nullptr));check(output->Initialize(nullptr));check(output->SetSize(width,height));
        WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;check(output->SetPixelFormat(&format));check(output->WriteSource(rgba.Get(),nullptr));
        check(output->Commit());check(encoder->Commit());output.Reset();encoder.Reset();stream.Reset();
        std::wstring result(prefix.begin(),prefix.end());result+=L".png";
        if(!MoveFileExW(tmp.c_str(),result.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Unable to save converted decal");
        return 0;
    } catch(const std::exception& e) {
        std::ofstream log("thumbnail_renderer/decal-errors.log",std::ios::app);log<<e.what()<<'\n';
        return 1;
    }
}
