
#include "userosc.h"

void OSC_INIT(uint32_t platform, uint32_t api)
{
    (void)platform;
    (void)api;
}

void OSC_CYCLE(const user_osc_param_t* const params, int32_t* yn, const uint32_t frames)
{
    (void)params;
    q31_t* __restrict y = (q31_t*)yn;
    const q31_t* y_e = y + frames;

    for (; y != y_e;) {

        float sig = 0;
        *(y++) = f32_to_q31(sig);
    }
}

void OSC_NOTEON(const user_osc_param_t* const params)
{
    (void)params;
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
