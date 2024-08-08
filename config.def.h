/* NOTE: This is used by gen_incs for generating font_inc.h and shader_inc.h */
#define FONT_SIZE            40u
#define HSV_LERP_SHADER_NAME "slider_lerp.glsl"

/* NOTE: Values below here are just used for initializing the ctx in main.
 * They are not needed if you are are embedding into another application. */

#define COLOUR_PICKER_FG (Color){.r = 0xea, .g = 0xe1, .b = 0xb4, .a = 0xff}
#define COLOUR_PICKER_BG (Color){.r = 0x26, .g = 0x1e, .b = 0x22, .a = 0xff}

#define STARTING_COLOUR  (v4){.r = 0.53, .g = 0.82, .b = 0.59, .a = 1.00}
#define HOVER_COLOUR     (v4){.r = 0.85, .g = 0.38, .b = 0.38, .a = 1.00}
#define CURSOR_COLOUR    (v4){.r = 0.85, .g = 0.38, .b = 0.38, .a = 1.00}

