#pragma once
#ifndef _ENVLFO_H
#define _ENVLFO_H

/*
 * envlfo
 * Simple Attack-Decay or Attack-Sustain-Release envelope,
 * combined with a triangular LFO.
 * Intended for wavetable index modulation.
 * Author: Grzegorz Szwoch (GregVuki)
 */

#include <stdint.h>
#include "compat.h"

#define FIXED_ONE 0x80000000U // 1<<31
#define TRI_SHIFT 0x40000000U // 1<<30

typedef enum { ENV_IDLE, ENV_A, ENV_D, ENV_S } EnvStage;

typedef struct {
    EnvStage stage; // current envelope stage
    q7_24_t out_val; // last computed output value, Q7.24
    uint32_t env_val; // current envelope value, normalized to 0..1, UQ1.31
    uint32_t arate; // envelope rate for the attack stage
    uint32_t drate; // envelope rate for the decay/release stage
    int32_t sus_val; // envelope value in sustain state, Q7.24
    uint32_t lfo_phase; // LFO phase
    uint32_t lfo_step; // LFO phase step
    float sample_rate; // sampling rate, used to compute envelope duration
    int8_t env_amount; // envelope amount (modulation depth)
    int8_t lfo_amount; // LFO amount (modulation depth)
    int8_t decay_scale; // envelope scaler for the decay/release stage
    int8_t hold; // if 1, hold the envelope after the attack, until note off
} EnvLfoState;

/*  envlfo_init
    Initialize AD envelope.
    srate: sampling rate in Hz
*/
_INLINE void envlfo_init(EnvLfoState* state, float srate)
{
    state->stage = ENV_IDLE;
    state->out_val = 0;
    state->env_val = 0;
    state->arate = FIXED_ONE;
    state->drate = FIXED_ONE;
    state->sus_val = 0;
    state->lfo_phase = TRI_SHIFT;
    state->lfo_step = 0;
    state->sample_rate = srate;
    state->env_amount = 0;
    state->lfo_amount = 0;
    state->decay_scale = 0;
    state->hold = 0;
}

/*  envlfo_reset
    Reset the envelope state
*/
_INLINE void envlfo_reset(EnvLfoState* state)
{
    state->stage = ENV_IDLE;
    state->env_val = 0;
    state->out_val = 0;
    state->lfo_phase = TRI_SHIFT; // start the triangle wave LFO going up
}

/*
    Envelope parameters
    -------------------
*/

/*  envlfo_set_hold
    Set the hold mode.
    hold == 0: [note on] attack, decay, lfo; [note off] lfo
    hold == 1: [note on] attack, lfo; [note off] decay
*/
_INLINE void envlfo_set_hold(EnvLfoState* state, int8_t hold)
{
    state->hold = hold;
}

/*  envlfo_set_env_amount
    Set the envelope amount (modulation depth).
    amount: the modulation depth, signed 8-bit integer
*/
_INLINE void envlfo_set_env_amount(EnvLfoState* state, int8_t amount)
{
    state->env_amount = amount;
    if (state->hold)
        state->sus_val = state->env_amount * 0x1000000;
}

/*  envlfo_set_atime
    Set the attack time.
    atime: attack time in seconds
*/
_INLINE void envlfo_set_atime(EnvLfoState* state, float atime)
{
    if (atime > 1e-6)
        state->arate = (uint32_t)((float)FIXED_ONE / (state->sample_rate * atime) + 0.5f);
    else
        state->arate = FIXED_ONE;
}

/*  envlfo_set_dtime
    Set the decay time.
    dtime: decay time in seconds.
*/
_INLINE void envlfo_set_dtime(EnvLfoState* state, float dtime)
{
    if (dtime > 1e-6)
        state->drate = (uint32_t)((float)FIXED_ONE / (state->sample_rate * dtime) + 0.5f);
    else
        state->drate = FIXED_ONE;
}

/*  envlfo_set_arate
    Set the attack rate.
    arate: attack rate (Q31).
*/
_INLINE void envlfo_set_arate(EnvLfoState* state, uint32_t arate)
{
    state->arate = arate;
}

/*  envlfo_set_drate
    Set the decay rate.
    drate: decay rate (Q31).
*/
_INLINE void envlfo_set_drate(EnvLfoState* state, uint32_t drate)
{
    state->drate = drate;
}

/*
    LFO parameters
    --------------
*/

/*  envlfo_set_lfo_amount
    Set the LFO amount (modulation depth). Negative values invert the LFO phase.
    amount: the modulation depth, singed 8-bit integer
*/
_INLINE void envlfo_set_lfo_amount(EnvLfoState* state, int8_t amount)
{
    state->lfo_amount = amount;
}

/*  envlfo_set_lfo_frequency
    Set the LFO freqency.
    freq: modulation frequency, in Hz
*/
_INLINE void envlfo_set_lfo_frequency(EnvLfoState* state, float freq)
{
    state->lfo_step = (uint32_t)((freq / state->sample_rate) * (1 << 31) + 0.5) << 1;
}

/*  envlfo_set_lfo_rate
    Set the LFO rate.
    rate: phase increment per sample.
*/
_INLINE void envlfo_set_lfo_rate(EnvLfoState* state, uint32_t rate)
{
    state->lfo_step = rate;
}

/*
    Signal generation
    -----------------
*/

/*  envlfo_note_on
    Trigger the envelope attack.
*/
_INLINE void envlfo_note_on(EnvLfoState* state)
{
    envlfo_reset(state);
    if (state->arate < FIXED_ONE) {
        state->stage = ENV_A;
        state->env_val = 0;
    } else if (state->hold) {
        state->stage = ENV_S;
        state->env_val = FIXED_ONE;
    } else if (state->drate < FIXED_ONE) {
        state->stage = ENV_D;
        state->env_val = FIXED_ONE;
    } else {
        state->stage = ENV_IDLE;
        state->env_val = 0;
    }
}

/*  envlfo_note_off
    Trigger the envelope decay.
*/
_INLINE void envlfo_note_off(EnvLfoState* state)
{
    if (state->stage == ENV_S) {
        state->stage = ENV_D;
        state->decay_scale = (int8_t)(state->out_val >> 24);
    } else if (state->stage == ENV_A) {
        state->stage = ENV_D;
        state->decay_scale = state->env_amount;
    }
}

/*  envlfo_get
    Generate and return the envelope + LFO value.
    steps: before generating the value, advance the phase by this number of samples.
    Returns: the current value, Q7.24 (-128..127).
*/
_INLINE q7_24_t envlfo_get(EnvLfoState* state, uint32_t steps)
{
    switch (state->stage) {
    case ENV_A:
        // in the attack stage
        state->env_val += state->arate * steps; // advance
        if (state->env_val & FIXED_ONE) {
            // attack finished - overflow caused by going above 1
            if (state->hold) {
                // keep the envelope in the sustain stage
                state->stage = ENV_S;
                state->env_val = FIXED_ONE;
                state->sus_val = state->env_amount * 0x1000000; // *(1<<24)
            } else if (state->drate < FIXED_ONE) {
                // transition to the decay stage
                state->stage = ENV_D;
                state->env_val = FIXED_ONE - (state->env_val - FIXED_ONE);
                state->decay_scale = state->env_amount;
            } else {
                // envelope has finished
                state->stage = ENV_IDLE;
                state->env_val = 0;
            }
        }
        state->out_val = (q7_24_t)(state->env_val >> 7) * state->env_amount;
        break;
    case ENV_D:
        // in the decay or release stage
        state->env_val -= state->drate * steps; // advance
        if (state->env_val & FIXED_ONE) {
            // decay finished - overflow caused by going below 0
            state->stage = state->hold ? ENV_IDLE : ENV_S;
            state->env_val = 0;
            state->sus_val = 0;
            state->out_val = 0;
            // if we are at zero, start the triangle wave LFO going down - shift the initial phase
            state->lfo_phase += TRI_SHIFT << 1;
        } else {
            state->out_val = (q7_24_t)(state->env_val >> 7) * state->decay_scale;
        }
        break;
    case ENV_S: {
        // in the sustain stage - the LFO is active
        // calculate the LFO value: 2 * abs(phase) - 1
        const int32_t x = (int32_t)(state->lfo_phase);
        const int32_t mask = x >> 31;
        const int32_t lfo_val = (x ^ mask) - mask - (int32_t)TRI_SHIFT; // value in Q30
        state->lfo_phase += state->lfo_step * steps;
        // scale the LFO value and add it to the envelope sustain value
        state->out_val = state->sus_val + (lfo_val >> 6) * state->lfo_amount;
    } break;
    default:
        // in the idle stage - the envelope and the LFO are inactive
        state->out_val = 0;
    }
    return state->out_val;
}

#endif
