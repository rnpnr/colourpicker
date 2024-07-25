/* See LICENSE for copyright details */
#include <dlfcn.h>
#include <fcntl.h>
#include <raylib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.c"

static const char *fontpath = "/home/rnp/.local/share/fonts/Aozora Mincho Medium.ttf";

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

int
main(void)
{
	ColourPickerCtx ctx = {
		.window_size = { .w = 720, .h = 960 },

		.mode  = CPM_HSV,
		.flags = CPF_REFILL_TEXTURE,

		.colour = { .r = 0.53, .g = 0.82, .b = 0.59, .a = 1.0 },

		.bg = { .r = 0x26, .g = 0x1e, .b = 0x22, .a = 0xff },
		.fg = { .r = 0xea, .g = 0xe1, .b = 0xb4, .a = 0xff },

		.colour_stack = {
			.fade_param = 1.0f,
			.scales = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },
			.items  = {
				{ .r = 0.04, .g = 0.04, .b = 0.04, .a = 1.00 },
				{ .r = 0.92, .g = 0.88, .b = 0.78, .a = 1.00 },
				{ .r = 0.59, .g = 0.11, .b = 0.25, .a = 1.00 },
				{ .r = 0.11, .g = 0.59, .b = 0.36, .a = 1.00 },
				{ .r = 0.14, .g = 0.29, .b = 0.72, .a = 1.00 },
			},
		},
	};

	#ifndef _DEBUG
	SetTraceLogLevel(LOG_NONE);
	#endif

	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(ctx.window_size.w, ctx.window_size.h, "Colour Picker");

	ctx.font      = LoadFontEx(fontpath, 128, 0, 0);
	ctx.font_size = 44;

	ctx.hsv_texture = LoadRenderTexture(360, 360);

	while(!WindowShouldClose()) {
		do_debug();

		BeginDrawing();
		do_colour_picker(&ctx);
		EndDrawing();
	}

	v4 rgba = {0};
	switch (ctx.mode) {
	case CPM_RGB: rgba = ctx.colour;             break;
	case CPM_HSV: rgba = hsv_to_rgb(ctx.colour); break;
	default:      ASSERT(0);                     break;
	}

	Color rl        = colour_from_normalized(rgba);
	u32 packed_rgba = rl.r << 24 | rl.g << 16 | rl.b << 8 | rl.a << 0;

	printf("0x%08X|{.r = %0.02f, .g = %0.02f, .b = %0.02f, .a = %0.02f}\n",
	       packed_rgba, rgba.r, rgba.g, rgba.b, rgba.a);

	return 0;
}
