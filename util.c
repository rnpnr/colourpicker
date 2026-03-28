/* See LICENSE for copyright details */
#ifndef _UTIL_C_
#define _UTIL_C_

#include "rstd_compiler.h"
#include "rstd_intrinsics.h"
#include "rstd_types.h"
#include "rstd_core.h"

#include "lora_sb_0_inc.h"
#include "lora_sb_1_inc.h"
#include "shader_inc.h"
#include "config.h"

#define fmod_f32(a, b) __builtin_fmodf((a), (b))

#if ARCH_ARM64
function force_inline u64
rdtsc(void)
{
	register u64 cntvct asm("x0");
	asm volatile ("mrs x0, cntvct_el0" : "=x"(cntvct));
	return cntvct;
}

#elif ARCH_X64
#define rdtsc() __rdtsc()
#endif

#ifdef _DEBUG
#define DEBUG_EXPORT
#else
#define DEBUG_EXPORT function
#endif

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

typedef enum {
	NumberConversionResult_Invalid,
	NumberConversionResult_OutOfRange,
	NumberConversionResult_Success,
} NumberConversionResult;

typedef enum {
	NumberConversionKind_Invalid,
	NumberConversionKind_Integer,
	NumberConversionKind_Float,
} NumberConversionKind;

typedef struct {
	NumberConversionResult result;
	NumberConversionKind   kind;
	union {
		u64 U64;
		s64 S64;
		f64 F64;
	};
	str8 unparsed;
} NumberConversion;

typedef enum c{
	ColourKind_RGB,
	ColourKind_HSV,
	ColourKind_Last,
} ColourKind;

enum colour_picker_mode {
	CPM_PICKER   = 0,
	CPM_SLIDERS  = 1,
	CPM_LAST
};

typedef enum {
	ColourPickerFlag_Ready         = 1 << 0,
	ColourPickerFlag_RefillTexture = 1 << 1,
	ColourPickerFlag_PrintDebug    = 1 << 30,
} ColourPickerFlags;

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

#define HOVER_SPEED            5.0f

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
	str8 *labels;
	u32   state;
	u32   length;
} Cycler;

typedef struct Variable Variable;
typedef enum {
	InteractionKind_None,
	InteractionKind_Set,
	InteractionKind_Text,
	InteractionKind_Drag,
	InteractionKind_Scroll,
} InteractionKind;

typedef struct {
	Variable *active;
	Variable *hot;
	Variable *next_hot;

	Rect rect;
	Rect hot_rect;

	InteractionKind kind;
} InteractionState;

typedef enum {
	VariableFlag_Text             = 1 << 0,
	VariableFlag_UpdateStoredMode = 1 << 30,
} VariableFlags;

typedef enum {
	VariableKind_F32,
	VariableKind_U32,
	VariableKind_F32Reference,
	VariableKind_Button,
	VariableKind_Cycler,
	VariableKind_HexColourInput,
} VariableKind;

struct Variable {
	union {
		u32 U32;
		f32 F32;
		f32  *F32Reference;
		void *generic;
		Cycler  cycler;
	};
	VariableKind  kind;
	VariableFlags flags;
	f32           parameter;
};

typedef struct {
	Variable colour_kind_cycler;
} SliderModeState;

typedef struct {
	v4 colour, previous_colour;
	ColourStackState colour_stack;

	uv2 window_size;
	v2  window_pos;
	v2  mouse_pos;
	v2  last_mouse;

	Font  font;
	Color bg, fg;

	TextInputState   text_input_state;
	InteractionState interaction;

	ModeChangeState mcs;
	PickerModeState pms;
	SliderState     ss;
	StatusBarState  sbs;
	ButtonState     buttons[2];

	SliderModeState slider_mode_state;

	s32 held_idx;

	f32 selection_hover_t[2];
	v4  hover_colour;
	v4  cursor_colour;

	Shader picker_shader;
	RenderTexture slider_texture;
	RenderTexture picker_texture;

	s32 mode_id, colour_mode_id, colours_id;
	s32 regions_id, radius_id, border_thick_id;

	ColourPickerFlags flags;
	ColourKind        stored_colour_kind;
	enum colour_picker_mode mode;
} ColourPickerCtx;

#define IsHex(a) (IsDigit(a) || Between((a), 'a', 'f') || Between((a), 'A', 'F'))

function v4
rgb_to_hsv(v4 rgb)
{
	v4 hsv = {0};
	f32 M = Max(rgb.r, Max(rgb.g, rgb.b));
	f32 m = Min(rgb.r, Min(rgb.g, rgb.b));
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
	rgba.r = hsv.z - hsv.z * hsv.y * Max(0, Min(1, Min(k, 4 - k)));
	k      = fmod_f32(3 + hsv.x * 6, 6);
	rgba.g = hsv.z - hsv.z * hsv.y * Max(0, Min(1, Min(k, 4 - k)));
	k      = fmod_f32(1 + hsv.x * 6, 6);
	rgba.b = hsv.z - hsv.z * hsv.y * Max(0, Min(1, Min(k, 4 - k)));
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

function v2
add_v2(v2 a, v2 b)
{
	v2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

function NumberConversion
integer_from_str8(str8 raw, b32 hex)
{
	read_only local_persist alignas(64) s8 lut[64] = {
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};

	NumberConversion result = {.unparsed = raw};

	s64 i     = 0;
	s64 scale = 1;
	if (raw.length > 0 && raw.data[0] == '-') {
		scale = -1;
		i     =  1;
	}

	if (raw.length - i > 2 && raw.data[i] == '0' && (raw.data[1] == 'x' || raw.data[1] == 'X')) {
		hex = 1;
		i += 2;
	}

	#define integer_conversion_body(radix, clamp) do {\
		for (; i < raw.length; i++) {\
			s64 value = lut[Min((u8)(raw.data[i] - (u8)'0'), clamp)];\
			if (value >= 0) {\
				if (result.U64 > (U64_MAX - (u64)value) / radix) {\
					result.result = NumberConversionResult_OutOfRange;\
					result.U64    = U64_MAX;\
					return result;\
				} else {\
					result.U64 = radix * result.U64 + (u64)value;\
				}\
			} else {\
				break;\
			}\
		}\
	} while (0)

	if (hex) integer_conversion_body(16u, 63u);
	else     integer_conversion_body(10u, 15u);

	#undef integer_conversion_body

	result.unparsed = (str8){.length = raw.length - i, .data = raw.data + i};
	result.result   = i > 0 ? NumberConversionResult_Success : NumberConversionResult_Invalid;
	result.kind     = NumberConversionKind_Integer;
	if (scale < 0) result.U64 = 0 - result.U64;

	return result;
}

function NumberConversion
number_from_str8(str8 s)
{
	NumberConversion result  = {.unparsed = s};
	NumberConversion integer = integer_from_str8(s, 0);
	if (integer.result == NumberConversionResult_Success) {
		if (integer.unparsed.length != 0 && integer.unparsed.data[0] == '.') {
			s = integer.unparsed;
			s.data++;
			s.length--;

			while (s.length > 0 && s.data[s.length - 1] == '0') s.length--;

			NumberConversion fractional = integer_from_str8(s, 0);
			if (fractional.result == NumberConversionResult_Success || s.length == 0) {
				result.F64 = (f64)fractional.U64;

				u64 divisor = (u64)(fractional.unparsed.data - s.data);
				while (divisor > 0) { result.F64 /= 10.0; divisor--; }

				result.F64 += (f64)integer.S64;

				result.result   = NumberConversionResult_Success;
				result.kind     = NumberConversionKind_Float;
				result.unparsed = fractional.unparsed;
			}
		} else {
			result = integer;
		}
	}
	return result;
}

function void
stream_append(Stream *s, void *data, s64 count)
{
	s->errors |= (s->cap - s->widx) < count;
	if (!s->errors) {
		memory_copy(s->data + s->widx, data, count);
		s->widx += (s32)count;
	}
}

function void
stream_append_byte(Stream *s, u8 b)
{
	stream_append(s, &b, 1);
}

function void
stream_append_hex_u64_width(Stream *s, u64 n, s64 width)
{
	assert(width <= 16);
	if (!s->errors) {
		u8  buf[16];
		u8 *end = buf + sizeof(buf);
		u8 *beg = end;
		while (n) {
			*--beg = (u8)"0123456789abcdef"[n & 0x0F];
			n >>= 4;
		}
		while (end - beg < width)
			*--beg = '0';
		stream_append(s, beg, end - beg);
	}
}

function void
stream_append_hex_u64(Stream *s, u64 n)
{
	stream_append_hex_u64_width(s, n, 2);
}

function void
stream_append_str8(Stream *s, str8 str)
{
	stream_append(s, str.data, str.length);
}

function void
stream_append_u64_width(Stream *s, u64 n, s64 width)
{
	u8 tmp[64];
	u8 *end = tmp + countof(tmp);
	u8 *beg = end;
	width = Min((s64)countof(tmp), width);

	do { *--beg = (u8)('0' + (n % 10)); } while (n /= 10);
	while (end - beg > 0 && (end - beg) < width)
		*--beg = '0';

	stream_append(s, beg, end - beg);
}

function void
stream_append_u64(Stream *s, u64 n)
{
	stream_append_u64_width(s, n, 0);
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
	stream_append_hex_u64(s, c.r);
	stream_append_hex_u64(s, c.g);
	stream_append_hex_u64(s, c.b);
	stream_append_hex_u64(s, c.a);
}

#endif /* _UTIL_C_ */
