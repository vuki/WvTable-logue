#pragma once
#ifndef _WTDEF_H
#define _WTDEF_H

/*
 * wtdef
 * Definition of PPG waves and wavetables
 * extracted from PPG WAVE 2.3 ROM
 */

#define NWAVES 204
#define WT_BASE 27
#define WT_SYNC 28
#define WT_STEP 29
#define WT_UPPER 30
#define WV_TRIANGLE 60
#define WV_PULSE 61
#define WV_SQUARE 62
#define WV_SAW 63
#define LAST_MEM_WAVE 59.f
#define STANDARD_WAVES 60.f
#define STD_TRIANGLE NWAVES
#define STD_PULSE (NWAVES + 1)
#define STD_SQUARE (NWAVES + 2)
#define STD_SAW (NWAVES + 3)

/*
 * Definition of waves (from PPG WAVE 2.3 ROM).
 * Rows: waves. Index: wave mumber (a reduced set of 204 waves).
 * Columns: wave samples. 64 samples per wave. Only the first half of the period.
 * The second half is calculated by reflecting the first half in both directions.
 * Sample values are 8 bit unsigned int.
 */
extern const unsigned char WAVES[NWAVES][64];

/*
 * Indices into table of positions.
 * Index: wavetable number, 0 to 30.
 * Values: offset into WT_POS table (start of the wavetable).
 */
extern const unsigned short WT_IDX[];

/*
 * Wavetable definition. Positions and indices of waves read directly from memory.
 * Values are pairs:
 * - first item: wavetable slot (0 to 60),
 * - second item: wave number, from WAVES table.
 */
extern const unsigned char WT_POS[][2];

/*
 * Scalers for wave interpolation.
 * WSCALER[i] = 1.f / (i+1)
 */
extern const float WSCALER[32];

#endif
