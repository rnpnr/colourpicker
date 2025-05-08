#include <raylib.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define function static

#define ISSPACE(a)  ((a) == ' ' || (a) == '\t')

typedef struct {uint8_t *data; ptrdiff_t len;} str8;

function str8
read_whole_file(char *name, str8 *mem)
{
	str8 res = {0};
	FILE *fp = fopen(name, "r");

	if (!fp) {
		fputs("Failed to open file!\n", stdout);
		exit(1);
	}

	fseek(fp, 0, SEEK_END);
	res.len = ftell(fp);
	rewind(fp);

	if (mem->len < res.len) {
		fputs("Not enough space for reading file!\n", stdout);
		exit(1);
	}
	res.data = mem->data;
	res.len  = fread(res.data, 1, res.len, fp);
	fclose(fp);

	mem->data += res.len;
	mem->len  -= res.len;

	return res;
}

function str8
get_line(str8 *s)
{
	str8 res = {.data = s->data};
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

/* NOTE: modified from raylib */
function void
export_font_as_code(char *font_path, char *output_name, int font_size, str8 mem)
{
	str8 raw            = read_whole_file(font_path, &mem);
	Font font         = {0};
	font.baseSize     = font_size;
	font.glyphCount   = 95;
	font.glyphPadding = 4;

	font.glyphs = LoadFontData(raw.data, raw.len, font.baseSize, 0, font.glyphCount, FONT_DEFAULT);
	if (font.glyphs == NULL) {
		printf("Failed to load font data: %s\n", font_path);
		exit(1);
	}

	Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, font.baseSize,
	                                font.glyphPadding, 0);

	FILE *fp = fopen(output_name, "w");
	if (fp == NULL) {
		printf("Failed to open output font file: %s\n", output_name);
		exit(1);
	}

	char suffix[256];
	strncpy(suffix, GetFileNameWithoutExt(output_name), 256 - 1);

	#define TEXT_BYTES_PER_LINE 16

	int image_data_size      = GetPixelDataSize(atlas.width, atlas.height, atlas.format);
	int comp_data_size       = 0;
	unsigned char *comp_data = CompressData(atlas.data, image_data_size, &comp_data_size);

	// Save font image data (compressed)
	fprintf(fp, "#define COMPRESSED_DATA_SIZE_FONT_%s %i\n\n", TextToUpper(suffix), comp_data_size);
	fprintf(fp, "// Font image pixels data compressed (DEFLATE)\n");
	fprintf(fp, "// NOTE: Original pixel data simplified to GRAYSCALE\n");
	fprintf(fp, "static unsigned char fontData_%s[COMPRESSED_DATA_SIZE_FONT_%s] = { ", suffix, TextToUpper(suffix));
	for (int i = 0; i < comp_data_size - 1; i++) fprintf(fp, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%02x,\n    " : "0x%02x, "), comp_data[i]);
	fprintf(fp, "0x%02x };\n\n", comp_data[comp_data_size - 1]);
	RL_FREE(comp_data);

	// Save font recs data
	fprintf(fp, "// Font characters rectangles data\n");
	fprintf(fp, "static Rectangle fontRecs_%s[%i] = {\n", suffix, font.glyphCount);
	for (int i = 0; i < font.glyphCount; i++)
		fprintf(fp, "    { %1.0f, %1.0f, %1.0f , %1.0f },\n", font.recs[i].x, font.recs[i].y, font.recs[i].width, font.recs[i].height);
	fprintf(fp, "};\n\n");

	// Save font glyphs data
	// NOTE: Glyphs image data not saved (grayscale pixels), it could be generated from image and recs
	fprintf(fp, "// Font glyphs info data\n");
	fprintf(fp, "// NOTE: No glyphs.image data provided\n");
	fprintf(fp, "static GlyphInfo fontGlyphs_%s[%i] = {\n", suffix, font.glyphCount);
	for (int i = 0; i < font.glyphCount; i++)
		fprintf(fp, "    { %i, %i, %i, %i, { 0 }},\n", font.glyphs[i].value, font.glyphs[i].offsetX, font.glyphs[i].offsetY, font.glyphs[i].advanceX);
	fprintf(fp, "};\n\n");

	// Custom font loading function
	fprintf(fp, "// Font loading function: %s\n", suffix);
	fprintf(fp, "static Font LoadFont_%s(void)\n{\n", suffix);
	fprintf(fp, "    Font font = { 0 };\n\n");
	fprintf(fp, "    font.baseSize = %i;\n", font.baseSize);
	fprintf(fp, "    font.glyphCount = %i;\n", font.glyphCount);
	fprintf(fp, "    font.glyphPadding = %i;\n\n", font.glyphPadding);
	fprintf(fp, "    // Custom font loading\n");
	fprintf(fp, "    // NOTE: Compressed font image data (DEFLATE), it requires DecompressData() function\n");
	fprintf(fp, "    int fontDataSize_%s = 0;\n", suffix);
	fprintf(fp, "    unsigned char *data = DecompressData(fontData_%s, COMPRESSED_DATA_SIZE_FONT_%s, &fontDataSize_%s);\n", suffix, TextToUpper(suffix), suffix);
	fprintf(fp, "    Image imFont = { data, %i, %i, 1, %i };\n\n", atlas.width, atlas.height, atlas.format);
	fprintf(fp, "    // Load texture from image\n");
	fprintf(fp, "    font.texture = LoadTextureFromImage(imFont);\n");
	fprintf(fp, "    UnloadImage(imFont);  // Uncompressed data can be unloaded from memory\n\n");
	fprintf(fp, "    // Assign glyph recs and info data directly\n");
	fprintf(fp, "    // WARNING: This font data must not be unloaded\n");
	fprintf(fp, "    font.recs = fontRecs_%s;\n", suffix);
	fprintf(fp, "    font.glyphs = fontGlyphs_%s;\n\n", suffix);
	fprintf(fp, "    return font;\n");
	fprintf(fp, "}\n");

	fclose(fp);
}

int
main(void)
{
	static uint8_t mem[2u * 1024u * 1024u];
	str8 smem = {.data = mem, .len = sizeof(mem)};

	SetTraceLogLevel(LOG_NONE);
	int font_sizes[] = { FONT_SIZE, FONT_SIZE/2 };
	for (unsigned int i = 0; i < sizeof(font_sizes)/sizeof(*font_sizes); i++) {
		str8 tmem = smem;
		str8 rmem = smem;
		size_t tlen  = snprintf((char *)tmem.data, tmem.len, "lora_sb_%d_inc.h", i);
		rmem.len    -= (tlen + 1);
		rmem.data   += (tlen + 1);
		export_font_as_code("assets/Lora-SemiBold.ttf", (char *)tmem.data, font_sizes[i], rmem);
	}

	FILE *out_file = fopen("external/include/shader_inc.h", "w");
	if (!out_file) {
		fputs("Failed to open necessary files!\n", stdout);
		return 1;
	}

	str8 shader_data = read_whole_file(HSV_LERP_SHADER_NAME, &smem);
	str8 s = shader_data;
	/* NOTE: skip over license notice */
	str8 line = get_line(&s);
	fputs("static char *g_hsv_shader_text =\n\t", out_file);
	do {
		line = get_line(&s);
		while (line.len && ISSPACE(*line.data)) {
			line.data++;
			line.len--;
		}
		if (line.len) {
			fputc('"', out_file);
			fwrite(line.data, 1, line.len, out_file);
			fputs("\\n\"\n\t", out_file);
		}
	} while (s.len > 0);
	fputs(";\n", out_file);
	fclose(out_file);

	return 0;
}
