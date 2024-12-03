"""Test wavetable oscillator"""

import numpy as np
import wvtable as wt

BLOCKSIZE = 32
SRATE = 48000


def generate_wave(wavetable, wave, pitch, duration, skew=0):
    """Generate a single wave.
    wavetable: number of the wavetable, 0..95
    wave: index 0..61
    pitch: 0..127
    duration: in seconds
    skew: 0..1023
    """
    num_samples = int(duration * SRATE)
    buf = np.zeros(num_samples)
    pos = 0
    wt.osc_init()
    wt.osc_set_wavetable(wavetable)
    wt.osc_set_wave(round(wave * (1024 / 61)))
    if skew > 0:
        wt.osc_set_skew(skew)
    pitch_par = round(pitch * 256)
    param = wt.UserOscParam(pitch=pitch_par, shape_lfo=0)
    wt.osc_noteon(param)
    while num_samples > 0:
        frame = wt.osc_cycle(param, BLOCKSIZE)
        ns = min(BLOCKSIZE, num_samples)
        buf[pos : pos + ns] = np.array(frame, np.int32).astype(float) * 2**-31
        pos += ns
        num_samples -= BLOCKSIZE
    return buf


def sweep_table(wavetable, pitch, duration, skew=0):
    """Sweep through the whole table.
    wavetable: number of the wavetable, 0..95
    pitch: 0..127
    duration: in seconds
    skew: 0..1023
    """
    num_samples = int(duration * SRATE)
    buf = np.zeros(num_samples)
    pos = 0
    wt.osc_init()
    wt.osc_set_wavetable(wavetable)
    wt.osc_set_wave(0)
    if skew > 0:
        wt.osc_set_skew(skew)
    pitch_par = round(pitch * 256)
    param = wt.UserOscParam(pitch=pitch_par, shape_lfo=0)
    wt.osc_noteon(param)
    index = 0
    delta_index = (2**30 - 1) / num_samples
    while num_samples > 0:
        param.shape_lfo = round(index)
        frame = wt.osc_cycle(param, BLOCKSIZE)
        ns = min(BLOCKSIZE, num_samples)
        buf[pos : pos + ns] = np.array(frame, np.int32).astype(float) * 2**-31
        pos += ns
        num_samples -= BLOCKSIZE
        index += ns * delta_index
    return buf


def pitch_sweep(wavetable, index, start_pitch, end_pitch, duration, skew=0):
    """Generate a given wave with a pitch sweep.
    wavetable: number of the wavetable, 0..95
    index: 0..64
    start_pitch: beginning of the sweep, 0..127
    end_pitch: end of the sweep, 0..127
    duration: in seconds
    skew: 0..1023
    """
    num_samples = int(duration * SRATE)
    buf = np.zeros(num_samples)
    pos = 0
    wt.osc_init()
    wt.osc_set_wavetable(wavetable)
    wt.osc_set_wave(round(wave * (1024 / 61)))
    if skew > 0:
        wt.osc_set_skew(skew)
    param = wt.UserOscParam(pitch=69, shape_lfo=0)
    wt.osc_noteon(param)
    pitch = start_pitch
    delta_pitch = 256 * (end_pitch - start_pitch) / num_samples
    while num_samples > 0:
        param.pitch = round(pitch)
        frame = wt.osc_cycle(param, BLOCKSIZE)
        ns = min(BLOCKSIZE, num_samples)
        buf[pos : pos + ns] = np.array(frame, np.int32).astype(float) * 2**-31
        pos += ns
        num_samples -= BLOCKSIZE
        pitch += ns * delta_pitch
    return buf


def skew_sweep(wavetable, wave, pitch, duration):
    """Generate a given wave with a skew sweep.
    wavetable: number of the wavetable, 0..95
    wave: index 0..61
    pitch: 0..127
    duration: in seconds
    """
    num_samples = int(duration * SRATE)
    buf = np.zeros(num_samples)
    pos = 0
    wt.osc_init()
    wt.osc_set_wavetable(wavetable)
    wt.osc_set_wave(round(wave * (1024 / 61)))
    pitch_par = round(pitch * 256)
    param = wt.UserOscParam(pitch=pitch_par, shape_lfo=0)
    wt.osc_noteon(param)
    skew = 0
    delta_skew = 1024 / num_samples
    while num_samples > 0:
        wt.osc_set_skew(round(skew))
        frame = wt.osc_cycle(param, BLOCKSIZE)
        ns = min(BLOCKSIZE, num_samples)
        buf[pos : pos + ns] = np.array(frame, np.int32).astype(float) * 2**-31
        pos += ns
        num_samples -= BLOCKSIZE
        skew += ns * delta_skew
    return buf


if __name__ == '__main__':
    import soundfile as sf

    wtable = 0
    wave = 40
    pitch = 69

    # y = generate_wave(wtable, wave, pitch, 3.0)
    # sf.write(f'wt{wtable}_w{wave}_p{pitch}.wav', y, SRATE, 'FLOAT')

    # y = sweep_table(wtable, pitch, 10.0)
    # sf.write(f'wtsweep_wt{wtable}_p{pitch}.wav', y, SRATE, 'FLOAT')

    # p1 = 21
    # p2 = 127
    # y = pitch_sweep(wtable, wave, p1, p2, 10.0)
    # sf.write(f'psweep_wt{wtable}_w{wave}_p{p1}-{p2}.wav', y, SRATE, 'FLOAT')

    y = skew_sweep(wtable, wave, pitch, 10.0)
    sf.write(f'skewsweep_wt{wtable}_w{wave}_p{pitch}.wav', y, SRATE, 'FLOAT')
