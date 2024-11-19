#pragma once

// Compatibility header used if the code is used outside the logue-sdk

#include <stdint.h>
#include <math.h>

#define __fast_inline static inline

// #ifdef _MSC_VER
// #include <immintrin.h>
// #define __CLZ _lzcnt_u32
// #endif

#define k_note_mod_fscale (0.00392156862745098f)
#define k_note_max_hz (23679.643054f)
#define k_samplerate (48000)
#define k_samplerate_recipf (2.08333333333333e-005f)

typedef uint32_t q31_t;

/**
 * Internal realtime parameters
 */
typedef struct user_osc_param {
    /** Value of LFO implicitely applied to shape parameter */
    int32_t shape_lfo;
    /** Current pitch. high byte: note number, low byte: fine (0-255) */
    uint16_t pitch;
    /** Current cutoff value (0x0000-0x1fff) */
    uint16_t cutoff;
    /** Current resonance value (0x0000-0x1fff) */
    uint16_t resonance;
    uint16_t reserved0[3];
} user_osc_param_t;

/**
 * User facing osc-specific parameters
 */
typedef enum {
    /** Edit parameter 1 */
    k_user_osc_param_id1 = 0,
    /** Edit parameter 2 */
    k_user_osc_param_id2,
    /** Edit parameter 3 */
    k_user_osc_param_id3,
    /** Edit parameter 4 */
    k_user_osc_param_id4,
    /** Edit parameter 5 */
    k_user_osc_param_id5,
    /** Edit parameter 6 */
    k_user_osc_param_id6,
    /** Shape parameter */
    k_user_osc_param_shape,
    /** Alternative Shape parameter: generally available via a shift function */
    k_user_osc_param_shiftshape,
    k_num_user_osc_param_id
} user_osc_param_id_t;

/** Clip upper bound of x to m (inclusive)
 */
__fast_inline float clipmaxf(const float x, const float m)
{
    return (x < m) ? x : m;
}

/** Linear interpolation
 */
__fast_inline float linintf(const float fr, const float x0, const float x1)
{
    return x0 + fr * (x1 - x0);
}

/**
 * Get Hertz value for note
 *
 * @param note Note in [0-151] range.
 * @return     Corresponding Hertz value.
 */
__fast_inline float osc_notehzf(uint8_t note)
{
    // return midi_to_hz_lut_f[clipmaxu32(note,k_midi_to_hz_size-1)];
    return 440.f * powf(2.f, (note - 69.f) * 0.08333333333333333f);
}

// Callbacks
void OSC_INIT(uint32_t platform, uint32_t api);
void OSC_CYCLE(const user_osc_param_t* const params, int32_t* framebuf, const uint32_t nframes);
void OSC_NOTEON(const user_osc_param_t* const params);
void OSC_NOTEOFF(const user_osc_param_t* const params);
void OSC_PARAM(uint16_t index, uint16_t value);
