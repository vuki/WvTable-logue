/*
 * WvTable.cpp
 * Wavetable generator inspired by PPG Wave.
 * Main file for logue SDK plugin.
 * Author: Grzegorz Szwoch (GregVuki)
 */

// #define OVS_2x
// #define OVS_4x

#include "userosc.h"
#include "wtgen.h"

#define FORCE_WAVE_RELOAD 0xffffffff

static struct {
    float frequency;
    uint32_t base_nwave;
    uint32_t set_wavenum;
    struct {
        uint16_t nwave_int; // 10 bit
        uint16_t nwave_frac; // 10 bit
        uint16_t env_attack;
        uint16_t env_decay;
        uint16_t env_amount;
    } saved, newpar;
    uint16_t pitch;
} g_osc_params;

static WtGenState g_gen_state;

__fast_inline void update_frequency(uint16_t pitch)
{
    const uint16_t note = pitch >> 8;
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

__fast_inline float calc_envelope_rate(uint16_t par)
{
    // convert parameter to time in seconds
    if (par == 0)
        return 1.f;
    float tenv = 0;
    if (par <= 34)
        tenv = (float)par * 0.01470588235294118f; // (0.5 * v / 34)
    else if (par <= 68)
        tenv = (float)(par - 34) * 0.0588235294117647f + 0.5f; // (2 * (v-34) / 34 + 0.5)
    else
        tenv = (float)(par - 68) * 0.25f + 2.5f; // (0.25 * (v-68) + 2.5)
    // calculate the envelope rate = 1 / (fs * t)
    // envelope operates at 48000 Hz, not oversampled
    return 1.f / (k_samplerate * tenv);
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
    (void)platform;
    (void)api;
    wtgen_init(&g_gen_state, k_samplerate);
    g_osc_params.frequency = 0;
    g_osc_params.base_nwave = 0;
    g_osc_params.set_wavenum = FORCE_WAVE_RELOAD;
    g_osc_params.saved.nwave_int = g_osc_params.newpar.nwave_int = 0;
    g_osc_params.saved.nwave_frac = g_osc_params.newpar.nwave_frac = 0;
    g_osc_params.saved.env_amount = g_osc_params.newpar.env_amount = 0;
    g_osc_params.saved.env_attack = g_osc_params.newpar.env_attack = 0;
    g_osc_params.saved.env_decay = g_osc_params.newpar.env_decay = 0;
    g_osc_params.pitch = 0;
}

void OSC_CYCLE(const user_osc_param_t* const params, int32_t* framebuf, const uint32_t nframes)
{
    // check for pitch change
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }

    // base wave number
    if ((g_osc_params.newpar.nwave_int != g_osc_params.saved.nwave_int)
        || (g_osc_params.newpar.nwave_frac != g_osc_params.saved.nwave_frac)) {

        // integer part: round 10-bit val to UQ7, convert to UQ7.25
        // fractional part: Q10 to Q25
        g_osc_params.base_nwave = ((g_osc_params.newpar.nwave_int >> 3) << 25) | (g_osc_params.newpar.nwave_frac << 15);
        g_osc_params.saved.nwave_int = g_osc_params.newpar.nwave_int;
        g_osc_params.saved.nwave_frac = g_osc_params.newpar.nwave_frac;
    }

#if 0 // TODO
    // wave modulation
    // const uint32_t bit_shift = 31 - __CLZ(nframes); // log2(nframes)
    float mod_rate = 0;
    if (g_gen_state.wave_env_stage == ENV_S) {
        // LFO
        const float mod_lfo = (float)params->shape_lfo * 2.9802322387695312e-08f; // normalize to -64..64
        const float denom = (nframes == 64) ? 0.015625f : (nframes == 32 ? 0.03125f : 1.f / nframes);
        mod_rate = (mod_lfo - g_gen_state.wave_mod) * denom;
    }
#endif

    // sample generation
    q31_t* __restrict py = (q31_t*)framebuf;
    const q31_t* py_e = py + nframes;
    for (; py != py_e;) {
#if 0 // TODO
      // wave modulation
        if (g_gen_state.wave_env_stage == ENV_S) {
            // LFO
            g_gen_state.wave_mod += mod_rate;
        } else if (g_gen_state.wave_env_stage == ENV_A) {
            // envelope in attack stage
            g_gen_state.wave_env_value += g_gen_state.wave_env_arate;
            if (g_gen_state.wave_env_value > 1.f) {
                g_gen_state.wave_env_value = 2.f - g_gen_state.wave_env_value;
                g_gen_state.wave_env_stage = ENV_D;
            }
            g_gen_state.wave_mod = g_gen_state.wave_env_value * g_gen_state.wave_env_amount;
        } else if (g_gen_state.wave_env_stage == ENV_D) {
            // envelope in decay stage
            g_gen_state.wave_env_value += g_gen_state.wave_env_drate;
            if (g_gen_state.wave_env_value < 0.f) {
                g_gen_state.wave_env_value = 0;
                g_gen_state.wave_env_stage = ENV_S;
            }
            g_gen_state.wave_mod = g_gen_state.wave_env_value * g_gen_state.wave_env_amount;
        }
#endif

        // update wave number
        // TODO: add modulation (beware of overflow!)
        const uint32_t nwave_new = g_osc_params.base_nwave; // + g_gen_state.wave_mod;
        if (nwave_new != g_osc_params.set_wavenum) {
            set_wave_number(&g_gen_state.osc, nwave_new);
            g_osc_params.set_wavenum = nwave_new;
        }

        // sample generation
        *(py++) = f32_to_q31(generate(&g_gen_state));
    }
}

void OSC_NOTEON(const user_osc_param_t* const params)
{
    wtgen_reset(&g_gen_state);

    // wave envelope times
    if (g_osc_params.newpar.env_attack != g_osc_params.saved.env_attack) {
        g_gen_state.wave_env_arate = calc_envelope_rate(g_osc_params.newpar.env_attack);
        g_osc_params.saved.env_attack = g_osc_params.newpar.env_attack;
    }
    if (g_osc_params.newpar.env_decay != g_osc_params.saved.env_decay) {
        g_gen_state.wave_env_drate = -calc_envelope_rate(g_osc_params.newpar.env_decay);
        g_osc_params.saved.env_decay = g_osc_params.newpar.env_decay;
    }
    // wave envelope amount
    if (g_osc_params.newpar.env_amount != g_osc_params.saved.env_amount) {
        // scale 0..200 to -64..64 (value 0 is 0 amount - logue bug)
        const uint32_t ea = g_osc_params.newpar.env_amount;
        g_gen_state.wave_env_amount = (ea > 0) ? (((float)ea - 100.f) * 0.64f) : 0;
        g_osc_params.saved.env_amount = g_osc_params.newpar.env_amount;
    }
    // set envelope stage
    if (g_osc_params.saved.env_attack != 0) {
        g_gen_state.wave_env_stage = ENV_A;
    } else if (g_osc_params.saved.env_decay != 0) {
        g_gen_state.wave_env_stage = ENV_D;
        g_gen_state.wave_env_value = 1.f;
        g_gen_state.wave_mod = g_gen_state.wave_env_value * g_gen_state.wave_env_amount;
    } else {
        g_gen_state.wave_env_stage = ENV_S;
    }

    // check for pitch change
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }
}

void OSC_NOTEOFF(const user_osc_param_t* const params)
{
    (void)params;
}

void OSC_PARAM(uint16_t index, uint16_t value)
{

    switch (index) {
    case k_user_osc_param_id1:
        // Param 1: wavetable number
        if (value < 61) {
            set_wavetable(&g_gen_state, (uint8_t)value);
            g_osc_params.set_wavenum = FORCE_WAVE_RELOAD;
        }
        break;

    case k_user_osc_param_id2:
        // Param2: wave envelope attack time (0..100)
        g_osc_params.newpar.env_attack = value;
        break;

    case k_user_osc_param_id3:
        // Param3: wave envelope decay time (0..100)
        g_osc_params.newpar.env_decay = value;
        break;

    case k_user_osc_param_id4:
        // Param4: wave envelope amount (1..200)
        g_osc_params.newpar.env_amount = value;
        break;

    case k_user_osc_param_shape:
        // Shape: integer part of the wave number
        g_osc_params.newpar.nwave_int = value;
        break;

    case k_user_osc_param_shiftshape:
        // Shift+Shape: fractional part of the wave number
        g_osc_params.newpar.nwave_frac = value;
        break;

    default:
        break;
    }
}
