"""TF2 wear categories use float32 comparisons, including their endpoints."""
import struct
from .models import AssetError

def wear_level(value):
    if not 0<=value<=1:raise AssetError('Wear must be in the range 0 to 1')
    for index,threshold in enumerate((0.2,0.4,0.6,0.8),1):
        boundary=struct.unpack('<f',struct.pack('<f',threshold))[0]
        if value<=boundary:return index
    return 5
