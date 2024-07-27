/* See LICENSE for copyright details */
#include <dlfcn.h>
#include <fcntl.h>
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.c"

#ifdef _DEBUG
typedef struct timespec Filetime;

static const char *libname = "./libcolourpicker.so";
static void *libhandle;

typedef void (do_colour_picker_fn)(ColourPickerCtx *);
static do_colour_picker_fn *do_colour_picker;

static Filetime
get_filetime(const char *name)
{
	struct stat sb;
	if (stat(name, &sb) < 0)
		return (Filetime){0};
	return sb.st_mtim;
}

static i32
compare_filetime(Filetime a, Filetime b)
{
	return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec);
}

static void
load_library(const char *lib)
{
	/* NOTE: glibc is buggy gnuware so we need to check this */
	if (libhandle)
		dlclose(libhandle);
	libhandle = dlopen(lib, RTLD_NOW|RTLD_LOCAL);
	if (!libhandle)
		fprintf(stderr, "do_debug: dlopen: %s\n", dlerror());

	do_colour_picker = dlsym(libhandle, "do_colour_picker");
	if (!do_colour_picker)
		fprintf(stderr, "do_debug: dlsym: %s\n", dlerror());
}

static void
do_debug(void)
{
	static Filetime updated_time;
	Filetime test_time = get_filetime(libname);
	if (compare_filetime(test_time, updated_time)) {
		sync();
		load_library(libname);
		updated_time = test_time;
	}

}
#else

static void do_debug(void) { }
#include "colourpicker.c"

#endif /* _DEBUG */

static void __attribute__((noreturn))
usage(char *argv0)
{
	printf("usage: %s [-h ????????] [-r ?.??] [-g ?.??] [-b ?.??] [-a ?.??]\n"
	       "\t-h:          Hexadecimal Colour\n"
	       "\t-r|-g|-b|-a: Floating Point Colour Value\n", argv0);
	exit(1);
}

static f32
parse_f32(char *s)
{
	f32 res = atof(s);
	return CLAMP01(res);
}

static u32
parse_u32(char *s)
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

int
main(i32 argc, char *argv[])
{
	ColourPickerCtx ctx = {
		.window_size = { .w = 720, .h = 860 },

		.mode        = CPM_PICKER,
		.colour_mode = CM_HSV,
		.flags       = CPF_REFILL_TEXTURE,

		.colour = { .r = 0.53, .g = 0.82, .b = 0.59, .a = 1.0 },

		.bg = { .r = 0x26, .g = 0x1e, .b = 0x22, .a = 0xff },
		.fg = { .r = 0xea, .g = 0xe1, .b = 0xb4, .a = 0xff },

		.hover_colour = {.r = 0.85, .g = 0.38, .b = 0.38, .a = 1.00},

		.colour_stack = {
			.fade_param = 1.0f,
			.scales = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
			.items  = {
				{ .r = 0.04, .g = 0.04, .b = 0.04, .a = 1.00 },
				{ .r = 0.92, .g = 0.88, .b = 0.78, .a = 1.00 },
				{ .r = 0.34, .g = 0.23, .b = 0.50, .a = 1.00 },
				{ .r = 0.59, .g = 0.11, .b = 0.25, .a = 1.00 },
				{ .r = 0.20, .g = 0.60, .b = 0.24, .a = 1.00 },
				{ .r = 0.14, .g = 0.29, .b = 0.72, .a = 1.00 },
				{ .r = 0.11, .g = 0.59, .b = 0.36, .a = 1.00 },
				{ .r = 0.72, .g = 0.37, .b = 0.19, .a = 1.00 },
			},
		},
	};

	ctx.mcs.mode_visible_t = 1;
	ctx.mcs.next_mode      = -1;
	for (u32 i = 0; i < CPM_LAST; i++)
		ctx.mcs.scales[i] = 1;

	ctx.previous_colour = hsv_to_rgb(ctx.colour);

	{
		v4 rgb = hsv_to_rgb(ctx.colour);
		for (i32 i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				if (!argv[i + 1] || !ISDIGIT(argv[i + 1][0]) ||
				    (argv[i][1] == 'h' && !ISHEX(argv[i + 1][0])))
					usage(argv[0]);

				switch (argv[i][1]) {
				case 'h':
					rgb = normalize_colour(parse_u32(argv[i + 1]));
					ctx.colour = rgb_to_hsv(rgb);
					break;
				case 'r': rgb.r = parse_f32(argv[i + 1]); break;
				case 'g': rgb.g = parse_f32(argv[i + 1]); break;
				case 'b': rgb.b = parse_f32(argv[i + 1]); break;
				case 'a': rgb.a = parse_f32(argv[i + 1]); break;
				default:  usage(argv[0]);                 break;
				}
				i++;
			} else {
				usage(argv[0]);
			}
		}
		ctx.colour = rgb_to_hsv(rgb);
	}

	#ifndef _DEBUG
	SetTraceLogLevel(LOG_NONE);
	#endif

	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(ctx.window_size.w, ctx.window_size.h, "Colour Picker");

	ctx.font_size = 40;
	ctx.font      = LoadFontEx("assets/Lora-SemiBold.ttf", ctx.font_size, 0, 0);

	while(!WindowShouldClose()) {
		do_debug();

		BeginDrawing();
		do_colour_picker(&ctx);
		EndDrawing();
	}

	v4 rgba = {0};
	switch (ctx.mode) {
	case CM_RGB: rgba = ctx.colour;             break;
	case CM_HSV: rgba = hsv_to_rgb(ctx.colour); break;
	default:     ASSERT(0);                     break;
	}

	u32 packed_rgba = pack_rl_colour(colour_from_normalized(rgba));
	printf("0x%08X|{.r = %0.03f, .g = %0.03f, .b = %0.03f, .a = %0.03f}\n",
	       packed_rgba, rgba.r, rgba.g, rgba.b, rgba.a);

	return 0;
}
