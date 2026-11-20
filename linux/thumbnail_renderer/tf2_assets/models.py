"""Resolve and extract Source model companions without changing TF2 files."""
from pathlib import Path, PurePosixPath
import json
import struct
from .vpk import VPK

class AssetError(Exception):
    pass

def load_catalog(path):
    with open(path,encoding='utf-8') as stream:
        data=json.load(stream)
    if not isinstance(data.get('weapons'),list): raise AssetError('Invalid weapon catalog')
    return data

def extract_model(tf_dir, model, destination):
    tf_dir=Path(tf_dir)
    model=PurePosixPath(model.replace('\\','/'))
    valid=(str(model).startswith('models/weapons/') or
           str(model).startswith('models/workshop/weapons/') or
           str(model)=='models/items/paintkit_tool.mdl')
    if model.is_absolute() or '..' in model.parts or not valid or model.suffix.lower()!='.mdl':
        raise AssetError('Expected a weapon model or the War Paint tool model')
    stem=str(model)[:-4]
    files=[str(model),stem+'.vvd',stem+'.dx90.vtx']
    for optional in [stem+'.phy',stem+'.dx80.vtx',stem+'.sw.vtx']:
        files.append(optional)
    archive=VPK(tf_dir/'tf2_misc_dir.vpk')
    missing=[name for name in files[:3] if name not in archive.entries and not (tf_dir/name).is_file()]
    if missing: raise AssetError('Required model files are missing: '+', '.join(missing))
    extracted=[]
    root=Path(destination).resolve()
    for name in files:
        target=(root/Path(name)).resolve()
        try:
            target.relative_to(root)
        except ValueError:
            raise AssetError('Invalid output path')
        source=tf_dir/Path(name)
        if source.is_file(): data=source.read_bytes()
        elif name in archive.entries: data=archive.read(name)
        else: continue
        if name.endswith('.mdl') and data[:4]!=b'IDST': raise AssetError('Invalid MDL signature')
        if name.endswith('.vvd') and data[:4]!=b'IDSV': raise AssetError('Invalid VVD signature')
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
        extracted.append(target)
    return extracted
