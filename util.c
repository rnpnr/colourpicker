#include <stddef.h>
#include <stdint.h>

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
	Vector2 rv;
	f32 E[2];
} v2;

typedef struct {
	uv2 window_size;
} ColourPickerCtx;
