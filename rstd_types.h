////////////////////////
// NOTE: Standard Types

#ifndef RSTD_TYPES_H
#define RSTD_TYPES_H

#if COMPILER_MSVC
  typedef unsigned __int64  u64;
  typedef signed   __int64  s64;
  typedef unsigned __int32  u32;
  typedef signed   __int32  s32;
  typedef unsigned __int16  u16;
  typedef signed   __int16  s16;
  typedef unsigned __int8   u8;
  typedef signed   __int8   s8;
#else
  typedef __UINT64_TYPE__   u64;
  typedef __INT64_TYPE__    s64;
  typedef __UINT32_TYPE__   u32;
  typedef __INT32_TYPE__    s32;
  typedef __UINT16_TYPE__   u16;
  typedef __INT16_TYPE__    s16;
  typedef __UINT8_TYPE__    u8;
  typedef __INT8_TYPE__     s8;
  typedef _Float16          f16;
#endif

typedef u8     b8;
typedef u16    b16;
typedef u32    b32;
typedef u64    b64;
typedef float  f32;
typedef double f64;
typedef s64    sptr;
typedef u64    uptr;

#define U64_MAX (0xFFFFFFFFFFFFFFFFull)
#define U32_MAX (0xFFFFFFFFul)
#define U16_MAX (0xFFFFu)
#define U8_MAX  (0xFFu)
#define S64_MAX (0x7FFFFFFFFFFFFFFFll)
#define S32_MAX (0x7FFFFFFFl)
#define S16_MAX (0x7FFF)
#define S8_MAX  (0x7F)

#define GB(a)   ((u64)(a) << 30ULL)
#define MB(a)   ((u64)(a) << 20ULL)
#define KB(a)   ((u64)(a) << 10ULL)

typedef struct {s64 length; u16 *data;} str16;
typedef struct {s64 length; u8  *data;} str8;

#define str8(s)      (str8){.length = (s64)sizeof(s) - 1, .data = (u8 *)s}
#define str8_comp(s) {sizeof(s) - 1, (u8 *)s}

#pragma pack(push, 1)
typedef struct {
	u16 file_type;
	u32 file_size;
	u16 reserved_1;
	u16 reserved_2;
	u32 bitmap_offset;

	u32 size;
	s32 width;
	s32 height;
	u16 planes;
	u16 bits_per_pixel;

	u32 compression;
	u32 size_of_bitmap;

	s32 horizontal_resolution;
	s32 vertical_resolution;
	u32 colours_used;
	u32 colours_important;

	u32 red_mask;
	u32 green_mask;
	u32 blue_mask;
	u32 alpha_mask;

	u32 colour_space_type;
	u32 cie_xyz_triples[9];
	u32 gamma_red;
	u32 gamma_green;
	u32 gamma_blue;
} rstd_bitmap_header;
#pragma pack(pop)

#endif /* RSTD_TYPES_H */
