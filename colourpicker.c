/* See LICENSE for copyright details */
#include <emmintrin.h>
#include <immintrin.h>

#include <raylib.h>
#include <stdio.h>

#include "util.c"

static const char *mode_labels[CPM_LAST][4] = {
	[CPM_RGB] = { "R", "G", "B", "A" },
	[CPM_HSV] = { "H", "S", "V", "A" },
};

static f32
move_towards_f32(f32 current, f32 target, f32 delta)
{
	f32 result;
	f32 remaining = target - current;

	if (ABS(remaining) < ABS(delta))
		return target;

	if (target < current)
		result = current - delta;
	else
		result = current + delta;

	return result;
}

static v2
left_align_text_in_rect(Rect r, const char *text, Font font, f32 fontsize)
{
	v2 ts    = { .rv = MeasureTextEx(font, text, fontsize, 0) };
	v2 delta = { .h = r.size.h - ts.h };
	return (v2) { .x = r.pos.x, .y = r.pos.y + 0.5 * delta.h, };
}

static v2
right_align_text_in_rect(Rect r, const char *text, Font font, f32 fontsize)
{
	v2 ts    = { .rv = MeasureTextEx(font, text, fontsize, 0) };
	v2 delta = { .h = r.size.h - ts.h };
	return (v2) {
		.x = r.pos.x + r.size.w - ts.w,
	        .y = r.pos.y + 0.5 * delta.h,
	};
}

static v2
center_align_text_in_rect(Rect r, const char *text, Font font, f32 fontsize)
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
	/* f(k(n))   = V - V*S*max(0, min(k, min(4 - k, 1)))
	 * k(n)      = fmod((n + H * 6), 6)
	 * (R, G, B) = (f(n = 5), f(n = 3), f(n = 1))
	 */

	__m128 H, S, V, k, n, six, four, one, zero;
	six  = _mm_set1_ps(6);
	four = _mm_set1_ps(4);
	one  = _mm_set1_ps(1);
	zero = _mm_set1_ps(0);

	_Alignas(16) f32 nval[4] = {5.0f, 3.0f, 1.0f, 0.0f};
	n = _mm_load_ps(nval);
	H = _mm_set1_ps(hsv.x);
	S = _mm_set1_ps(hsv.y);
	V = _mm_set1_ps(hsv.z);

	k = _mm_add_ps(n, _mm_mul_ps(six, H));
	__m128 rem = _mm_round_ps(_mm_div_ps(k, six), _MM_FROUND_TO_NEG_INF);
	__m128 mod = _mm_sub_ps(k, _mm_mul_ps(rem, six));

	__m128 t = _mm_min_ps(_mm_sub_ps(four, mod), one);
	t = _mm_max_ps(zero, _mm_min_ps(mod, t));
	t = _mm_mul_ps(t, _mm_mul_ps(S, V));

	v4 rgba;
	_mm_storeu_ps(rgba.E, _mm_sub_ps(V, t));
	rgba.a = hsv.a;

	return rgba;
}

static void
fill_hsv_image(Image img, v4 hsv)
{
	f32 param = 0, line_length = (f32)img.height / 3, tmp;
	v2 top = {0}, bottom = {0};
	bottom.y = line_length;

	/* H component */
	for (u32 i = 0; i < img.width; i++) {
		Color rgb = ColorFromHSV(param, hsv.y, hsv.z);
		ImageDrawLineV(&img, top.rv, bottom.rv, rgb);
		top.x    += 1.0;
		bottom.x += 1.0;
		param    += 360.0 / img.width;
	}

	param     = 0.0;
	top.x     = 0.0;
	bottom.x  = 0.0;
	top.y    += line_length;
	bottom.y += line_length;

	/* S component */
	tmp   = hsv.y;
	hsv.y = 0;
	for (u32 i = 0; i < img.width; i++) {
		Color rgb = ColorFromNormalized(hsv_to_rgb(hsv).rv);
		ImageDrawLineV(&img, top.rv, bottom.rv, rgb);
		top.x    += 1.0;
		bottom.x += 1.0;
		hsv.y    += 1.0 / img.width;
	}

	hsv.y     = tmp;
	top.x     = 0.0;
	bottom.x  = 0.0;
	top.y    += line_length;
	bottom.y += line_length;

	/* V component */
	tmp   = hsv.z;
	hsv.z = 0;
	for (u32 i = 0; i < img.width; i++) {
		Color rgb = ColorFromNormalized(hsv_to_rgb(hsv).rv);
		ImageDrawLineV(&img, top.rv, bottom.rv, rgb);
		top.x    += 1.0;
		bottom.x += 1.0;
		hsv.z    += 1.0 / img.width;
	}
}

static void
get_slider_subrects(Rect r, Rect *label, Rect *slider, Rect *value)
{
	if (label) *label = cut_rect_left(r, 0.08);
	if (value) *value = cut_rect_right(r, 0.87);
	if (slider) {
		*slider = cut_rect_middle(r, 0.1, 0.85);
		slider->size.h *= 0.7;
		slider->pos.y  += r.size.h * 0.15;
	}
}

static void
do_slider(ColourPickerCtx *ctx, Rect r, i32 label_idx, f32 dt)
{
	static i32 held_idx = -1;

	Rect lr, sr, vr;
	get_slider_subrects(r, &lr, &sr, &vr);

	const char *label = mode_labels[ctx->mode][label_idx];
	v2 fpos = center_align_text_in_rect(lr, label, ctx->font, ctx->font_size);
	DrawTextEx(ctx->font, label, fpos.rv, ctx->font_size, 0, ctx->fg);

	v2 mouse = { .rv = GetMousePosition() };
	b32 hovering = CheckCollisionPointRec(mouse.rv, sr.rr);

	if (hovering && held_idx == -1)
		held_idx = label_idx;

	if (held_idx != -1) {
		f32 current = ctx->colour.E[held_idx];
		f32 wheel = GetMouseWheelMove();
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			current = (mouse.x - sr.pos.x) / sr.size.w;
		current += wheel / 255;
		CLAMP(current, 0.0, 1.0);
		ctx->colour.E[held_idx] = current;
		if (ctx->mode == CPM_HSV)
			ctx->flags |= CPF_REFILL_TEXTURE;
	}

	if (IsMouseButtonUp(MOUSE_BUTTON_LEFT))
		held_idx = -1;

	f32 current = ctx->colour.E[label_idx];
	Rect srl = cut_rect_left(sr, current);
	Rect srr = cut_rect_right(sr, current);

	if (ctx->mode == CPM_RGB) {
		Color sel, left, right;
		v4 cl     = ctx->colour;
		v4 cr     = ctx->colour;
		cl.E[label_idx] = 0;
		cr.E[label_idx] = 1;
		left      = ColorFromNormalized(cl.rv);
		right     = ColorFromNormalized(cr.rv);
		sel       = ColorFromNormalized(ctx->colour.rv);
		DrawRectangleGradientEx(srl.rr, left, left, sel, sel);
		DrawRectangleGradientEx(srr.rr, sel, sel, right, right);
	} else {
		if (label_idx == 3) { /* Alpha */
			Color sel   = ColorFromNormalized(hsv_to_rgb(ctx->colour).rv);
			Color left  = sel;
			Color right = sel;
			left.a  = 0;
			right.a = 255;
			DrawRectangleGradientEx(srl.rr, left, left, sel, sel);
			DrawRectangleGradientEx(srr.rr, sel, sel, right, right);
		} else {
			Rect tr    = {0};
			tr.pos.y  += ctx->hsv_img.height / 3 * label_idx;
			tr.size.h  = sr.size.h;
			tr.size.w  = ctx->hsv_img.width;
			DrawTexturePro(ctx->hsv_texture, tr.rr, sr.rr, (Vector2){0}, 0, WHITE);
		}
	}

	DrawRectangleRoundedLinesEx(sr.rr, 1, 0, 12, ctx->bg);
	DrawRectangleRoundedLinesEx(sr.rr, 1, 0, 3, Fade(BLACK, 0.8));

	{
		static f32 slider_scale[4] = { 1, 1, 1, 1 };
		f32 scale_target = 1.5;
		f32 scale_delta  = (scale_target - 1.0) * 8 * dt;

		b32 should_scale = (held_idx == -1 && hovering) ||
		                   (held_idx != -1 && label_idx == held_idx);
		f32 scale = slider_scale[label_idx];
		scale     = move_towards_f32(scale,
		                             should_scale? scale_target : 1.0,
		                             scale_delta);
		slider_scale[label_idx] = scale;

		f32 half_tri_w = 8;
		f32 tri_h = 12;
		v2 t_mid = {
			.x = sr.pos.x + current * sr.size.w,
			.y = sr.pos.y,
		};
		v2 t_left = {
			.x = (t_mid.x - scale * half_tri_w),
			.y = (t_mid.y - scale * tri_h),
		};
		v2 t_right = {
			.x = (t_mid.x + scale * half_tri_w),
			.y = (t_mid.y - scale * tri_h),
		};
		DrawTriangle(t_right.rv, t_left.rv, t_mid.rv, ctx->fg);

		t_mid.y   += sr.size.h;
		t_left.y  += sr.size.h + 2 * tri_h * scale;
		t_right.y += sr.size.h + 2 * tri_h * scale;
		DrawTriangle(t_mid.rv, t_left.rv, t_right.rv, ctx->fg);
	}

	const char *value = TextFormat("%0.02f", current);
	fpos = right_align_text_in_rect(vr, value, ctx->font, 0.9 * ctx->font_size);
	DrawTextEx(ctx->font, value, fpos.rv, 0.9 * ctx->font_size, 0, ctx->fg);
}

static void
do_status_bar(ColourPickerCtx *ctx, Rect r, f32 dt)
{
	Rect hex_r  = cut_rect_left(r, 0.5);
	Rect mode_r = cut_rect_right(r, 0.85);
	mode_r.pos.y  += mode_r.size.h * 0.15;
	mode_r.size.h *= 0.7;

	v2 mouse = { .rv = GetMousePosition() };

	b32 mode_collides = CheckCollisionPointRec(mouse.rv, mode_r.rr);
	if (mode_collides) {
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			if (ctx->mode == CPM_HSV) {
				ctx->mode = CPM_RGB;
				ctx->colour = hsv_to_rgb(ctx->colour);
			} else {
				ctx->mode++;
				ctx->flags |= CPF_REFILL_TEXTURE;
				ctx->colour = rgb_to_hsv(ctx->colour);
			}
		}
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
			if (ctx->mode == CPM_RGB) {
				ctx->mode = CPM_HSV;
				ctx->colour = rgb_to_hsv(ctx->colour);
			} else {
				ctx->flags |= CPF_REFILL_TEXTURE;
				ctx->mode--;
				ctx->colour = hsv_to_rgb(ctx->colour);
			}
		}
	}

	const char *label = TextFormat("RGB: ");
	v2 label_size = {.rv = MeasureTextEx(ctx->font, label, ctx->font_size, 0)};
	Rect label_r = cut_rect_left(hex_r,  label_size.x / hex_r.size.w);
	hex_r        = cut_rect_right(hex_r, label_size.x / hex_r.size.w);

	i32 hex_collides = CheckCollisionPointRec(mouse.rv, hex_r.rr);
	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
		const char *new = TextToLower(GetClipboardText());
		u32 r, g, b, a;
		sscanf(new, "%02x%02x%02x%02x", &r, &g, &b, &a);
		ctx->colour.rv = ColorNormalize((Color){ .r = r, .g = g, .b = b, .a = a });
		if (ctx->mode == CPM_HSV)
			ctx->colour = rgb_to_hsv(ctx->colour);
	}

	Color hc;
	if (ctx->mode == CPM_HSV)
		hc = ColorFromNormalized(hsv_to_rgb(ctx->colour).rv);
	else
		hc = ColorFromNormalized(ctx->colour.rv);
	const char *hex = TextFormat("%02x%02x%02x%02x", hc.r, hc.g, hc.b, hc.a);

	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		SetClipboardText(hex);

	static f32 scale_hex = 1.0;
	f32 scale_target = 1.05;
	f32 scale_delta  = (scale_target - 1.0) * 8 * dt;
	scale_hex = move_towards_f32(scale_hex, hex_collides? scale_target : 1.0, scale_delta);

	v2 fpos = left_align_text_in_rect(label_r, label, ctx->font, ctx->font_size);
	DrawTextEx(ctx->font, label, fpos.rv, ctx->font_size, 0, ctx->fg);

	fpos = left_align_text_in_rect(hex_r, hex, ctx->font, scale_hex * ctx->font_size);
	DrawTextEx(ctx->font, hex, fpos.rv, scale_hex * ctx->font_size, 0, ctx->fg);

	static f32 scale_mode = 1.0;
	scale_mode = move_towards_f32(scale_mode, mode_collides? scale_target : 1.0, scale_delta);

	char *mtext;
	switch (ctx->mode) {
	case CPM_RGB:  mtext = "RGB"; break;
	case CPM_HSV:  mtext = "HSV"; break;
	case CPM_LAST: ASSERT(0);     break;
	}
	fpos = right_align_text_in_rect(mode_r, mtext, ctx->font, scale_mode * ctx->font_size);
	DrawTextEx(ctx->font, mtext, fpos.rv, scale_mode * ctx->font_size, 0, ctx->fg);
}

static void
do_colour_stack_item(ColourPickerCtx *ctx, v2 mouse, Rect r, i32 item_idx,
                     f32 fade_param, f32 stack_offset_y, f32 dt)
{
	static f32 stack_scales[ARRAY_COUNT(ctx->colour_stack.items)] = { 1, 1, 1, 1, 1 };
	f32 stack_scale_target = 1.2f;
	f32 stack_scale_delta  = (stack_scale_target - 1) * 8 * dt;

	v4 colour = ctx->colour_stack.items[item_idx];

	b32 stack_collides = CheckCollisionPointRec(mouse.rv, r.rr);
	if (stack_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		if (ctx->mode == CPM_HSV) {
			ctx->colour = rgb_to_hsv(colour);
			ctx->flags |= CPF_REFILL_TEXTURE;
		} else {
			ctx->colour = colour;
		}
	}

	stack_scales[item_idx] = move_towards_f32(stack_scales[item_idx],
	                                          stack_collides? stack_scale_target : 1,
	                                          stack_scale_delta);
	f32 scale = stack_scales[item_idx];
	Rect draw_rect = {
		.pos = {
			.x = r.pos.x - (scale - 1) * r.size.w / 2,
			.y = r.pos.y - (scale - 1) * r.size.h / 2 + stack_offset_y,
		},
		.size = { .x = r.size.w * scale, .y = r.size.h * scale },
	};
	Color disp = ColorFromNormalized(colour.rv);
	DrawRectangleRounded(draw_rect.rr, 1, 0, Fade(disp, 1 - fade_param));
	DrawRectangleRoundedLinesEx(draw_rect.rr, 1, 0, 3.0, Fade(BLACK, 1 - fade_param));
}

static void
do_colour_stack(ColourPickerCtx *ctx, Rect sa, f32 dt)
{
	v2 mouse = { .rv = GetMousePosition() };

	Rect r    = sa;
	r.size.h *= 0.12;
	r.size.w *= 0.7;
	r.pos.x  += sa.size.w * 0.15;
	r.pos.y  += sa.size.h * 0.06;

	static v4  last_colour    = {0};
	static f32 fade_param     = 1.0f;
	static f32 stack_offset_y = 0;
	f32 stack_off_target      = -sa.size.h * 0.16;
	f32 stack_off_delta       = -stack_off_target * 5 * dt;

	b32 fade_stack = fade_param != 1.0f;
	if (fade_stack) {
		Rect draw_rect   = r;
		draw_rect.pos.y += stack_offset_y;
		r.pos.y += sa.size.h * 0.16;
		Color old = Fade(ColorFromNormalized(last_colour.rv), fade_param);
		DrawRectangleRounded(draw_rect.rr, 1, 0, old);
		DrawRectangleRoundedLinesEx(draw_rect.rr, 1, 0, 3.0, Fade(BLACK, fade_param));
	}

	for (u32 i = 0; i < 4; i++) {
		i32 cidx = (ctx->colour_stack.widx + i) % ARRAY_COUNT(ctx->colour_stack.items);
		do_colour_stack_item(ctx, mouse, r, cidx, 0, stack_offset_y, dt);
		r.pos.y += sa.size.h * 0.16;
	}

	i32 last_idx = (ctx->colour_stack.widx + 4) % ARRAY_COUNT(ctx->colour_stack.items);
	do_colour_stack_item(ctx, mouse, r, last_idx, fade_stack? fade_param : 0,
	                     stack_offset_y, dt);

	fade_param     = move_towards_f32(fade_param, fade_stack? 0 : 1, 8 * dt);
	stack_offset_y = move_towards_f32(stack_offset_y, fade_stack? stack_off_target : 0,
	                                  stack_off_delta);
	if (stack_offset_y == stack_off_target) {
		fade_param = 1.0f;
		stack_offset_y = 0;
	}

	r.pos.y = sa.pos.y + sa.size.h - r.size.h;
	r.pos.x  += r.size.w * 0.1;
	r.size.w *= 0.8;

	b32 push_collides = CheckCollisionPointRec(mouse.rv, r.rr);
	static f32 y_shift = 0.0;
	f32 y_target       = -0.2 * r.size.h;
	f32 y_delta        = -y_target * 8 * dt;

	y_shift = move_towards_f32(y_shift, push_collides? y_target : 0, y_delta);

	v2 center = {
		.x = r.pos.x + 0.5 * r.size.w,
		.y = r.pos.y + 0.5 * r.size.h,
	};
	v2 t_top  = {
		.x = center.x,
		.y = center.y - 0.3 * r.size.h + y_shift,
	};
	v2 t_left = {
		.x = center.x - 0.3 * r.size.w - 0.75 * y_shift,
		.y = center.y + 0.3 * r.size.h,
	};
	v2 t_right = {
		.x = center.x + 0.3 * r.size.w + 0.75 * y_shift,
		.y = center.y + 0.3 * r.size.h,
	};

	DrawTriangle(t_top.rv, t_left.rv, t_right.rv, ctx->fg);
	if (push_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		fade_param -= 1e-6;
		v4 colour  = ctx->colour;
		if (ctx->mode == CPM_HSV)
			colour = hsv_to_rgb(colour);

		last_colour = ctx->colour_stack.items[ctx->colour_stack.widx];
		ctx->colour_stack.items[ctx->colour_stack.widx++] = colour;

		if (ctx->colour_stack.widx == ARRAY_COUNT(ctx->colour_stack.items))
			ctx->colour_stack.widx = 0;
	}
}

DEBUG_EXPORT void
do_colour_picker(ColourPickerCtx *ctx)
{
	ClearBackground(ctx->bg);

#ifdef _DEBUG
	DrawFPS(20, 20);
#endif

	f32 dt = GetFrameTime();
	uv2 ws = ctx->window_size;

	v2 pad = { .x = 0.05 * (f32)ws.w, .y = 0.05 * (f32)ws.h };
	Rect upper = {
		.pos  = { .x = pad.x, .y = pad.y },
		.size = { .w = (f32)ws.w * 0.9, .h = (f32)ws.h * 0.9 / 2 },
	};
	Rect lower = {
		.pos  = { .x = pad.x, .y = pad.y + (f32)ws.h * 0.9 / 2, },
		.size = { .w = (f32)ws.w * 0.9, .h = (f32)ws.h * 0.9 / 2 },
	};

	{
		Rect ca = cut_rect_left(upper, 0.8);
		Rect sa = cut_rect_right(upper, 0.8);

		v4 vcolour = ctx->mode == CPM_HSV ? hsv_to_rgb(ctx->colour) : ctx->colour;
		Color colour = ColorFromNormalized(vcolour.rv);
		v2 cc = { .x = ca.pos.x + 0.47 * ca.size.w, .y = ca.pos.y + 0.5 * ca.size.h };
		DrawRing(cc.rv, 0.4 * ca.size.w, 0.42 * ca.size.w, 0, 360, 69, Fade(BLACK, 0.5));
		DrawCircleSector(cc.rv, 0.4 * ca.size.w, 0, 360, 69, colour);

		do_colour_stack(ctx, sa, dt);
	}

	{
		Rect sb = lower;
		sb.size.h *= 0.25;

		do_status_bar(ctx, sb, dt);

		Rect r    = lower;
		r.pos.y  += 0.25 * ((f32)ws.h / 2);
		r.size.h *= 0.15;

		if (ctx->flags & CPF_REFILL_TEXTURE) {
			Rect sr;
			get_slider_subrects(r, 0, &sr, 0);
			if (ctx->hsv_img.width != (i32)(sr.size.w + 0.5)) {
				i32 w = sr.size.w + 0.5;
				i32 h = sr.size.h * 3;
				h += 3 - (h % 3);
				ImageResize(&ctx->hsv_img, w, h);
				UnloadTexture(ctx->hsv_texture);
				ctx->hsv_texture = LoadTextureFromImage(ctx->hsv_img);
			}
			fill_hsv_image(ctx->hsv_img, ctx->colour);
			UpdateTexture(ctx->hsv_texture, ctx->hsv_img.data);
			ctx->flags &= ~CPF_REFILL_TEXTURE;
		}

		for (i32 i = 0; i < 4; i++) {
			do_slider(ctx, r, i, dt);
			r.pos.y += r.size.h + 0.03 * ((f32)ws.h / 2);
		}
	}
}
