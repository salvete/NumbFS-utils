// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2025, Hongzhen Luo
 */

#ifndef __NUMBFS_UTILS_H
#define __NUMBFS_UTILS_H

#include <assert.h>

#define NUMBFS_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2 * !!(condition)]))
#define BUG_ON(cond)        assert(!(cond))

#define BITS_PER_BYTE       8
#define BYTES_PER_BLOCK     512

#define __round_mask(x, y)      ((__typeof__(x))((y)-1))
#define round_up(x, y)          ((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y)        ((x) & ~__round_mask(x, y))

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))

#if __BYTE_ORDER == __LITTLE_ENDIAN
/*
 * The host byte order is the same as network byte order,
 * so these functions are all just identity.
 */
#define cpu_to_le16(x) ((__u16)(x))
#define cpu_to_le32(x) ((__u32)(x))
#define cpu_to_le64(x) ((__u64)(x))
#define le16_to_cpu(x) ((__u16)(x))
#define le32_to_cpu(x) ((__u32)(x))
#define le64_to_cpu(x) ((__u64)(x))

#else
#if __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le16(x) (__builtin_bswap16(x))
#define cpu_to_le32(x) (__builtin_bswap32(x))
#define cpu_to_le64(x) (__builtin_bswap64(x))
#define le16_to_cpu(x) (__builtin_bswap16(x))
#define le32_to_cpu(x) (__builtin_bswap32(x))
#define le64_to_cpu(x) (__builtin_bswap64(x))
#else
#pragma error
#endif
#endif

#endif
