/* See LICENSE for copyright details */
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.c"

#ifdef _DEBUG
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct timespec Filetime;

global const char *libname = "./colourpicker.so";
global void *libhandle;

typedef void (do_colour_picker_fn)(ColourPickerCtx *, f32 dt, Vector2 window_pos, Vector2 mouse);
global do_colour_picker_fn *do_colour_picker;

function Filetime
get_filetime(const char *name)
{
	struct stat sb;
	if (stat(name, &sb) < 0)
		return (Filetime){0};
	return sb.st_mtim;
}

function s32
compare_filetime(Filetime a, Filetime b)
{
	return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec);
}

function void
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

function void
do_debug(void)
{
	local_persist Filetime updated_time;
	Filetime test_time = get_filetime(libname);
	if (compare_filetime(test_time, updated_time)) {
		load_library(libname);
		updated_time = test_time;
	}

}
#else

#define do_debug(...)
#include "colourpicker.c"

#endif /* _DEBUG */

function void __attribute__((noreturn))
usage(char *argv0)
{
	printf("usage: %s [-h ????????] [-r ?.??] [-g ?.??] [-b ?.??] [-a ?.??]\n"
	       "\t-h:          Hexadecimal Colour\n"
	       "\t-r|-g|-b|-a: Floating Point Colour Value\n", argv0);
	exit(1);
}

extern s32
main(s32 argc, char *argv[])
{
	ColourPickerCtx ctx = {
		.window_size = { .w = 640, .h = 860 },

		.mode               = CPM_PICKER,
		.stored_colour_kind = ColourKind_HSV,
		.flags              = ColourPickerFlag_RefillTexture,

		.text_input_state = {.idx = -1, .cursor_t = 1, .cursor_t_target = 0},
		.mcs = {.mode_visible_t = 1, .next_mode = -1},
		.ss  = {.scale_t = {1, 1, 1, 1}},

		.bg = COLOUR_PICKER_BG,
		.fg = COLOUR_PICKER_FG,

		.colour        = STARTING_COLOUR,
		.hover_colour  = HOVER_COLOUR,
		.cursor_colour = CURSOR_COLOUR,

		.colour_stack = {
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

	for (u32 i = 0; i < CPM_LAST; i++)
		ctx.mcs.buttons[i].hover_t = 1;

	{
		v4 rgb = hsv_to_rgb(ctx.colour);
		for (s32 i = 1; i < argc; i++) {
			if (argv[i][0] == '-') {
				if (argv[i][1] == 'v') {
					printf("colour picker %s\n", VERSION);
					return 0;
				}
				if (argv[i + 1] == NULL ||
				    (argv[i][1] == 'h' && !ISHEX(argv[i + 1][0])))
					usage(argv[0]);

				switch (argv[i][1]) {
				case 'h':
					rgb = normalize_colour(parse_hex_u32(str8_from_c_str(argv[i + 1])));
					ctx.colour = rgb_to_hsv(rgb);
					break;
				case 'r': rgb.r = parse_f64(str8_from_c_str(argv[i + 1])); CLAMP01(rgb.r); break;
				case 'g': rgb.g = parse_f64(str8_from_c_str(argv[i + 1])); CLAMP01(rgb.g); break;
				case 'b': rgb.b = parse_f64(str8_from_c_str(argv[i + 1])); CLAMP01(rgb.b); break;
				case 'a': rgb.a = parse_f64(str8_from_c_str(argv[i + 1])); CLAMP01(rgb.a); break;
				default:  usage(argv[0]);                                 break;
				}
				i++;
			} else {
				usage(argv[0]);
			}
		}
		ctx.colour          = rgb_to_hsv(rgb);
		ctx.previous_colour = rgb;
	}
	ctx.pms.base_hue = ctx.colour.x;

	#ifndef _DEBUG
	SetTraceLogLevel(LOG_NONE);
	#endif

	SetConfigFlags(FLAG_VSYNC_HINT);
	InitWindow(ctx.window_size.w, ctx.window_size.h, "Colour Picker");
	/* NOTE: do this after initing so that the window starts out floating in tiling wm */
	SetWindowMinSize(324, 324 * WINDOW_ASPECT_RATIO);
	SetWindowState(FLAG_WINDOW_RESIZABLE);

	{
		Image icon;
		RenderTexture icon_texture = LoadRenderTexture(128, 128);
		BeginDrawing();
		BeginTextureMode(icon_texture);
		ClearBackground(ctx.bg);
		DrawCircleGradient(64, 64, 48, rl_colour_from_normalized(hsv_to_rgb(ctx.colour)), ctx.bg);
		DrawRing((Vector2){64, 64}, 45, 48, 0, 360, 128, SELECTOR_BORDER_COLOUR);
		EndTextureMode();
		EndDrawing();
		icon = LoadImageFromTexture(icon_texture.texture);
		SetWindowIcon(icon);
		UnloadRenderTexture(icon_texture);
		UnloadImage(icon);
	}

	ctx.font = LoadFont_lora_sb_0_inc();

	while(!WindowShouldClose()) {
		do_debug();

		BeginDrawing();
		ClearBackground(ctx.bg);
		do_colour_picker(&ctx, GetFrameTime(), (Vector2){0}, GetMousePosition());
		EndDrawing();
	}

	v4 rgba = {0};
	switch (ctx.stored_colour_kind) {
	case ColourKind_RGB: rgba = ctx.colour;             break;
	case ColourKind_HSV: rgba = hsv_to_rgb(ctx.colour); break;
	InvalidDefaultCase;
	}

	u32 packed_rgba = pack_rl_colour(rl_colour_from_normalized(rgba));
	printf("0x%08X|{.r = %0.03f, .g = %0.03f, .b = %0.03f, .a = %0.03f}\n",
	       packed_rgba, rgba.r, rgba.g, rgba.b, rgba.a);

	return 0;
}
