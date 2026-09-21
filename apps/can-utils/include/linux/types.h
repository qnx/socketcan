#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

/*
 * Thunk through to QNX types
 */
#include <stdbool.h>
#include <stdint.h>
#include <sys/neutrino.h>
#include <sys/types.h>

typedef uint64_t __u64;
typedef uint32_t __u32;
typedef uint16_t __u16;
typedef uint8_t  __u8;
typedef int64_t  __s64;
typedef int32_t  __s32;
typedef int16_t  __s16;
typedef int8_t   __s8;
typedef __u64 u64;
typedef __u32 u32;
typedef __u16 u16;
typedef __u8  u8;
typedef __s64 s64;
typedef __s32 s32;
typedef __s16 s16;
typedef __s8  s8;

#define _SIZE_T
#define _CADDR_T
#define _SSIZE_T
#define _PTRDIFF_T

typedef clockid_t __kernel_clockid_t;

#endif /* _LINUX_TYPES_H */
