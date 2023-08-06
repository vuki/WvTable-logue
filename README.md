# WvTable-logue

This is a custom oscillator for the Korg _Minilogue-xd_ synthesizer. It may also work on _NTS-1_ and _Prologue_, but it was not tested. It is not compatible with _Drumlogue_. The oscillator implements a wavetable signal generator, inspired by _PPG Wave_ instruments from early 80s. However, it is not meant to be an emulation of these synthesizers. A single oscillator is implemented, using waves and wavetables extracted from _PPG Wave 2.3_ ROM. Wavetables can be swept manually, as well as with an envelope generator or an LFO. Due to the limited wave resolution, the oscillator produces lo-fi sounds, with audible, high-pitched distortion, similar to the original PPG instruments.

# Parameters

All parameters are described in more detail further in the document.

| Parameter   | Range        | Decription                                                                                       |
| ----------- | ------------ | ------------------------------------------------------------------------------------------------ |
| Shape       | 0-100%       | Wave position in the wavetable, in percentage of the wavetable length (0 to 128).                |
| Shift+Shape | 0-100%       | Phase distortion amount. 0: no distortion, 100%: maximum distortion.                             |
| Wavetable   | 0-60         | Wavetable number. 0-29: mode 1 (base + upper wavetable). 30-60: mode 2 (only base wavetable).    |
| Env Attack  | 0-100        | Attack time of the wave number modulation envelope.                                              |
| Env Decay   | 0-100        | Decay time of the wave number modulation envelope.                                               |
| Env Amount  | -99% to 100% | Amount of the wave number modulation envelope. 100% is 100 wavetable positions.                  |
| LFO2 Rate   | 0-100        | Rate (frequency) of the additional LFO modulating the wave number.                               |
| LFO2 Amount | 0-100        | Amount (amplitude) of the additional LFO modulating the wave number, in the wavetable positions. |


# Waves and wavetables

This oscillator implements the wavetable synthesis method, as designed by Wolfgang Palm in early 1980s and implemented in his _PPG Wave_ synthesizers. Here, the term _wavetable_ means "a table of waves", not "a table of wave samples", as it is commonly used nowadays. Please refer to [this document](https://www.hermannseib.com/documents/PPGWTbl.pdf) for the depiction of the original PPG waves and wavetables.

A _wave_ (or _wavecycle_) is a single period of a harmonic wave, stored digitally in memory. A waveform is produced by cyclic reading of the wave samples from memory. The PPG synthesizer waves consist of 64 samples of the first half of the cycle. The second half is the same as the first one but inverted in time and amplitude. The whole wave cycle is therefore antisymmetric and consists of 128 samples, stored with 8-bit resolution. There are 204 unique waves in the oscillator.

A _wavetable_ is an ordered collection of waves. _Wave number_ is the position inside the wavetable. The original _PPG Wave_ had 31 wavetables, numbered 0-30, with 64 waves each. Wavetable 30 (the upper wavetable) was special, it could be stacked on top of another wavetable, resulting in a wavetable of 128 positions. Some positions in the wavetable were filled with waves stored in memory, the remaining ones were interpolated (crossfaded). The wavetables 28 (sync wave) and 29 (step wave) were fully calculated.

_Wave sweep_ is a modulation of the wave number (wavetable position) during sound generation. Changing the wave number modifies the sound timbre. Modulation may be performed manually, with LFO or with an envelope generator. Wave sweep is the main feature of the characteristic _PPG Wave_ sound. Most wavetables have smooth transitions between the neighboring waves, which gives a pleasant effect. There are exceptions – some wavetables have abrupt wave changes (e.g., wavetable 20, 22, 25 or 30). During the sweep, the wave number wraps around the range.

This oscillator uses different wavetable layout from the original _PPG Wave_ tables. Each wavetable has 128 positions, and the wave number may be fractional (unlike the _PPG Wave_ that used only integer wave numbers). There are 61 wavetables, in two modes.

**Mode 1 (wavetables 0 to 29)**

- Wave numbers 0-60: _base_ wavetable, waves 0-60 from _PPG_ wavetables 0-29.
- Wave numbers 64-124: _upper_ wavetable (30 from _PPG_), _in reversed order_ (waves 60-0).
- Wave numbers 60-64 and 124-0: transitions between the wavetables.

**Mode 2 (wavetables 30 to 60)**

- Wave numbers 0-60: waves 0-60 from _PPG_ wavetables 0-30.
- Wave numbers 64-124: the same wavetable, _in reversed order_ (waves 60-0).
- Wave numbers 60-64 and 124-0: the same wave (60 and 0) repeated.

Mode 1 is more suitable for small wave number modulation, or no modulation at all. It gives an easy access to the waves from the upper wavetable. Mode 2 is better for wide wavetable sweeps, resulting in smooth transitions between the waves.

The original _PPG Wave_ wavetables had standard waves at positions 60-63 (a triangle, a pulse, a square and a saw). In this oscillator, these waves are omitted, as they are not needed (they are available in the analog oscillators) and they disrupt the wavetable sweep, resulting in a harsh sound.

# Wave envelope

The oscillator has a simple, two-stage (attack-decay, AD) envelope for wave number modulation. The envelope has three parameters: the _attack time_, the _decay time_ and the _amount_. At the _Note On_ event, the envelope is activated, the wave number changes from the base value (set with the _Shape_ controller) to (base + amount) in the attack time, then it goes back to the base value in the decay time. The amount may be positive or negative. The wave number wraps around the range (128 becomes 0, -1 becomes 127, etc.).

The amount is expressed in wavetable positions, the maximum modulation is ±100 wave numbers. Due to the _Minilogue xd_ bug, when the oscillator is loaded into memory, the display shows value -99% for the amount parameter, but the actual value is 0.

The attack and the decay times can be set as parameter values 0-100, which corresponds to the range 0 to almost 10 seconds, and the relation is exponential (see the table below). The exact equation is: $time = 0.1 * exp(0.046 * param)$. Either time may be zero, in that case the envelope transitions to the next stage immediately. If both times are set to zero, the envelope does not run.

| Parameter value           |    0 |   35 |   50 |   65 |   85 |  100 |
| ------------------------- | ---: | ---: | ---: | ---: | ---: | ---: |
| Envelope time, in seconds |    0 |  0.5 |    1 |    2 |    5 | 9.95 |

# Wave LFO & LFO2

A low frequency oscillator (LFO) may be used for a cyclic modulation of the wave number. There are two LFOs available: the main one from the synthesizer, and an additional one (LFO2) in the oscillator. Both LFOs may be used at the same time.

Both LFOs become active only after the wave envelope has completed the decay stage. If the envelope attack or/and decay time is nonzero, the LFO is effectively delayed, even if the envelope amount is zero.

If the main LFO of the instrument is routed to the Shape parameter, it is used by the oscillator to modulate the wave number, according to the _Rate_ and _Intensity_ values set with the instrument knobs. The maximum modulation range is near 128 wavetable positions.

There is a supplementary LFO2 implemented in the oscillator, which uses a triangular wave only. Activating the LFO2 for the wave number modulation allows using the main LFO for the cutoff or pitch modulation. The LFO2 is controlled by two parameters: _rate_ (LFO frequency) and _amount_ (LFO amplitude), both set as parameter values 0 to 100. For the LFO amount, the value is scaled in wavetable positions. The LFO rate can be set in the range 0-20 Hz, and the relation is approximately exponential, as shown in the table below.

| Parameter value  |    0 |   13 |   25 |   50 |   63 |   82 |   90 |  100 |
| ---------------- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LFO2 rate, in Hz |    0 | 0.52 |    1 |    2 |    5 | 10.1 | 14.9 | 20.3 |

# Phase distortion

Phase distortion was not present in the original _PPG Wave_ oscillators. It is an additional method of modifying the wave shape and the sound timbre. It is controlled with _Shift+Shape_.

The wave cycles read from memory are composed from two half-cycles. The first half is read as is, the second one is read back-to-front, with inverted amplitude. If the phase distortion is disabled, both halves of the cycle are read with the same speed, and the resulting complete cycle is perfectly antisymmetric.

Activating the phase distortion breaks this symmetry: the first half of the cycle is read faster, and the second half is read slower. The wave cycle becomes asymmetric, and this has a significant influence on the sound timbre. The larger the phase distortion parameter, the larger the difference in the read speed.

Phase distortion has no effect on the wavetables that are calculated (the lower part of the wavetables 28 and 29, and the whole wavetables 58 and 59).

# About the sound quality

*PPG Wave*s were early hybrid (digital-analog) synthesizers and as such, they were limited by the available technology. As a result, the sound they produced was distorted, with audible high-pitched components. This distortion is especially noticeable for higher pitches, and for waves with a complex shape and a wide range of spectral components. Nowadays, this lo-fi character of synthetic sounds is considered a feature of these synthesizers.

There are two main causes of the synthetic sound distortion.

1. _Interpolation errors_. Interpolation is reading wave samples or waves from the wavetable at positions that are not integers. The original _PPG Wave_ oscillators did not use any interpolation in these cases – fractional part of the position was simply truncated. Because there are only 128 samples per cycle, and the samples are stored with 8-bit resolution, the resulting wave shape is not smooth, it is more a piecewise-linear function. This results in adding frequency components to the signal. The higher the wave frequency, the higher the level of distortion.
2. _Aliasing_. Pitch shifting upwards of the waves read from memory causes the frequency components to go above the half of the sampling rate, resulting in aliasing, which has similar character of distortion to the interpolation errors. Unconfirmed sources say that the original _PPG Wave_ oscillator used about 195 kHz sampling rate, which limited the aliasing distortion to some degree. Still, they are clearly audible for higher pitches, especially for waves with rich frequency content.

In this implementation of the wavetable oscillator, the first problem is partially mitigated by applying a linear interpolation when samples are read from non-integer positions, as well as when reading wavetables at non-integer positions (which was not possible in the original oscillator). As a result, the produced wave is smoother, with reduced audible distortion.

On the other hand, this oscillator generates wave samples at a 96 kHz sampling rate, which is twice the actual sample rate of the synthesizer, but only a half of the alleged sampling rate of the _PPG Wave_. This is as much as the limited processing power of the Cortex-M4 processor used in the _Minilogue xd_ allows for. Therefore, the aliasing distortion in this oscillator are more audible than for _PPG Wave_, especially at higher pitches.

To conclude: this lo-fi, retro character of the sound produced with this oscillator should be considered a feature. The distortion amount can of course be reduced by decreasing the filter cutoff.

# Installing

The GitHub repository contains the binaries of the oscillator for the three supported synthesizers in the _Releases_ section. Only the _Minilogue xd_ version was tested.

The oscillator can be installed in the synthesizer like any other compatible oscillator. _Korg Sound Librarian_ or the command line _logue-cli_ tool may be used. The synthesizer must be connected through USB to a computer which has Korg USB driver installed. Refer to the Korg documentation for details.

With **Sound Librarian** : start the application and make sure that it communicates with the synthesizer via the USB MIDI port. Switch to the _User Oscillators_ tab. Click the _User OSC/FX Receive All_ button and wait until the list is populated. Drag the oscillator file onto a selected slot. Press the _User OSC/FX Send All_ button. Done.

With **logue-cli** : run the following command, substituting the slot number 0-15 for ```#``` and providing the path to the correct oscillator file version for the synthesizer (example for _Minilogue xd_):

```
logue-cli load -u WvTable.mnlgxdunit -s #
```

# Building

The GitHub repository contains a minimized version of `logue-sdk`. To build the oscillator, one needs only a supported compiler (GCC for ARM), as well as `make` and `zip` utilities installed.

Scripts in the `logue-sdk/tools/gcc_ directory` download the compiler, version `gcc-arm-none-eabi-10-2020-q4-major`. This version was tested by the author, but other GCC versions should work as well.

Building the oscillator requires only issuing the `make install` command in the main directory of the repository. All three binaries for the supported synthesizers should be created in this directory.

If you already have a working compiler installed, you can pass the location of a directory containing compiler binaries (`bin`) as a `GCC_BIN_PATH` parameter when invoking `make`. For example:

```
make install GCC_BIN_PATH=../../gcc-arm-none-eabi-10-2020-q4-major/bin
```

# License

The software is licensed under the terms of the GNU General Public License v3.0. See the LICENSE.md file for details.

The data contained in the _wtdef.c_ file was extracted from the _PPG Wave 2.3_ ROM, publicly available on the Internet. The author of the project does not claim any rights to this data. Legal status of this data is unclear. In the post on the KVR forum made in June 2011, Hermann Seib stated that "... the wavetables that have been copied from the PPG range of synthesizers. These, as I've been assured by Wolfgang Palm, are not protected in any way" ([source](https://www.kvraudio.com/forum/viewtopic.php?t=321167)). However, the situation might have changed, especially after PPG was acquired by Plugin Alliance in March 2020. Be advised.

# References

Several sources worth mentioning.

PPG Wave ROM Waveforms and Wavetables, by Hermann Seib. Making this oscillator would be impossible without this document. [https://www.hermannseib.com/documents/PPGWTbl.pdf](https://www.hermannseib.com/documents/PPGWTbl.pdf)

Hermann Seib's PPG System. From the PPG Wave guru, Hermann Seib. Lots of information, manuals and the Wave VST simulator. [https://www.hermannseib.com/english/synths/ppg/default.htm](https://www.hermannseib.com/english/synths/ppg/default.htm)

Paula Maddox's PPG Webpages. [http://ppg.synth.net/](http://ppg.synth.net/)

PPG CDROM v.3. A collection of PPG Wave related files, compiled by Paula Maddox. Used to be sold, now apparently abandonware. [https://archive.org/details/ppg-cdrom-v3](https://archive.org/details/ppg-cdrom-v3)

Owner manuals for PPG Wave 2.2 and 2.3. [https://www.synthxl.com/wave/](https://www.synthxl.com/wave/)

Synthesizer ROM Archive. Contains ROM dumps from many synthesizers, including _PPG 2.3 OS V6.zip_ used in this project. [https://dbwbp.com/index.php/9-misc/37-synth-eprom-dumps](https://dbwbp.com/index.php/9-misc/37-synth-eprom-dumps)

Waldorf Wave VST. A commercial VST plugin, PPG Wave emulation. [https://waldorfmusic.com/ppg-wave-3-v-en/](https://waldorfmusic.com/ppg-wave-3-v-en/) and the manual: [https://cloud.waldorfmusic.com/index.php/s/24NtZA9NzF8pJcF/download](https://cloud.waldorfmusic.com/index.php/s/24NtZA9NzF8pJcF/download)

Till Kopper; PPG Wave. Some (limited) technical info on the PPG Wave. [https://till-kopper.de/ppg-wave2\_3\_V8-3.html](https://till-kopper.de/ppg-wave2_3_V8-3.html)

Gearspace: Microwave 1 vs PPG. A forum topic that mentions some technical details of the PPG Wave, including its sampling frequency. [https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1100425-microwave-1-vs-ppg.html](https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1100425-microwave-1-vs-ppg.html)

KVR: Waldorf/prophet wavetables and copyright. Forum topic that mentions the lack of copyright to the PPG wavetables; also, a link to Hermann Seib's small utility _showwtbl_ that displays waves and wavetables. [https://www.kvraudio.com/forum/viewtopic.php?t=321167](https://www.kvraudio.com/forum/viewtopic.php?t=321167)

Wikipedia: PPG Wave. A brief encyclopedia entry. [https://en.wikipedia.org/wiki/PPG\_Wave](https://en.wikipedia.org/wiki/PPG_Wave)

Great Synthesizers: a general description of the PPG Wave. [https://greatsynthesizers.com/en/review/ppg-wave-2-2-wave-2-3-the-one-and-only/](https://greatsynthesizers.com/en/review/ppg-wave-2-2-wave-2-3-the-one-and-only/)
