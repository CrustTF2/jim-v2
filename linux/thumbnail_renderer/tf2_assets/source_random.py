"""Use the installed 64-bit TF2 random-stream implementation."""
import ctypes
import os
from pathlib import Path
from .models import AssetError

class SourceRandom:
    def __init__(self,tf,seed):
        if not 0<=seed<1<<64:raise AssetError('Seed must be an unsigned 64-bit integer')
        path=Path(tf).resolve().parent/'bin'/'x64'
        try:
            self._dirs=[os.add_dll_directory(str(path)),os.add_dll_directory(str(path.parent))]
            self.dll=ctypes.CDLL(str(path/'vstdlib.dll'))
            construct=getattr(self.dll,'??0CUniformRandomStream@@QEAA@XZ')
            set_seed=getattr(self.dll,'?SetSeed@CUniformRandomStream@@UEAAXH@Z')
            self._float=getattr(self.dll,'?RandomFloat@CUniformRandomStream@@UEAAMMM@Z')
            self._int=getattr(self.dll,'?RandomInt@CUniformRandomStream@@UEAAHHH@Z')
            construct.argtypes=[ctypes.c_void_p];construct.restype=ctypes.c_void_p
            set_seed.argtypes=[ctypes.c_void_p,ctypes.c_int32];set_seed.restype=None
            self._float.argtypes=[ctypes.c_void_p,ctypes.c_float,ctypes.c_float];self._float.restype=ctypes.c_float
            self._int.argtypes=[ctypes.c_void_p,ctypes.c_int32,ctypes.c_int32];self._int.restype=ctypes.c_int32
            # More than the published CUniformRandomStream layout, with natural alignment.
            self._objects=[ctypes.create_string_buffer(512),ctypes.create_string_buffer(512)]
            for parity,obj in enumerate(self._objects):
                value=sum(((seed>>(2*i+parity))&1)<<i for i in range(32))
                construct(obj);set_seed(obj,ctypes.c_int32(value).value)
        except (OSError,AttributeError) as error:
            raise AssetError('Could not load the installed TF2 64-bit random stream: '+str(error)) from error
        self.index=0

    def stage(self):
        obj=self._objects[self.index];self.index^=1
        return SourceStageRandom(self,obj)

class SourceStageRandom:
    def __init__(self,owner,obj):self.owner=owner;self.obj=obj
    def uniform(self,low,high):return self.owner._float(self.obj,low,high)
    def boolean(self):return bool(self.owner._int(self.obj,0,1))
