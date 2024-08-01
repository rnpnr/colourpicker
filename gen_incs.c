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
read_whole_file(char *name)
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

	res.data = malloc(res.len);
	if (!res.data) {
		fputs("Failed to allocate space for reading file!\n", stdout);
		exit(1);
	}
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

static Font
load_font(s8 font_data, Image *out, int font_size)
{
	Font font          = {0};
	font.baseSize      = font_size;
	font.glyphCount    = 95;
	font.glyphPadding  = 0;
	font.glyphs        = LoadFontData(font_data.data, font_data.len, font.baseSize, 0,
	                                  font.glyphCount, FONT_DEFAULT);

	if (font.glyphs != NULL) {
		font.glyphPadding = 4;
		*out = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount,
		                         font.baseSize, font.glyphPadding, 0);

		// Update glyphs[i].image to use alpha, required to be used on ImageDrawText()
		for (int i = 0; i < font.glyphCount; i++) {
			UnloadImage(font.glyphs[i].image);
			font.glyphs[i].image = ImageFromImage(*out, font.recs[i]);
		}
	}
	return font;
}

/* NOTE: Modified from raylib. Used to save font data without opening a window */
static void
export_font_as_code(Font font, Image image, const char *fileName)
{
	#define TEXT_BYTES_PER_LINE     20
	#define MAX_FONT_DATA_SIZE      1024*1024       // 1 MB

	// Get file name from path
	char fileNamePascal[256] = { 0 };
	strncpy(fileNamePascal, TextToPascal(GetFileNameWithoutExt(fileName)), 256 - 1);

	// NOTE: Text data buffer size is estimated considering image data size in bytes
	// and requiring 6 char bytes for every byte: "0x00, "
	char *txt = calloc(MAX_FONT_DATA_SIZE, 1);

	int off = 0;
	off += sprintf(txt + off, "////////////////////////////////////////////////////////////////////////////////////////\n");
	off += sprintf(txt + off, "//                                                                                    //\n");
	off += sprintf(txt + off, "// FontAsCode exporter v1.0 - Font data exported as an array of bytes                 //\n");
	off += sprintf(txt + off, "//                                                                                    //\n");
	off += sprintf(txt + off, "// more info and bugs-report:  github.com/raysan5/raylib                              //\n");
	off += sprintf(txt + off, "// feedback and support:       ray[at]raylib.com                                      //\n");
	off += sprintf(txt + off, "//                                                                                    //\n");
	off += sprintf(txt + off, "// Copyright (c) 2018-2024 Ramon Santamaria (@raysan5)                                //\n");
	off += sprintf(txt + off, "//                                                                                    //\n");
	off += sprintf(txt + off, "// ---------------------------------------------------------------------------------- //\n");
	off += sprintf(txt + off, "//                                                                                    //\n");
	off += sprintf(txt + off, "// TODO: Fill the information and license of the exported font here:                  //\n");
	off += sprintf(txt + off, "//                                                                                    //\n");
	off += sprintf(txt + off, "// Font name:    ....                                                                 //\n");
	off += sprintf(txt + off, "// Font creator: ....                                                                 //\n");
	off += sprintf(txt + off, "// Font LICENSE: ....                                                                 //\n");
	off += sprintf(txt + off, "//                                                                                    //\n");
	off += sprintf(txt + off, "////////////////////////////////////////////////////////////////////////////////////////\n\n");

	// Support font export and initialization
	// NOTE: This mechanism is highly coupled to raylib
	int imageDataSize = GetPixelDataSize(image.width, image.height, image.format);

	// Image data is usually GRAYSCALE + ALPHA and can be reduced to GRAYSCALE
	//ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

	// WARNING: Data is compressed using raylib CompressData() DEFLATE,
	// it requires to be decompressed with raylib DecompressData(), that requires
	// compiling raylib with SUPPORT_COMPRESSION_API config flag enabled

	// Compress font image data
	int compDataSize = 0;
	unsigned char *compData = CompressData((const unsigned char *)image.data, imageDataSize, &compDataSize);

	// Save font image data (compressed)
	off += sprintf(txt + off, "#define COMPRESSED_DATA_SIZE_FONT_%s %i\n\n", TextToUpper(fileNamePascal), compDataSize);
	off += sprintf(txt + off, "// Font image pixels data compressed (DEFLATE)\n");
	off += sprintf(txt + off, "// NOTE: Original pixel data simplified to GRAYSCALE\n");
	off += sprintf(txt + off, "static unsigned char fontData_%s[COMPRESSED_DATA_SIZE_FONT_%s] = { ", fileNamePascal, TextToUpper(fileNamePascal));
	for (int i = 0; i < compDataSize - 1; i++) off += sprintf(txt + off, ((i%TEXT_BYTES_PER_LINE == 0)? "0x%02x,\n    " : "0x%02x, "), compData[i]);
	off += sprintf(txt + off, "0x%02x };\n\n", compData[compDataSize - 1]);
	RL_FREE(compData);

	// Save font recs data
	off += sprintf(txt + off, "// Font characters rectangles data\n");
	off += sprintf(txt + off, "static Rectangle fontRecs_%s[%i] = {\n", fileNamePascal, font.glyphCount);
	for (int i = 0; i < font.glyphCount; i++) {
		off += sprintf(txt + off, "    { %1.0f, %1.0f, %1.0f , %1.0f },\n",
		               font.recs[i].x, font.recs[i].y, font.recs[i].width, font.recs[i].height);
	}
	off += sprintf(txt + off, "};\n\n");

	// Save font glyphs data
	// NOTE: Glyphs image data not saved (grayscale pixels),
	// it could be generated from image and recs
	off += sprintf(txt + off, "// Font glyphs info data\n");
	off += sprintf(txt + off, "// NOTE: No glyphs.image data provided\n");
	off += sprintf(txt + off, "static GlyphInfo fontGlyphs_%s[%i] = {\n", fileNamePascal, font.glyphCount);
	for (int i = 0; i < font.glyphCount; i++) {
		off += sprintf(txt + off, "    { %i, %i, %i, %i, { 0 }},\n",
		                     font.glyphs[i].value, font.glyphs[i].offsetX,
		                     font.glyphs[i].offsetY, font.glyphs[i].advanceX);
	}
	off += sprintf(txt + off, "};\n\n");

	// Custom font loading function
	off += sprintf(txt + off, "// Font loading function: %s\n", fileNamePascal);
	off += sprintf(txt + off, "static Font LoadFont_%s(void)\n{\n", fileNamePascal);
	off += sprintf(txt + off, "    Font font = { 0 };\n\n");
	off += sprintf(txt + off, "    font.baseSize = %i;\n", font.baseSize);
	off += sprintf(txt + off, "    font.glyphCount = %i;\n", font.glyphCount);
	off += sprintf(txt + off, "    font.glyphPadding = %i;\n\n", font.glyphPadding);
	off += sprintf(txt + off, "    // Custom font loading\n");
	off += sprintf(txt + off, "    // NOTE: Compressed font image data (DEFLATE), it requires DecompressData() function\n");
	off += sprintf(txt + off, "    int fontDataSize_%s = 0;\n", fileNamePascal);
	off += sprintf(txt + off, "    unsigned char *data = DecompressData(fontData_%s, COMPRESSED_DATA_SIZE_FONT_%s, &fontDataSize_%s);\n", fileNamePascal, TextToUpper(fileNamePascal), fileNamePascal);
	off += sprintf(txt + off, "    Image imFont = { data, %i, %i, 1, %i };\n\n", image.width, image.height, image.format);
	off += sprintf(txt + off, "    // Load texture from image\n");
	off += sprintf(txt + off, "    font.texture = LoadTextureFromImage(imFont);\n");
	off += sprintf(txt + off, "    UnloadImage(imFont);  // Uncompressed data can be unloaded from memory\n\n");
	// We have two possible mechanisms to assign font.recs and font.glyphs data,
	// that data is already available as global arrays, we two options to assign that data:
	//  - 1. Data copy. This option consumes more memory and Font MUST be unloaded by user, requiring additional code
	//  - 2. Data assignment. This option consumes less memory and Font MUST NOT be unloaded by user because data is on protected DATA segment
	off += sprintf(txt + off, "    // Assign glyph recs and info data directly\n");
	off += sprintf(txt + off, "    // WARNING: This font data must not be unloaded\n");
	off += sprintf(txt + off, "    font.recs = fontRecs_%s;\n", fileNamePascal);
	off += sprintf(txt + off, "    font.glyphs = fontGlyphs_%s;\n\n", fileNamePascal);
	off += sprintf(txt + off, "    return font;\n");
	off += sprintf(txt + off, "}\n");

	// NOTE: Text data size exported is determined by '\0' (NULL) character
	SaveFileText(fileName, txt);
}

int
main(void)
{
	SetTraceLogLevel(LOG_NONE);
	Image atlas;
	s8 font_data = read_whole_file("assets/Lora-SemiBold.ttf");
	Font font = load_font(font_data, &atlas, FONT_SIZE);
	export_font_as_code(font, atlas, "font_inc.h");

	FILE *out_file = fopen("shader_inc.h", "w");
	if (!out_file) {
		fputs("Failed to open necessary files!\n", stdout);
		return 1;
	}

	s8 shader_data = read_whole_file(HSV_LERP_SHADER_NAME);
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
