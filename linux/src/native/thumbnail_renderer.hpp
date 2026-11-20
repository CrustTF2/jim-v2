// Native offscreen renderer for inventory thumbnails.
#pragma once
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include <png.h>
#include "paint_compositor.hpp"
#include "studio_model.hpp"
#include <array>
#include <map>
#include <regex>
#include <sstream>

class NativeThumbnailRenderer {
 using GLuintProc=GLuint (APIENTRY*)(GLenum);
 using SourceProc=void (APIENTRY*)(GLuint,GLsizei,const char* const*,const GLint*);
 using CompileProc=void (APIENTRY*)(GLuint);
 using QueryProc=void (APIENTRY*)(GLuint,GLenum,GLint*);
 using LogProc=void (APIENTRY*)(GLuint,GLsizei,GLsizei*,char*);
 using CreateProgramProc=GLuint (APIENTRY*)();
 using AttachProc=void (APIENTRY*)(GLuint,GLuint);
 using ProgramProc=void (APIENTRY*)(GLuint);
 using UniformLocationProc=GLint (APIENTRY*)(GLuint,const char*);
 using Uniform1iProc=void (APIENTRY*)(GLint,GLint);
 using Uniform1fProc=void (APIENTRY*)(GLint,GLfloat);
 using ActiveTextureProc=void (APIENTRY*)(GLenum);
 static LRESULT CALLBACK window_proc(HWND h,UINT msg,WPARAM w,LPARAM l){return DefWindowProcW(h,msg,w,l);}
 template<class T>T glfn(const char* name){auto p=wglGetProcAddress(name);if(!p||p==reinterpret_cast<PROC>(1)||p==reinterpret_cast<PROC>(2)||p==reinterpret_cast<PROC>(3)||p==reinterpret_cast<PROC>(-1))throw std::runtime_error(std::string("OpenGL function unavailable: ")+name);return reinterpret_cast<T>(p);}
 HWND window_=nullptr;HDC dc_=nullptr;HGLRC gl_=nullptr;GLuint program_=0;
 GLuintProc create_shader_;SourceProc shader_source_;CompileProc compile_shader_;QueryProc shader_status_,program_status_;LogProc shader_log_,program_log_;
 CreateProgramProc create_program_;AttachProc attach_;ProgramProc link_,use_,delete_shader_,delete_program_;
 UniformLocationProc uniform_;Uniform1iProc uniform1i_;Uniform1fProc uniform1f_;ActiveTextureProc active_;
 std::wstring tf_;VpkArchive& misc_;VpkArchive& textures_;
 std::map<std::string,StudioModel> models_;
 struct Material {std::vector<uint8_t> pixels,spec;unsigned width=0,height=0,sw=1,sh=1;float boost=1,factor=60;bool skip=false;};
 std::map<std::string,Material> materials_;size_t material_bytes_=0;
 static std::wstring wide(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,nullptr,0);if(!n)throw std::runtime_error("Invalid path");std::wstring w(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.c_str(),-1,&w[0],n);w.pop_back();return w;}
 std::vector<uint8_t> asset(VpkArchive& archive,const std::string& name,bool optional=false){
  std::ifstream f(tf_+L"/"+wide(name),std::ios::binary|std::ios::ate);
  if(f){auto n=f.tellg();if(n<0||n>512ll*1024*1024)throw std::runtime_error("Invalid asset");std::vector<uint8_t> b(size_t(n),0);f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);if(!f)throw std::runtime_error("Asset read failed");return b;}
  if(archive.contains(name))return archive.read(name);if(optional)return {};throw std::runtime_error("Asset not found: "+name);
 }
 static std::string normalize(std::string s){for(auto& c:s)if(c=='\\')c='/';return s;}
 StudioModel& model(const std::string& name){
  auto it=models_.find(name);if(it!=models_.end())return it->second;
  auto path=normalize(name);if(path.size()<4||path.substr(path.size()-4)!=".mdl"||path.find("..")!=std::string::npos)throw std::runtime_error("Invalid model");
  auto stem=path.substr(0,path.size()-4);auto mdl=asset(misc_,path),vvd=asset(misc_,stem+".vvd"),vtx=asset(misc_,stem+".dx90.vtx");
  return models_.emplace(name,read_studio_model(mdl,vvd,vtx)).first->second;
 }
 Material& material(const std::string& name,const std::vector<std::string>& dirs,int frame){
  std::string key=name+"#"+std::to_string(frame);for(auto& d:dirs)key+="#"+d;auto found=materials_.find(key);if(found!=materials_.end())return found->second;
  std::vector<std::string> names{name};for(auto& d:dirs)names.push_back(normalize(d)+name);
  Material m;for(auto candidate:names){
   candidate=normalize(candidate);while(!candidate.empty()&&candidate[0]=='/')candidate.erase(candidate.begin());
   if(candidate.size()>4&&candidate.substr(candidate.size()-4)==".vmt")candidate.resize(candidate.size()-4);
   auto data=asset(misc_,"materials/"+candidate+".vmt",true);if(data.empty())continue;std::string text(data.begin(),data.end());std::smatch match;
   if(std::regex_search(text,std::regex("^\\s*\"?Refract\"?\\s*\\{",std::regex::icase))){m.skip=true;return materials_.emplace(key,std::move(m)).first->second;}
   if(!std::regex_search(text,match,std::regex("\"?\\$basetexture\"?\\s+\"([^\"]+)\"",std::regex::icase)))continue;
   auto base=normalize(match[1]);if(base.size()>4&&base.substr(base.size()-4)==".vtf")base.resize(base.size()-4);
   auto pixels=asset(textures_,"materials/"+base+".vtf",true);if(pixels.empty())continue;m.pixels=decode_vtf(pixels,m.width,m.height,unsigned(frame));
   if(std::regex_search(text,match,std::regex("\"?\\$phongexponenttexture\"?\\s+\"([^\"]+)\"",std::regex::icase))){
    auto spec=normalize(match[1]);if(spec.size()>4&&spec.substr(spec.size()-4)==".vtf")spec.resize(spec.size()-4);
    auto sp=asset(textures_,"materials/"+spec+".vtf",true);if(!sp.empty())m.spec=decode_vtf(sp,m.sw,m.sh);
   }
   if(m.spec.empty())m.spec={96,96,96,255};
   auto setting=[&](const char* field,float fallback){std::smatch s;std::regex re(std::string("\"?\\$")+field+"\"?\\s+\"?([0-9.]+)",std::regex::icase);return std::regex_search(text,s,re)?std::stof(s[1]):fallback;};
   m.boost=setting("phongboost",1);m.factor=setting("phongexponentfactor",60);
   size_t bytes=m.pixels.size()+m.spec.size();if(material_bytes_+bytes>64ull*1024*1024){materials_.clear();material_bytes_=0;}material_bytes_+=bytes;
   return materials_.emplace(key,std::move(m)).first->second;
  }
  throw std::runtime_error("Material not found: "+name);
 }
 static GLuint upload(const uint8_t* data,unsigned w,unsigned h){
  GLuint id;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);return id;
 }
 GLuint shader(GLenum type,const char* source){
  GLuint s=create_shader_(type);shader_source_(s,1,&source,nullptr);compile_shader_(s);GLint ok=0;shader_status_(s,0x8B81,&ok);if(!ok){char buf[2048]{};shader_log_(s,sizeof(buf),nullptr,buf);throw std::runtime_error(std::string("Shader failed: ")+buf);}return s;
 }
public:
 NativeThumbnailRenderer(const std::wstring& tf,VpkArchive& misc,VpkArchive& textures):tf_(tf),misc_(misc),textures_(textures){
  WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"TF2NativeThumbnailWorker";RegisterClassW(&wc);
  window_=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,512,320,nullptr,nullptr,wc.hInstance,nullptr);if(!window_)throw std::runtime_error("Thumbnail window failed");
  dc_=GetDC(window_);PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cAlphaBits=8;pfd.cDepthBits=24;pfd.iLayerType=PFD_MAIN_PLANE;
  int pf=ChoosePixelFormat(dc_,&pfd);if(!pf||!SetPixelFormat(dc_,pf,&pfd))throw std::runtime_error("OpenGL pixel format failed");
  gl_=wglCreateContext(dc_);if(!gl_||!wglMakeCurrent(dc_,gl_))throw std::runtime_error("OpenGL context failed");
  create_shader_=glfn<GLuintProc>("glCreateShader");shader_source_=glfn<SourceProc>("glShaderSource");compile_shader_=glfn<CompileProc>("glCompileShader");shader_status_=glfn<QueryProc>("glGetShaderiv");shader_log_=glfn<LogProc>("glGetShaderInfoLog");
  create_program_=glfn<CreateProgramProc>("glCreateProgram");attach_=glfn<AttachProc>("glAttachShader");link_=glfn<ProgramProc>("glLinkProgram");use_=glfn<ProgramProc>("glUseProgram");delete_shader_=glfn<ProgramProc>("glDeleteShader");delete_program_=glfn<ProgramProc>("glDeleteProgram");program_status_=glfn<QueryProc>("glGetProgramiv");program_log_=glfn<LogProc>("glGetProgramInfoLog");uniform_=glfn<UniformLocationProc>("glGetUniformLocation");uniform1i_=glfn<Uniform1iProc>("glUniform1i");uniform1f_=glfn<Uniform1fProc>("glUniform1f");active_=glfn<ActiveTextureProc>("glActiveTexture");
  const char* vs="#version 120\nvarying vec3 normal;varying vec2 uv;void main(){gl_Position=gl_Vertex;normal=normalize(gl_Normal);uv=gl_MultiTexCoord0.xy;}";
  const char* fs="#version 120\nuniform sampler2D diffuse_texture;uniform sampler2D phong_texture;uniform float phong_boost;uniform float phong_exponent_factor;varying vec3 normal;varying vec2 uv;void main(){vec3 n=normalize(normal);vec3 light=normalize(vec3(-0.3,0.65,0.7));vec4 surface=texture2D(diffuse_texture,uv);float diffuse=0.84+0.16*abs(dot(n,light))+0.04*max(n.y,0.0);vec3 h=normalize(light+vec3(0.0,0.0,1.0));float exponent=clamp(texture2D(phong_texture,uv).r*phong_exponent_factor,5.0,100.0);float phong=pow(abs(dot(n,h)),exponent)*phong_boost*mix(0.25,1.0,surface.a);gl_FragColor=vec4(clamp(pow(surface.rgb,vec3(0.92))*diffuse+vec3(phong*1.10),0.0,1.0),1.0);}";
  GLuint vert=shader(0x8B31,vs),frag=shader(0x8B30,fs);program_=create_program_();attach_(program_,vert);attach_(program_,frag);link_(program_);delete_shader_(vert);delete_shader_(frag);GLint ok=0;program_status_(program_,0x8B82,&ok);if(!ok){char b[2048]{};program_log_(program_,sizeof(b),nullptr,b);throw std::runtime_error(std::string("Shader link failed: ")+b);}
 }
 ~NativeThumbnailRenderer(){if(gl_){wglMakeCurrent(dc_,gl_);if(program_)delete_program_(program_);wglMakeCurrent(nullptr,nullptr);wglDeleteContext(gl_);}if(dc_)ReleaseDC(window_,dc_);if(window_)DestroyWindow(window_);}
 NativeThumbnailRenderer(const NativeThumbnailRenderer&)=delete;NativeThumbnailRenderer& operator=(const NativeThumbnailRenderer&)=delete;
 std::vector<uint8_t> render(const std::string& path,const std::vector<uint8_t>& paint,bool stat_clock,float clock_scale,int clock_count){
  wglMakeCurrent(dc_,gl_);if(models_.size()>16)models_.clear();auto& weapon=model(path);std::vector<const ModelVertex*> points;for(auto& part:weapon.parts)for(auto index:part.indices)points.push_back(&weapon.vertices[index]);if(points.empty())throw std::runtime_error("Empty weapon");
  float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};for(auto p:points)for(int i=0;i<3;++i){lo[i]=std::min(lo[i],p->pos[i]);hi[i]=std::max(hi[i],p->pos[i]);}
  float extent[3],center[3];for(int i=0;i<3;++i){extent[i]=hi[i]-lo[i];center[i]=(hi[i]+lo[i])*.5f;}
  float rot[9]={0,0,-1,0,1,0,1,0,0};if(extent[1]>extent[2]*1.4f&&extent[1]>extent[0]*1.4f){float r[]={0,-1,0,0,0,1,-1,0,0};std::copy(r,r+9,rot);}
  const float aspect=1.6f;float scale=.88f/std::max({std::abs(rot[3])*extent[0]/2+std::abs(rot[4])*extent[1]/2+std::abs(rot[5])*extent[2]/2,(std::abs(rot[0])*extent[0]/2+std::abs(rot[1])*extent[1]/2+std::abs(rot[2])*extent[2]/2)/aspect,1e-5f});
  float radius=0;for(auto p:points){float d=0;for(int i=0;i<3;++i)d+=(p->pos[i]-center[i])*(p->pos[i]-center[i]);radius=std::max(radius,std::sqrt(d));}
  bool perspective=false;float focalx=1.f/std::tan(54.f*3.14159265f/360.f),focaly=focalx*aspect,panx=0,pany=0;
  if(weapon.has_icon_camera){
   auto c=weapon.icon_camera.v;for(int i=0;i<3;++i){rot[i]=-c[4*i+1];rot[3+i]=c[4*i+2];rot[6+i]=-c[4*i];center[i]=c[4*i+3];}
   float lox=1e30f,loy=1e30f,hix=-1e30f,hiy=-1e30f;int visible=0;
   for(auto p:points){float v[3]={};for(int i=0;i<3;++i)for(int j=0;j<3;++j)v[i]+=rot[i*3+j]*(p->pos[j]-center[j]);if(v[2]>=-.1f)continue;float x=v[0]/-v[2]*focalx,y=v[1]/-v[2]*focaly;lox=std::min(lox,x);hix=std::max(hix,x);loy=std::min(loy,y);hiy=std::max(hiy,y);++visible;}
   perspective=true;if(visible){float fit=.88f/std::max({(hix-lox)/2,(hiy-loy)/2,1e-5f});focalx*=fit;focaly*=fit;panx=-(hix+lox)/2*fit;pany=-(hiy+loy)/2*fit;}
  }
  glViewport(0,0,512,320);glClearColor(0,0,0,0);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);use_(program_);
  uniform1i_(uniform_(program_,"diffuse_texture"),0);uniform1i_(uniform_(program_,"phong_texture"),1);
  active_(0x84C0);GLuint paint_id=upload(paint.data(),512,512);
  ModelMatrix identity{{1,0,0,0,0,1,0,0,0,0,1,0}};
  auto draw=[&](StudioModel& m,const ModelMatrix& matrix,bool painted){
   for(auto& part:m.parts){int frame=0;if(!painted){std::smatch match;if(std::regex_search(part.material,match,std::regex("stattrack_dial([0-9]{3})$",std::regex::icase))){int digit=std::stoi(match[1]);frame=(clock_count/int(std::pow(10,digit)))%10;}}
    auto& mat=material(part.material,m.material_dirs,frame);if(mat.skip)continue;
    active_(0x84C0);GLuint base=painted?paint_id:upload(mat.pixels.data(),mat.width,mat.height);if(painted)glBindTexture(GL_TEXTURE_2D,paint_id);
    active_(0x84C1);GLuint spec=upload(mat.spec.data(),mat.sw,mat.sh);uniform1f_(uniform_(program_,"phong_boost"),std::min(.8f,std::max(.05f,mat.boost*.2f)));uniform1f_(uniform_(program_,"phong_exponent_factor"),std::max(1.f,mat.factor));
    glBegin(GL_TRIANGLES);for(auto index:part.indices){auto& vertex=m.vertices[index];float pos[3]={},normal[3]={},v[3]={},n[3]={};
     for(int i=0;i<3;++i){pos[i]=matrix.v[4*i+3];for(int j=0;j<3;++j){pos[i]+=matrix.v[4*i+j]*vertex.pos[j];normal[i]+=matrix.v[4*i+j]*vertex.normal[j];}}
     for(int i=0;i<3;++i)for(int j=0;j<3;++j){v[i]+=rot[3*i+j]*(pos[j]-center[j]);n[i]+=rot[3*i+j]*normal[j];}
     glNormal3fv(n);glTexCoord2fv(vertex.uv);if(perspective)glVertex4f(v[0]*focalx+panx*-v[2],v[1]*focaly+pany*-v[2],-v[2]*1.0002f-.2f,-v[2]);
     else glVertex4f(v[0]*scale/aspect+panx,v[1]*scale+pany,-v[2]*.95f/std::max(radius,1e-5f),1);
    }glEnd();active_(0x84C1);glDeleteTextures(1,&spec);if(!painted){active_(0x84C0);glDeleteTextures(1,&base);}
   }
  };
  draw(weapon,identity,true);
  if(stat_clock){auto parent=weapon.bones.find("c_weapon_stattrack");if(parent!=weapon.bones.end()){auto& clock=model("models/weapons/c_models/stattrack.mdl");auto child=clock.bones.find("c_weapon_stattrack");if(child!=clock.bones.end()){ModelMatrix s{{clock_scale,0,0,0,0,clock_scale,0,0,0,0,clock_scale,0}};auto transform=multiply_affine(multiply_affine(inverse_affine(parent->second),s),child->second);draw(clock,transform,false);}}}
  active_(0x84C0);glDeleteTextures(1,&paint_id);glFinish();std::vector<uint8_t> high(512*320*4);glReadPixels(0,0,512,320,GL_RGBA,GL_UNSIGNED_BYTE,high.data());
  std::vector<uint8_t> out(256*160*4);for(int y=0;y<160;++y)for(int x=0;x<256;++x){unsigned sum[4]={};for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){auto p=&high[((319-(y*2+dy))*512+x*2+dx)*4];for(int c=0;c<4;++c)sum[c]+=p[c];}auto p=&out[(y*256+x)*4];p[3]=uint8_t((sum[3]+2)/4);for(int c=0;c<3;++c)p[c]=sum[3]?uint8_t(std::min(255u,(sum[c]*255+sum[3]/2)/sum[3])):0;}
  return out;
 }
};
inline void save_thumbnail_png(const std::string& path,const std::vector<uint8_t>& pixels){
 FILE* f=std::fopen(path.c_str(),"wb");if(!f)throw std::runtime_error("Could not create thumbnail");
 png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);if(!png){std::fclose(f);throw std::runtime_error("PNG encoder unavailable");}
 png_infop info=png_create_info_struct(png);if(!info){png_destroy_write_struct(&png,nullptr);std::fclose(f);throw std::runtime_error("PNG encoder failed");}
 if(setjmp(png_jmpbuf(png))){png_destroy_write_struct(&png,&info);std::fclose(f);throw std::runtime_error("PNG write failed");}
 png_init_io(png,f);png_set_IHDR(png,info,256,160,8,PNG_COLOR_TYPE_RGBA,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);png_write_info(png,info);
 for(unsigned y=0;y<160;++y)png_write_row(png,const_cast<png_bytep>(pixels.data()+y*256*4));png_write_end(png,nullptr);png_destroy_write_struct(&png,&info);std::fclose(f);
}
