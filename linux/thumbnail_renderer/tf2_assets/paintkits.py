"""Read the legacy paint-compositing graphs, preserving duplicate KV keys."""
from pathlib import Path
import re
from .vpk import VPK
from .models import AssetError

TOKEN = re.compile(r'//[^\r\n]*|"((?:\\.|[^"\\])*)"|([{}])')

def read_definition(tf_dir, name):
    if not re.fullmatch(r'[A-Za-z0-9_]+',name): raise AssetError('Invalid paint definition name')
    archive=VPK(Path(tf_dir)/'tf2_misc_dir.vpk')
    key='scripts/items/paintkits_master.txt'
    if key not in archive.entries: raise AssetError('TF2 paintkits_master.txt is missing')
    source=archive.read(key).decode('utf-8-sig')
    match=re.search(r'"'+re.escape(name)+r'"\s*\{',source)
    if not match: return None
    tokens=TOKEN.finditer(source,match.end()-1)
    first=next(tokens,None)
    if not first or first.group(2)!='{': raise AssetError('Invalid paint definition')
    def parse():
        pairs=[]
        while True:
            token=next(tokens,None)
            while token and token.group(0).startswith('//'):token=next(tokens,None)
            if token is None:raise AssetError('Unterminated paint definition')
            if token.group(2)=='}':return pairs
            if token.group(1) is None:raise AssetError('Expected paint field')
            key=token.group(1)
            value=next(tokens,None)
            while value and value.group(0).startswith('//'):value=next(tokens,None)
            if value is None:raise AssetError('Missing paint field value')
            if value.group(2)=='{': value=parse()
            elif value.group(1) is not None:value=value.group(1)
            else:raise AssetError('Invalid paint field value')
            pairs.append([key,value])
    return parse()

def texture_paths(graph):
    result=set()
    def walk(pairs):
        for key,value in pairs:
            if isinstance(value,list):walk(value)
            elif key in ('texture','texture_red','texture_blue','groups') and value:result.add(value)
    walk(graph)
    return sorted(result)
