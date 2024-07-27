/* See LICENSE for copyright details */
#ifndef _UTIL_C_
#define _UTIL_C_
#include <emmintrin.h>
#include <immintrin.h>

#include <stddef.h>
#include <stdint.h>

#ifdef _DEBUG
#define ASSERT(c) do { if (!(c)) asm("int3; nop"); } while (0);
#define DEBUG_EXPORT
#else
#define ASSERT(c)
#define DEBUG_EXPORT static
#endif

typedef int32_t   i32;
typedef uint32_t  u32;
typedef uint32_t  b32;
typedef float     f32;
typedef double    f64;
typedef ptrdiff_t size;

typedef union {
	struct { u32 w, h; };
	struct { u32 x, y; };
	u32 E[2];
} uv2;

typedef union {
	struct { f32 x, y; };
	struct { f32 w, h; };
	Vector2 rv;
	f32 E[2];
} v2;

typedef union {
	struct { f32 x, y, z, w; };
	struct { f32 r, g, b, a; };
	Vector4 rv;
	f32 E[4];
} v4;

typedef union {
	struct { v2 pos, size; };
	Rectangle rr;
} Rect;

enum colour_picker_mode {
	CPM_RGB,
	CPM_HSV,
	CPM_LAST
};

enum colour_picker_flags {
	CPF_REFILL_TEXTURE = 1 << 0,
};

#define STACK_ROUNDNESS    0.5f
#define COLOUR_STACK_ITEMS 8
typedef struct {
	v4  last;
	v4  items[COLOUR_STACK_ITEMS];
	f32 scales[COLOUR_STACK_ITEMS];
	i32 widx;
	f32 fade_param;
	f32 yoff;
} ColourStackState;

typedef struct {
	v4 colour, previous_colour;
	ColourStackState colour_stack;

	Font font;
	i32  font_size;
	uv2 window_size;
	Color bg, fg;

	f32 selection_hover_t[2];
	v4  hover_colour;

	v2 mouse_pos;

	f32 dt;

	RenderTexture hsv_texture;

	u32  flags;
	enum colour_picker_mode mode;
} ColourPickerCtx;

#define ARRAY_COUNT(a) (sizeof(a) / sizeof(*a))
#define ABS(x)         ((x) < 0 ? (-x) : (x))
#define CLAMP(x, a, b) ((x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define CLAMP01(a)     CLAMP(a, 0, 1)

#define ISDIGIT(a)     ((a) >= '0' && (a) <= '9')
#define ISHEX(a)       (ISDIGIT(a) || ((a) >= 'a' && (a) <= 'f') || ((a) >= 'A' && (a) <= 'F'))

static Color
colour_from_normalized(v4 colour)
{
	__m128 colour_v = _mm_loadu_ps(colour.E);
	__m128 scale    = _mm_set1_ps(255.0f);
	__m128i result  = _mm_cvtps_epi32(_mm_mul_ps(colour_v, scale));
	_Alignas(16) u32 outu[4];
	_mm_store_si128((__m128i *)outu, result);
	return (Color){.r = outu[0] & 0xFF, .g = outu[1] & 0xFF, .b = outu[2] & 0xFF, .a = outu[3] & 0xFF };
}

static v4
rgb_to_hsv(v4 rgb)
{
	__m128 rgba = _mm_loadu_ps(rgb.E);
	__m128 gbra = _mm_shuffle_ps(rgba, rgba, _MM_SHUFFLE(3, 0, 2, 1));
	__m128 brga = _mm_shuffle_ps(gbra, gbra, _MM_SHUFFLE(3, 0, 2, 1));

	__m128 Max  = _mm_max_ps(rgba, _mm_max_ps(gbra, brga));
	__m128 Min  = _mm_min_ps(rgba, _mm_min_ps(gbra, brga));
	__m128 C    = _mm_sub_ps(Max, Min);

	__m128 t = _mm_div_ps(_mm_sub_ps(gbra, brga), C);

	_Alignas(16) f32 aval[4] = { 0, 2, 4, 0 };
	t = _mm_add_ps(t, _mm_load_ps(aval));

	/* TODO: does (G - B) / C ever exceed 6.0? */
	/* NOTE: 1e9 ensures that the remainder after floor is 0.
	 * This limits the fmodf to apply only to element [0] */
	_Alignas(16) f32 div[4] = { 6, 1e9, 1e9, 1e9 };
	__m128 six = _mm_set1_ps(6);
	__m128 rem = _mm_floor_ps(_mm_div_ps(t, _mm_load_ps(div)));
	t = _mm_sub_ps(t, _mm_mul_ps(rem, six));

	__m128 zero    = _mm_set1_ps(0);
	__m128 maxmask = _mm_cmpeq_ps(rgba, Max);

	__m128 H = _mm_div_ps(_mm_blendv_ps(zero, t, maxmask), six);
	__m128 S = _mm_div_ps(C, Max);

	/* NOTE: Make sure H & S are 0 instead of NaN when V == 0 */
	__m128 zeromask = _mm_cmpeq_ps(zero, Max);
	H = _mm_blendv_ps(H, zero, zeromask);
	S = _mm_blendv_ps(S, zero, zeromask);

	__m128 H0 = _mm_shuffle_ps(H, H, _MM_SHUFFLE(3, 0, 0, 0));
	__m128 H1 = _mm_shuffle_ps(H, H, _MM_SHUFFLE(3, 1, 1, 1));
	__m128 H2 = _mm_shuffle_ps(H, H, _MM_SHUFFLE(3, 2, 2, 2));
	H         = _mm_or_ps(H0, _mm_or_ps(H1, H2));

	/* NOTE: keep only element [0] from H vector; Max contains V & A */
	__m128 hva = _mm_blend_ps(Max, H, 0x01);
	v4 res;
	_mm_storeu_ps(res.E, _mm_blend_ps(hva, S, 0x02));
	return res;
}

static v4
hsv_to_rgb(v4 hsv)
{
	/* f(k(n))   = V - V*S*max(0, min(k, min(4 - k, 1)))
	 * k(n)      = fmod((n + H * 6), 6)
	 * (R, G, B) = (f(n = 5), f(n = 3), f(n = 1))
	 */
	_Alignas(16) f32 nval[4] = {5.0f, 3.0f, 1.0f, 0.0f};
	__m128 n   = _mm_load_ps(nval);
	__m128 H   = _mm_set1_ps(hsv.x);
	__m128 S   = _mm_set1_ps(hsv.y);
	__m128 V   = _mm_set1_ps(hsv.z);
	__m128 six = _mm_set1_ps(6);

	__m128 t   = _mm_add_ps(n, _mm_mul_ps(six, H));
	__m128 rem = _mm_floor_ps(_mm_div_ps(t, six));
	__m128 k   = _mm_sub_ps(t, _mm_mul_ps(rem, six));

	t = _mm_min_ps(_mm_sub_ps(_mm_set1_ps(4), k), _mm_set1_ps(1));
	t = _mm_max_ps(_mm_set1_ps(0), _mm_min_ps(k, t));
	t = _mm_mul_ps(t, _mm_mul_ps(S, V));

	v4 rgba;
	_mm_storeu_ps(rgba.E, _mm_sub_ps(V, t));
	rgba.a = hsv.a;

	return rgba;
}

static v4
normalize_colour(u32 rgba)
{
	return (v4){
		.r = ((rgba >> 24) & 0xFF) / 255.0f,
		.g = ((rgba >> 16) & 0xFF) / 255.0f,
		.b = ((rgba >>  8) & 0xFF) / 255.0f,
		.a = ((rgba >>  0) & 0xFF) / 255.0f,
	};
}

static u32
pack_rl_colour(Color colour)
{
	return colour.r << 24 | colour.g << 16 | colour.b << 8 | colour.a << 0;
}

#endif /* _UTIL_C_ */
