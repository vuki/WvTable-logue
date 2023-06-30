#pragma once
#ifndef _COMPAT_H
#define _COMPAT_H

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
#define _INLINE static inline
#endif // #ifndef NO_FORCE_INLINE

#endif
