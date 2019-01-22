#pragma once
#include <cstdint>
#include <cstdio>

#define NTH_BIT(x, n) (((x) >> (n)) & 1)

#define DEBUG(...) printf(__VA_ARGS__)

/* Integer type shortcuts */
typedef uint8_t  u8;  typedef int8_t  s8;
typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32;
typedef uint64_t u64; typedef int64_t s64;
