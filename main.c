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
	ColourPickerCtx ctx = {0};
	ctx.window_size = (uv2){.w = 720, .h = 960};
	ctx.bg = (Color){ .r = 0x26, .g = 0x1e, .b = 0x22, .a = 0xff };
	ctx.fg = (Color){ .r = 0xea, .g = 0xe1, .b = 0xb4, .a = 0xff };
	ctx.colour.a = 1.0;

	#ifndef _DEBUG
	SetTraceLogLevel(LOG_ERROR);
	#endif

	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(ctx.window_size.w, ctx.window_size.h, "Colour Picker");

	ctx.font      = LoadFontEx(fontpath, 128, 0, 0);
	ctx.font_size = 44;

	ctx.hsv_img     = GenImageColor(360, 360, BLACK);
	ctx.hsv_texture = LoadTextureFromImage(ctx.hsv_img);

	while(!WindowShouldClose()) {
		do_debug();

		BeginDrawing();
		do_colour_picker(&ctx);
		EndDrawing();
	}
}
