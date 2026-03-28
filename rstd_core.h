#ifndef RSTD_CORE_H
#define RSTD_CORE_H

/////////////////////////
// NOTE: Standard Macros
#define function      static
#define global        static
#define local_persist static

#ifndef asm
  #define asm __asm__
#endif

#ifndef typeof
  #define typeof __typeof__
#endif

#define alignof       _Alignof
#define static_assert _Static_assert

#define countof(a) (sizeof(a) / sizeof(*a))

#define arg_list(type, ...) (type []){__VA_ARGS__}, sizeof((type []){__VA_ARGS__}) / sizeof(type)

#define Abs(a)           ((a) < 0 ? (-a) : (a))
#define Between(x, a, b) ((x) >= (a) && (x) <= (b))
#define Clamp(x, a, b)   ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define Clamp01(a)       Clamp(a, 0, 1)
#define Min(a, b)        ((a) < (b) ? (a) : (b))
#define Max(a, b)        ((a) > (b) ? (a) : (b))

#define IsDigit(c)       (Between((c), '0', '9'))

#ifdef _DEBUG
  #define assert(c) do { if (!(c)) debugbreak(); } while (0)
#else  /* !_DEBUG */
  #define assert(c)
#endif /* !_DEBUG */

#define InvalidCodePath    assert(0)
#define InvalidDefaultCase default:{ assert(0); }break

////////////////////////
// NOTE: Core Functions

#if COMPILER_MSVC

function force_inline u32
clz_u32(u32 a)
{
	u32 result = 32, index;
	if (a) {
		_BitScanReverse(&index, a);
		result = index;
	}
	return result;
}

function force_inline u32
ctz_u32(u32 a)
{
	u32 result = 32, index;
	if (a) {
		_BitScanForward(&index, a);
		result = index;
	}
	return result;
}

function force_inline u64
clz_u64(u64 a)
{
	u64 result = 64, index;
	if (a) {
		_BitScanReverse64(&index, a);
		result = index;
	}
	return result;
}

function force_inline u64
ctz_u64(u64 a)
{
	u64 result = 64, index;
	if (a) {
		_BitScanForward64(&index, a);
		result = index;
	}
	return result;
}

#else /* !COMPILER_MSVC */

function force_inline u32
clz_u32(u32 a)
{
	u32 result = 32;
	if (a) result = (u32)__builtin_clz(a);
	return result;
}

function force_inline u32
ctz_u32(u32 a)
{
	u32 result = 32;
	if (a) result = (u32)__builtin_ctz(a);
	return result;
}

function force_inline u64
clz_u64(u64 a)
{
	u64 result = 64;
	if (a) result = (u64)__builtin_clzll(a);
	return result;
}

function force_inline u64
ctz_u64(u64 a)
{
	u64 result = 64;
	if (a) result = (u64)__builtin_ctzll(a);
	return result;
}

#endif /* !COMPILER_MSVC */

function void *
memory_clear(void *restrict destination, u8 byte, s64 size)
{
	u8 *p = destination;
	while (size > 0) p[--size] = byte;
	return p;
}

function void
memory_copy(void *restrict destination, void *restrict source, s64 size)
{
	u8 *s = source, *d = destination;
	for (; size > 0; size--) *d++ = *s++;
}

function force_inline s64
round_up_to(s64 value, s64 multiple)
{
	s64 result = value;
	if (value % multiple != 0)
		result += multiple - value % multiple;
	return result;
}

// NOTE: from Hacker's Delight
function force_inline u64
round_down_power_of_two(u64 a)
{
	u64 result = 0x8000000000000000ULL >> clz_u64(a);
	return result;
}

function force_inline u64
round_up_power_of_two(u64 a)
{
	u64 result = 0x8000000000000000ULL >> (clz_u64(a - 1) - 1);
	return result;
}


//////////////////////////
// NOTE: String Functions

function str8
str8_from_c_str(char *s)
{
	str8 result = {.data = (u8 *)s};
	if (s) {
		while (*s) s++;
		result.length = (u8 *)s - result.data;
	}
	return result;
}

#endif /* RSTD_CORE_H */
