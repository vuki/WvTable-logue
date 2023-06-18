
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
}

void OSC_CYCLE(const user_osc_param_t* const params, int32_t* yn, const uint32_t frames)
{
    (void)params;

    // check for pitch change
    if (params->pitch != g_osc_params.pitch) {
        update_frequency(params->pitch);
    }

    // sample generation
    q31_t* __restrict py = (q31_t*)yn;
    const q31_t* py_e = py + frames;
    for (; py != py_e;) {
        float sig = wt_generate(&g_gen_state);
        *(py++) = f32_to_q31(sig);
    }
}

void OSC_NOTEON(const user_osc_param_t* const params)
{
    (void)params;
    g_gen_state.osc[0].phase = 0;
    g_gen_state.osc[1].phase = 0;
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
        wtgen_set_wave(&g_gen_state, (float)value);
        break;

    case k_user_osc_param_id2:
        // Param2: sub osc wave number
        g_osc_params.newpar.sub_wave = value;
        wtgen_set_sub_wave(&g_gen_state, (float)value);
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
