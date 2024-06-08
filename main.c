#include <dlfcn.h>
#include <fcntl.h>
#include <raylib.h>
#include <sys/stat.h>

#include "util.c"

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
	dlclose(libhandle);
	libhandle = dlopen(lib, RTLD_NOW|RTLD_LOCAL);
	do_colour_picker = dlsym(libhandle, "do_colour_picker");
}

int
main(void)
{
	ColourPickerCtx ctx;
	ctx.window_size = (uv2){.w = 720, .h = 960};

	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(ctx.window_size.w, ctx.window_size.h, "Colour Picker");
	load_library(libname);

	Filetime updated_time = get_filetime(libname);
	while(!WindowShouldClose()) {
		BeginDrawing();
		do_colour_picker(&ctx);
		EndDrawing();

		Filetime test_time = get_filetime(libname);
		if (compare_filetime(test_time, updated_time)) {
			load_library(libname);
			updated_time = test_time;
		}
	}
}
