"""Portable Source-style uniform random streams used for War Paints."""
import struct
from .models import AssetError

_IM = 2147483647
_IA = 16807
_IQ = 127773
_IR = 2836
_NTAB = 32
_NDIV = 1 + (_IM - 1) // _NTAB


def _f32(value):
    return struct.unpack('f', struct.pack('f', value))[0]


class _Stream:
    def __init__(self, seed):
        if seed >= 1 << 31:
            seed -= 1 << 32
        self.seed = seed if seed < 0 else -seed
        self.last = 0
        self.table = [0] * _NTAB

    def number(self):
        if self.seed <= 0 or not self.last:
            self.seed = max(1, -self.seed)
            for j in range(_NTAB + 7, -1, -1):
                k = self.seed // _IQ
                self.seed = _IA * (self.seed - k * _IQ) - _IR * k
                if self.seed < 0:
                    self.seed += _IM
                if j < _NTAB:
                    self.table[j] = self.seed
            self.last = self.table[0]
        k = self.seed // _IQ
        self.seed = _IA * (self.seed - k * _IQ) - _IR * k
        if self.seed < 0:
            self.seed += _IM
        j = self.last // _NDIV
        self.last = self.table[j]
        self.table[j] = self.seed
        return self.last

    def uniform(self, low, high):
        low, high = _f32(low), _f32(high)
        value = min(_f32(_f32(1.0 / _IM) * self.number()), _f32(1.0 - 1.2e-7))
        return _f32(_f32(value * _f32(high - low)) + low)

    def boolean(self):
        # RandomInt(0, 1), with the same rejection-sampling range as Source.
        largest = 0x7fffffff - ((0x80000000) % 2)
        value = self.number()
        while value > largest:
            value = self.number()
        return bool(value % 2)


class SourceRandom:
    def __init__(self, tf, seed):
        if not 0 <= seed < 1 << 64:
            raise AssetError('Seed must be an unsigned 64-bit integer')
        self.streams = []
        for parity in range(2):
            value = sum(((seed >> (2 * i + parity)) & 1) << i for i in range(32))
            self.streams.append(_Stream(value))
        self.index = 0

    def stage(self):
        result = self.streams[self.index]
        self.index ^= 1
        return result
