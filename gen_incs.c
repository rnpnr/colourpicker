#include <raylib.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define ISSPACE(a)  ((a) == ' ' || (a) == '\t')

typedef struct {uint8_t *data; ptrdiff_t len;} s8;

static s8
read_whole_file(char *name, s8 mem)
{
	s8 res = {0};
	FILE *fp = fopen(name, "r");

	if (!fp) {
		fputs("Failed to open file!\n", stdout);
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	res.len = ftell(fp);
	rewind(fp);

	if (mem.len < res.len) {
		fputs("Not enough space for reading file!\n", stdout);
		exit(1);
	}
	res.data = mem.data;
	fread(res.data, res.len, 1, fp);
	fclose(fp);
	return res;
}

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
	static uint8_t mem[2u * 1024u * 1024u];
	s8 smem = {.data = mem, .len = sizeof(mem)};

	SetTraceLogLevel(LOG_NONE);
	char *font_inc_name = "lora_sb_inc.h";
	if (!ExportFontAsCodeEx("assets/Lora-SemiBold.ttf", font_inc_name, FONT_SIZE, 0, 0))
		printf("Failed to export font: %s\n", font_inc_name);

	FILE *out_file = fopen("external/include/shader_inc.h", "w");
	if (!out_file) {
		fputs("Failed to open necessary files!\n", stdout);
		return 1;
	}

	s8 shader_data = read_whole_file(HSV_LERP_SHADER_NAME, smem);
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

	return 0;
}
