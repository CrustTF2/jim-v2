"""Create a private Source search path for an experimental painted preview."""
from pathlib import Path
import re,struct
from .models import AssetError,extract_model
from .vpk import VPK
from .compositor import Compositor

MODEL='models/weapons/c_models/c_rocketlauncher/c_rocketlauncher.mdl'
MATERIAL='materials/models/weapons/c_models/c_rocketlauncher/c_rocketlauncher.vmt'

def encode_vtf(image):
    image=image.convert('RGBA')
    w,h=image.size
    if w<1 or h<1 or w>65535 or h>65535: raise AssetError('Invalid VTF image dimensions')
    header=bytearray(80)
    struct.pack_into('<4sIIIHHIHH',header,0,b'VTF\0',7,2,80,w,h,0,1,0)
    struct.pack_into('<f',header,48,1.0)
    struct.pack_into('<I',header,52,12)  # IMAGE_FORMAT_BGRA8888
    struct.pack_into('<B',header,56,1)   # one high-resolution mip
    struct.pack_into('<i',header,57,-1)  # no low-resolution thumbnail
    struct.pack_into('<BBH',header,61,0,0,1)
    return bytes(header)+image.tobytes('raw','BGRA')

def prepare(tf,stage,definition,wear,seed,team,size=512):
    tf=Path(tf).resolve();stage=Path(stage).resolve()
    if not definition.startswith('concealedkiller_rocketlauncher_'):
        raise AssetError('Painted model preview currently supports the original Concealed Killer rocket launcher skins only')
    compositor=Compositor(tf,size,seed,team)
    atlas=compositor.render(definition,wear)
    models=extract_model(tf,MODEL,stage/'assets')
    archive=VPK(tf/'tf2_misc_dir.vpk')
    material=archive.read(MATERIAL).decode('utf-8-sig')
    material,count=re.subn(r'("\$basetexture"\s*)"[^"]+"',r'\1"tf2_renderer/paint_atlas"',material,count=1,flags=re.I)
    if count!=1:raise AssetError('Cannot locate rocket launcher base texture in material')
    material_path=stage/MATERIAL
    material_path.parent.mkdir(parents=True,exist_ok=True)
    material_path.write_text(material,encoding='utf-8')
    atlas_path=stage/'materials/tf2_renderer/paint_atlas.vtf'
    atlas_path.parent.mkdir(parents=True,exist_ok=True)
    atlas_path.write_bytes(encode_vtf(atlas))
    # Keep all search paths in a private directory. Original TF2 archives remain
    # read-only and are mounted after our single material override.
    original=(tf/'gameinfo.txt').read_text(encoding='utf-8-sig')
    replacement='SearchPaths\n\t\t{\n\t\t\tgame+mod+mod_write+default_write_path "|gameinfo_path|."\n'
    gameinfo,count=re.subn(r'SearchPaths\s*\{',lambda _:replacement,original,count=1,flags=re.I)
    if count!=1:raise AssetError('Cannot locate TF2 SearchPaths in gameinfo.txt')
    (stage/'gameinfo.txt').write_text(gameinfo,encoding='utf-8')
    return models[0],atlas_path,compositor.warnings
