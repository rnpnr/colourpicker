/* See LICENSE for copyright details */
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
	if (target < current) {
		current -= delta;
		if (current < target)
			current = target;
	} else {
		current += delta;
		if (current > target)
			current = target;
	}
	return current;
}

static v4
move_towards_v4(v4 current, v4 target, v4 delta)
{
	return (v4){
		.x = move_towards_f32(current.x, target.x, delta.x),
		.y = move_towards_f32(current.y, target.y, delta.y),
		.z = move_towards_f32(current.z, target.z, delta.z),
		.w = move_towards_f32(current.w, target.w, delta.w),
	};
}

static v4
scaled_sub_v4(v4 a, v4 b, f32 scale)
{
	return (v4){
		.x = scale * (a.x - b.x),
		.y = scale * (a.y - b.y),
		.z = scale * (a.z - b.z),
		.w = scale * (a.w - b.w),
	};
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

static Rect
scale_rect_centered(Rect r, f32 scale)
{
	r.pos.x  += ABS(1 - scale) / 2 * r.size.w;
	r.pos.y  += ABS(1 - scale) / 2 * r.size.h;
	r.size.h *= scale;
	r.size.w *= scale;
	return r;
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

static void
fill_hsv_texture(RenderTexture texture, v4 hsv)
{
	f32 line_length = (f32)texture.texture.height / 3;
	v2 vtop = {0};
	v2 vbot = { .y = vtop.y + line_length };
	v2 sbot = { .y = vbot.y + line_length };
	v2 hbot = { .y = sbot.y + line_length };

	v4 h = hsv, s = hsv, v = hsv;
	h.x = 0;
	s.y = 0;
	v.z = 0;

	f32 inc = 1.0 / texture.texture.width;
	BeginTextureMode(texture);
		for (u32 i = 0; i < texture.texture.width; i++) {
			DrawLineV(sbot.rv, hbot.rv, colour_from_normalized(hsv_to_rgb(h)));
			DrawLineV(vbot.rv, sbot.rv, colour_from_normalized(hsv_to_rgb(s)));
			DrawLineV(vtop.rv, vbot.rv, colour_from_normalized(hsv_to_rgb(v)));
			hbot.x += 1.0;
			sbot.x += 1.0;
			vtop.x += 1.0;
			vbot.x += 1.0;
			h.x    += inc;
			s.y    += inc;
			v.z    += inc;
		}
	EndTextureMode();
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
		switch (ctx->mode) {
		case CPM_HSV: ctx->flags |= CPF_REFILL_TEXTURE; break;
		default: break;
		}
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
		left      = colour_from_normalized(cl);
		right     = colour_from_normalized(cr);
		sel       = colour_from_normalized(ctx->colour);
		DrawRectangleGradientEx(srl.rr, left, left, sel, sel);
		DrawRectangleGradientEx(srr.rr, sel, sel, right, right);
	} else {
		if (label_idx == 3) { /* Alpha */
			Color sel   = colour_from_normalized(hsv_to_rgb(ctx->colour));
			Color left  = sel;
			Color right = sel;
			left.a  = 0;
			right.a = 255;
			DrawRectangleGradientEx(srl.rr, left, left, sel, sel);
			DrawRectangleGradientEx(srr.rr, sel, sel, right, right);
		} else {
			Rect tr    = {0};
			tr.pos.y  += ctx->hsv_texture.texture.height / 3 * label_idx;
			tr.size.h  = sr.size.h;
			tr.size.w  = ctx->hsv_texture.texture.width;
			DrawTexturePro(ctx->hsv_texture.texture, tr.rr, sr.rr, (Vector2){0}, 0, WHITE);
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
		scale     = move_towards_f32(scale, should_scale? scale_target : 1.0, scale_delta);
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
	fpos = right_align_text_in_rect(vr, value, ctx->font, ctx->font_size);
	DrawTextEx(ctx->font, value, fpos.rv, ctx->font_size, 0, ctx->fg);
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
			switch (ctx->mode++) {
			case CPM_HSV:
				ctx->mode    = CPM_RGB;
				ctx->colour  = hsv_to_rgb(ctx->colour);
				break;
			case CPM_RGB:
				ctx->flags  |= CPF_REFILL_TEXTURE;
				ctx->colour  = rgb_to_hsv(ctx->colour);
				break;
			case CPM_LAST: ASSERT(0); break;
			}
		}
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
			switch (ctx->mode--) {
			case CPM_HSV:
				ctx->colour  = hsv_to_rgb(ctx->colour);
				break;
			case CPM_RGB:
				ctx->mode    = CPM_HSV;
				ctx->colour  = rgb_to_hsv(ctx->colour);
				ctx->flags  |= CPF_REFILL_TEXTURE;
				break;
			case CPM_LAST: ASSERT(0); break;
			}
		}
	}

	const char *label = "RGB: ";
	v2 label_size = {.rv = MeasureTextEx(ctx->font, label, ctx->font_size, 0)};
	Rect label_r = cut_rect_left(hex_r,  label_size.x / hex_r.size.w);
	hex_r        = cut_rect_right(hex_r, label_size.x / hex_r.size.w);

	i32 hex_collides = CheckCollisionPointRec(mouse.rv, hex_r.rr);
	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
		const char *new = TextToLower(GetClipboardText());
		u32 r, g, b, a;
		sscanf(new, "%02x%02x%02x%02x", &r, &g, &b, &a);
		ctx->colour.rv = ColorNormalize((Color){ .r = r, .g = g, .b = b, .a = a });
		switch (ctx->mode) {
		case CPM_HSV: ctx->colour = rgb_to_hsv(ctx->colour); break;
		default: break;
		}
	}

	Color hc;
	switch (ctx->mode) {
	case CPM_HSV:  hc = colour_from_normalized(hsv_to_rgb(ctx->colour)); break;
	case CPM_RGB:  hc = colour_from_normalized(ctx->colour);             break;
	case CPM_LAST: ASSERT(0); break;
	}
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
do_colour_stack_item(ColourPickerCtx *ctx, v2 mouse, Rect r, i32 item_idx, b32 fade, f32 dt)
{
	ColourStackState *css = &ctx->colour_stack;
	f32 fade_param = fade? css->fade_param : 0;
	f32 stack_scale_target = (f32)(ARRAY_COUNT(css->items) + 1) / ARRAY_COUNT(css->items);
	f32 stack_scale_delta  = (stack_scale_target - 1) * 8 * dt;

	v4 colour = css->items[item_idx];

	b32 stack_collides = CheckCollisionPointRec(mouse.rv, r.rr);
	if (stack_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		switch (ctx->mode) {
		case CPM_HSV:
			ctx->colour = rgb_to_hsv(colour);
			ctx->flags |= CPF_REFILL_TEXTURE;
			break;
		case CPM_RGB:
			ctx->colour = colour;
			break;
		case CPM_LAST:
			ASSERT(0);
			break;
		}
	}

	css->scales[item_idx] = move_towards_f32(css->scales[item_idx],
	                                         stack_collides? stack_scale_target : 1,
	                                         stack_scale_delta);
	f32 scale = css->scales[item_idx];
	Rect draw_rect = {
		.pos = {
			.x = r.pos.x - (scale - 1) * r.size.w / 2,
			.y = r.pos.y - (scale - 1) * r.size.h / 2 + css->yoff,
		},
		.size = { .x = r.size.w * scale, .y = r.size.h * scale },
	};
	Color disp = colour_from_normalized(colour);
	DrawRectangleRounded(draw_rect.rr, STACK_ROUNDNESS, 0, Fade(disp, 1 - fade_param));
	draw_rect = scale_rect_centered(draw_rect, 0.96);
	DrawRectangleRoundedLinesEx(draw_rect.rr, STACK_ROUNDNESS, 0, 3.0, Fade(BLACK, 1 - fade_param));
}

static void
do_colour_stack(ColourPickerCtx *ctx, Rect sa, f32 dt)
{
	ColourStackState *css = &ctx->colour_stack;

	v2 mouse = { .rv = GetMousePosition() };

	Rect r    = sa;
	r.size.h *= 1.0 / (ARRAY_COUNT(css->items) + 3);
	r.size.w *= 0.5;
	r.pos.x  += (sa.size.w - r.size.w) * 0.5;

	f32 y_pos_delta = r.size.h + 10;

	f32 stack_off_target = -y_pos_delta;
	f32 stack_off_delta  = -stack_off_target * 5 * dt;

	b32 fade_stack = css->fade_param != 1.0f;
	if (fade_stack) {
		Rect draw_rect   = r;
		draw_rect.pos.y += css->yoff;
		r.pos.y += y_pos_delta;
		Color old = Fade(colour_from_normalized(css->last), css->fade_param);
		DrawRectangleRounded(draw_rect.rr, STACK_ROUNDNESS, 0, old);
		DrawRectangleRoundedLinesEx(draw_rect.rr, STACK_ROUNDNESS, 0, 3.0,
		                            Fade(BLACK, css->fade_param));
	}

	u32 loop_items = ARRAY_COUNT(css->items) - 1;
	for (u32 i = 0; i < loop_items; i++) {
		i32 cidx = (css->widx + i) % ARRAY_COUNT(css->items);
		do_colour_stack_item(ctx, mouse, r, cidx, 0, dt);
		r.pos.y += y_pos_delta;
	}

	i32 last_idx = (css->widx + loop_items) % ARRAY_COUNT(css->items);
	do_colour_stack_item(ctx, mouse, r, last_idx, fade_stack, dt);

	css->fade_param = move_towards_f32(css->fade_param, fade_stack? 0 : 1, 8 * dt);
	css->yoff       = move_towards_f32(css->yoff, fade_stack? stack_off_target : 0, stack_off_delta);
	if (css->yoff == stack_off_target) {
		css->fade_param = 1.0f;
		css->yoff       = 0;
	}

	r.pos.y   = sa.pos.y + sa.size.h - r.size.h;
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
		css->fade_param -= 1e-6;
		v4 colour  = ctx->colour;
		switch (ctx->mode) {
		case CPM_HSV: colour = hsv_to_rgb(colour); break;
		default: break;
		}

		css->last = css->items[css->widx];
		css->items[css->widx++] = colour;

		if (css->widx == ARRAY_COUNT(css->items))
			css->widx = 0;
	}
}

static void
do_colour_selector(ColourPickerCtx *ctx, Rect r, f32 dt)
{
	Color colour = {0}, pcolour = colour_from_normalized(ctx->previous_colour);
	switch (ctx->mode) {
	case CPM_HSV:  colour = colour_from_normalized(hsv_to_rgb(ctx->colour)); break;
	case CPM_RGB:  colour = colour_from_normalized(ctx->colour);             break;
	case CPM_LAST: ASSERT(0); break;
	}

	Rect cs[2] = {cut_rect_left(r, 0.5), cut_rect_right(r, 0.5)};
	DrawRectangleRec(cs[0].rr, pcolour);
	DrawRectangleRec(cs[1].rr, colour);

	v2 mouse = { .rv = GetMousePosition() };

	u32 fg_packed_rgba = ctx->fg.r << 24 | ctx->fg.g << 16 | ctx->fg.b << 8 | ctx->fg.a << 0;
	v4 fg     = normalize_colour(fg_packed_rgba);
	f32 scale = 6;
	v4 delta  = scaled_sub_v4(fg, ctx->hover_colour, scale * dt);
	char *labels[2] = {"Revert", "Apply"};

	i32 pressed_idx = -1;
	for (i32 i = 0; i < ARRAY_COUNT(cs); i++) {
		if (CheckCollisionPointRec(mouse.rv, cs[i].rr)) {
			ctx->selection_colours[i] = move_towards_v4(ctx->selection_colours[i],
			                                            ctx->hover_colour, delta);
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
				pressed_idx = i;
		} else {
			ctx->selection_colours[i] = move_towards_v4(ctx->selection_colours[i],
			                                            fg, delta);
		}

		v2 fpos = center_align_text_in_rect(cs[i], labels[i], ctx->font, ctx->font_size);
		v2 pos  = fpos;
		pos.x  += 2;
		pos.y  += 2.5;
		DrawTextEx(ctx->font, labels[i], pos.rv, ctx->font_size, 0, Fade(BLACK, 0.8));
		DrawTextEx(ctx->font, labels[i], fpos.rv, ctx->font_size, 0,
		           colour_from_normalized(ctx->selection_colours[i]));
	}

	DrawRectangleRoundedLinesEx(r.rr, STACK_ROUNDNESS, 0, 12, ctx->bg);
	DrawRectangleRoundedLinesEx(r.rr, STACK_ROUNDNESS, 0, 3, Fade(BLACK, 0.8));

	if (pressed_idx == 0) {
		switch (ctx->mode) {
		case CPM_RGB:
			ctx->colour = ctx->previous_colour;
			break;
		case CPM_HSV:
			ctx->colour  = rgb_to_hsv(ctx->previous_colour);
			ctx->flags  |= CPF_REFILL_TEXTURE;
			break;
		case CPM_LAST: ASSERT(0); break;
		}
	} else if (pressed_idx == 1) {
		switch (ctx->mode) {
		case CPM_RGB:  ctx->previous_colour = ctx->colour;             break;
		case CPM_HSV:  ctx->previous_colour = hsv_to_rgb(ctx->colour); break;
		case CPM_LAST: ASSERT(0); break;
		}
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
		Rect sb = ca;

		ca.size.h *= 0.15;
		f32 y_pad  = 0.55 * ca.size.h;

		sb.size.h *= 0.1;
		sb.pos.y  += upper.size.h - sb.size.h;
		do_status_bar(ctx, sb, dt);

		do_colour_stack(ctx, sa, dt);

		if (ctx->flags & CPF_REFILL_TEXTURE) {
			Rect sr;
			get_slider_subrects(ca, 0, &sr, 0);
			if (ctx->hsv_texture.texture.width != (i32)(sr.size.w + 0.5)) {
				i32 w = sr.size.w + 0.5;
				i32 h = sr.size.h * 3;
				h += 3 - (h % 3);
				UnloadRenderTexture(ctx->hsv_texture);
				ctx->hsv_texture = LoadRenderTexture(w, h);
			}
			fill_hsv_texture(ctx->hsv_texture, ctx->colour);
			ctx->flags &= ~CPF_REFILL_TEXTURE;
		}

		for (i32 i = 0; i < 4; i++) {
			do_slider(ctx, ca, i, dt);
			ca.pos.y += ca.size.h + y_pad;
		}
	}

	{
		Rect cb    = lower;
		cb.size.h *= 0.25;
		cb.pos.y  += 0.02 * lower.size.h;
		do_colour_selector(ctx, cb, dt);
	}
}
