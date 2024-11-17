/*
 * WvTable.cpp
 * Wavetable generator inspired by PPG Wave.
 * Main file for logue SDK plugin.
 * Author: Grzegorz Szwoch (GregVuki)
 */

#define OVS_2x

#include <userosc.h>
#include "compat.h"
#include "wtgen.h"
#include "envlfo.h"
#include "decimator.h"

struct {
    uint32_t nwave; // base wavetable index, without modulation
    uint32_t env_arate; // envelope attack
    uint32_t env_drate; // envelope decay/release
    uint16_t pitch; // last pitch value that was received
    uint8_t wt_num; // wavetable number
    int8_t env_hold; // 1: ASR envelope, 0: AD envelope
} g_osc_params;

WtGenState g_gen_state;
EnvLfoState g_mod_state;

#if defined(OVS_4x)
#define OVS 4
DecimatorState g_decimator;
DecimatorState g_decimator2;
#elif defined(OVS_2x)
#define OVS 2
DecimatorState g_decimator;
#else
#define OVS 1
#endif

// Envelope LUT: parameter (0.100) to rate
// tau = 0.1 * exp(0.046 * par)
const uint32_t ENV_LUT[101] = { 0x80000000, 0x6850f, 0x63a05, 0x5f25b, 0x5adea, 0x56c8c, 0x52e1f, 0x4f280, 0x4b990,
    0x4832f, 0x44f40, 0x41da6, 0x3ee47, 0x3c10a, 0x395d5, 0x36c91, 0x34529, 0x31f86, 0x2fb94, 0x2d940, 0x2b876, 0x29927,
    0x27b3f, 0x25eb0, 0x24369, 0x2295d, 0x2107c, 0x1f8ba, 0x1e209, 0x1cc5d, 0x1b7aa, 0x1a3e6, 0x19105, 0x17efe, 0x16dc6,
    0x15d54, 0x14da0, 0x13ea0, 0x1304d, 0x1229f, 0x1158e, 0x10913, 0xfd28, 0xf1c7, 0xe6e8, 0xdc87, 0xd29c, 0xc924,
    0xc019, 0xb777, 0xaf37, 0xa756, 0x9fd1, 0x98a1, 0x91c5, 0x8b37, 0x84f5, 0x7efa, 0x7945, 0x73d1, 0x6e9c, 0x69a3,
    0x64e3, 0x605a, 0x5c05, 0x57e2, 0x53ef, 0x5029, 0x4c8e, 0x491d, 0x45d4, 0x42b0, 0x3fb0, 0x3cd3, 0x3a17, 0x377b,
    0x34fc, 0x329a, 0x3054, 0x2e28, 0x2c15, 0x2a19, 0x2835, 0x2666, 0x24ac, 0x2306, 0x2173, 0x1ff2, 0x1e82, 0x1d23,
    0x1bd4, 0x1a93, 0x1962, 0x183e, 0x1727, 0x161c, 0x151e, 0x142b, 0x1342, 0x1265, 0x1191 };

// LFO rate LUT: parameter (0..100) to rate (UQ32)
// rate = 0.25 * (exp((log(9)/50) * par) - 1) = 0.25 * (exp(0.0.043944 * par) - 1)
// 0: 0 s, 50: 2 s, 100: 20 s
const uint32_t LFO_LUT[101] = { 0, 0x3ed, 0x807, 0xc50, 0x10cb, 0x1579, 0x1a5d, 0x1f79, 0x24d0, 0x2a64, 0x3039, 0x3650,
    0x3cae, 0x4354, 0x4a48, 0x518b, 0x5922, 0x6110, 0x6959, 0x7201, 0x7b0d, 0x8482, 0x8e62, 0x98b5, 0xa37e, 0xaec3,
    0xba8a, 0xc6d8, 0xd3b4, 0xe124, 0xef2e, 0xfdda, 0x10d2e, 0x11d33, 0x12df0, 0x13f6d, 0x151b4, 0x164cc, 0x178c1,
    0x18d9a, 0x1a364, 0x1ba28, 0x1d1f2, 0x1eace, 0x204c7, 0x21fec, 0x23c48, 0x259eb, 0x278e2, 0x2993d, 0x2bb0d, 0x2de61,
    0x3034c, 0x329e0, 0x3522f, 0x37c4d, 0x3a850, 0x3d64d, 0x4065b, 0x43892, 0x46d0a, 0x4a3de, 0x4dd28, 0x51905, 0x55792,
    0x598ef, 0x5dd3c, 0x6249a, 0x66f2d, 0x6bd19, 0x70e86, 0x7639b, 0x7bc83, 0x81969, 0x87a7b, 0x8dfea, 0x949e8, 0x9b8a8,
    0xa2c62, 0xaa54f, 0xb23ab, 0xba7b4, 0xc31ab, 0xcc1d5, 0xd5879, 0xdf5e2, 0xe9a5d, 0xf463b, 0xff9d2, 0x10b57b,
    0x117991, 0x124677, 0x131c91, 0x13fc4a, 0x14e60f, 0x15da55, 0x16d995, 0x17e44c, 0x18fafe, 0x1a1e35, 0x1b4e82 };

__fast_inline void update_frequency(uint16_t pitch)
{
    if (pitch == g_osc_params.pitch)
        return; // not changed
    // Calculate frequency in Hz for a given pitch number.
    const uint8_t note = pitch >> 8; // integer part of the pitch
    const uint16_t mod = pitch & 0xFF; // fractional part of the pitch
    float freq = osc_notehzf(note); // from lookup table
    if (mod > 0) {
#if 0
        // linear interpolation
        const float f1 = osc_notehzf(note + 1);
        freq = clipmaxf(linintf(mod * k_note_mod_fscale, freq, f1), k_note_max_hz);
#else
        // quadratic interpolation
        const float frac = (float)mod * 0.00390625f; // 1/256
        freq *= 0.00171723f * frac * frac + 0.05774266f * frac + 1.0000016f;
#endif
    }
    set_frequency(&g_gen_state, freq);
    g_osc_params.pitch = pitch;
}

/*
    OSC_INIT
    Called when the module is loaded into memory.
*/

void OSC_INIT(uint32_t platform, uint32_t api)
{
    (void)platform;
    (void)api;
    wtgen_init(&g_gen_state, k_samplerate * OVS);
    envlfo_init(&g_mod_state, k_samplerate);
    g_osc_params.nwave = 0;
    g_osc_params.env_arate = 0;
    g_osc_params.env_drate = 0;
    g_osc_params.pitch = 0;
    g_osc_params.wt_num = 0;
    g_osc_params.env_hold = 0;
#if defined(OVS_4x)
    decimator_reset(&g_decimator);
    decimator_reset(&g_decimator2);
#elif defined(OVS_2x)
    decimator_reset(&g_decimator);
#endif
}

/*
    OSC_NOTEON
    Called when a note is started.
    params.pitch: note pitch, UQ8.8.
*/

void OSC_NOTEON(const user_osc_param_t* const params)
{
    update_frequency(params->pitch);
    // prepare the oscillator
    wtgen_reset(&g_gen_state);
    set_wavetable(&g_gen_state, g_osc_params.wt_num);
    // prepare the modulator
    envlfo_set_arate(&g_mod_state, g_osc_params.env_arate);
    envlfo_set_drate(&g_mod_state, g_osc_params.env_drate);
    envlfo_set_hold(&g_mod_state, g_osc_params.env_hold);
    envlfo_note_on(&g_mod_state);
    // prepare the decimator
#if defined(OVS_4x)
    decimator_reset(&g_decimator);
    decimator_reset(&g_decimator2);
#elif defined(OVS_2x)
    decimator_reset(&g_decimator);
#endif
}

/*
    OSC_NOTEON
    Called when a note is started.
    params.pitch: note pitch, UQ8.8.
*/

void OSC_NOTEOFF(const user_osc_param_t* const params)
{
    (void)params;
    envlfo_note_off(&g_mod_state);
}

/*
    OSC_CYCLE
    Called when a buffer of samples must be generated.
    params.pitch: note pitch, uint16, UQ8.8.
    params.shape_lfo: LFO value for shape modulation, int32.
    framebuf: buffer for the generated samples, Q31.
    nframes: number of samples to generate (usually: 32).
*/

void OSC_CYCLE(const user_osc_param_t* const params, int32_t* framebuf, const uint32_t nframes)
{
    // check for pitch change (it may be modulated)
    update_frequency(params->pitch);

    // Calculate the wavetable index (Q7.24).
    // Index changes are updated once per block (normally, every 32 samples).
    int32_t nwave = g_osc_params.nwave;
    // main LFO modulation
    nwave += params->shape_lfo;
    // internal envelope + LFO, updated at the last sample
    nwave += envlfo_get(&g_mod_state, nframes);
    set_wave_number(&g_gen_state, nwave);
    // Any overflow will be handled within set_wave_number.
    // If the modulation is to be applied on every sample,
    // then the index change per sample is:
    // change_per_block >> (31-log2(nframes))
    // change_per_block >> (31 - __CLZ(nframes))

    // sample generation
    q31_t* __restrict py = (q31_t*)framebuf;
    const q31_t* const py_e = py + nframes;
    while (py != py_e) {
#if defined(OVS_4x)
        // generate with 4x oversampling
        const float y1 = generate(&g_gen_state);
        const float y2 = generate(&g_gen_state);
        const float y3 = generate(&g_gen_state);
        const float y4 = generate(&g_gen_state);
        const float y5 = decimator_do(&g_decimator2, y1, y2);
        const float y6 = decimator_do(&g_decimator2, y5, y6);
        const float y = decimator_do(&g_decimator, y5, y6);
#elif defined(OVS_2x)
        // generate with 2x oversampling
        const float y1 = generate(&g_gen_state);
        const float y2 = generate(&g_gen_state);
        const float y = decimator_do(&g_decimator, y1, y2);
#else
        const float y = generate(&g_gen_state);
#endif
        // convert float (-128..128) to Q31
        // scale by c.a. 0.95 to account for the ringing caused by the decimation
        *(py++) = (int32_t)(y * 15000000.f + 0.5f);
    }
}

/*
    OSC_PARAM
    Called when an [index] parameter [value] is changed.
*/

void OSC_PARAM(uint16_t index, uint16_t value)
{

    switch (index) {
    case k_user_osc_param_id1:
        // Param 1: wavetable number (0..95)
        g_osc_params.wt_num = (uint8_t)value;
        break;

    case k_user_osc_param_id2:
        // Param2: wave envelope attack time (0..100)
        g_osc_params.env_arate = ENV_LUT[value];
        // will be applied on Note On
        break;

    case k_user_osc_param_id3:
        // Param3: wave envelope decay time (1..200)
        if (value >= 100) {
            // positive values: ASR envelope
            g_osc_params.env_arate = ENV_LUT[value - 100];
            g_osc_params.env_hold = 1;
        } else if (value > 0) {
            // negative values: AD emvelope
            g_osc_params.env_arate = ENV_LUT[100 - value];
            g_osc_params.env_hold = 0;
        } else {
            // value 0: disable envelope (minilogue bug)
            g_osc_params.env_arate = ENV_LUT[0];
            g_osc_params.env_hold = 0;
        }
        // will be applied on Note On
        break;

    case k_user_osc_param_id4:
        // Param4: wave envelope amount (1..200)
        // param (1..200) to amnount (-99..100)
        // ignore 0 value - logue bug
        {
            const int32_t env_amount = (value > 0) ? ((int32_t)value - 100) : 0;
            envlfo_set_env_amount(&g_mod_state, (int8_t)env_amount);
        }
        break;

    case k_user_osc_param_id5:
        // Param5: LFO2 rate (0..100), maps to 0..20 Hz, exponential curve
        envlfo_set_lfo_rate(&g_mod_state, LFO_LUT[value]);
        break;

    case k_user_osc_param_id6:
        // Param6: LFO2 amount (0..100)
        envlfo_set_lfo_amount(&g_mod_state, (int8_t)value);
        break;

    case k_user_osc_param_shape:
        // Shape: wavetable index
        // 10 bit value (UQ6.4) mapped to Q7.24
        g_osc_params.nwave = value << 20;
        break;

    case k_user_osc_param_shiftshape:
        // Shift+Shape: phase skew
        // breakpoint = 64 - (value/16)
        set_skew(&g_gen_state, (1024UL - (uint32_t)value) << 21); // UQ7.25
        break;

    default:
        break;
    }
}
