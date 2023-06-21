
#include "userosc.h"
#include "wtgen.h"

typedef struct {
    uint16_t pitch;
    float frequency;
    int32_t shape_lfo;
    float sub_freq_ratio;
    struct {
        uint16_t main_wave;
        uint16_t sub_wave;
        uint16_t wavetable;
        uint16_t sub_mix;
        uint16_t sub_detune;
        uint16_t env_attack;
        uint16_t env_decay;
        uint16_t env_amount;
    } saved, newpar;
} OscParams;

static OscParams g_osc_params;
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
    wtgen_set_frequency(&g_gen_state, freq);
    wtgen_set_sub_frequency(&g_gen_state, freq * 0.5f); // # TEMP - apply detune
    g_osc_params.pitch = pitch;
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
    (void)platform;
    (void)api;
    wtgen_init(&g_gen_state, k_samplerate, OVS_NONE);
    // TEST envelope
    // g_gen_state.wave_env_arate = 5.208333333333333e-06f; // 4s
    // g_gen_state.wave_env_drate = -5.208333333333333e-06f; // 4s
    // g_gen_state.wave_env_amount = 64.f;
}

void OSC_CYCLE(const user_osc_param_t* const params, int32_t* yn, const uint32_t frames)
{
    (void)params;

    // check for pitch change
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }

    // wave modulation
    float mod_rate = 0;
    if (g_gen_state.wave_env_stage == ENV_S) {
        // LFO
        const float mod_lfo = (float)params->shape_lfo * 2.9802322387695312e-08f; // normalize to -64..64
        const float denom = (frames == 64) ? 0.015625f : (frames == 32 ? 0.03125f : 1.f / frames);
        mod_rate = (mod_lfo - g_gen_state.wave_mod) * denom;
    }

    // sample generation
    q31_t* __restrict py = (q31_t*)yn;
    const q31_t* py_e = py + frames;
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
        // sample generation
        *(py++) = f32_to_q31(wt_generate(&g_gen_state));
    }
}

void OSC_NOTEON(const user_osc_param_t* const params)
{
    (void)params;
    g_gen_state.osc[0].phase = 0;
    g_gen_state.osc[1].phase = 0;
    g_gen_state.wave_mod = 0;
    g_gen_state.wave_env_value = 0;
    if (g_gen_state.wave_env_arate != 0) {
        g_gen_state.wave_env_stage = ENV_A;
    } else if (g_gen_state.wave_env_drate != 0) {
        g_gen_state.wave_env_stage = ENV_D;
        g_gen_state.wave_env_value = 1.f;
        g_gen_state.wave_mod = g_gen_state.wave_env_value * g_gen_state.wave_env_amount;
    } else {
        g_gen_state.wave_env_stage = ENV_S;
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
        // Param1: main osc wave number
        g_osc_params.newpar.main_wave = value;
        g_gen_state.osc[0].req_wave = (float)value;
        break;

    case k_user_osc_param_id2:
        // Param2: sub osc wave number
        g_osc_params.newpar.sub_wave = value;
        g_gen_state.osc[1].req_wave = (float)value;
        break;

    case k_user_osc_param_id3:
        // Param 3: wavetable number
        g_osc_params.newpar.wavetable = value;
        wtgen_set_wavetable(&g_gen_state, (uint8_t)value);
        break;

    case k_user_osc_param_id4:
        break;

    case k_user_osc_param_id5:
        break;

    case k_user_osc_param_id6:
        break;

    case k_user_osc_param_shape:
        // Shape: sub oscillator mix
        g_osc_params.newpar.sub_mix = value;
        g_gen_state.sub_mix = (float)value * 0.0009765625f; // 1/1024
        break;

    case k_user_osc_param_shiftshape:
        break;

    default:
        break;
    }
}
