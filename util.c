/* See LICENSE for copyright details */
#include <stddef.h>
#include <stdint.h>

#define ASSERT(c) do { if (!(c)) __builtin_debugtrap(); } while (0);

typedef int32_t   i32;
typedef uint32_t  u32;
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
	RGB,
	HSV,
};

typedef struct {
	uv2 window_size;
	Font font;
	v4 colour;
	enum colour_picker_mode mode;
} ColourPickerCtx;

#define CLAMP(x, a, b) ((x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x))
