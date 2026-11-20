"""Convert a downloaded Steam Cloud decal to a local badge without opening UI."""
from pathlib import Path
import io,os,re,struct,sys
ROOT=Path(__file__).resolve().parent
sys.path[:0]=[str(ROOT),str(ROOT/'vendor')]
from PIL import Image
from tf2_assets.compositor import decode_vtf


def decode(data):
    Image.MAX_IMAGE_PIXELS=16_000_000
    if not data.startswith(b'VTF\0'):
        with Image.open(io.BytesIO(data)) as image:
            image.load()
            return image.convert('RGBA')
    major,minor,header=struct.unpack_from('<III',data,4)
    width,height=struct.unpack_from('<HH',data,16)
    frames=struct.unpack_from('<H',data,24)[0]
    fmt,mips=struct.unpack_from('<IB',data,52)
    if width<1 or height<1 or width*height>16_000_000 or frames<1 or major!=7 or minor<3:
        raise ValueError('Unsupported VTF dimensions or version')
    if fmt in (13,15):return decode_vtf(data)
    formats={0:('RGBA',4),2:('RGB',3),12:('BGRA',4)}
    if fmt not in formats:raise ValueError('Unsupported VTF image format')
    count=struct.unpack_from('<I',data,68)[0]
    offset=next((struct.unpack_from('<I',data,84+8*i)[0] for i in range(count) if struct.unpack_from('<I',data,80+8*i)[0]==0x30),None)
    if offset is None:raise ValueError('Missing VTF image data')
    mode,bpp=formats[fmt]
    for level in range(mips-1,0,-1):offset+=max(1,width>>level)*max(1,height>>level)*bpp*frames
    size=width*height*bpp
    if len(data)<offset+size:raise ValueError('Truncated VTF image')
    return Image.frombytes('RGBA' if bpp==4 else 'RGB',(width,height),data[offset:offset+size],'raw',mode).convert('RGBA')


def main(handle):
    if not re.fullmatch(r'[0-9a-f]{16}',handle):raise ValueError('Invalid decal handle')
    folder=ROOT/'decals';raw=folder/(handle+'.raw');target=folder/(handle+'.png')
    image=decode(raw.read_bytes())
    image.thumbnail((512,512),Image.Resampling.LANCZOS)
    tmp=target.with_suffix('.tmp');image.save(tmp,format='PNG');os.replace(tmp,target)

if __name__=='__main__':
    try:main(sys.argv[1])
    except Exception as error:
        with (ROOT/'decal-errors.log').open('a',encoding='utf-8') as log:log.write(str(error)+'\n')
        raise SystemExit(1)
