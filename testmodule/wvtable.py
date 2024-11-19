"""Python interface to test library wvtable.dll"""

import ctypes as ct
from pathlib import Path

# Load library
curdir = Path(__file__).resolve().parent
libname = 'wvtable.dll'

try:
    lib = ct.CDLL(curdir / libname)
except FileNotFoundError:
    try:
        lib = ct.CDLL(curdir / 'build/Release' / libname)
    except FileExistsError:
        # last chance
        try:
            lib = ct.CDLL(curdir / 'build/Debug' / libname)
        except FileExistsError:
            raise RuntimeError(f'Library file {libname} not found.')


# WvTable.c


class UserOscParam(ct.Structure):
    _fields_ = [
        ('shape_lfo', ct.c_int32),
        ('pitch', ct.c_uint16),
        ('cutoff', ct.c_uint16),
        ('resonance', ct.c_uint16),
        ('reserved0', ct.c_uint16 * 3),
    ]


k_user_osc_param_id1 = 0
k_user_osc_param_id2 = 1
k_user_osc_param_id3 = 2
k_user_osc_param_id4 = 3
k_user_osc_param_id5 = 4
k_user_osc_param_id6 = 5
k_user_osc_param_shape = 6
k_user_osc_param_shiftshape = 7
k_num_user_osc_param_id = 8

_osc_init = lib.OSC_INIT
_osc_init.argtypes = [ct.c_uint32, ct.c_uint32]
_osc_init.restype = None
_osc_init.__doc__ = '_osc_init(uint32_t platform, uint32_t api)'

_osc_cycle = lib.OSC_CYCLE
_osc_cycle.argtypes = [ct.POINTER(UserOscParam), ct.POINTER(ct.c_int32), ct.c_int32]
_osc_cycle.restype = None

osc_noteon = lib.OSC_NOTEON
osc_noteon.argtypes = [ct.POINTER(UserOscParam)]
osc_noteon.restype = None
osc_noteon.__doc__ = 'osc_noteon(params)'

osc_noteoff = lib.OSC_NOTEOFF
osc_noteoff.argtypes = [ct.POINTER(UserOscParam)]
osc_noteoff.restype = None
osc_noteoff.__doc__ = 'osc_noteoff(params)'

# Do not use directly: values out of range may be passed to the functions.
_osc_param = lib.OSC_PARAM
_osc_param.argtypes = [ct.c_uint16, ct.c_uint16]
_osc_param.restype = None
_osc_param.__doc__ = 'osc_param(uint16_t index, uint16_t value)'


def osc_init():
    _osc_init(0, 0)
    for n in range(k_num_user_osc_param_id):
        _osc_param(n, 0)


def osc_cycle(params: UserOscParam, nframes: int):
    """osc_cycle(params, nframes)"""
    framebuf = (ct.c_int32 * nframes)()
    _osc_cycle(ct.byref(params), framebuf, nframes)
    return list(framebuf)


def osc_set_wave(wavenum: int):
    """Set wave number - wavetable position (0..1023)"""
    _osc_param(k_user_osc_param_shape, wavenum & 0x3FF)


def osc_set_skew(skew: int):
    """Set skew (0..1023)"""
    _osc_param(k_user_osc_param_shiftshape, skew & 0x3FF)


def osc_set_wavetable(param: int):
    """Set wavetable number (0..95)"""
    _osc_param(k_user_osc_param_id1, max(0, min(param, 95)))


def osc_set_env_attack(param: int):
    """Set envelope attack rate (0..100)"""
    _osc_param(k_user_osc_param_id2, max(0, min(param, 100)))


def osc_set_env_decay(param: int):
    """Set envelope decay rate (-99..100)"""
    _osc_param(k_user_osc_param_id3, max(-99, min(param, 100)) + 100)


def osc_set_env_amount(param: int):
    """Set envelope amount (-99..100)"""
    _osc_param(k_user_osc_param_id4, max(-99, min(param, 100)) + 100)


def osc_set_lfo_rate(param: int):
    """Set LFO2 rate (0..100)"""
    _osc_param(k_user_osc_param_id5, max(0, min(param, 100)))


def osc_set_lfo_amount(param: int):
    """Set LFO2 amount (0..100)"""
    _osc_param(k_user_osc_param_id6, max(0, min(param, 100)))


#

if __name__ == '__main__':
    import numpy as np
    from matplotlib import pyplot as plt

    osc_init()

    osc_set_wavetable(0)
    osc_set_wave(int(24 * (1024 / 61)))
    # osc_set_skew(750)

    param = UserOscParam(pitch=(69 << 8), shape_lfo=0)
    osc_noteon(param)

    out = []
    nsamples = 500
    block = 32
    n = 0
    while n < nsamples:
        y = osc_cycle(param, block)
        yf = np.array(y, np.int32).astype(float) * 2**-31
        out.append(yf)
        n += block

    out = np.hstack(out)

    plt.plot(out)
    plt.grid()
    plt.show()
