/* See LICENSE for copyright details */
#include <stdio.h>
#include <raylib.h>
#include "util.c"

static const char *mode_labels[CPM_LAST][4] = {
	[CPM_RGB] = { "R:", "G:", "B:", "A:" },
	[CPM_HSV] = { "H:", "S:", "V:", "A:" },
};

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

static v4
rgb_to_hsv(v4 rgb)
{
	Vector3 hsv = ColorToHSV(ColorFromNormalized(rgb.rv));
	return (v4){ .x = hsv.x / 360, .y = hsv.y, .z = hsv.z, .w = rgb.a };
}

static v4
hsv_to_rgb(v4 hsv)
{
	Color rgba = ColorFromHSV(hsv.x * 360, hsv.y, hsv.z);
	rgba.a = hsv.a * 255;
	return (v4){ .rv = ColorNormalize(rgba) };
}

static void
draw_slider_rect(Rect r, v4 colour, enum colour_picker_mode mode, i32 idx, Color bg)
{
	Color sel, left, right;
	if (mode != CPM_HSV) {
		v4 cl     = colour;
		v4 cr     = colour;
		cl.E[idx] = 0;
		cr.E[idx] = 1;
		left      = ColorFromNormalized(cl.rv);
		right     = ColorFromNormalized(cr.rv);
		sel       = ColorFromNormalized(colour.rv);
	} else {
		v4 tmp = colour;
		switch (idx) {
		case 0: /* H */
			left  = ColorFromHSV(0,   colour.y, colour.z);
			right = ColorFromHSV(360, colour.y, colour.z);
			break;
		case 1: /* S */
			left  = ColorFromHSV(colour.x * 360, 0, colour.z);
			right = ColorFromHSV(colour.x * 360, 1, colour.z);
			break;
		case 2: /* V */
			left  = ColorFromHSV(colour.x * 360, colour.y, 0);
			right = ColorFromHSV(colour.x * 360, colour.y, 1);
			break;
		case 3:
			tmp.a = 0;
			left  = ColorFromNormalized(hsv_to_rgb(tmp).rv);
			tmp.a = 1;
			right = ColorFromNormalized(hsv_to_rgb(tmp).rv);
			break;
		}
		if (idx != 3) {
			left.a  = colour.a;
			right.a = colour.a;
		}
		sel = ColorFromNormalized(hsv_to_rgb(colour).rv);
	}

	f32 current = colour.E[idx];
	Rect srl = cut_rect_left(r, current);
	Rect srr = cut_rect_right(r, current);
	DrawRectangleGradientEx(srl.rr, left, left, sel, sel);
	DrawRectangleGradientEx(srr.rr, sel, sel, right, right);
	DrawRectangleRoundedLines(r.rr, 1, 0, 12, bg);
}

static void
do_slider(ColourPickerCtx *ctx, Rect r, i32 label_idx)
{
	Rect lr = cut_rect_left(r, 0.08);
	Rect vr = cut_rect_right(r, 0.85);

	Rect sr = cut_rect_middle(r, 0.1, 0.85);
	sr.size.h *= 0.75;
	sr.pos.y  += r.size.h * 0.125;

	const char *label = mode_labels[ctx->mode][label_idx];
	v2 fpos = center_text_in_rect(lr, label, ctx->font, 48);
	DrawTextEx(ctx->font, label, fpos.rv, 48, 0, ctx->fg);

	f32 current = ctx->colour.E[label_idx];
	v2 mouse = { .rv = GetMousePosition() };
	if (CheckCollisionPointRec(mouse.rv, sr.rr)) {
		f32 wheel = GetMouseWheelMove();
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			current = (mouse.x - sr.pos.x) / sr.size.w;
		current += wheel / 255;
		CLAMP(current, 0.0, 1.0);
		ctx->colour.E[label_idx] = current;
	}

	draw_slider_rect(sr, ctx->colour, ctx->mode, label_idx, ctx->bg);

	{
		f32 half_tri_w = 8;
		f32 tri_h = 12;
		v2 t_mid = {
			.x = sr.pos.x + current * sr.size.w,
			.y = sr.pos.y,
		};
		v2 t_left = {
			.x = t_mid.x - half_tri_w,
			.y = t_mid.y - tri_h,
		};
		v2 t_right = {
			.x = t_mid.x + half_tri_w,
			.y = t_mid.y - tri_h,
		};
		DrawTriangle(t_right.rv, t_left.rv, t_mid.rv, ctx->fg);

		t_mid.y   += sr.size.h;
		t_left.y  += sr.size.h + 2 * tri_h;
		t_right.y += sr.size.h + 2 * tri_h;
		DrawTriangle(t_mid.rv, t_left.rv, t_right.rv, ctx->fg);
	}

	const char *value = TextFormat("%0.02f", current);
	fpos = center_text_in_rect(vr, value, ctx->font, 36);
	DrawTextEx(ctx->font, value, fpos.rv, 36, 0, ctx->fg);
}

static void
do_status_bar(ColourPickerCtx *ctx, Rect r)
{
	Rect hex_r  = cut_rect_left(r, 0.5);
	Rect mode_r = cut_rect_right(r, 0.7);

	v2 mouse = { .rv = GetMousePosition() };

	if (CheckCollisionPointRec(mouse.rv, mode_r.rr)) {
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			if (ctx->mode == CPM_HSV) {
				ctx->mode = CPM_RGB;
				ctx->colour = hsv_to_rgb(ctx->colour);
			} else {
				ctx->mode++;
				ctx->colour = rgb_to_hsv(ctx->colour);
			}
		}
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
			if (ctx->mode == CPM_RGB) {
				ctx->mode = CPM_HSV;
				ctx->colour = rgb_to_hsv(ctx->colour);
			} else {
				ctx->mode--;
				ctx->colour = hsv_to_rgb(ctx->colour);
			}
		}
	}

	i32 hex_collides = CheckCollisionPointRec(mouse.rv, hex_r.rr);
	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
		const char *new = TextToLower(GetClipboardText());
		u32 r, g, b, a;
		sscanf(new, "%02x%02x%02x%02x", &r, &g, &b, &a);
		ctx->colour.rv = ColorNormalize((Color){ .r = r, .g = g, .b = b, .a = a });
	}

	Color hc = ColorFromNormalized(ctx->colour.rv);
	const char *hex = TextFormat("%02x%02x%02x%02x", hc.r, hc.g, hc.b, hc.a);
	v2 fpos = center_text_in_rect(hex_r, hex, ctx->font, 48);
	DrawTextEx(ctx->font, hex, fpos.rv, 48, 0, ctx->fg);

	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		SetClipboardText(hex);

	char *mtext;
	switch (ctx->mode) {
	case CPM_RGB:  mtext = "RGB"; break;
	case CPM_HSV:  mtext = "HSV"; break;
	case CPM_LAST: ASSERT(0);     break;
	}
	fpos = center_text_in_rect(mode_r, mtext, ctx->font, 48);
	DrawTextEx(ctx->font, mtext, fpos.rv, 48, 0, ctx->fg);
}

void
do_colour_picker(ColourPickerCtx *ctx)
{
	ClearBackground(ctx->bg);
	DrawFPS(20, 20);

	uv2 ws = ctx->window_size;

	{
		v4 vcolour = ctx->mode == CPM_HSV ? hsv_to_rgb(ctx->colour) : ctx->colour;
		Color colour = ColorFromNormalized(vcolour.rv);
		v2  cc = { .x = (f32)ws.w / 2, .y = (f32)ws.h / 4 };
		DrawCircleSector(cc.rv, 0.6 * cc.x, 0, 360, 69, RED);
		DrawCircleSector(cc.rv, 0.58 * cc.x, 0, 360, 69, colour);
	}

	{
		Rect subregion = {
			.pos  = { .x = 0.05 * (f32)ws.w, .y = (f32)ws.h / 2, },
			.size = { .w = (f32)ws.w * 0.9, .h = (f32)ws.h / 2 * 0.9 },
		};

		Rect sb = subregion;
		sb.size.h *= 0.25;

		do_status_bar(ctx, sb);

		Rect r    = subregion;
		r.pos.y  += 0.25 * ((f32)ws.h / 2);
		r.size.h *= 0.15;

		for (i32 i = 0; i < 4; i++) {
			do_slider(ctx, r, i);
			r.pos.y += r.size.h + 0.03 * ((f32)ws.h / 2);
		}
	}
}
