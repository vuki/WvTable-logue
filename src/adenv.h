#pragma once
#ifndef _ADENV_H
#define _ADENV_H

/*
 * adenv
 * Simple Attack-Decay envelope
 * Author: Grzegorz Szwoch (GregVuki)
 */

#include <stdint.h>
#include "compat.h"

#define ENV_MAX 0x80000000
#define ENV_MASK_VAL (ENV_MAX - 1)

typedef enum { ENV_S, ENV_A, ENV_D } EnvStage;

typedef struct {
    EnvStage stage;
    uint32_t val; // Q31
    uint32_t arate;
    uint32_t drate;
    float sample_rate;
} ADEnvState;

/*  adenv_init
    Initialize AD envelope
    srate: sampling rate in Hz
*/
_INLINE void adenv_init(ADEnvState* state, float srate)
{
    state->stage = ENV_S;
    state->val = 0;
    state->arate = ENV_MAX;
    state->drate = ENV_MAX;
    state->sample_rate = srate;
}

/*  adenv_reset
    Reset the envelope state
*/
_INLINE void adenv_reset(ADEnvState* state)
{
    state->stage = ENV_S;
    state->val = 0;
}

/*  adenv_set_atime
    Set attack time
    atime: attack time in seconds
*/
_INLINE void adenv_set_atime(ADEnvState* state, float atime)
{
    if (atime > 0)
        state->arate = (uint32_t)((float)ENV_MAX / (state->sample_rate * atime) + 0.5f);
    else
        state->arate = ENV_MAX;
}

/*  adenv_set_dtime
    Set decay time
    dtime: decay time in seconds
*/
_INLINE void adenv_set_dtime(ADEnvState* state, float dtime)
{
    if (dtime > 0)
        state->drate = (uint32_t)((float)ENV_MAX / (state->sample_rate * dtime) + 0.5f);
    else
        state->drate = ENV_MAX;
}

/*  adenv_set_arate
    Set attack rate
    arate: attack rate (Q31)
*/
_INLINE void adenv_set_arate(ADEnvState* state, uint32_t arate)
{
    state->arate = arate;
}

/*  adenv_set_drate
    Set decay rate
    drate: decay rate (Q31)
*/
_INLINE void adenv_set_drate(ADEnvState* state, uint32_t drate)
{
    state->drate = drate;
}

/*  adenv_note_on
    Trigger the envelope
*/
_INLINE void adenv_note_on(ADEnvState* state)
{
    if (state->stage != ENV_S) {
        state->stage = ENV_A; // retrigger
        return;
    }
    if (state->arate < ENV_MAX) {
        state->stage = ENV_A;
        state->val = 0;
    } else if (state->drate < ENV_MAX) {
        state->stage = ENV_D;
        state->val = ENV_MAX - 1;
    } else {
        state->stage = ENV_S;
        state->val = 0;
    }
}

/*  adenv_note_off
    Trigger the envelope decay
*/
_INLINE void adenv_note_off(ADEnvState* state)
{
    if (state->stage == ENV_A) {
        state->stage = ENV_D;
    }
}

/*  adenv_is_active
    Return 1 if envelope is active, 0 if inactive
*/
_INLINE int adenv_is_active(const ADEnvState* state)
{
    return state->stage == ENV_S ? 0 : 1;
}

/*  adenv_get
    Generate and return envelope value.
    Returns: envelope value, UQ31 (0..1)
*/
_INLINE uint32_t adenv_get(ADEnvState* state)
{
    if (state->stage == ENV_S) {
        return 0; // not active
    }
    if (state->stage == ENV_A) {
        state->val += state->arate;
        if (state->val & ENV_MAX) {
            // overflow caused by going above 1
            state->val = ENV_MAX - (state->val & ENV_MASK_VAL) - 1;
            state->stage = ENV_D;
        }
    } else if (state->stage == ENV_D) {
        state->val -= state->drate;
    }
    if (state->val & ENV_MAX) {
        // overflow caused by going below zero
        state->val = 0;
        state->stage = ENV_S;
    }
    return state->val & ENV_MASK_VAL;
}

#endif
