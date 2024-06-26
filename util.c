/* See LICENSE for copyright details */
#ifndef _UTIL_C_
#define _UTIL_C_

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

typedef struct {
	v4  last;
	v4  items[5];
	f32 scales[5];
	i32 widx;
	f32 fade_param;
	f32 yoff;
} ColourStackState;

typedef struct {
	v4 colour;
	ColourStackState colour_stack;

	Font font;
	i32  font_size;
	uv2 window_size;
	Color bg, fg;

	RenderTexture hsv_texture;

	u32  flags;
	enum colour_picker_mode mode;
} ColourPickerCtx;

#define ARRAY_COUNT(a) (sizeof(a) / sizeof(*a))
#define ABS(x)         ((x) < 0 ? (-x) : (x))
#define CLAMP(x, a, b) ((x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x))

#endif /* _UTIL_C_ */
