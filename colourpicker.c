/* See LICENSE for copyright details */
#include <raylib.h>
#include "util.c"

static v2
center_text_in_rect(Rect r, const char *text, Font font, f32 fontsize)
{
	v2 ts    = { .rv = MeasureTextEx(font, text, fontsize, 0) };
	v2 delta = { .w = r.size.w - ts.w, .h = r.size.h - ts.h };
	return (v2) {
		.x = r.pos.x + 0.5 * delta.w,
		.y = r.pos.y + 0.5 * delta.h,
	};
}

static Rect
cut_rect_middle(Rect r, f32 left, f32 right)
{
	ASSERT(left <= right);
	r.pos.x  += r.size.w * left;
	r.size.w  = r.size.w * (right - left);
	return r;
}

static Rect
cut_rect_left(Rect r, f32 fraction)
{
	r.size.w *= fraction;
	return r;
}

static Rect
cut_rect_right(Rect r, f32 fraction)
{
	r.pos.x  += fraction * r.size.w;
	r.size.w *= (1 - fraction);
	return r;
}

static f32
do_slider(Rect r, char *label, f32 current, Font font)
{
	Rect lr = cut_rect_left(r, 0.1);
	Rect vr = cut_rect_right(r, 0.85);

	Rect sr = cut_rect_middle(r, 0.1, 0.85);
	sr.size.h *= 0.75;
	sr.pos.y  += r.size.h * 0.125;

	v2 fpos = center_text_in_rect(lr, label, font, 48);
	DrawTextEx(font, label, fpos.rv, 48, 0, LIGHTGRAY);

	v2 mouse = { .rv = GetMousePosition() };
	if (CheckCollisionPointRec(mouse.rv, sr.rr)) {
		f32 wheel = GetMouseWheelMove();
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			current = (mouse.x - sr.pos.x) / sr.size.w;
		current += 4 * wheel / sr.size.w;
		CLAMP(current, 0.0, 1.0);
	}

	DrawRectangleRounded(sr.rr, 1.0, 0, PURPLE);
	v2 start = {
		.x = sr.pos.x + current * sr.size.w,
		.y = sr.pos.y + sr.size.h,
	};
	v2 end = start;
	end.y -= sr.size.h;
	DrawLineEx(start.rv, end.rv, 8, BLACK);

	const char *value = TextFormat("%0.02f", current);
	fpos = center_text_in_rect(vr, value, font, 36);
	DrawTextEx(font, value, fpos.rv, 36, 0, LIGHTGRAY);

	return current;
}

static enum colour_picker_mode
do_status_bar(Rect r, enum colour_picker_mode mode, Color colour, Font font)
{
	Rect hr = cut_rect_left(r, 0.5);
	Rect sr = cut_rect_right(r, 0.7);

	const char *hex = TextFormat("%02x%02x%02x%02x",
	                             colour.r, colour.b, colour.g,
	                             colour.a);
	v2 fpos = center_text_in_rect(hr, hex, font, 48);
	DrawTextEx(font, hex, fpos.rv, 48, 0, LIGHTGRAY);

	v2 mouse = { .rv = GetMousePosition() };

	if (CheckCollisionPointRec(mouse.rv, hr.rr)) {
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			SetClipboardText(hex);
	}

	if (CheckCollisionPointRec(mouse.rv, sr.rr)) {
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			if (mode == HSV)
				mode = RGB;
			else
				mode++;
		}
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
			if (mode == RGB)
				mode = HSV;
			else
				mode--;
		}
	}

	char *mtext;
	switch (mode) {
	case RGB: mtext = "RGB"; break;
	case HSV: mtext = "HSV"; break;
	}
	fpos = center_text_in_rect(sr, mtext, font, 48);
	DrawTextEx(font, mtext, fpos.rv, 48, 0, LIGHTGRAY);

	return mode;
}

void
do_colour_picker(ColourPickerCtx *ctx)
{
	ClearBackground(BLACK);
	DrawFPS(20, 20);

	uv2 ws = ctx->window_size;
	Color colour = ColorFromNormalized(ctx->colour.rv);

	{
		v2  cc = { .x = (f32)ws.w / 2, .y = (f32)ws.h / 4 };
		DrawCircleSector(cc.rv, 0.6 * cc.x, 0, 360, 69, RED);
		DrawCircleSector(cc.rv, 0.58 * cc.x, 0, 360, 69, colour);
	}

	{
		v2 start = { .x = 0, .y = (f32)ws.h / 2 };
		v2 end   = { .x = ws.h, .y = (f32)ws.h / 2 };
		DrawLineEx(start.rv, end.rv, 8, BLUE);
	}

	{
		Rect subregion = {
			.pos  = { .x = 0.05 * (f32)ws.w, .y = (f32)ws.h / 2, },
			.size = { .w = (f32)ws.w * 0.9, .h = (f32)ws.h / 2 * 0.9 },
		};

		Rect sb = subregion;
		sb.size.h *= 0.25;
		ctx->mode  = do_status_bar(sb, ctx->mode, colour, ctx->font);

		Rect r    = subregion;
		r.pos.y  += 0.25 * ((f32)ws.h / 2);
		r.size.h *= 0.15;

		ctx->colour.r = do_slider(r, "R:", ctx->colour.r, ctx->font);

		r.pos.y += r.size.h + 0.03 * ((f32)ws.h / 2);
		ctx->colour.g = do_slider(r, "G:", ctx->colour.g, ctx->font);

		r.pos.y += r.size.h + 0.03 * ((f32)ws.h / 2);
		ctx->colour.b = do_slider(r, "B:", ctx->colour.b, ctx->font);

		r.pos.y += r.size.h + 0.03 * ((f32)ws.h / 2);
		ctx->colour.a = do_slider(r, "A:", ctx->colour.a, ctx->font);
	}
}
