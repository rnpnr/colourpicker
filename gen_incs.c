#include <raylib.h>

#include <stdlib.h>
#include <stdio.h>

#include "config.h"

#define ISSPACE(a)  ((a) == ' ' || (a) == '\t')

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

	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	/* NOTE: skip over license notice */
	getline(&line, &len, shader_file);
	fputs("static char *g_hsv_shader_text =\n\t", out_file);
	while ((read = getline(&line, &len, shader_file)) != -1) {
		char *s;
		for (s = line; *s; s++, read--) {
			if (!ISSPACE(*s))
				break;
		}
		if (read > 1) {
			fputc('"', out_file);
			fwrite(s, read - 1, 1, out_file);
			fputs("\\n\"\n\t", out_file);
		}
	}
	fputs(";\n", out_file);
	fclose(out_file);
	fclose(shader_file);

	return 0;
}
