#pragma once
#ifndef _WTDEF_H
#define _WTDEF_H

/*
 * wtdef.h
 * Definition of PPG waves and wavetables
 * extracted from PPG WAVE 2.3 ROM
 */

#define NWAVES 204
#define WT_SYNC 28
#define WT_STEP 29
#define WT_UPPER 30
#define WAVE_SYNC (NWAVES + 1)
#define WAVE_STEP (NWAVES + 2)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Definition of waves (from PPG WAVE 2.3 ROM).
 * Rows: waves. Index: wave mumber (a reduced set of 204 waves).
 * Columns: wave samples. 64 samples per wave. Only the first half of the period.
 * The second half is calculated by reflecting the first half in both directions.
 * Sample values are 8 bit unsigned int.
 */
extern const unsigned char WAVES[NWAVES][64];

/*
 * Wavetable definition. A 2D table.
 * First index: wavetable number 1..27 or 30.
 * Second index: position in the wavetable definition.
 * Wavetable definition values are pairs:
 * - first item: wavetable slot (0 to 60),
 * - second item: wave number, from WAVES table.
 */
extern const unsigned char* WAVETABLES[];

/*
 * Scalers for wave interpolation.
 * WSCALER[i] = 1.f / (i+1)
 */
extern const float WSCALER[32];

/*
 * For wavetable 28 (sync): number of position changes that cause a sync.
 * Index: wave number.
 */
extern const unsigned char WT28_SPAN[];

#ifdef __cplusplus
}
#endif

#endif
