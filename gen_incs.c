#include <raylib.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"

#define ISSPACE(a)  ((a) == ' ' || (a) == '\t')

typedef struct {int8_t *data; ptrdiff_t len;} s8;

static s8
get_line(s8 *s)
{
	s8 res = {.data = s->data};
	while (s->len && s->data[0] != '\n') {
		s->data++;
		s->len--;
		res.len++;
	}
	/* NOTE: skip over trailing \n */
	s->data++;
	s->len--;
	return res;
}

int
main(void)
{
	SetConfigFlags(FLAG_WINDOW_HIDDEN);
	SetTraceLogLevel(LOG_NONE);
	InitWindow(640, 480, "");
	Font font = LoadFontEx("assets/Lora-SemiBold.ttf", FONT_SIZE, 0, 0);
	ExportFontAsCode(font, "font_inc.h");
	CloseWindow();

	FILE *shader_file, *out_file;
	shader_file = fopen(HSV_LERP_SHADER_NAME, "r");
	out_file    = fopen("shader_inc.h", "w");

	if (!shader_file || !out_file) {
		fputs("Failed to open necessary files!\n", stdout);
		return 1;
	}

	fseek(shader_file, 0, SEEK_END);
	s8 shader_data = {.len = ftell(shader_file)};
	rewind(shader_file);

	shader_data.data = malloc(shader_data.len);
	if (!shader_data.data) {
		fputs("Failed to allocate space for reading shader file!\n", stdout);
		return 1;
	}
	fread(shader_data.data, shader_data.len, 1, shader_file);

	s8 s = shader_data;
	/* NOTE: skip over license notice */
	s8 line = get_line(&s);
	fputs("static char *g_hsv_shader_text =\n\t", out_file);
	do {
		line = get_line(&s);
		while (line.len && ISSPACE(*line.data)) {
			line.data++;
			line.len--;
		}
		if (line.len) {
			fputc('"', out_file);
			fwrite(line.data, line.len, 1, out_file);
			fputs("\\n\"\n\t", out_file);
		}
	} while (s.len > 0);
	fputs(";\n", out_file);
	fclose(out_file);
	fclose(shader_file);

	return 0;
}
