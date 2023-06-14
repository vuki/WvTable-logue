
#include "userosc.h"
#include "wtdef.h"

typedef struct {
    uint16_t pitch;
    float frequency;
    int32_t shape_lfo;
    float sub_freq_ratio;
    struct {
        uint16_t osc_wave;
        uint16_t sub_wave;
        uint16_t wavetable;
        uint16_t sub_mix;
        uint16_t sub_detune;
        uint16_t env_attack;
        uint16_t env_decay;
        uint16_t env_amount;
    } saved, newpar;
} OscParams;

// Wavetable generator state
typedef struct {
    float nwave; // normalized wave number, 0..64
    float set_wave; // last wave number (raw) that was set
    float req_wave; // wave number (raw) that the caller wants to set
    uint8_t ntable; // wavetable number that was set, 0..30
    uint8_t wtn; // currently used wavetable: base or upper
    uint8_t use_upper; // if not 0, use upper wavetable at waves 64..127
    uint8_t wt_def_pos; // position of wave definition for stored waves
    uint8_t wave1; // number of the first stored wave (before the wave)
    uint8_t wave2; // number of the second stored wave (after the wave)
    uint8_t retro_mode; // use 'retro mode' for sample generation (no interpolation)
    float alpha_w; // linear interpolation coefficient
    float phase; // signal phase, 0..128
    float step; // step to increase the phase
} WtState;

typedef struct {
    WtState osc;
    float srate;
    uint8_t ovs_mode;
    float phase_scaler;
} WtGenState;

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
    g_gen_state.osc.step = g_gen_state.phase_scaler * freq;
    g_osc_params.pitch = pitch;
}

__fast_inline float generate(WtGenState* __restrict state)
{
    float alpha[2];
    float y = 0;
    uint8_t k;
    uint8_t ipos = (uint8_t)state->osc.phase;
    alpha[1] = state->osc.phase - ipos;
    alpha[0] = 1.f - alpha[1];
    const uint8_t* p_wave = &WAVES[state->osc.wave1][0];
    for (k = 0; k < 2; k++, ipos++) {
        if (alpha[k])
            y += alpha[k] * (int8_t)(((ipos & 0x40) ? (~p_wave[~ipos & 0x3F]) : (p_wave[ipos & 0x3F])) ^ 0x80);
    }
    state->osc.phase += state->osc.step;
    if (state->osc.phase > 128.f)
        state->osc.phase -= 128.f;
    return y * 0.0078125; // y/128
}

void OSC_INIT(uint32_t platform, uint32_t api)
{
    (void)platform;
    (void)api;

    g_gen_state.srate = k_samplerate;
    g_gen_state.phase_scaler = 128.f / g_gen_state.srate;
    g_gen_state.osc.wave1 = 54;
    g_gen_state.osc.wave2 = 55;
    g_gen_state.osc.step = 0;
    g_osc_params.pitch = 0;
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
        float sig = generate(&g_gen_state);
        *(py++) = f32_to_q31(sig);
    }
}

void OSC_NOTEON(const user_osc_param_t* const params)
{
    (void)params;
    g_gen_state.osc.phase = 0;
}

void OSC_NOTEOFF(const user_osc_param_t* const params)
{
    (void)params;
}

void OSC_PARAM(uint16_t index, uint16_t value)
{

    switch (index) {
    case k_user_osc_param_id1:
        break;

    case k_user_osc_param_id2:
        break;

    case k_user_osc_param_id3:
        break;

    case k_user_osc_param_id4:
        break;

    case k_user_osc_param_id5:
        break;

    case k_user_osc_param_id6:
        break;

    case k_user_osc_param_shape:
        break;

    case k_user_osc_param_shiftshape:
        break;

    default:
        break;
    }
}
