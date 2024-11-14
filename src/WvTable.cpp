/*
 * WvTable.cpp
 * Wavetable generator inspired by PPG Wave.
 * Main file for logue SDK plugin.
 * Author: Grzegorz Szwoch (GregVuki)
 */

#define OVS_2x
// #define OVS_4x

#include "userosc.h"
#include "wtgen.h"
#include "adenv.h"
#include "decimator.h"

#define FORCE_WAVE_RELOAD 0xffffffff
#define MASK_WAVE_UPPER 0xffc00000
#define MASK_WAVE_LOWER 0x3ff000
#define WTNUM_FLAG 0x8000
#define WTNUM_MASK 0x7f

struct {
    float frequency;
    uint32_t base_nwave;
    uint32_t set_wavenum;
    int32_t nwave_mod;
    int32_t env_amount;
    int32_t lfo2_phase;
    int32_t lfo2_step;
    int32_t lfo2_amount;
    uint16_t wt_num;
    uint16_t pitch;
} g_osc_params;

WtGenState g_gen_state;
ADEnvState g_env_state;

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

extern const uint32_t ENV_LUT[];

__fast_inline void update_frequency(uint16_t pitch)
{
    const uint8_t note = pitch >> 8;
    const uint16_t mod = pitch & 0xFF;
    float freq = osc_notehzf(note);
    if (mod > 0) {
        const float f1 = osc_notehzf(note + 1);
        freq = clipmaxf(linintf(mod * k_note_mod_fscale, freq, f1), k_note_max_hz);
    }
    set_frequency(&g_gen_state, freq);
    g_osc_params.pitch = pitch;
    g_osc_params.frequency = freq;
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
    (void)platform;
    (void)api;
    wtgen_init(&g_gen_state, k_samplerate * OVS);
    adenv_init(&g_env_state, k_samplerate);
    g_osc_params.frequency = 0;
    g_osc_params.base_nwave = 0;
    g_osc_params.set_wavenum = FORCE_WAVE_RELOAD;
    g_osc_params.nwave_mod = 0;
    g_osc_params.env_amount = 0;
    g_osc_params.wt_num = WTNUM_FLAG;
    g_osc_params.pitch = 0;
#if defined(OVS_4x)
    decimator_reset(&g_decimator);
    decimator_reset(&g_decimator2);
#elif defined(OVS_2x)
    decimator_reset(&g_decimator);
#endif
}

__fast_inline int32_t generate_sample(int32_t mod_rate)
{
    // wave modulation
    int32_t nwave_mod = 0; // Q7.24
    if (g_env_state.stage == ENV_S) {
        // use main LFO
        g_osc_params.nwave_mod += mod_rate;
        nwave_mod = g_osc_params.nwave_mod;
        if (g_osc_params.lfo2_amount) {
            // use LFO2 (triangle)
            g_osc_params.lfo2_phase += g_osc_params.lfo2_step;
            const int32_t m = g_osc_params.lfo2_phase >> 31;
            int32_t v = (g_osc_params.lfo2_phase ^ m) - m;
            v = (v - int32_t(1 << 30)); // Q30
            nwave_mod += (v >> 6) * g_osc_params.lfo2_amount;
        }
    } else {
        // use envelope
        const uint32_t env_val = adenv_get(&g_env_state); // UQ31
        nwave_mod = (int32_t)(env_val >> 7) * g_osc_params.env_amount; // Q7.24
    }

    // update wave number
    uint32_t nwave_new = g_osc_params.base_nwave; // UQ7.25
    if (nwave_mod != 0) {
        // convert base wave number UQ7.25 to Q7.24 (>>1),
        // add modulation (possible overflow),
        // rescale back (<<1).
        nwave_new = (uint32_t)((((int32_t)nwave_new >> 1) + nwave_mod) << 1);
    }
    if (nwave_new != g_osc_params.set_wavenum) {
        // set_wave_number(&g_gen_state, nwave_new);
        set_wave_number(&g_gen_state, (int32_t)(nwave_new >> 1)); // FIXME !!!
        g_osc_params.set_wavenum = nwave_new;
    }

    // sample generation
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

    return (int32_t)(y * 16777215.f + 0.5f); // float (-128..128) to Q31
}

void OSC_CYCLE(const user_osc_param_t* const params, int32_t* framebuf, const uint32_t nframes)
{
    // check for wavetable change
    if (g_osc_params.wt_num & WTNUM_FLAG) {
        g_osc_params.wt_num &= WTNUM_MASK;
        set_wavetable(&g_gen_state, (uint8_t)g_osc_params.wt_num);
        g_osc_params.set_wavenum = FORCE_WAVE_RELOAD;
#ifndef _MSC_VER
        // skip samples calculation for this frame
        uint32_t i;
        for (i = 0; i < nframes; i++)
            framebuf[i] = 0;
        return;
#endif
    }

    // check for pitch change
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }

    // wave modulation
    int32_t mod_rate = 0; // Q7.24
    if (g_env_state.stage == ENV_S) {
        // use LFO; shape_lfo: Q7.24
        const uint32_t bit_shift = 31 - __CLZ(nframes); // 31-log2(nframes)
        mod_rate = (params->shape_lfo - g_osc_params.nwave_mod) >> bit_shift;
    }

    // sample generation
    q31_t* __restrict py = (q31_t*)framebuf;
    const q31_t* py_e = py + nframes;
    for (; py != py_e;) {
        *(py++) = generate_sample(mod_rate);
    }
}

void OSC_NOTEON(const user_osc_param_t* const params)
{
    wtgen_reset(&g_gen_state);
    g_osc_params.nwave_mod = 0;
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }
    adenv_note_on(&g_env_state);
    g_osc_params.lfo2_phase = (1 << 30);
#if defined(OVS_4x)
    decimator_reset(&g_decimator);
    decimator_reset(&g_decimator2);
#elif defined(OVS_2x)
    decimator_reset(&g_decimator);
#endif
}

void OSC_NOTEOFF(const user_osc_param_t* const params)
{
    (void)params;
    adenv_note_off(&g_env_state);
}

void OSC_PARAM(uint16_t index, uint16_t value)
{

    switch (index) {
    case k_user_osc_param_id1:
        // Param 1: wavetable number
        if (value < 61) {
            g_osc_params.wt_num = value | WTNUM_FLAG;
        }
        break;

    case k_user_osc_param_id2:
        // Param2: wave envelope attack time (0..100)
        adenv_set_arate(&g_env_state, ENV_LUT[value]);
        break;

    case k_user_osc_param_id3:
        // Param3: wave envelope decay time (0..100)
        adenv_set_drate(&g_env_state, ENV_LUT[value]);
        break;

    case k_user_osc_param_id4:
        // Param4: wave envelope amount (1..200)
        // param (1..200) to amnount (-99..100)
        // ignore 0 value - logue bug
        g_osc_params.env_amount = (value > 0) ? ((int32_t)value - 100) : 0;
        break;

    case k_user_osc_param_id5:
        // Param5: LFO2 rate (0..100)
        float rate;
        // exponential function approximation with 3 linear segments
        if (value < 50)
            rate = 0.04f * value;
        else if (value < 80)
            rate = 0.23f * (value - 50.f) + 2.f;
        else
            rate = 0.6f * (value - 80.f) + 8.9f;
        g_osc_params.lfo2_step = (int32_t)(0.5f + (1 << 30) * rate * k_samplerate_recipf) << 2;
        break;

    case k_user_osc_param_id6:
        // Param6: LFO2 amount (0..100)
        g_osc_params.lfo2_amount = value;
        break;

    case k_user_osc_param_shape:
        // Shape: wave number with 1/8 resolution
        g_osc_params.base_nwave = (g_osc_params.base_nwave & MASK_WAVE_LOWER) | (value << 22);
        break;

    case k_user_osc_param_shiftshape:
        // Shift+Shape: phase skew
        // breakpoint = 64 - (value/16)
        set_skew(&g_gen_state, (uint32_t(1024) - (uint32_t)value) << 21);
        break;

    default:
        break;
    }
}

// Envelope LUT: parameter (0.100) to rate (Q31)
// tau = 0.1 * exp(0.046 * par)
const uint32_t ENV_LUT[101] = { 0x80000000, 0x6850f, 0x63a05, 0x5f25b, 0x5adea, 0x56c8c, 0x52e1f, 0x4f280, 0x4b990,
    0x4832f, 0x44f40, 0x41da6, 0x3ee47, 0x3c10a, 0x395d5, 0x36c91, 0x34529, 0x31f86, 0x2fb94, 0x2d940, 0x2b876, 0x29927,
    0x27b3f, 0x25eb0, 0x24369, 0x2295d, 0x2107c, 0x1f8ba, 0x1e209, 0x1cc5d, 0x1b7aa, 0x1a3e6, 0x19105, 0x17efe, 0x16dc6,
    0x15d54, 0x14da0, 0x13ea0, 0x1304d, 0x1229f, 0x1158e, 0x10913, 0xfd28, 0xf1c7, 0xe6e8, 0xdc87, 0xd29c, 0xc924,
    0xc019, 0xb777, 0xaf37, 0xa756, 0x9fd1, 0x98a1, 0x91c5, 0x8b37, 0x84f5, 0x7efa, 0x7945, 0x73d1, 0x6e9c, 0x69a3,
    0x64e3, 0x605a, 0x5c05, 0x57e2, 0x53ef, 0x5029, 0x4c8e, 0x491d, 0x45d4, 0x42b0, 0x3fb0, 0x3cd3, 0x3a17, 0x377b,
    0x34fc, 0x329a, 0x3054, 0x2e28, 0x2c15, 0x2a19, 0x2835, 0x2666, 0x24ac, 0x2306, 0x2173, 0x1ff2, 0x1e82, 0x1d23,
    0x1bd4, 0x1a93, 0x1962, 0x183e, 0x1727, 0x161c, 0x151e, 0x142b, 0x1342, 0x1265, 0x1191 };