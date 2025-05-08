/* See LICENSE for copyright details */
#ifndef _UTIL_C_
#define _UTIL_C_
#include <stddef.h>
#include <stdint.h>

typedef uint8_t   u8;
typedef int32_t   s32;
typedef uint32_t  u32;
typedef uint32_t  b32;
typedef int64_t   s64;
typedef uint64_t  u64;
typedef float     f32;
typedef double    f64;
typedef ptrdiff_t sz;

#define function      static
#define global        static
#define local_persist static

#include "lora_sb_0_inc.h"
#include "lora_sb_1_inc.h"
#include "config.h"

/* NOTE(rnp): symbols in release builds shaders are embedded in the binary */
#ifndef _DEBUG
extern char _binary_slider_lerp_glsl_start[];
#endif

#ifndef asm
#define asm __asm__
#endif

#define FORCE_INLINE inline __attribute__((always_inline))

#define fmod_f32(a, b) __builtin_fmodf((a), (b))

#ifdef __ARM_ARCH_ISA_A64
#define debugbreak() asm volatile ("brk 0xf000")

function FORCE_INLINE u64
rdtsc(void)
{
	register u64 cntvct asm("x0");
	asm volatile ("mrs x0, cntvct_el0" : "=x"(cntvct));
	return cntvct;
}

#elif __x86_64__
#include <immintrin.h>
#define debugbreak() asm volatile ("int3; nop")

#define rdtsc() __rdtsc()
#endif

#ifdef _DEBUG
#define ASSERT(c) do { if (!(c)) debugbreak(); } while (0);
#define DEBUG_EXPORT
#else
#define ASSERT(c)
#define DEBUG_EXPORT function
#endif

typedef struct { sz len; u8 *data; } str8;
#define str8(s) (str8){.len = sizeof(s) - 1, .data = (u8 *)s}

typedef struct {
	u8  *data;
	u32 cap;
	u32 widx;
	s32 fd;
	b32 errors;
} Stream;

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
	CPF_PRINT_DEBUG    = 1 << 30,
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
	s32 widx;
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
	s32 next_mode;
} ModeChangeState;

typedef struct {
	f32 scale_t[3];
	f32 base_hue;
	f32 fractional_hue;
} PickerModeState;

typedef struct {
	s32  idx; /* TODO(rnp): remove */
	s32  count;
	s32  cursor;
	f32  cursor_hover_p; /* TODO(rnp): remove */
	f32  cursor_t;
	f32  cursor_t_target; /* TODO(rnp): remove */
	u8   buf[64];
} TextInputState;

typedef struct {
	v4 colour, previous_colour;
	ColourStackState colour_stack;

	uv2 window_size;
	v2  window_pos;
	v2  mouse_pos;

	Font  font;
	Color bg, fg;

	TextInputState  text_input_state;
	ModeChangeState mcs;
	PickerModeState pms;
	SliderState     ss;
	StatusBarState  sbs;
	ButtonState     buttons[2];

	s32 held_idx;

	f32 selection_hover_t[2];
	v4  hover_colour;
	v4  cursor_colour;

	Shader picker_shader;
	RenderTexture slider_texture;
	RenderTexture picker_texture;

	s32 mode_id, colour_mode_id, colours_id;
	s32 regions_id, radius_id, border_thick_id;

	u32  flags;
	enum colour_mode        colour_mode;
	enum colour_picker_mode mode;
} ColourPickerCtx;

#define countof(a) (s64)(sizeof(a) / sizeof(*a))

#define ABS(x)         ((x) < 0 ? (-x) : (x))
#define MIN(a, b)      ((a) < (b) ? (a) : (b))
#define MAX(a, b)      ((a) < (b) ? (b) : (a))
#define CLAMP(x, a, b) ((x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define CLAMP01(a)     CLAMP(a, 0, 1)

#define ISDIGIT(a)     ((a) >= '0' && (a) <= '9')
#define ISHEX(a)       (ISDIGIT(a) || ((a) >= 'a' && (a) <= 'f') || ((a) >= 'A' && (a) <= 'F'))

function Color
rl_colour_from_normalized(v4 colour)
{
	Color result;
	result.r = colour.r * 255;
	result.g = colour.g * 255;
	result.b = colour.b * 255;
	result.a = colour.a * 255;
	return result;
}

function v4
rgb_to_hsv(v4 rgb)
{
	v4 hsv = {0};
	f32 M = MAX(rgb.r, MAX(rgb.g, rgb.b));
	f32 m = MIN(rgb.r, MIN(rgb.g, rgb.b));
	if (M - m > 0) {
		f32 C = M - m;
		if (M == rgb.r) {
			hsv.x = fmod_f32((rgb.g - rgb.b) / C, 6) / 6.0;
		} else if (M == rgb.g) {
			hsv.x = ((rgb.b - rgb.r) / C + 2) / 6.0;
		} else {
			hsv.x = ((rgb.r - rgb.g) / C + 4) / 6.0;
		}
		hsv.y = M? C / M : 0;
		hsv.z = M;
		hsv.a = rgb.a;
	}
	return hsv;
}

function v4
hsv_to_rgb(v4 hsv)
{
	v4 rgba;
	f32 k  = fmod_f32(5 + hsv.x * 6, 6);
	rgba.r = hsv.z - hsv.z * hsv.y * MAX(0, MIN(1, MIN(k, 4 - k)));
	k      = fmod_f32(3 + hsv.x * 6, 6);
	rgba.g = hsv.z - hsv.z * hsv.y * MAX(0, MIN(1, MIN(k, 4 - k)));
	k      = fmod_f32(1 + hsv.x * 6, 6);
	rgba.b = hsv.z - hsv.z * hsv.y * MAX(0, MIN(1, MIN(k, 4 - k)));
	rgba.a = hsv.a;
	return rgba;
}

function v4
normalize_colour(u32 rgba)
{
	v4 result;
	result.r = ((rgba >> 24) & 0xFF) / 255.0f;
	result.g = ((rgba >> 16) & 0xFF) / 255.0f;
	result.b = ((rgba >>  8) & 0xFF) / 255.0f;
	result.a = ((rgba >>  0) & 0xFF) / 255.0f;
	return result;
}

function u32
pack_rl_colour(Color colour)
{
	return colour.r << 24 | colour.g << 16 | colour.b << 8 | colour.a << 0;
}

function u32
parse_hex_u32(str8 s)
{
	u32 res = 0;

	/* NOTE: skip over '0x' or '0X' */
	if (s.len > 2 && s.data[0] == '0' && (s.data[1] == 'x' || s.data[1] == 'X')) {
		s.data += 2;
		s.len  -= 2;
	}

	for (; s.len > 0; s.len--, s.data++) {
		res <<= 4;
		if (ISDIGIT(*s.data)) {
			res |= *s.data - '0';
		} else if (ISHEX(*s.data)) {
			/* NOTE: convert to lowercase first then convert to value */
			*s.data |= 0x20;
			res     |= *s.data - 0x57;
		} else {
			/* NOTE: do nothing (treat invalid value as 0) */
		}
	}
	return res;
}

function f64
parse_f64(str8 s)
{
	f64 integral = 0, fractional = 0, sign = 1;

	if (s.len && *s.data == '-') {
		sign *= -1;
		s.data++;
		s.len--;
	}

	while (s.len && ISDIGIT(*s.data)) {
		integral *= 10;
		integral += *s.data - '0';
		s.data++;
		s.len--;
	}

	if (s.len && *s.data == '.') { s.data++; s.len--; }

	while (s.len) {
		ASSERT(s.data[s.len - 1] != '.');
		fractional *= 0.1f;
		fractional += (s.data[--s.len] - '0') * 0.1f;
	}

	f64 result = sign * (integral + fractional);

	return result;
}

function str8
str8_from_c_str(char *s)
{
	str8 result = {.data = (u8 *)s};
	if (s) {
		while (*s) s++;
		result.len = (u8 *)s - result.data;
	}
	return result;
}

function void
stream_append_byte(Stream *s, u8 b)
{
	s->errors |= s->widx + 1 > s->cap;
	if (!s->errors)
		s->data[s->widx++] = b;
}

function void
stream_append_hex_u8(Stream *s, u32 n)
{
	local_persist u8 hex[16] = {"0123456789abcdef"};
	s->errors |= (s->cap - s->widx) < 2;
	if (!s->errors) {
		s->data[s->widx + 1] = hex[(n >> 0) & 0x0f];
		s->data[s->widx + 0] = hex[(n >> 4) & 0x0f];
		s->widx += 2;
	}
}

function void
stream_append_str8(Stream *s, str8 str)
{
	s->errors |= (s->cap - s->widx) < str.len;
	if (!s->errors) {
		for (sz i = 0; i < str.len; i++)
			s->data[s->widx++] = str.data[i];
	}
}

function void
stream_append_u64(Stream *s, u64 n)
{
	u8 tmp[64];
	u8 *end = tmp + sizeof(tmp);
	u8 *beg = end;
	do { *--beg = '0' + (n % 10); } while (n /= 10);
	stream_append_str8(s, (str8){.len = end - beg, .data = beg});
}

function void
stream_append_f64(Stream *s, f64 f, s64 prec)
{
	if (f < 0) {
		stream_append_byte(s, '-');
		f *= -1;
	}

	/* NOTE: round last digit */
	f += 0.5f / prec;

	if (f >= (f64)(-1UL >> 1)) {
		stream_append_str8(s, str8("inf"));
	} else {
		u64 integral = f;
		u64 fraction = (f - integral) * prec;
		stream_append_u64(s, integral);
		stream_append_byte(s, '.');
		for (u64 i = prec / 10; i > 1; i /= 10) {
			if (i > fraction)
				stream_append_byte(s, '0');
		}
		stream_append_u64(s, fraction);
	}
}

function void
stream_append_colour(Stream *s, Color c)
{
	stream_append_hex_u8(s, c.r);
	stream_append_hex_u8(s, c.g);
	stream_append_hex_u8(s, c.b);
	stream_append_hex_u8(s, c.a);
}

#endif /* _UTIL_C_ */
