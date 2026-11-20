"""Read selected TF2 VPK UI textures; export PNGs without changing the game."""
from pathlib import Path
import struct
import zlib


class VPK:
    def __init__(self,path):
        self.path=Path(path)
        self.data=self.path.read_bytes()
        signature,version,tree_size=struct.unpack_from('<III',self.data)
        if signature!=0x55AA1234 or version not in (1,2): raise ValueError('Unsupported VPK')
        header_size=12 if version==1 else 28
        self.embedded=header_size+tree_size
        pos=header_size
        self.entries={}
        def string():
            nonlocal pos
            end=self.data.index(b'\0',pos)
            value=self.data[pos:end].decode('utf8')
            pos=end+1
            return value
        while True:
            extension=string()
            if not extension: break
            while True:
                folder=string()
                if not folder: break
                while True:
                    name=string()
                    if not name: break
                    crc,preload,index,offset,size,terminator=struct.unpack_from('<IHHIIH',self.data,pos)
                    if terminator!=65535: raise ValueError('Invalid directory entry')
                    pos+=18
                    prefix=self.data[pos:pos+preload];pos+=preload
                    key=(folder+'/' if folder!=' ' else '')+name+'.'+extension
                    self.entries[key]=(crc,index,offset,size,prefix)
        if pos!=self.embedded: raise ValueError('Invalid tree length')

    def read(self,key):
        crc,index,offset,size,prefix=self.entries[key]
        if index==0x7fff: data=self.data[self.embedded+offset:self.embedded+offset+size]
        else:
            archive=self.path.with_name(self.path.name.replace('_dir.vpk',f'_{index:03}.vpk'))
            with archive.open('rb') as stream:
                stream.seek(offset);data=stream.read(size)
        result=prefix+data
        if zlib.crc32(result)!=crc: raise ValueError('CRC mismatch: '+key)
        return result


def vtf_to_png(data,destination):
    from PIL import Image
    if data[:4]!=b'VTF\0': raise ValueError('Invalid VTF')
    major,minor,header_size=struct.unpack_from('<III',data,4)
    width,height=struct.unpack_from('<HH',data,16)
    frames=struct.unpack_from('<H',data,24)[0]
    fmt,mips=struct.unpack_from('<IB',data,52)
    depth=struct.unpack_from('<H',data,63)[0] if minor>=2 else 1
    if major!=7 or minor<3 or frames!=1 or depth!=1 or fmt not in (13,15):
        raise ValueError('Expected single-frame DXT1/DXT5 VTF')
    count=struct.unpack_from('<I',data,68)[0]
    offset=None
    for i in range(count):
        tag,position=struct.unpack_from('<II',data,80+8*i)
        if tag==0x30: offset=position
    if offset is None: raise ValueError('Missing high resolution image resource')
    block_bytes=8 if fmt==13 else 16
    def size(w,h): return max(1,(w+3)//4)*max(1,(h+3)//4)*block_bytes
    # VTF stores mipmaps from smallest to largest.
    for level in range(mips-1,0,-1): offset+=size(max(1,width>>level),max(1,height>>level))
    raw=data[offset:offset+size(width,height)]
    image=Image.frombytes('RGBA',(width,height),raw,'bcn',(1 if fmt==13 else 3,))
    destination=Path(destination);destination.parent.mkdir(parents=True,exist_ok=True)
    image.save(destination)
    return image


if __name__=='__main__':
    import argparse,json,hashlib
    parser=argparse.ArgumentParser()
    parser.add_argument('vpk',type=Path)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    vpk=VPK(args.vpk)
    records=[]
    for name in ['statclock','strange','unusual']:
        key='materials/vgui/viewmode_'+name+'.vtf'
        data=vpk.read(key)
        image=vtf_to_png(data,args.output/(name+'.png'))
        assert image.getextrema()[3][0]==0 and image.getextrema()[3][1]>0
        records.append({'file':name+'.png','source':key,'source_sha256':hashlib.sha256(data).hexdigest()})
        print(name,image.size,'RGBA alpha range',image.getextrema()[3])
    (args.output/'sources.json').write_text(json.dumps(records,indent=2))
