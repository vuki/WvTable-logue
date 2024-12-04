# WvTable-logue

This is a custom oscillator for the Korg _Minilogue-xd_ synthesizer. It may also work on _NTS-1_ and _Prologue_, but it was not tested. It is not compatible with other logue instruments. 

WvTable-logue is a wavetable oscillator inspired by _PPG Wave_ instruments from early 80s. It uses the waves and wavetables from the _PPG Wave 2.3_ ROM. However, it is not intended to be an emulation of the _PPG Wave_, which used two independent wavetable oscillators, each with a suboscillator, running at a sampling rate c.a. 195 kHz. Due to the limited computing power of the _Cortex-M4_ processor in Minilogue-xd, only a single oscillator, running at a sample rate 96 kHz, is implemented. However, the wavetable can still be swept, either manually or with a LFO or an envelope. Thanks to the low-resolution waves, the oscillator produces lo-fi sounds, with audible, high-pitched distortion, similar to the original PPG instruments.

# Parameters

All parameters are described in more detail further in the document.

| Parameter                   | Range      | Decription                                                                                                                                                                                                                                                                                                 |
| --------------------------- | ---------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Shape                       | 0-100      | Wavetable index, in percentage of the wavetable length (0 to 61).<br/> The index is (shape * 0.61)                                                                                                                                                                                                         |
| Shift+Shape                 | 0-100      | [Wavecycle skew](#wavecycle-skew) amount. 0: no skew, 100%: maximum skew.                                                                                                                                                                                                                                  |
| Parameter 1<br/>Wavetable   | 0-95       | [Wavetable number](#wavetables).<br/>0-31: [Mode 1](#mode-1-wavetables-0---31) (fractional index, sample interpolation).<br/>32-63: [Mode 2](#mode-2-wavetables-32---63) (integer index, sample interpolation).<br/> 64-95: [Mode 3](#mode-3-wavetables-64---95) (integer index, no sample interpolation). |
| Parameter 2<br/>Env Attack  | 0-100      | Attack time of the wavetable index [envelope](#wavetable-index-envelope).                                                                                                                                                                                                                                  |
| Parameter 3<br/>Env Decay   | -99 to 100 | Decay or release time of the wavetable index [envelope](#wavetable-index-envelope).<br/>Negative values: decay time (AD envelope).<br/>Positive values: release time (ASR envelope). envelope.                                                                                                             |
| Parameter 4<br/>Env Amount  | -99 to 100 | Amount of the wavetable index [envelope](#wavetable-index-envelope).                                                                                                                                                                                                                                       |
| Parameter 5<br/>LFO2 Rate   | 0-100      | [LFO2](#wavetable-index-modulation-with-lfo--lfo2) wavetable index modulation rate (frequency).                                                                                                                                                                                                            |
| Parameter 6<br/>LFO2 Amount | 0-100      | [LFO2](#wavetable-index-modulation-with-lfo--lfo2) wavetable index modulation depth (amplitude).                                                                                                                                                                                                           |


# Wavetable synthesis in _WvTable_

This oscillator implements the wavetable synthesis method, as designed by Wolfgang Palm in early 1980s and implemented in his _PPG Wave_ synthesizers. Here, the term _wavetable_ means "a table of waves", not "a table of wave samples", as it is commonly used nowadays. Please refer to [this document](https://www.hermannseib.com/documents/PPGWTbl.pdf) for the depiction of the original PPG waves and wavetables.

A _wavecycle_ is a single period of a harmonic wave, stored digitally in memory. A continuous wave is produced by cyclic reading of the wavecycle samples from memory. The PPG synthesizer wavecycles consist of 64 samples of the first half of the cycle. The second half is the same as the first one but mirrored in time and amplitude. The whole wavecycle is therefore antisymmetric and consists of 128 samples, stored with 8-bit resolution. There are 220 unique waves in the oscillator.

A _wavetable_ is an ordered collection of waves. _Wave index_ is the position inside the wavetable. The index may be modulated while the wave is generated, which is called a _wavetable sweep_. This is the characteristic feature of this synthesis method, whichh makes the produced sound dynamic and live. The index may be modulated either manually or with an LFO and an envelope generator.



# Wavetables

The oscillator uses 32 __wavetables__. similar to the ones used in the _PPG Wave_. Each wavetable may be used in one of three modes (see below), which gives a total of 96 wavetables that can be selected.

Each wavetable has 61 positions, numbered 0 to 61. Selected __wavecycles__, stored in the memory, are placed at the defined positions within the wavetable. The remaining positions are filled by interpolation (morphing) of the memory wavecycles. The __wavetable index__ defines the wave that is used for generating the continuous wave, by looping the wavecycle. In Mode 1, the index may be a fractional number, while in the Modes 2 and 3, the index must be an integer.

The original _PPG Wave_ wavetables had 64 integer positions, with the standard waves at positions 60-63 (a triangle, a pulse, a square and a saw; see [this document](https://www.hermannseib.com/documents/PPGWTbl.pdf)). In this oscillator, these waves are omitted, as they are not needed (they are available in the analog oscillators) and they disrupt the wavetable sweep, resulting in a harsh sound. Unlike the original wavetables, a memory wavecycle is also available at the last position (61).

Most of the wavetables have a smooth transition between the adjacent wavecycles. Wavetables 20, 22, 25 and 30 have more rapid changes between the wavecycles.

Wavetable 13 is broken - it contains random data (a digital noise) instead of wavecycles. It was like this in the original wavetables, so this oscillator also uses the same wavetable (some people actually like it). The original, intended wavecycles were overwritten by other data.

Wavetable 26 is corrupted by the overflow errors - the original wavetable is retained in this oscillator.

Wavetables 28 (sync) and 29 (step) are special - they are computed in memory. The waves generated in this oscillator are slightly different from the original wavetables, especially the wavetable 28 in Mode 1. The wavetable 29 is not really usefull (it's jut a PWM wave), but it's left here for completeness.

Wavetable 30 is the _upper wavetable_ - here each integer index in the wavetable corresponds to a different wavecycle (there are no smooth transitions). This wavetable is normally used by selecting a specific wavecycle, it is not meant for sweeping the table.

Wavetable 31 is the same as wavetable 26, but with the overflow errors fixed.


# Wavetable modes

Each of the 32 wavetables may be used in one of three modes.

## Mode 1: wavetables 0 - 31

In Mode 1, the wavetable index may be any fractional number between 0 and 61. When the wavetable is swept, the memory wavecycles are interpolated (smoothed), so that the changes in the timbre are smooth. Also, sample values within the wavecycle are interpolated linearly, so the resulting wave is smooth (as much as the linear interpolation allows). This mode provides the cleanest sound, with the least amount of distortion, and it allows for smooth modulation of the wave index.

## Mode 2: wavetables 32 - 63

Mode 2 uses the same wavetables, but the wavetable index must be an integer (the fractional part of the index is cut off). This mode makes it easier to select a specific integer position in the wavetable. However, when the table is swept, changes in the timbre are more rapid and more audible (a stepping effect may be observed). The wave values between the samples are still interpolated, so the resulting wave is as smooth, as in the Mode 1.

## Mode 3: wavetables 64 - 95

In Mode 3, the wavecycles are selected the same way as in the Mode 2, but additionally, there is no interpolation between the samples (sample positions are integers), This mode is the closest to the original _PPG Wave_ oscillator and it produces the most dirty signal of all three modes, with the largest amount of distortion. The resulting waves are not smooth, but stepped, which is the most evidennt for lower frequencies (a low-pass filter in the synthesizer removes some of these distortion).


# Wavetable index modulation

The wavetable index is set by the user, but it can also modulated during the sound production, which allows for dynamic changes in timbre. The index may be modulated:

- manually, with the _Shape_ knob,
- manually, with the joystick or any MIDI controller assigned to the _Shape_ parameter,
- with the LFO of the synth, routed to the _Shape_ parameter,
- with the additional, triangular LFO in the oscillator,
- with the simple envelope generator in the oscillator.

The effect of all modulators is cumulative.

It is possible that the modulated wavetable index will go beyond the range 0 to 61. In this case, the values outside this range are mirrored (folded). For example, 62 becomes 60, -3 becomes 3, etc. This allows for more drastic timbre changes if the modulation depth is high.


## Wavetable index envelope

The oscillator has a simple, two-stage envelope for wave number modulation. Depending on the settings, it may operate as an AD (_attack-decay_) or ASR (_attack-sustain-release_) envelope. The envelope generator has three parameters: the _attack time_, the _decay/release time_ and the _amount_ (which may be positive or negative). 

The AD envelope is used if the _decay/release time_ parameter is negative - the absolute value of this parameter is the decay time. At the _Note On_ event, the envelope is activated, the wavetable index changes from the base value (set with the _Shape_ controller) to (base + amount) in the attack time, then it immediately goes back to the base value in the decay time.

The ADS envelope is used if the _decay/release time_ parameter is positive or zero. At the _Note On_ event, the envelope is activated, the wavetable index changes from the base value to (base + amount) in the attack time, and stays at this level. At the _Note Off_ event, the index goes back to the base value in the release time.

The amount is expressed in wavetable index, the maximum modulation is ±100 wavetable positions. Due to the way the _Minilogue xd_ works, when the oscillator is loaded into memory, the display shows that the amount  value is -99%, but the actual value is 0.

The attack and the decay/release times can be set as parameter values 0-100, corresponding to the range 0 to almost 10 seconds, and the relation is exponential (see the table below). The exact equation is: $time = 0.1 * exp(0.046 * param)$. Either section time may be zero, in that case the envelope transitions to the next stage immediately. If both times are set to zero, the envelope does not run.

| Parameter value           |    0 |   35 |   50 |   65 |   85 |  100 |
| ------------------------- | ---: | ---: | ---: | ---: | ---: | ---: |
| Envelope time, in seconds |    0 |  0.5 |    1 |    2 |    5 | 9.95 |


## Wavetable index modulation with LFO & LFO2

A low frequency oscillator (LFO) may be used for a cyclic modulation of the wavetable index. There are two LFOs available: the main one from the synthesizer, and an additional one (LFO2) in the oscillator. Both LFOs may be used at the same time.

If the main LFO of the instrument is routed to the Shape parameter, it is used by the oscillator to modulate the wavetable index, according to the _Rate_ and _Intensity_ values set with the instrument knobs. The maximum modulation range is near 128 wavetable positions. The main LFO influences the wavetable index value for the whole duration of the sound.

A supplementary LFO2 is available in the oscillator. This LFO uses a triangular wave only. Activating the LFO2 for the waveyable index modulation allows using the main LFO for the cutoff or pitch modulation. Contrary to the main LFO, the LFO2 influences the wavetable index __only if the envelope is not in the attack or the decay/release stage__. In the AD envelope, the LFO2 is active after the decay phase finishes. In the ASR envelope, the LFO2 is active after the attack phase is completed. Note: if the envelope attack time is nonzero, the LFO is effectively delayed, even if the envelope amount is zero.

The LFO2 is controlled by two parameters: _rate_ (LFO frequency) and _amount_ (LFO amplitude), both set as parameter values 0 to 100. For the LFO amount, the value is scaled in wavetable positions. The LFO rate can be set in the range 0-20 Hz, and the relation is exponential, as shown in the table below.

| Parameter value  |    0 |   25 |   37 |   50 |   70 |   85 |   94 |  100 |
| ---------------- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LFO2 rate, in Hz |    0 |  0.5 | 1.02 |    2 | 5.16 | 10.2 | 15.3 | 20.0 |


# Wavecycle skew

Wavecycle skew is an additional function that was not present in the original _PPG Wave_ oscillators. It is an additional method of modifying the wavecycle shape and the sound timbre. It is controlled with _Shift+Shape_.

The wavecycles read from memory are composed from two half-cycles. The first half is read as is, the second one is read back-to-front, with inverted amplitude. If the skew is set to zero, both halves of the cycle are read with the same speed, and the resulting complete cycle is perfectly antisymmetric.

Increasing the skew value breaks this symmetry: the first half of the cycle is shorttened, and the second half is extended. The wave cycle loses its symmetry, which has a significant influence on the sound timbre. The larger the skew value, the more audible difference inthe timbre is obtained.

Skew has no effect on the wavetables 28 (sync) and 29 (step), as they do not use the wavecycles read from the memory.


# About the sound quality

*PPG Wave*s were early hybrid (digital-analog) synthesizers and as such, they were limited by the available technology. The samples were stored with 8-bit resolution, the cycles were only 128 samples long, no interpolation was used, no anti-aliasing measures were applied. As a result, the sound they produced was distorted, with audible high-frequency, non-harmonic components. This distortion is especially noticeable for higher pitches, and for waves with a complex shape and a wide range of spectral components. Nowadays, this lo-fi character of synthetic sounds is considered a feature of these synthesizers.

There are two main causes of the synthetic sound distortion.

1. _Interpolation errors_. Interpolation is reading wave samples or waves from the wavetable at positions that are not integers. The original _PPG Wave_ oscillators did not use any interpolation in these cases – fractional part of the position was simply truncated. Because there are only 128 samples per cycle, and the samples are stored with 8-bit resolution, the resulting wave shape is not smooth, but rather stepped. This results in adding frequency components to the signal. The higher the wave frequency, the higher the level of distortion.
2. _Aliasing_. Pitch shifting upwards of the waves read from memory may cause the frequency components to go above the half of the sampling rate, resulting in aliasing, which has similar character of distortion to the interpolation errors. Unconfirmed sources say that the original _PPG Wave_ oscillator used about 195 kHz sampling rate, which limited the aliasing distortion to some degree. Still, they are clearly audible for higher pitches, especially for waves with rich frequency content.

In this implementation of the wavetable oscillator, the first problem is partially mitigated by applying a linear interpolation when samples are read from non-integer positions, as well as when reading wavetables at non-integer positions (which was not possible in the original oscillator). As a result, the produced wave is smoother, with reduced audible distortion. Modes 2 and 3 disable the interpolation, so the distortion is more audible.

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

The GitHub repository contains a minimized version of `logue-sdk`. To build the oscillator, you only need a supported compiler (GCC for ARM), as well as `make` and `zip` utilities installed.

Scripts in the `logue-sdk/tools/gcc`_ directory download the compiler, version `gcc-arm-none-eabi-10-2020-q4-major`. This version was tested by the author, but other GCC versions should work as well.

Building the oscillator requires only issuing the `make install` command in the main directory of the repository. All three binaries for the supported synthesizers should be created in this directory.

If you already have a working compiler installed, you can pass the location of a directory containing compiler binaries (`bin`) as a `GCC_BIN_PATH` parameter when invoking `make`. For example:

```
make install GCC_BIN_PATH=../../gcc-arm-none-eabi-10-2020-q4-major/bin
```


# License

The software is licensed under the terms of the GNU General Public License v3.0. See the LICENSE.md file for details.

The data contained in the _wtdef.c_ file was extracted from the _PPG Wave 2.3_ ROM, publicly available on the Internet. The author of the project does not claim any rights to this data. According to the author's knowledge, this data is not protected by any active patents. In the post on the KVR forum made in June 2011, Hermann Seib stated that "... the wavetables that have been copied from the PPG range of synthesizers (...), as I've been assured by Wolfgang Palm, are not protected in any way" ([source](https://www.kvraudio.com/forum/viewtopic.php?t=321167)).

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
