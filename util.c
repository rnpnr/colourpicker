/* See LICENSE for copyright details */
#ifndef _UTIL_C_
#define _UTIL_C_
#include <emmintrin.h>
#include <immintrin.h>

#include <stddef.h>
#include <stdint.h>

#include "shader_inc.h"
#include "lora_sb_0_inc.h"
#include "lora_sb_1_inc.h"
#include "config.h"

#ifndef asm
#define asm __asm__
#endif

#ifdef _DEBUG
#define ASSERT(c) do { if (!(c)) asm("int3; nop"); } while (0);
#define DEBUG_EXPORT
#else
#define ASSERT(c)
#define DEBUG_EXPORT static
#endif

typedef uint8_t   u8;
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

enum colour_mode {
	CM_RGB  = 0,
	CM_HSV  = 1,
	CM_LAST
};

enum colour_picker_mode {
	CPM_PICKER   = 0,
	CPM_SLIDERS  = 1,
	CPM_LAST
};

enum colour_picker_flags {
	CPF_REFILL_TEXTURE = 1 << 0,
};

enum input_indices {
	INPUT_HEX,
	INPUT_R,
	INPUT_G,
	INPUT_B,
	INPUT_A
};

enum mouse_pressed {
	MOUSE_NONE   = 0 << 0,
	MOUSE_LEFT   = 1 << 0,
	MOUSE_RIGHT  = 1 << 1,
};

enum cardinal_direction { NORTH, EAST, SOUTH, WEST };

#define WINDOW_ASPECT_RATIO    (4.3f/3.2f)

#define BUTTON_HOVER_SPEED     8.0f

#define SLIDER_BORDER_COLOUR   (Color){.r = 0x00, .g = 0x00, .b = 0x00, .a = 0xCC}
#define SLIDER_BORDER_WIDTH    3.0f
#define SLIDER_ROUNDNESS       0.035f
#define SLIDER_SCALE_SPEED     8.0f
#define SLIDER_SCALE_TARGET    1.5f
#define SLIDER_TRI_SIZE        (v2){.x = 6, .y = 8}

#define SELECTOR_BORDER_COLOUR SLIDER_BORDER_COLOUR
#define SELECTOR_BORDER_WIDTH  SLIDER_BORDER_WIDTH
#define SELECTOR_ROUNDNESS     0.3f

#define RECT_BTN_BORDER_WIDTH  (SLIDER_BORDER_WIDTH + 3.0f)

#define TEXT_HOVER_SPEED       5.0f

typedef struct {
	f32 hover_t;
} ButtonState;

#define COLOUR_STACK_ITEMS 8
typedef struct {
	ButtonState buttons[COLOUR_STACK_ITEMS];
	v4  items[COLOUR_STACK_ITEMS];
	v4  last;
	i32 widx;
	f32 fade_param;
	f32 y_off_t;
	ButtonState tri_btn;
} ColourStackState;

typedef struct {
	f32 scale_t[4];
	f32 colour_t[4];
} SliderState;

typedef struct {
	f32 hex_hover_t;
	ButtonState mode;
} StatusBarState;

typedef struct {
	ButtonState buttons[CPM_LAST];
	f32 mode_visible_t;
	i32 next_mode;
} ModeChangeState;

typedef struct {
	f32 scale_t[3];
	f32 base_hue;
	f32 fractional_hue;
} PickerModeState;

typedef struct {
	i32  idx;
	i32  cursor;
	f32  cursor_hover_p;
	f32  cursor_t;
	f32  cursor_t_target;
	i32  buf_len;
	char buf[64];
} InputState;

typedef struct {
	v4 colour, previous_colour;
	ColourStackState colour_stack;

	uv2 window_size;
	v2  window_pos;
	v2  mouse_pos;

	f32 dt;

	Font font;
	i32  font_size;
	Color bg, fg;

	InputState      is;
	ModeChangeState mcs;
	PickerModeState pms;
	SliderState     ss;
	StatusBarState  sbs;
	ButtonState     buttons[2];

	i32 held_idx;

	f32 selection_hover_t[2];
	v4  hover_colour;
	v4  cursor_colour;

	Shader picker_shader;
	RenderTexture slider_texture;
	RenderTexture picker_texture;

	i32 mode_id, colour_mode_id, colours_id;
	i32 regions_id, size_id;
	i32 radius_id, border_thick_id;

	u32  flags;
	enum colour_mode        colour_mode;
	enum colour_picker_mode mode;
} ColourPickerCtx;

#define ARRAY_COUNT(a) (sizeof(a) / sizeof(*a))
#define ABS(x)         ((x) < 0 ? (-x) : (x))
#define MIN(a, b)      ((a) < (b) ? (a) : (b))
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

	__m128 zero = _mm_set1_ps(0);
	__m128 six  = _mm_set1_ps(6);

	_Alignas(16) f32 aval[4]  = {0, 2, 4, 0};
	_Alignas(16) f32 scale[4] = {1.0/6.0f, 0, 0, 0};
	/* NOTE if C == 0 then take H as 0/1 (which are equivalent in HSV) */
	__m128 t    = _mm_div_ps(_mm_sub_ps(gbra, brga), C);
	t           = _mm_and_ps(t, _mm_cmpneq_ps(zero, C));
	t           = _mm_add_ps(t, _mm_load_ps(aval));
	/* TODO: does (G - B) / C ever exceed 6.0? */
	/* NOTE: Compute fmodf on element [0] */
	t           = _mm_sub_ps(t, _mm_mul_ps(_mm_floor_ps(_mm_mul_ps(t, _mm_load_ps(scale))), six));

	__m128 H = _mm_div_ps(_mm_and_ps(t, _mm_cmpeq_ps(rgba, Max)), six);
	__m128 S = _mm_div_ps(C, Max);

	/* NOTE: Make sure H & S are 0 instead of NaN when V == 0 */
	H = _mm_and_ps(H, _mm_cmpneq_ps(zero, Max));
	S = _mm_and_ps(S, _mm_cmpneq_ps(zero, Max));

	__m128 H0 = _mm_shuffle_ps(H, H, _MM_SHUFFLE(3, 0, 0, 0));
	__m128 H1 = _mm_shuffle_ps(H, H, _MM_SHUFFLE(3, 1, 1, 1));
	__m128 H2 = _mm_shuffle_ps(H, H, _MM_SHUFFLE(3, 2, 2, 2));
	H         = _mm_or_ps(H0, _mm_or_ps(H1, H2));

	/* NOTE: keep only element [0] from H vector; Max contains V & A */
	__m128 hva  = _mm_blend_ps(Max, H, 0x01);
	__m128 hsva = _mm_blend_ps(hva, S, 0x02);
	hsva        = _mm_min_ps(hsva, _mm_set1_ps(1));

	v4 res;
	_mm_storeu_ps(res.E, hsva);
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

static u32
parse_hex_u32(char *s)
{
	u32 res = 0;

	/* NOTE: skip over '0x' or '0X' */
	if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X'))
		s += 2;

	for (; *s; s++) {
		res <<= 4;
		if (ISDIGIT(*s)) {
			res |= *s - '0';
		} else if (ISHEX(*s)) {
			/* NOTE: convert to lowercase first then convert to value */
			*s  |= 0x20;
			res |= *s - 0x57;
		} else {
			/* NOTE: do nothing (treat invalid value as 0) */
		}
	}
	return res;
}

static f32
parse_f32(char *s)
{
	return atof(s);
}

#endif /* _UTIL_C_ */
