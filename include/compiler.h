
/* some compiler features; from klibc and gcc docs */

#ifndef _COMPILER_H
#define _COMPILER_H

/* likely/unlikely */
#if defined(__GNUC__) && (__GNUC_MAJOR__ > 2 || (__GNUC_MAJOR__ == 2 && __GNUC_MINOR__ >= 95))
# define likely(x)   __builtin_expect((x), 1)
# define unlikely(x) __builtin_expect((x), 0)
#else
# define likely(x)   (x)
# define unlikely(x) (x)
#endif

#endif


