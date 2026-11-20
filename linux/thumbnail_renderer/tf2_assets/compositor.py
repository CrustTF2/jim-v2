"""Experimental CPU paint compositor for legacy TF2 graphs.

This produces a texture atlas, not a rendered weapon. Seeded values use the
installed game's random stream; shader and sampling differences can remain.
"""
from pathlib import Path
import math, struct
import numpy as np
from PIL import Image
from .models import AssetError
from .vpk import VPK
from .paintkits import read_definition
from .source_random_linux import SourceRandom
from .wear import wear_level

def srgb_to_linear(rgb):
    return np.where(rgb<=0.04045,rgb/12.92,((rgb+0.055)/1.055)**2.4)

def linear_to_srgb(rgb):
    rgb=np.maximum(rgb,0)
    return np.where(rgb<=0.0031308,rgb*12.92,1.055*np.power(rgb,1/2.4)-0.055)

def to_image(pixels):
    rgba=np.clip(pixels,0,1).copy()
    rgba[:,:,:3]=linear_to_srgb(rgba[:,:,:3])
    return Image.fromarray(np.uint8(np.clip(rgba,0,1)*255+0.5),'RGBA')

def decode_vtf(data,frame=0):
    if data[:4] != b'VTF\0': raise AssetError('Invalid VTF signature')
    major,minor,header=struct.unpack_from('<III',data,4)
    w,h=struct.unpack_from('<HH',data,16)
    frames=struct.unpack_from('<H',data,24)[0]
    fmt,mips=struct.unpack_from('<IB',data,52)
    depth=struct.unpack_from('<H',data,63)[0] if minor>=2 else 1
    if major!=7 or minor<3 or frames<1 or depth!=1 or fmt not in (12,13,14,15,20):
        raise AssetError(f'Unsupported VTF: version {major}.{minor}, format {fmt}, frames {frames}, depth {depth}')
    count=struct.unpack_from('<I',data,68)[0]
    offset=next((struct.unpack_from('<I',data,84+8*i)[0] for i in range(count) if struct.unpack_from('<I',data,80+8*i)[0]==0x30),None)
    if offset is None:raise AssetError('VTF is missing image data')
    block=8 if fmt in (13,20) else 16
    def size(x,y):return x*y*4 if fmt==12 else max(1,(x+3)//4)*max(1,(y+3)//4)*block
    for level in range(mips-1,0,-1):offset+=frames*size(max(1,w>>level),max(1,h>>level))
    if not 0<=frame<frames:raise AssetError('VTF frame out of range')
    offset+=frame*size(w,h)
    raw=data[offset:offset+size(w,h)]
    if len(raw)!=size(w,h):raise AssetError('Truncated VTF image')
    if fmt==12:return Image.frombytes('RGBA',(w,h),raw,'raw','BGRA')
    return Image.frombytes('RGBA',(w,h),raw,'bcn',({13:1,14:2,15:3,20:1}[fmt],))

class Compositor:
    def __init__(self,tf, size=512, seed=0, team='red',shared=None):
        if size<32 or size>2048:raise AssetError('Resolution must be between 32 and 2048')
        if team not in ('red','blue'):raise AssetError('Team must be red or blue')
        self.tf=Path(tf);self.size=size;self.rng=SourceRandom(self.tf,seed);self.team=team;self.stage_values={}
        shared=shared if shared is not None else {}
        if 'archives' not in shared:shared['archives']=[VPK(self.tf/'tf2_textures_dir.vpk'),VPK(self.tf/'tf2_misc_dir.vpk')]
        self.archives=shared['archives'];self.cache=shared.setdefault('textures',{});self.linear_cache=shared.setdefault('linear',{});self.warnings=[]
        coordinates=shared.setdefault('coordinates',{})
        if size not in coordinates:
            y,x=np.mgrid[0:size,0:size].astype(np.float32)
            coordinates[size]=((x+0.5)/size,(y+0.5)/size)
        self.u,self.v=coordinates[size]

    def texture(self,path,*,transparency=False):
        if not path or '..' in path or path.startswith(('/', '\\')):raise AssetError('Invalid texture path')
        path=path.replace('\\','/').casefold()
        if path.endswith(('.tga','.vtf')):path=path.rsplit('.',1)[0]
        cache_key=(path,'transparent') if transparency else path
        if cache_key in self.cache:return self.cache[cache_key]
        key='materials/'+path+'.vtf'
        loose=self.tf/key
        if loose.is_file():data=loose.read_bytes()
        else:
            archive=next((a for a in self.archives if key in a.entries),None)
            if not archive:
                # Some old paint graphs reference AO maps removed from modern TF2.
                if path.endswith('_ao'):
                    self.warnings.append('Missing ambient occlusion map; using white: '+path)
                    value=np.ones((1,1,4),dtype=np.float32);self.cache[cache_key]=value;return value
                raise AssetError('Texture not found: '+key)
            data=archive.read(key)
        image=decode_vtf(data)
        if transparency:
            # Sticker alpha is genuine opacity, so resize it premultiplied to
            # keep transparent edges clean.
            image.thumbnail((self.size,self.size),Image.Resampling.LANCZOS)
        elif image.width>self.size or image.height>self.size:
            # Most paint-graph textures use alpha for a mask or specular data,
            # not transparency. RGBA thumbnail() premultiplies by alpha and
            # can erase perfectly valid colour, making the paint look black.
            channels=image.split()
            for channel in channels:channel.thumbnail((self.size,self.size),Image.Resampling.LANCZOS)
            image=Image.merge('RGBA',channels)
        result=np.asarray(image,dtype=np.float32)/255.0
        if sum(v.nbytes for v in self.cache.values())+result.nbytes <= 128*1024*1024:self.cache[cache_key]=result
        return result

    def number(self,fields,name,default,rng,inverse=False):
        value=next((v for k,v in reversed(fields) if k==name and isinstance(v,str)),None)
        if value is None:numbers=[default]
        else:
            try: numbers=[float(x) for x in value.split()]
            except ValueError:raise AssetError('Unsupported variable in '+name+': '+value)
            if not numbers:numbers=[default]
        low=numbers[0];high=numbers[1] if len(numbers)>1 else low
        if inverse:
            low=1/low if low else 0.00001;high=1/high if high else 0.00001
        return rng.uniform(low,high)

    def prepare_random(self,kind,fields):
        if kind=='select':return
        rng=self.rng.stage();values={}
        if kind=='apply_sticker':
            choices=[]
            for k,v in fields:
                if k=='sticker' and isinstance(v,list):
                    info=dict(v);choices.append({'base':info.get('base',''),'weight':float(info.get('weight',1)),'spec':info.get('spec','')})
                elif k=='sticker_base':choices.append({'base':v,'weight':1.,'spec':''})
                elif k=='sticker_weight' and choices:choices[-1]['weight']=float(v)
                elif k=='sticker_spec' and choices:choices[-1]['spec']=v
            if not choices or any(not c['base'] or c['weight']<0 for c in choices):raise AssetError('Invalid sticker choices')
            total=sum(c['weight'] for c in choices)
            if total<=0:raise AssetError('Sticker weights must add up to a positive value')
            selected=rng.uniform(0,total);chosen=choices[-1]
            for choice in choices:
                selected-=choice['weight']
                if selected<0:chosen=choice;break
            values['sticker']=chosen
        if kind=='texture_lookup':
            for key in ('flip_u','flip_v'):
                raw=next((v for k,v in fields if k==key and isinstance(v,str)),'0')
                parts=raw.split()
                try:enabled=any(float(part)!=0 for part in parts)
                except ValueError:enabled=raw.lower() in ('true','yes')
                values[key]=rng.boolean() if enabled else False
            for key,default in [('translate_u',0),('translate_v',0),('rotation',0),('scale_uv',1)]:
                values[key]=self.number(fields,key,default,rng)
        for key,default in [('adjust_black',0),('adjust_offset',255),('adjust_gamma',1)]:
            values[key]=self.number(fields,key,default,rng,inverse=(key=='adjust_gamma'))
        self.stage_values[id(fields)]=values
        for child_kind,child_fields in fields:
            if isinstance(child_fields,list) and child_kind!='sticker':self.prepare_random(child_kind,child_fields)

    def apply_sticker(self,base,fields,settings):
        sticker=settings['sticker'];path=sticker['base']
        if path.casefold()=='patterns/stickers/blank':return base
        values=dict((k,v) for k,v in fields if isinstance(v,str))
        def point(name):
            try:
                result=[float(x) for x in values[name].split()]
                if len(result)!=2 or not all(math.isfinite(x) for x in result):raise ValueError()
                return np.asarray(result,dtype='f4')
            except (ValueError,KeyError):raise AssetError('Invalid sticker coordinate: '+name)
        tl=point('dest_tl');tr=point('dest_tr');bl=point('dest_bl')
        u_axis=tr-tl;v_axis=bl-tl;det=float(u_axis[0]*v_axis[1]-u_axis[1]*v_axis[0])
        if abs(det)<1e-10:raise AssetError('Sticker destination has zero area')
        corners=np.asarray([tl,tr,bl,tr+bl-tl]);low=np.floor(corners.min(axis=0)*self.size-.5).astype(int);high=np.ceil(corners.max(axis=0)*self.size-.5).astype(int)+1
        x0=max(0,int(low[0]));x1=min(self.size,int(high[0]));y0=max(0,int(low[1]));y1=min(self.size,int(high[1]))
        if x0>=x1 or y0>=y1:return base
        dx=self.u[y0:y1,x0:x1]-tl[0];dy=self.v[y0:y1,x0:x1]-tl[1]
        u=(dx*v_axis[1]-dy*v_axis[0])/det;v=(dy*u_axis[0]-dx*u_axis[1])/det
        inside=(u>=0)&(u<=1)&(v>=0)&(v<=1)
        if not inside.any():return base
        def sample(path,linear):
            source=self.texture(path,transparency=linear);h,w=source.shape[:2]
            if linear:
                source=source.copy();source[:,:,:3]=srgb_to_linear(source[:,:,:3])
            px=np.clip(u*w-.5,0,w-1);py=np.clip(v*h-.5,0,h-1)
            ix=np.floor(px).astype(np.int32);iy=np.floor(py).astype(np.int32)
            xnext=np.minimum(ix+1,w-1);ynext=np.minimum(iy+1,h-1)
            fx=(px-ix)[:,:,None];fy=(py-iy)[:,:,None]
            return (source[iy,ix]*(1-fx)+source[iy,xnext]*fx)*(1-fy)+(source[ynext,ix]*(1-fx)+source[ynext,xnext]*fx)*fy
        overlay=sample(path,True);alpha=overlay[:,:,3:4]*inside[:,:,None]
        # Source's compositor stores the specular intensity in atlas alpha.
        stem=path.rsplit('.',1)[0] if path.casefold().endswith(('.tga','.vtf')) else path
        spec=sticker.get('spec') or stem+'_spec'
        spec=spec.replace('\\','/').casefold()
        if spec.endswith(('.tga','.vtf')):spec=spec.rsplit('.',1)[0]
        key='materials/'+spec+'.vtf'
        if (self.tf/key).is_file() or any(key in archive.entries for archive in self.archives):
            specular=sample(spec,False)[:,:,0:1]
        else:specular=1.
        target=base[y0:y1,x0:x1]
        target[:,:,:3]=target[:,:,:3]*(1-alpha)+overlay[:,:,:3]*alpha
        target[:,:,3:4]=target[:,:,3:4]*(1-alpha)+specular*alpha
        return base

    def sample(self,path,fields=(),raw=False):
        source=self.texture(path);height,width=source.shape[:2]
        settings=self.stage_values.get(id(fields),{})
        scale=settings.get('scale_uv',1);angle=math.radians(settings.get('rotation',0))
        du=settings.get('translate_u',0);dv=settings.get('translate_v',0)
        u=(self.u+du)*scale*(-1 if settings.get('flip_u') else 1)
        v=(self.v+dv)*scale*(-1 if settings.get('flip_v') else 1)
        x=(u*math.cos(angle)-v*math.sin(angle))%1.0
        y=(u*math.sin(angle)+v*math.cos(angle))%1.0
        if raw:
            # Group IDs must not be interpolated or converted from sRGB.
            sampled=source[(y*height).astype(np.int32)%height,(x*width).astype(np.int32)%width].copy()
        else:
            # Match the GPU's wrapped bilinear texture sampling.
            px=x*width-0.5;py=y*height-0.5
            ix=np.floor(px).astype(np.int32);iy=np.floor(py).astype(np.int32)
            fx=(px-ix.astype(np.float32))[:,:,None];fy=(py-iy.astype(np.float32))[:,:,None]
            linear=self.linear_cache.get(path)
            if linear is None:
                linear=source.copy();linear[:,:,:3]=srgb_to_linear(linear[:,:,:3])
                # Limit reusable decoded textures to avoid unbounded memory growth.
                if sum(v.nbytes for v in self.linear_cache.values())+linear.nbytes <= 128*1024*1024:self.linear_cache[path]=linear
            top=linear[iy%height,ix%width]*(1-fx)+linear[iy%height,(ix+1)%width]*fx
            bottom=linear[(iy+1)%height,ix%width]*(1-fx)+linear[(iy+1)%height,(ix+1)%width]*fx
            sampled=top*(1-fy)+bottom*fy
        return self.adjust(sampled,settings)

    def adjust(self,sampled,settings):
        black=settings.get('adjust_black',0)/255.0
        offset=settings.get('adjust_offset',255)/255.0
        gamma=settings.get('adjust_gamma',1)
        if black or offset!=1 or gamma!=1:
            # Offset is the span from the black point to the white point.
            white=max(black+1e-5,black+offset)
            adjusted=sampled.copy();adjusted[:,:,:3]=linear_to_srgb(adjusted[:,:,:3])
            adjusted=np.power(np.clip((adjusted-black)/(white-black),0,1),gamma)
            adjusted[:,:,:3]=srgb_to_linear(adjusted[:,:,:3]);sampled=adjusted
        return sampled

    def evaluate(self,kind,fields):
        if kind=='texture_lookup':
            paths=dict((k,v) for k,v in fields if isinstance(v,str))
            return self.sample(paths.get('texture_'+self.team,paths.get('texture')),fields)
        if kind=='select':
            pairs=dict((k,v) for k,v in fields if isinstance(v,str))
            groups=self.sample(pairs.get('groups'),raw=True)[:,:,0]
            values=[float(v) for k,v in fields if k=='select']
            mask=np.isin(np.floor(groups*255/16+0.5),[math.floor(v/16+0.5) for v in values if v])
            return np.repeat(mask[:,:,None],4,axis=2).astype(np.float32)
        nodes=[(k,v) for k,v in fields if isinstance(v,list) and k!='sticker']
        if kind=='apply_sticker':
            if len(nodes)!=1:raise AssetError('Sticker stage requires one underlying image')
            settings=self.stage_values[id(fields)]
            return self.adjust(self.apply_sticker(self.evaluate(*nodes[0]),fields,settings),settings)
        if kind=='combine_lerp':
            if len(nodes)!=3:raise AssetError('Lerp requires exactly three operands')
            a,b,m=(self.evaluate(k,v) for k,v in nodes)
            return self.adjust(a*(1-m[:,:,0:1])+b*m[:,:,0:1],self.stage_values[id(fields)])
        if kind in ('combine_multiply','combine_add'):
            if not nodes:raise AssetError('Empty combination stage')
            out=self.evaluate(*nodes[0])
            for node in nodes[1:]:
                value=self.evaluate(*node)
                out=out*value if kind=='combine_multiply' else np.minimum(out+value,1)
            return self.adjust(out,self.stage_values[id(fields)])
        raise AssetError('Unsupported paint operation: '+kind)

    def render(self,definition,wear):
        if not 0<=wear<=1:raise AssetError('Wear must be in the range 0 to 1')
        graph=read_definition(self.tf,definition)
        if graph is None:raise AssetError('No legacy paint graph for '+definition)
        level=wear_level(wear)
        node=next((v for k,v in graph if k=='wear_level_'+str(level)),None)
        if node is None:raise AssetError('Wear level not available in paint graph')
        stages=[(k,v) for k,v in node if isinstance(v,list)]
        if len(stages)!=1:raise AssetError('Expected one root paint operation')
        self.prepare_random(*stages[0]);pixels=self.evaluate(*stages[0])
        return to_image(pixels)
