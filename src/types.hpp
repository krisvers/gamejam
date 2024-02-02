#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <limits>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef size_t usize;
typedef bool b8;

#define U8_MAX std::numeric_limits<u8>::max()
#define U16_MAX std::numeric_limits<u16>::max()
#define U32_MAX std::numeric_limits<u32>::max()
#define U64_MAX std::numeric_limits<u64>::max()
#define S8_MAX std::numeric_limits<s8>::max()
#define S16_MAX std::numeric_limits<s16>::max()
#define S32_MAX std::numeric_limits<s32>::max()
#define S64_MAX std::numeric_limits<s64>::max()
#define F32_MAX std::numeric_limits<f32>::max()
#define F64_MAX std::numeric_limits<f64>::max()
#define USIZE_MAX std::numeric_limits<usize>::max()
#define B8_MAX std::numeric_limits<b8>::max()

#endif
