#pragma once
#ifndef _COMPAT_H
#define _COMPAT_H

/*
    Compatibility definitions.
    Include this header after any SDK headers,
    but before any custom headers.
*/

#ifndef NO_FORCE_INLINE
#if defined(__GNUC__)
#ifndef __clang__
#define _INLINE static inline __attribute__((always_inline, optimize("Ofast")))
#else // #ifdef __clang__
#define _INLINE static inline __attribute__((always_inline))
#endif // #ifndef __clang__
#elif defined(_MSC_VER)
#define _INLINE static inline __forceinline
#else
#define _INLINE static inline
#endif // #ifdef __GNUC__
#else // #ifndef NO_FORCE_INLINE
#define _INLINE
#endif // #ifndef NO_FORCE_INLINE

/*
    Fixed math Q types
*/

// 32-bit types
typedef long q7_24_t; // Q7.24 signed
typedef unsigned long uq7_25_t; // Q7.25 unsigned

#endif
