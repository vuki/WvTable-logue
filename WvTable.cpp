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
#include "detune.h"

#ifdef OVS_2x
#include "decimator.h"
DecimatorState decimator;
#endif

static struct {
    uint16_t pitch;
    float frequency;
    float sub_freq_ratio;
    struct {
        uint16_t env_attack;
        uint16_t env_decay;
        uint16_t env_amount;
    } saved, newpar;
    uint8_t int_wavenum;
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
    set_main_frequency(&g_gen_state, freq);
    set_sub_frequency(&g_gen_state, freq * g_osc_params.sub_freq_ratio);
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
#if defined(OVS_4x)
    wtgen_init(&g_gen_state, 4 * k_samplerate);
#elif defined(OVS_2x)
    wtgen_init(&g_gen_state, 2 * k_samplerate);
    decimator_reset(&decimator);
#else
    wtgen_init(&g_gen_state, k_samplerate);
#endif
    g_osc_params.pitch = 0;
    g_osc_params.frequency = 0;
    g_osc_params.sub_freq_ratio = 1.f;
    g_osc_params.saved.env_amount = g_osc_params.newpar.env_amount = 0;
    g_osc_params.saved.env_attack = g_osc_params.newpar.env_attack = 0;
    g_osc_params.saved.env_decay = g_osc_params.newpar.env_decay = 0;
    g_osc_params.int_wavenum = 0;
}

void OSC_CYCLE(const user_osc_param_t* const params, int32_t* framebuf, const uint32_t nframes)
{
    // check for pitch change
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }

    // wave modulation
    float mod_rate = 0;
    if (g_gen_state.wave_env_stage == ENV_S) {
        // LFO
        const float mod_lfo = (float)params->shape_lfo * 2.9802322387695312e-08f; // normalize to -64..64
        const float denom = (nframes == 64) ? 0.015625f : (nframes == 32 ? 0.03125f : 1.f / nframes);
        mod_rate = (mod_lfo - g_gen_state.wave_mod) * denom;
    }

    // sample generation
    q31_t* __restrict py = (q31_t*)framebuf;
    const q31_t* py_e = py + nframes;
    for (; py != py_e;) {
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

        // update wave number
        const float nwave_main = g_gen_state.osc[0].req_wave + g_gen_state.wave_mod;
        if (nwave_main != g_gen_state.osc[0].set_wave) {
            set_wave_number(&g_gen_state.osc[0], nwave_main);
        }
        const float nwave_sub = g_gen_state.osc[1].req_wave + g_gen_state.wave_mod;
        if (nwave_sub != g_gen_state.osc[1].set_wave) {
            set_wave_number(&g_gen_state.osc[1], nwave_sub);
        }

#if defined(OVS_4x)
        static float yk[4];
        uint8_t k;
        for (k = 0; k < 4; k++) {
            yk[k] = generate(&g_gen_state);
        }
        *(py++) = f32_to_q31(0.25f * (yk[0] + yk[1] + yk[2] + yk[3]));
#elif defined(OVS_2x)
        *(py++) = f32_to_q31(generate_ovs(&g_gen_state, &decimator));
#else
        // sample generation
        *(py++) = f32_to_q31(generate(&g_gen_state));
#endif
    }
}

#if 0
void OSC_CYCLE2(const user_osc_param_t* const params, int32_t* framebuf, const uint32_t nframes)
{
    // Test function, fixed point

    // check for pitch change
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }

    static int32_t yk[4];
    static int32_t yn[2];
    uint8_t k, n;
    const uint8_t nwave[4] = { 79, 51, 52, 53 };
    WtGenState* state = &g_gen_state;
    const int32_t alphaw_1 = 0x800; // 0.5 in Q12
    const int32_t alphaw_2 = 0x666; // 0.4 in Q12
    const int32_t sub_mix = 0x999; // 0.6 in Q12

    q31_t* __restrict py = (q31_t*)framebuf;
    const q31_t* py_e = py + nframes;
    for (; py != py_e;) {

        for (n = 0; n < 2; n++) {
            for (k = 0; k < 4; k++) {
                const uint8_t nosc = k >> 1; // 0->0, 1->0, 2->1, 3->1
                const uint32_t phase = state->osc[nosc].phase;
                const uint8_t nwavek = nwave[k];
                const int32_t alpha = (int32_t)(phase & 0x1ffffff) >> 1; // Q24
                const uint8_t pos1 = (uint8_t)(phase >> 25); // UQ7
                const uint8_t pos2 = (pos1 + 1) & 0x7f; // UQ7
                const uint8_t* const pwave = WAVES[nwavek];
                const int8_t val1 = (int8_t)(((pos1 & 0x40) ? (~pwave[~pos1 & 0x3F]) : (pwave[pos1])) ^ 0x80);
                const int8_t val2 = (int8_t)(((pos2 & 0x40) ? (~pwave[~pos2 & 0x3F]) : (pwave[pos2])) ^ 0x80);
                yk[k] = (0x1000000 - alpha) * val1 + alpha * val2; // Q7.24
            }
            // wave interpolation
            yk[0] = (0x1000 - alphaw_1) * (yk[0] >> 12) + alphaw_1 * (yk[1] >> 12);
            yk[1] = (0x1000 - alphaw_2) * (yk[2] >> 12) + alphaw_2 * (yk[3] >> 12);
            // osc interpolation
            yn[n] = (0x1000 - sub_mix) * (yk[0] >> 12) + sub_mix * (yk[1] >> 12);
            state->osc[0].phase += state->osc[0].step;
            state->osc[1].phase += state->osc[1].step;
        }
#if defined(OVS_2x)
        const float ydec = decimator_do(&decimator, q31_to_f32(yn[0]), q31_to_f32(yn[1]));
        *(py++) = f32_to_q31(ydec);
#else
        *(py++) = (yn[0] >> 1) + (yn[1] >> 1); // TEST
#endif
    }
}
#endif

void OSC_NOTEON(const user_osc_param_t* const params)
{
    g_gen_state.osc[0].phase = 0;
    g_gen_state.osc[1].phase = 0;
    g_gen_state.wave_mod = 0;
    g_gen_state.wave_env_value = 0;

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
            g_osc_params.int_wavenum = 0;
        } else if (value < 91) {
            set_wavetable(&g_gen_state, (uint8_t)(value - 61));
            g_osc_params.int_wavenum = 1;
        }
        break;

    case k_user_osc_param_id2:
        // Param2: sub osc mix
        g_gen_state.sub_mix = (value <= 100) ? (value * 0.01f) : 0;
        break;

    case k_user_osc_param_id3:
        // Param 3: sub osc detune
        g_osc_params.sub_freq_ratio = DETUNE_TABLE[(value <= 200) ? value : 0];
        set_sub_frequency(&g_gen_state, g_osc_params.frequency * g_osc_params.sub_freq_ratio);
        break;

    case k_user_osc_param_id4:
        // Param4: wave envelope attack time (0..100)
        g_osc_params.newpar.env_attack = value;
        break;

    case k_user_osc_param_id5:
        // Param5: wave envelope decay time (0..100)
        g_osc_params.newpar.env_decay = value;
        break;

    case k_user_osc_param_id6:
        // Param6: wave envelope amount (1..200)
        g_osc_params.newpar.env_amount = value;
        break;

    case k_user_osc_param_shape:
        // Shape: main osc wave number
        g_gen_state.osc[0].req_wave = (g_osc_params.int_wavenum) ? ((float)(value >> 3)) : (value * 0.125f);
        break;

    case k_user_osc_param_shiftshape:
        // Shift+Shape: sub osc wave number
        g_gen_state.osc[1].req_wave = (g_osc_params.int_wavenum) ? ((float)(value >> 3)) : (value * 0.125f);
        break;

    default:
        break;
    }
}
