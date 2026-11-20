"""Persistent off-screen Source weapon renderer; no HLMV process required."""
from pathlib import Path
import math,re,time
from collections import OrderedDict
import numpy as np
from PIL import Image
import moderngl
from valvemdl import Mdl
from .models import extract_model,AssetError
from .vpk import VPK
from .compositor import Compositor,decode_vtf

VERTEX='''#version 330
in vec3 in_pos; in vec3 in_normal; in vec2 in_uv;
uniform mat3 rotation; uniform vec3 center; uniform float scale; uniform float aspect; uniform float depth_scale; uniform vec2 pan;
uniform int perspective; uniform vec2 focal; uniform mat4 model_transform;
out vec3 normal; out vec2 uv;
void main() { vec3 pos=(model_transform*vec4(in_pos,1.0)).xyz;
vec3 p=rotation*(pos-center); normal=normalize(rotation*mat3(model_transform)*in_normal); uv=in_uv;
if (perspective != 0) {
    gl_Position=vec4(p.x*focal.x+pan.x*(-p.z),p.y*focal.y+pan.y*(-p.z),-p.z*1.0002-0.2,-p.z);
} else {
    gl_Position=vec4(p.x*scale/aspect+pan.x,p.y*scale+pan.y,-p.z*depth_scale,1.0);
} }
'''
FRAGMENT='''#version 330
uniform sampler2D diffuse_texture; uniform sampler2D phong_texture;
uniform float phong_boost; uniform float phong_exponent_factor;
in vec3 normal; in vec2 uv; out vec4 color;
void main() { vec3 n=normalize(normal); vec3 light=normalize(vec3(-0.3,0.65,0.7));
vec4 surface=texture(diffuse_texture,uv);
float diffuse=0.84+0.16*abs(dot(n,light))+0.04*max(n.y,0.0);
vec3 halfdir=normalize(light+vec3(0.0,0.0,1.0));
float exponent=clamp(texture(phong_texture,uv).r*phong_exponent_factor,5.0,100.0);
float phong=pow(abs(dot(n,halfdir)),exponent)*phong_boost*mix(0.25,1.0,surface.a);
color=vec4(clamp(pow(surface.rgb,vec3(0.92))*diffuse+vec3(phong*1.10),0.0,1.0),1.0); }
'''

class Renderer:
    def __init__(self,tf,cache_dir):
        self.tf=Path(tf).resolve();self.cache_dir=Path(cache_dir)
        try:
            self.context=moderngl.create_standalone_context(require=330, backend='egl')
        except Exception:
            self.context=moderngl.create_standalone_context(require=330)
        self.program=self.context.program(vertex_shader=VERTEX,fragment_shader=FRAGMENT)
        self.models={};self.images={};self.materials={};self.gpu_textures={};self.framebuffers=OrderedDict();self.textures_archive=None;self.misc_archive=None;self.modern=None;self.paint_shared={};self.catalog=None

    def archives(self):
        if self.textures_archive is None:self.textures_archive=VPK(self.tf/'tf2_textures_dir.vpk')
        if self.misc_archive is None:self.misc_archive=VPK(self.tf/'tf2_misc_dir.vpk')

    def load_mesh(self,path):
        if path in self.models:return self.models[path]
        files=extract_model(self.tf,path,self.cache_dir)
        mdl=Mdl(str(files[0]));vtx=mdl.get_vtx('.dx90.vtx')
        if vtx is None:raise AssetError('Missing DirectX 9 mesh')
        vertices=mdl.vvd.mesh_vertices(0)
        if not vertices:raise AssetError('Model has no vertices')
        packed=np.asarray([[v.m_vecPosition.x,v.m_vecPosition.y,v.m_vecPosition.z,
            v.m_vecNormal.x,v.m_vecNormal.y,v.m_vecNormal.z,v.m_vecTexCoord.x,v.m_vecTexCoord.y] for v in vertices],dtype='f4')
        self.context.enable(moderngl.DEPTH_TEST)
        buffer=self.context.buffer(packed.tobytes())
        parts=[];used=[]
        for bp_id,bp in enumerate(mdl.bodyparts):
            if not bp.models or bp_id>=len(vtx.bodyparts):continue
            model=bp.models[0];vmodel=vtx.bodyparts[bp_id].models[0]
            if not vmodel.lods:continue
            for mesh_id,mesh in enumerate(model.meshes):
                if mesh_id>=len(vmodel.lods[0].meshes):continue
                for group in vmodel.lods[0].meshes[mesh_id].stripgroups:
                    mapping=np.asarray([v.origMeshVertID for v in group.vertices],dtype=np.int64)+(model.vertexindex//48)+mesh.vertexoffset
                    indices=[]
                    for strip in group.strips:
                        source=group.indices[strip.indexOffset:strip.indexOffset+strip.numIndices]
                        if strip.flags.STRIP_IS_TRILIST:
                            indices.extend(source)
                        elif strip.flags.STRIP_IS_TRISTRIP:
                            for i in range(len(source)-2):
                                a,b,c=source[i:i+3]
                                if a!=b and b!=c and a!=c:indices.extend((a,b,c) if i%2==0 else (b,a,c))
                        else:raise AssetError('Unsupported mesh strip')
                    if not indices:continue
                    resolved=mapping[np.asarray(indices,dtype=np.int64)]
                    if resolved.min()<0 or resolved.max()>=len(packed):raise AssetError('Invalid model vertex index')
                    index_buffer=self.context.buffer(resolved.astype('u4').tobytes())
                    vao=self.context.vertex_array(self.program,[(buffer,'3f 3f 2f','in_pos','in_normal','in_uv')],index_buffer=index_buffer,index_element_size=4)
                    texture_index=mesh.material
                    if mdl.skins and mesh.material < len(mdl.skins[0]):texture_index=mdl.skins[0][mesh.material]
                    name=mdl.textures[texture_index].name if texture_index<len(mdl.textures) else ''
                    parts.append((vao,index_buffer,name));used.append(resolved)
        if not parts:raise AssetError('No renderable triangles in model')
        visible=packed[np.unique(np.concatenate(used)),:3]
        center=(visible.min(axis=0)+visible.max(axis=0))/2
        camera=None
        attachment=next((a for a in mdl.attachments if a.name.casefold()=='icon_camera'),None)
        if attachment and 0<=attachment.localbone<len(mdl.bones):
            inverse_bind=np.eye(4,dtype='f4');inverse_bind[:3,:]=np.asarray(mdl.bones[attachment.localbone].poseToBone,dtype='f4')
            local=np.eye(4,dtype='f4');local[:3,:]=np.asarray(attachment.local,dtype='f4')
            try:
                camera=np.linalg.inv(inverse_bind)@local
            except np.linalg.LinAlgError:pass
        result={'buffer':buffer,'parts':parts,'center':center,'icon_camera':camera,
                'points':visible,'extent':np.ptp(visible,axis=0),
                'radius':float(np.max(np.linalg.norm(visible-center,axis=1))),
                'textures':mdl.cdtextures,
                'bones':{b.name.casefold():np.vstack((np.asarray(b.poseToBone,dtype='f4'),[0,0,0,1])) for b in mdl.bones}}
        self.models[path]=result
        return result

    def material_info(self,name,dirs,frame=0):
        material_key=(name,tuple(dirs),frame)
        if material_key in self.materials:return self.materials[material_key]
        self.archives()
        candidates=[name.replace('\\','/')]+[(d.replace('\\','/')+name).strip('/') for d in dirs]
        for candidate in candidates:
            key='materials/'+(candidate[:-4] if candidate.endswith('.vmt') else candidate)+'.vmt'
            if key not in self.misc_archive.entries:continue
            content=self.misc_archive.read(key).decode('utf-8',errors='replace')
            if re.match(r'\s*"?Refract"?\s*\{',content,re.I):
                # This shader describes a transparent refractive lens, not a
                # paintable surface; omit it until refraction is implemented.
                material=(None,None,0.0,0.0);self.materials[material_key]=material
                return material
            m=re.search(r'"?\$basetexture"?\s+"([^\"]+)"',content,re.I)
            if not m:continue
            name=m.group(1).replace('\\','/')
            if name.lower().endswith('.vtf'):name=name[:-4]
            texture='materials/'+name+'.vtf'
            if texture in self.textures_archive.entries:
                base=decode_vtf(self.textures_archive.read(texture),frame)
                spec=re.search(r'"?\$phongexponenttexture"?\s+"([^\"]+)"',content,re.I)
                spec_name=spec.group(1).replace('\\','/') if spec else ''
                if spec_name.lower().endswith('.vtf'):spec_name=spec_name[:-4]
                spec_key='materials/'+spec_name+'.vtf' if spec else ''
                exponent=decode_vtf(self.textures_archive.read(spec_key)) if spec_key in self.textures_archive.entries else Image.new('RGBA',(1,1),(96,96,96,255))
                def setting(key,default):
                    match=re.search(r'"?\$'+key+r'"?\s+"?([0-9.]+)',content,re.I)
                    return float(match.group(1)) if match else default
                material=(base,exponent,setting('phongboost',1.0),setting('phongexponentfactor',60.0))
                self.materials[material_key]=material
                return material
        raise AssetError('Could not resolve base material: '+name)

    def painted_texture(self,definition,wear,seed,team,paint_id,model_path,texture_size=512):
        key=(definition,float(wear),int(seed),team,paint_id,model_path,texture_size)
        if key not in self.images:
            compositor=Compositor(self.tf,size=texture_size,seed=seed,team=team,shared=self.paint_shared.setdefault(texture_size,{}))
            if paint_id:
                from .modern_paints import ModernPaints
                from .models import load_catalog
                if self.modern is None:self.modern=ModernPaints(self.tf)
                if self.catalog is None:self.catalog=load_catalog(Path(__file__).resolve().parent.parent/'catalog.json')
                image=self.modern.render(compositor,paint_id,model_path,self.catalog,wear)
            else:image=compositor.render(definition,wear)
            self.images[key]=(image,compositor.warnings)
        return self.images[key]

    def render(self,model_path,definition=None,wear=.1,seed=0,team='red',width=512,height=512,yaw=0,pitch=0,paint_id=None,zoom=1.0,pan_x=0.0,pan_y=0.0,texture_size=512,thumbnail=False,stat_clock=False,stat_clock_scale=1.0,stat_clock_count=0):
        if not 32<=width<=2048 or not 32<=height<=2048:raise AssetError('Image size must be between 32 and 2048')
        if texture_size not in (512,1024):raise AssetError('Texture resolution must be 512 or 1024')
        t=time.perf_counter();model=self.load_mesh(model_path)
        paint,warnings=self.painted_texture(definition,wear,seed,team,paint_id,model_path,texture_size) if definition or paint_id else (None,[])
        warnings=list(warnings)
        # Source's weapon models conventionally run along Z; map Z to screen X,
        # Y to screen Y, and X to depth. The controls rotate around that view.
        y=math.radians(yaw);p=math.radians(pitch)
        extent=model['extent']
        # Some melee models (notably knives) use Y for their long axis.
        # Turn those sideways for a thumbnail while keeping ordinary guns
        # and the square War Paint tool in their original orientation.
        if extent[1] > extent[2]*1.4 and extent[1] > extent[0]*1.4:
            base=np.array([[0,-1,0],[0,0,1],[-1,0,0]],dtype='f4')
        else:
            base=np.array([[0,0,-1],[0,1,0],[1,0,0]],dtype='f4')
        ry=np.array([[math.cos(y),0,math.sin(y)],[0,1,0],[-math.sin(y),0,math.cos(y)]],dtype='f4')
        rx=np.array([[1,0,0],[0,math.cos(p),-math.sin(p)],[0,math.sin(p),math.cos(p)]],dtype='f4')
        rotation=rx@ry@base
        aspect=width/height
        # Frame once using the default view, rather than refitting on every turn.
        default_bounds=np.abs(base)@(extent/2)
        scale=.88/max(float(default_bounds[1]),float(default_bounds[0])/aspect,1e-5)*zoom
        center=model['center']
        perspective=False
        focal=1.0/math.tan(math.radians(54.0)/2.0)
        focal_x=focal;focal_y=focal*aspect
        if thumbnail:
            camera=model['icon_camera']
            if camera is not None:
                axes=camera[:3,:3]
                rotation=np.stack((-axes[:,1],axes[:,2],-axes[:,0])).astype('f4')
                center=camera[:3,3]
                perspective=True
                # Preserve the camera's perspective while fitting its projected
                # bounds into the icon, so small viewport differences don't crop it.
                camera_points=(model['points']-center)@rotation.T
                visible_points=camera_points[camera_points[:,2]<-0.1]
                if len(visible_points):
                    coords=visible_points[:,:2]/(-visible_points[:,2,None])
                    coords=coords*np.array([focal_x,focal_y],dtype='f4')
                    lo=coords.min(axis=0);hi=coords.max(axis=0)
                    half=(hi-lo)/2;mid=(hi+lo)/2
                    fit=.88/max(float(half[0]),float(half[1]),1e-5)
                    focal_x*=fit;focal_y*=fit
                    pan_x=-float(mid[0])*fit;pan_y=-float(mid[1])*fit
            else:
                warnings.append('No inventory camera in this model; using a generic view.')
                # Fallback for models that have no game-authored inventory camera.
                projected=model['points']@rotation.T
                midpoint=(projected.min(axis=0)+projected.max(axis=0))/2
                center=rotation.T@midpoint
                bounds=(projected.max(axis=0)-projected.min(axis=0))/2
                scale=.88/max(float(bounds[1]),float(bounds[0])/aspect,1e-5)
                pan_x=pan_y=0
        self.program['rotation'].write(rotation.T.tobytes())
        self.program['center'].value=tuple(float(x) for x in center)
        self.program['scale'].value=scale;self.program['aspect'].value=aspect
        self.program['depth_scale'].value=.95/max(model['radius'],1e-5)
        self.program['pan'].value=(float(pan_x),float(pan_y))
        # TF2's ItemModelPanel resource specifies a 54-degree horizontal field of view.
        self.program['perspective'].value=int(perspective)
        self.program['focal'].value=(focal_x,focal_y)
        key=(width,height)
        if key not in self.framebuffers:
            resolved=self.context.simple_framebuffer(key,components=4)
            try:
                draw=self.context.simple_framebuffer(key,components=4,samples=4)
            except Exception:
                draw=resolved
            self.framebuffers[key]=(draw,resolved)
            while len(self.framebuffers)>3:
                _,(old_draw,old_resolved)=self.framebuffers.popitem(last=False)
                if old_draw is not old_resolved:old_draw.release()
                old_resolved.release()
        self.framebuffers.move_to_end(key)
        framebuffer,resolved=self.framebuffers[key]
        framebuffer.use();framebuffer.clear(0,0,0,0,depth=1)
        self.context.enable(moderngl.DEPTH_TEST);self.context.disable(moderngl.CULL_FACE)
        draw_models=[(model,np.eye(4,dtype='f4'),paint)]
        if stat_clock:
            attachment=self.load_mesh('models/weapons/c_models/stattrack.mdl')
            parent_bone=model['bones'].get('c_weapon_stattrack')
            child_bone=attachment['bones'].get('c_weapon_stattrack')
            if parent_bone is not None and child_bone is not None:
                matrix=np.linalg.inv(parent_bone)@np.diag([stat_clock_scale,stat_clock_scale,stat_clock_scale,1])@child_bone
                draw_models.append((attachment,matrix.astype('f4'),None))
            else:warnings.append('This model has no Stat Clock attachment bone.')
        for draw_model,matrix,draw_paint in draw_models:
          self.program['model_transform'].write(matrix.T.astype('f4').tobytes())
          for vao,index_buffer,name in draw_model['parts']:
            dial=re.search(r'stattrack_dial(\d{3})$',name) if draw_model is not model else None
            frame=(int(stat_clock_count)//(10**int(dial.group(1))))%10 if dial else 0
            base,exponent,boost,factor=self.material_info(name,draw_model['textures'],frame)
            if base is None:
                warnings.append('Refractive lens omitted in thumbnail.')
                continue
            if draw_paint is not None:
                image=draw_paint;texture_key=('paint',definition,float(wear),int(seed),team,paint_id,model_path,texture_size)
            else:
                image=base;texture_key=('base',name,tuple(draw_model['textures']),frame)
            if texture_key not in self.gpu_textures:
                image=image.convert('RGBA')
                texture=self.context.texture(image.size,4,image.tobytes())
                texture.filter=(moderngl.LINEAR,moderngl.LINEAR);self.gpu_textures[texture_key]=texture
            self.gpu_textures[texture_key].use(location=0)
            spec_key=('phong',name,tuple(draw_model['textures']))
            if spec_key not in self.gpu_textures:
                exponent=exponent.convert('RGBA')
                spec_texture=self.context.texture(exponent.size,4,exponent.tobytes())
                spec_texture.filter=(moderngl.LINEAR,moderngl.LINEAR);self.gpu_textures[spec_key]=spec_texture
            self.gpu_textures[spec_key].use(location=1)
            self.program['diffuse_texture'].value=0;self.program['phong_texture'].value=1
            self.program['phong_boost'].value=min(max(boost*.20,0.05),.8)
            self.program['phong_exponent_factor'].value=max(1.,factor)
            vao.render(moderngl.TRIANGLES)
        if framebuffer is not resolved:self.context.copy_framebuffer(resolved,framebuffer)
        result=Image.frombytes('RGBA',(width,height),resolved.read(components=4,alignment=1)).transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        if framebuffer is not resolved:
            # MSAA resolves transparent edges against black; PNG uses straight alpha.
            rgba=np.asarray(result).copy();alpha=rgba[:,:,3].astype(np.float32)
            partial=(alpha>0)&(alpha<255)
            rgba[partial,:3]=np.clip(np.rint(rgba[partial,:3].astype(np.float32)*255/alpha[partial,None]),0,255).astype(np.uint8)
            result=Image.fromarray(rgba,'RGBA')
        return result,warnings,time.perf_counter()-t

    def close(self):
        for model in self.models.values():
            for vao,index_buffer,name in model['parts']:vao.release();index_buffer.release()
            model['buffer'].release()
        for texture in self.gpu_textures.values():texture.release()
        for draw,resolved in self.framebuffers.values():
            if draw is not resolved:draw.release()
            resolved.release()
        self.program.release();self.context.release()
