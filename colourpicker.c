/* See LICENSE for copyright details */
#include <raylib.h>
#include <stdio.h>
#include <string.h> /* memmove */

#include "util.c"

static const char *mode_labels[CM_LAST][4] = {
	[CM_RGB] = { "R", "G", "B", "A" },
	[CM_HSV] = { "H", "S", "V", "A" },
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
lerp_v4(v4 a, v4 b, f32 t)
{
	return (v4){
		.x = a.x + t * (b.x - a.x),
		.y = a.y + t * (b.y - a.y),
		.z = a.z + t * (b.z - a.z),
		.w = a.w + t * (b.w - a.w),
	};
}

static v2
left_align_text_in_rect(Rect r, const char *text, Font font, f32 fontsize)
{
	v2 ts    = { .rv = MeasureTextEx(font, text, fontsize, 0) };
	v2 delta = { .h = r.size.h - ts.h };
	return (v2) { .x = r.pos.x, .y = r.pos.y + 0.5 * delta.h, };
}

static Rect
scale_rect_centered(Rect r, v2 scale)
{
	Rect or   = r;
	r.size.w *= scale.x;
	r.size.h *= scale.y;
	r.pos.x  += (or.size.w - r.size.w) / 2;
	r.pos.y  += (or.size.h - r.size.h) / 2;
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
draw_cardinal_triangle(v2 midpoint, v2 size, v2 scale, enum cardinal_direction direction,
                       Color colour)
{
	v2 t1, t2;
	switch (direction) {
	case NORTH:
		t1.x = midpoint.x - scale.x * size.x;
		t1.y = midpoint.y + scale.y * size.y;
		t2.x = midpoint.x + scale.x * size.x;
		t2.y = midpoint.y + scale.y * size.y;
		break;
	case EAST:
		t1.x = midpoint.x - scale.x * size.y;
		t1.y = midpoint.y - scale.y * size.x;
		t2.x = midpoint.x - scale.x * size.y;
		t2.y = midpoint.y + scale.y * size.x;
		break;
	case SOUTH:
		t1.x = midpoint.x + scale.x * size.x;
		t1.y = midpoint.y - scale.y * size.y;
		t2.x = midpoint.x - scale.x * size.x;
		t2.y = midpoint.y - scale.y * size.y;
		break;
	case WEST:
		t1.x = midpoint.x + scale.x * size.y;
		t1.y = midpoint.y + scale.y * size.x;
		t2.x = midpoint.x + scale.x * size.y;
		t2.y = midpoint.y - scale.y * size.x;
		break;
	default: ASSERT(0); return;
	}
	DrawTriangle(midpoint.rv, t1.rv, t2.rv, colour);

	#if 0
	DrawCircleV(midpoint.rv, 6, RED);
	DrawCircleV(t1.rv, 6, BLUE);
	DrawCircleV(t2.rv, 6, GREEN);
	#endif
}

static v4
get_formatted_colour(ColourPickerCtx *ctx, enum colour_mode format)
{
	switch (ctx->colour_mode) {
	case CM_RGB:
		switch (format) {
		case CM_RGB:  return ctx->colour;
		case CM_HSV:  return rgb_to_hsv(ctx->colour);
		case CM_LAST: ASSERT(0); break;
		}
		break;
	case CM_HSV:
		switch (format) {
		case CM_RGB:  return hsv_to_rgb(ctx->colour);
		case CM_HSV:  return ctx->colour;
		case CM_LAST: ASSERT(0); break;
		}
		break;
	case CM_LAST: ASSERT(0); break;
	}
	return (v4){0};
}

static void
store_formatted_colour(ColourPickerCtx *ctx, v4 colour, enum colour_mode format)
{
	switch (ctx->colour_mode) {
	case CM_RGB:
		switch (format) {
		case CM_RGB:  ctx->colour = colour;             break;
		case CM_HSV:  ctx->colour = hsv_to_rgb(colour); break;
		case CM_LAST: ASSERT(0);                        break;
		}
		break;
	case CM_HSV:
		switch (format) {
		case CM_RGB:  ctx->colour = rgb_to_hsv(colour); break;
		case CM_HSV:  ctx->colour = colour;             break;
		case CM_LAST: ASSERT(0);                        break;
		}
		ctx->flags |= CPF_REFILL_TEXTURE;
		break;
	case CM_LAST: ASSERT(0); break;
	}
}

static void
step_colour_mode(ColourPickerCtx *ctx, i32 inc)
{
	ASSERT(inc == 1 || inc == -1);

	enum colour_mode last_mode = ctx->colour_mode;

	ctx->colour_mode += inc;
	CLAMP(ctx->colour_mode, CM_RGB, CM_LAST);
	if (ctx->colour_mode == CM_LAST) {
		if (inc == 1) ctx->colour_mode = 0;
		else          ctx->colour_mode = CM_LAST + inc;
	}

	store_formatted_colour(ctx, ctx->colour, last_mode);
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
	if (value) *value = cut_rect_right(r, 0.83);
	if (slider) {
		*slider = cut_rect_middle(r, 0.1, 0.79);
		slider->size.h *= 0.7;
		slider->pos.y  += r.size.h * 0.15;
	}
}

static void
parse_and_store_text_input(ColourPickerCtx *ctx)
{

	v4   new_colour = {0};
	enum colour_mode new_mode = CM_LAST;
	if (ctx->is.idx == INPUT_HEX) {
		new_colour = normalize_colour(parse_hex_u32(ctx->is.buf));
		new_mode   = CM_RGB;
	} else {
		new_mode   = ctx->colour_mode;
		new_colour = ctx->colour;
		f32 fv = parse_f32(ctx->is.buf);
		CLAMP01(fv);
		switch(ctx->is.idx) {
		case INPUT_R: new_colour.r = fv; break;
		case INPUT_G: new_colour.g = fv; break;
		case INPUT_B: new_colour.b = fv; break;
		case INPUT_A: new_colour.a = fv; break;
		default: break;
		}
	}

	if (new_mode != CM_LAST)
		store_formatted_colour(ctx, new_colour, new_mode);
}

static void
set_text_input_idx(ColourPickerCtx *ctx, enum input_indices idx, Rect r, v2 mouse)
{
	if (ctx->is.idx != idx)
		parse_and_store_text_input(ctx);

	if (idx == INPUT_HEX) {
		Color hc = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
		ctx->is.buf_len = snprintf(ctx->is.buf, ARRAY_COUNT(ctx->is.buf),
		                           "%02x%02x%02x%02x", hc.r, hc.g, hc.b, hc.a);
	} else {
		f32 fv = 0;
		switch (idx) {
		case INPUT_R: fv = ctx->colour.r; break;
		case INPUT_G: fv = ctx->colour.g; break;
		case INPUT_B: fv = ctx->colour.b; break;
		case INPUT_A: fv = ctx->colour.a; break;
		default: break;
		}
		ctx->is.buf_len = snprintf(ctx->is.buf, ARRAY_COUNT(ctx->is.buf), "%0.02f", fv);
	}

	ctx->is.idx    = idx;
	ctx->is.cursor = -1;
	CLAMP(ctx->is.idx, -1, INPUT_A);
	if (ctx->is.idx == -1)
		return;

	ASSERT(CheckCollisionPointRec(mouse.rv, r.rr));
	ctx->is.cursor_hover_p = (mouse.x - r.pos.x) / r.size.w;
	CLAMP01(ctx->is.cursor_hover_p);
}

static void
do_text_input(ColourPickerCtx *ctx, Rect r, Color colour)
{
	static i32 max_chars[INPUT_A + 1] = {
		[INPUT_HEX] = 8,
		[INPUT_R]   = 4,
		[INPUT_B]   = 4,
		[INPUT_G]   = 4,
		[INPUT_A]   = 4,
	};

	v2 ts  = {.rv = MeasureTextEx(ctx->font, ctx->is.buf, ctx->font_size, 0)};
	v2 pos = {.x = r.pos.x, .y = r.pos.y + (r.size.y - ts.y) / 2};

	i32 buf_delta = ctx->is.buf_len - max_chars[ctx->is.idx];
	if (buf_delta < 0) buf_delta = 0;
	char *buf     = ctx->is.buf + buf_delta;
	DrawTextEx(ctx->font, buf, pos.rv, ctx->font_size, 0, colour);

	ctx->is.cursor_t = move_towards_f32(ctx->is.cursor_t, ctx->is.cursor_t_target,
	                                    1.5 * ctx->dt);
	if (ctx->is.cursor_t == ctx->is.cursor_t_target) {
		if (ctx->is.cursor_t_target == 0) ctx->is.cursor_t_target = 1;
		else                              ctx->is.cursor_t_target = 0;
	}

	v4 bg = ctx->cursor_colour;
	bg.a  = 0;
	Color cursor_colour = colour_from_normalized(lerp_v4(bg, ctx->cursor_colour,
	                                                     ctx->is.cursor_t));

	/* NOTE: guess a cursor position */
	if (ctx->is.cursor == -1) {
		/* NOTE: extra offset to help with putting a cursor at idx 0 */
		#define TEXT_HALF_CHAR_WIDTH 10
		f32 x_off = TEXT_HALF_CHAR_WIDTH, x_bounds = r.size.w * ctx->is.cursor_hover_p;
		u32 i;
		for (i = 0; i < ctx->is.buf_len && x_off < x_bounds; i++) {
			u32 idx = GetGlyphIndex(ctx->font, ctx->is.buf[i]);
			if (ctx->font.glyphs[idx].advanceX == 0)
				x_off += ctx->font.recs[idx].width;
			else
				x_off += ctx->font.glyphs[idx].advanceX;
		}
		ctx->is.cursor = i;
	}

	/* NOTE: Braindead NULL termination stupidity */
	char saved_c = buf[ctx->is.cursor - buf_delta];
	buf[ctx->is.cursor - buf_delta] = 0;

	v2 sts           = {.rv = MeasureTextEx(ctx->font, buf, ctx->font_size, 0)};
	f32 cursor_x     = r.pos.x + sts.x;
	f32 cursor_width = ctx->is.cursor == ctx->is.buf_len ? 20 : 6;

	buf[ctx->is.cursor - buf_delta] = saved_c;

	Rect cursor_r = {
		.pos  = {.x = cursor_x,     .y = pos.y},
		.size = {.w = cursor_width, .h = ts.h},
	};

	DrawRectangleRec(cursor_r.rr, cursor_colour);

	/* NOTE: handle multiple input keys on a single frame */
	i32 key = GetCharPressed();
	while (key > 0) {
		if (ctx->is.buf_len == (ARRAY_COUNT(ctx->is.buf) - 1)) {
			ctx->is.buf[ARRAY_COUNT(ctx->is.buf) - 1] = 0;
			break;
		}

		/* TODO: remove memmove */
		memmove(ctx->is.buf + ctx->is.cursor + 1,
		        ctx->is.buf + ctx->is.cursor,
		        ctx->is.buf_len - ctx->is.cursor + 1);

		ctx->is.buf[ctx->is.cursor++] = key;
		ctx->is.buf_len++;
		key = GetCharPressed();
	}

	if ((IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) && ctx->is.cursor > 0)
		ctx->is.cursor--;

	if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) &&
	    ctx->is.cursor < ctx->is.buf_len)
		ctx->is.cursor++;

	if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
	    ctx->is.cursor > 0) {
		/* TODO: remove memmove */
		ctx->is.cursor--;
		memmove(ctx->is.buf + ctx->is.cursor,
		        ctx->is.buf + ctx->is.cursor + 1,
			ctx->is.buf_len - ctx->is.cursor - 1);
		ctx->is.buf[--ctx->is.buf_len] = 0;
	}

	if (IsKeyPressed(KEY_ENTER)) {
		parse_and_store_text_input(ctx);
		ctx->is.idx = -1;
	}
}

static void
do_slider(ColourPickerCtx *ctx, Rect r, i32 label_idx, v2 relative_origin)
{
	Rect lr, sr, vr;
	get_slider_subrects(r, &lr, &sr, &vr);

	const char *label = mode_labels[ctx->colour_mode][label_idx];
	v2 fpos = center_align_text_in_rect(lr, label, ctx->font, ctx->font_size);
	DrawTextEx(ctx->font, label, fpos.rv, ctx->font_size, 0, ctx->fg);

	v2 test_pos = ctx->mouse_pos;
	test_pos.x -= relative_origin.x;
	test_pos.y -= relative_origin.y;
	b32 hovering = CheckCollisionPointRec(test_pos.rv, sr.rr);

	if (hovering && ctx->held_idx == -1)
		ctx->held_idx = label_idx;

	if (ctx->held_idx != -1) {
		f32 current = ctx->colour.E[ctx->held_idx];
		f32 wheel = GetMouseWheelMove();
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			current = (test_pos.x - sr.pos.x) / sr.size.w;
		current += wheel / 255;
		CLAMP01(current);
		ctx->colour.E[ctx->held_idx] = current;
		ctx->flags |= CPF_REFILL_TEXTURE;
	}

	if (IsMouseButtonUp(MOUSE_BUTTON_LEFT))
		ctx->held_idx = -1;

	f32 current = ctx->colour.E[label_idx];
	Rect srl = cut_rect_left(sr, current);
	Rect srr = cut_rect_right(sr, current);

	/* TODO: this should probably be a switch */
	if (ctx->colour_mode == CM_RGB) {
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

	DrawRectangleRoundedLinesEx(sr.rr, SLIDER_ROUNDNESS, 0, 4 * SLIDER_BORDER_WIDTH, ctx->bg);
	DrawRectangleRoundedLinesEx(sr.rr, SLIDER_ROUNDNESS, 0, SLIDER_BORDER_WIDTH,
	                            SLIDER_BORDER_COLOUR);

	{
		f32 scale_delta  = (SLIDER_SCALE_TARGET - 1.0) * SLIDER_SCALE_SPEED * ctx->dt;
		b32 should_scale = (ctx->held_idx == -1 && hovering) ||
		                   (ctx->held_idx != -1 && label_idx == ctx->held_idx);
		f32 scale = ctx->ss.scale_t[label_idx];
		scale     = move_towards_f32(scale, should_scale? SLIDER_SCALE_TARGET: 1.0,
		                             scale_delta);
		ctx->ss.scale_t[label_idx] = scale;

		v2 tri_scale = {.x = scale, .y = scale};
		v2 tri_mid   = {.x = sr.pos.x + current * sr.size.w, .y = sr.pos.y};
		draw_cardinal_triangle(tri_mid, SLIDER_TRI_SIZE, tri_scale, SOUTH, ctx->fg);
		tri_mid.y   += sr.size.h;
		draw_cardinal_triangle(tri_mid, SLIDER_TRI_SIZE, tri_scale, NORTH, ctx->fg);
	}

	{
		SliderState *s = &ctx->ss;
		b32 collides = CheckCollisionPointRec(test_pos.rv, vr.rr);
		if (collides && ctx->is.idx != (label_idx + 1)) {
			s->colour_t[label_idx] += TEXT_HOVER_SPEED * ctx->dt;
		} else {
			s->colour_t[label_idx] -= TEXT_HOVER_SPEED * ctx->dt;
		}
		CLAMP01(s->colour_t[label_idx]);

		if (!collides && ctx->is.idx == (label_idx + 1) &&
		    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			set_text_input_idx(ctx, -1, vr, test_pos);
			current = ctx->colour.E[label_idx];
		}

		v4 colour       = lerp_v4(normalize_colour(pack_rl_colour(ctx->fg)),
		                          ctx->hover_colour, s->colour_t[label_idx]);
		Color colour_rl = colour_from_normalized(colour);

		if (collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			set_text_input_idx(ctx, label_idx + 1, vr, test_pos);

		if (ctx->is.idx != (label_idx + 1)) {
			const char *value = TextFormat("%0.02f", current);
			fpos = left_align_text_in_rect(vr, value, ctx->font, ctx->font_size);
			DrawTextEx(ctx->font, value, fpos.rv, ctx->font_size, 0, colour_rl);
		} else {
			do_text_input(ctx, vr, colour_rl);
		}
	}
}

static void
do_status_bar(ColourPickerCtx *ctx, Rect r, v2 relative_origin)
{
	Rect hex_r  = cut_rect_left(r, 0.5);
	Rect mode_r;
	get_slider_subrects(r, 0, 0, &mode_r);

	char *mode_txt;
	switch (ctx->colour_mode) {
	case CM_RGB:  mode_txt = "RGB"; break;
	case CM_HSV:  mode_txt = "HSV"; break;
	case CM_LAST: ASSERT(0);        break;
	}
	v2 mode_ts     = {.rv = MeasureTextEx(ctx->font, mode_txt, ctx->font_size, 0)};
	mode_r.pos.y  += (mode_r.size.h - mode_ts.h) / 2;
	mode_r.size.w  = mode_ts.w;

	v2 test_pos  = ctx->mouse_pos;
	test_pos.x  -= relative_origin.x;
	test_pos.y  -= relative_origin.y;
	b32 mode_collides = CheckCollisionPointRec(test_pos.rv, mode_r.rr);
	if (mode_collides) {
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			step_colour_mode(ctx, 1);
		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
			step_colour_mode(ctx, -1);
	}

	Color hc          = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
	const char *hex   = TextFormat("%02x%02x%02x%02x", hc.r, hc.g, hc.b, hc.a);
	const char *label = "RGB: ";

	v2 label_size = {.rv = MeasureTextEx(ctx->font, label, ctx->font_size, 0)};
	v2 hex_size   = {.rv = MeasureTextEx(ctx->font, hex,   ctx->font_size, 0)};

	f32 extra_input_scale = 1.07;
	hex_r.size.w = extra_input_scale * (label_size.w + hex_size.w);

	Rect label_r = cut_rect_left(hex_r,  label_size.x / hex_r.size.w);
	hex_r        = cut_rect_right(hex_r, label_size.x / hex_r.size.w);

	i32 hex_collides = CheckCollisionPointRec(test_pos.rv, hex_r.rr);

	if (!hex_collides && ctx->is.idx == INPUT_HEX &&
	    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		set_text_input_idx(ctx, -1, hex_r, test_pos);
		hc  = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
		hex = TextFormat("%02x%02x%02x%02x", hc.r, hc.g, hc.b, hc.a);
	}

	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		set_text_input_idx(ctx, INPUT_HEX, hex_r, test_pos);

	if (hex_collides && ctx->is.idx != INPUT_HEX)
		ctx->sbs.hex_hover_t += TEXT_HOVER_SPEED * ctx->dt;
	else
		ctx->sbs.hex_hover_t -= TEXT_HOVER_SPEED * ctx->dt;

	if (mode_collides) ctx->sbs.mode_hover_t += TEXT_HOVER_SPEED * ctx->dt;
	else               ctx->sbs.mode_hover_t -= TEXT_HOVER_SPEED * ctx->dt;

	CLAMP01(ctx->sbs.hex_hover_t);
	CLAMP01(ctx->sbs.mode_hover_t);

	v4 fg          = normalize_colour(pack_rl_colour(ctx->fg));
	v4 hex_colour  = lerp_v4(fg, ctx->hover_colour, ctx->sbs.hex_hover_t);
	v4 mode_colour = lerp_v4(fg, ctx->hover_colour, ctx->sbs.mode_hover_t);

	v2 fpos = left_align_text_in_rect(label_r, label, ctx->font, ctx->font_size);
	DrawTextEx(ctx->font, label, fpos.rv, ctx->font_size, 0, ctx->fg);

	Color hex_colour_rl = colour_from_normalized(hex_colour);
	if (ctx->is.idx != INPUT_HEX) {
		fpos = left_align_text_in_rect(hex_r, hex, ctx->font, ctx->font_size);
		DrawTextEx(ctx->font, hex, fpos.rv, ctx->font_size, 0, hex_colour_rl);
	} else {
		do_text_input(ctx, hex_r, hex_colour_rl);
	}

	DrawTextEx(ctx->font, mode_txt, mode_r.pos.rv, ctx->font_size, 0,
	           colour_from_normalized(mode_colour));
}

static void
do_colour_stack_item(ColourPickerCtx *ctx, Rect r, i32 item_idx, b32 fade)
{
	ColourStackState *css = &ctx->colour_stack;
	f32 fade_param = fade? css->fade_param : 0;
	f32 stack_scale_target = (f32)(ARRAY_COUNT(css->items) + 1) / ARRAY_COUNT(css->items);
	f32 stack_scale_delta  = (stack_scale_target - 1) * 8 * ctx->dt;

	v4 colour = css->items[item_idx];

	b32 stack_collides = CheckCollisionPointRec(ctx->mouse_pos.rv, r.rr);
	if (stack_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		v4 hsv = rgb_to_hsv(colour);
		store_formatted_colour(ctx, hsv, CM_HSV);
		if (ctx->mode == CPM_PICKER) {
			ctx->pms.base_hue       = hsv.x;
			ctx->pms.fractional_hue = 0;
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
	draw_rect = scale_rect_centered(draw_rect, (v2){.x = 0.96, .y = 0.96});
	DrawRectangleRoundedLinesEx(draw_rect.rr, STACK_ROUNDNESS, 0, STACK_BORDER_WIDTH,
	                            Fade(STACK_BORDER_COLOUR, 1 - fade_param));
}

static void
do_colour_stack(ColourPickerCtx *ctx, Rect sa)
{
	ColourStackState *css = &ctx->colour_stack;

	/* NOTE: Small adjusment to align with mode text. TODO: Cleanup? */
	sa.pos.y  += 0.025 * sa.size.h;
	sa.size.h *= 0.98;

	Rect r    = sa;
	r.size.h *= 1.0 / (ARRAY_COUNT(css->items) + 3);
	r.size.w *= 0.75;
	r.pos.x  += (sa.size.w - r.size.w) * 0.5;

	f32 y_pos_delta = r.size.h + 10;

	f32 stack_off_target = -y_pos_delta;
	f32 stack_off_delta  = -stack_off_target * 5 * ctx->dt;

	b32 fade_stack = css->fade_param != 1.0f;
	if (fade_stack) {
		Rect draw_rect   = r;
		draw_rect.pos.y += css->yoff;
		r.pos.y += y_pos_delta;
		Color old = Fade(colour_from_normalized(css->last), css->fade_param);
		DrawRectangleRounded(draw_rect.rr, STACK_ROUNDNESS, 0, old);
		DrawRectangleRoundedLinesEx(draw_rect.rr, STACK_ROUNDNESS, 0, STACK_BORDER_WIDTH,
		                            Fade(STACK_BORDER_COLOUR, css->fade_param));
	}

	u32 loop_items = ARRAY_COUNT(css->items) - 1;
	for (u32 i = 0; i < loop_items; i++) {
		i32 cidx = (css->widx + i) % ARRAY_COUNT(css->items);
		do_colour_stack_item(ctx, r, cidx, 0);
		r.pos.y += y_pos_delta;
	}

	i32 last_idx = (css->widx + loop_items) % ARRAY_COUNT(css->items);
	do_colour_stack_item(ctx, r, last_idx, fade_stack);

	css->fade_param = move_towards_f32(css->fade_param, fade_stack? 0 : 1, 8 * ctx->dt);
	css->yoff       = move_towards_f32(css->yoff, fade_stack? stack_off_target : 0, stack_off_delta);
	if (css->yoff == stack_off_target) {
		css->fade_param = 1.0f;
		css->yoff       = 0;
	}

	r.pos.y   = sa.pos.y + sa.size.h - r.size.h;
	r.pos.x  += r.size.w * 0.1;
	r.size.w *= 0.8;

	b32 push_collides  = CheckCollisionPointRec(ctx->mouse_pos.rv, r.rr);
	static f32 param   = 0.0;
	param = move_towards_f32(param, push_collides? 1 : 0, 8 * ctx->dt);

	v2 tri_size  = {.x = 0.25 * r.size.w,          .y = 0.5 * r.size.h};
	v2 tri_scale = {.x = 1 - 0.5 * param,          .y = 1 + 0.3 * param};
	v2 tri_mid   = {.x = r.pos.x + 0.5 * r.size.w, .y = r.pos.y - 0.3 * r.size.h * param};
	draw_cardinal_triangle(tri_mid, tri_size, tri_scale, NORTH, ctx->fg);

	if (push_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		css->fade_param         -= 1e-6;
		css->last                = css->items[css->widx];
		css->items[css->widx++]  = get_formatted_colour(ctx, CM_RGB);
		if (css->widx == ARRAY_COUNT(css->items))
			css->widx = 0;
	}
}

static void
do_colour_selector(ColourPickerCtx *ctx, Rect r)
{
	Color colour  = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
	Color pcolour = colour_from_normalized(ctx->previous_colour);

	Rect cs[2] = {cut_rect_left(r, 0.5), cut_rect_right(r, 0.5)};
	DrawRectangleRec(cs[0].rr, pcolour);
	DrawRectangleRec(cs[1].rr, colour);

	v4 fg     = normalize_colour(pack_rl_colour(ctx->fg));
	char *labels[2] = {"Revert", "Apply"};

	i32 pressed_idx = -1;
	for (i32 i = 0; i < ARRAY_COUNT(cs); i++) {
		if (CheckCollisionPointRec(ctx->mouse_pos.rv, cs[i].rr) && ctx->held_idx == -1) {
			ctx->selection_hover_t[i] += TEXT_HOVER_SPEED * ctx->dt;

			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
				pressed_idx = i;
		} else {
			ctx->selection_hover_t[i] -= TEXT_HOVER_SPEED * ctx->dt;
		}

		CLAMP01(ctx->selection_hover_t[i]);

		v4 colour = lerp_v4(fg, ctx->hover_colour, ctx->selection_hover_t[i]);

		v2 fpos = center_align_text_in_rect(cs[i], labels[i], ctx->font, ctx->font_size);
		v2 pos  = fpos;
		pos.x  += 1.75;
		pos.y  += 2;
		DrawTextEx(ctx->font, labels[i], pos.rv, ctx->font_size, 0, Fade(BLACK, 0.8));
		DrawTextEx(ctx->font, labels[i], fpos.rv, ctx->font_size, 0,
		           colour_from_normalized(colour));
	}

	DrawRectangleRoundedLinesEx(r.rr, SELECTOR_ROUNDNESS, 0, 4 * SELECTOR_BORDER_WIDTH, ctx->bg);
	DrawRectangleRoundedLinesEx(r.rr, SELECTOR_ROUNDNESS, 0, SELECTOR_BORDER_WIDTH,
	                            SELECTOR_BORDER_COLOUR);
	v2 start  = cs[1].pos;
	v2 end    = cs[1].pos;
	end.y    += cs[1].size.h;
	DrawLineEx(start.rv, end.rv, SELECTOR_BORDER_WIDTH, SELECTOR_BORDER_COLOUR);

	if      (pressed_idx == 0) store_formatted_colour(ctx, ctx->previous_colour, CM_RGB);
	else if (pressed_idx == 1) ctx->previous_colour = get_formatted_colour(ctx, CM_RGB);

	if (pressed_idx != -1) {
		ctx->pms.base_hue       = get_formatted_colour(ctx, CM_HSV).x;
		ctx->pms.fractional_hue = 0;
	}
}

static void
do_slider_mode(ColourPickerCtx *ctx, v2 relative_origin)
{
	Rect tr  = {
		.size = {
			.w = ctx->picker_texture.texture.width,
			.h = ctx->picker_texture.texture.height
		}
	};
	Rect sb    = tr;
	Rect ss    = tr;
	ss.size.h *= 0.15;

	if (ctx->flags & CPF_REFILL_TEXTURE) {
		Rect sr;
		get_slider_subrects(ss, 0, &sr, 0);
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

	BeginTextureMode(ctx->slider_texture);
	ClearBackground(ctx->bg);

	sb.size.h *= 0.1;
	do_status_bar(ctx, sb, relative_origin);

	ss.pos.y   += 1.2 * sb.size.h;
	f32 y_pad  = 0.525 * ss.size.h;

	for (i32 i = 0; i < 4; i++) {
		do_slider(ctx, ss, i, relative_origin);
		ss.pos.y += ss.size.h + y_pad;
	}

	EndTextureMode();
}


#define PM_LEFT   0
#define PM_MIDDLE 1
#define PM_RIGHT  2

static v4
do_vertical_slider(ColourPickerCtx *ctx, v2 test_pos, Rect r, i32 idx,
                   v4 bot_colour, v4 top_colour, v4 colour)
{
	v4 hsv[] = {bot_colour, top_colour};

	b32 hovering = CheckCollisionPointRec(test_pos.rv, r.rr);

	BeginShaderMode(ctx->picker_shader);

	i32 mode = 0;
	SetShaderValue(ctx->picker_shader, ctx->mode_id, &mode, SHADER_UNIFORM_INT);
	SetShaderValue(ctx->picker_shader, ctx->size_id, r.size.E, SHADER_UNIFORM_VEC2);
	SetShaderValueV(ctx->picker_shader, ctx->hsv_id, (f32 *)hsv, SHADER_UNIFORM_VEC4, 2);

	if (hovering && ctx->held_idx == -1) {
		colour.x -= GetMouseWheelMove() * (bot_colour.x - top_colour.x) / 36;
		CLAMP(colour.x, top_colour.x, bot_colour.x);
	}

	if (hovering && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ctx->held_idx == -1)
		ctx->held_idx = idx;

	if (ctx->held_idx == idx) {
		CLAMP(test_pos.y, r.pos.y, r.pos.y + r.size.h);
		f32 new_t = (test_pos.y - r.pos.y) / r.size.h;
		colour.x  = top_colour.x + new_t * (bot_colour.x - top_colour.x);
	}

	DrawRectangleRec(r.rr, BLACK);

	EndShaderMode();

	DrawRectangleRoundedLinesEx(r.rr, SLIDER_ROUNDNESS, 0, 4 * SLIDER_BORDER_WIDTH, ctx->bg);
	DrawRectangleRoundedLinesEx(r.rr, SLIDER_ROUNDNESS, 0, SLIDER_BORDER_WIDTH,
	                            SLIDER_BORDER_COLOUR);

	f32 param = (colour.x - top_colour.x) / (bot_colour.x - top_colour.x);
	{
		/* TODO: move this to ctx */
		static f32 slider_scale[2] = { 1, 1 };
		f32 scale_delta  = (SLIDER_SCALE_TARGET - 1.0f) * SLIDER_SCALE_SPEED * ctx->dt;

		b32 should_scale = (ctx->held_idx == -1 && hovering) ||
		                   (ctx->held_idx != -1 && ctx->held_idx == idx);
		f32 scale = slider_scale[idx];
		scale     = move_towards_f32(scale, should_scale? SLIDER_SCALE_TARGET : 1.0,
		                             scale_delta);
		slider_scale[idx] = scale;

		v2 tri_scale = {.x = scale, .y = scale};
		v2 tri_mid   = {.x = r.pos.x, .y = r.pos.y + (param * r.size.h)};
		draw_cardinal_triangle(tri_mid, SLIDER_TRI_SIZE, tri_scale, EAST, ctx->fg);
		tri_mid.x   += r.size.w;
		draw_cardinal_triangle(tri_mid, SLIDER_TRI_SIZE, tri_scale, WEST, ctx->fg);
	}

	return colour;
}

static void
do_picker_mode(ColourPickerCtx *ctx, v2 relative_origin)
{
	if (!IsShaderReady(ctx->picker_shader)) {
#ifdef _DEBUG
		ctx->picker_shader = LoadShader(0, HSV_LERP_SHADER_NAME);
#else
		ctx->picker_shader = LoadShaderFromMemory(0, g_hsv_shader_text);
#endif
		ctx->mode_id       = GetShaderLocation(ctx->picker_shader, "u_mode");
		ctx->hsv_id        = GetShaderLocation(ctx->picker_shader, "u_hsv");
		ctx->size_id       = GetShaderLocation(ctx->picker_shader, "u_size");
		ctx->offset_id     = GetShaderLocation(ctx->picker_shader, "u_offset");
	}

	v4 colour = get_formatted_colour(ctx, CM_HSV);
	colour.x  = ctx->pms.base_hue + ctx->pms.fractional_hue;

	Rect tr  = {
		.size = {
			.w = ctx->picker_texture.texture.width,
			.h = ctx->picker_texture.texture.height
		}
	};

	Rect hs1 = cut_rect_left(tr, 0.2);
	Rect hs2 = cut_rect_middle(tr, 0.2, 0.4);
	Rect sv  = cut_rect_right(tr, 0.4);

	v2 test_pos  = ctx->mouse_pos;
	test_pos.x  -= relative_origin.x;
	test_pos.y  -= relative_origin.y;

	BeginTextureMode(ctx->picker_texture);

	ClearBackground(ctx->bg);

	v4 hsv[2] = {colour, colour};
	hsv[0].x = 0;
	hsv[1].x = 1;
	f32 last_hue = colour.x;
	colour = do_vertical_slider(ctx, test_pos,
	                            scale_rect_centered(hs1, (v2){.x = 0.5, .y = 0.95}),
	                            PM_LEFT, hsv[1], hsv[0], colour);
	if (colour.x != last_hue)
		ctx->pms.base_hue = colour.x - ctx->pms.fractional_hue;

	f32 fraction = 0.1;
	if (ctx->pms.base_hue - 0.5 * fraction < 0) {
		hsv[0].x = 0;
		hsv[1].x = fraction;
	} else if (ctx->pms.base_hue + 0.5 * fraction > 1) {
		hsv[0].x = 1 - fraction;
		hsv[1].x = 1;
	} else {
		hsv[0].x = ctx->pms.base_hue - 0.5 * fraction;
		hsv[1].x = ctx->pms.base_hue + 0.5 * fraction;
	}

	colour = do_vertical_slider(ctx, test_pos,
	                            scale_rect_centered(hs2, (v2){.x = 0.5, .y = 0.95}),
	                            PM_MIDDLE, hsv[1], hsv[0], colour);
	ctx->pms.fractional_hue = colour.x - ctx->pms.base_hue;

	BeginShaderMode(ctx->picker_shader);
		/* NOTE: S-V Plane */
		sv.pos.y  += SLIDER_BORDER_WIDTH;
		sv.pos.x  += SLIDER_BORDER_WIDTH;
		sv.size.y -= 2 * SLIDER_BORDER_WIDTH;
		sv.size.x -= 2 * SLIDER_BORDER_WIDTH;
		v2 offset = {.x = sv.pos.x};

		b32 hovering = CheckCollisionPointRec(test_pos.rv, sv.rr);
		if (hovering && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ctx->held_idx == -1)
			ctx->held_idx = PM_RIGHT;

		if (ctx->held_idx == PM_RIGHT) {
			CLAMP(test_pos.x, sv.pos.x, sv.pos.x + sv.size.w);
			CLAMP(test_pos.y, sv.pos.y, sv.pos.y + sv.size.h);
			colour.y = (test_pos.x - sv.pos.x) / sv.size.w;
			colour.z = (sv.pos.y + sv.size.h - test_pos.y) / sv.size.h;
		}

		i32 mode = 1;
		SetShaderValue(ctx->picker_shader, ctx->mode_id, &mode, SHADER_UNIFORM_INT);
		SetShaderValue(ctx->picker_shader, ctx->size_id, sv.size.E, SHADER_UNIFORM_VEC2);
		SetShaderValue(ctx->picker_shader, ctx->offset_id, offset.E, SHADER_UNIFORM_VEC2);
		SetShaderValueV(ctx->picker_shader, ctx->hsv_id, colour.E, SHADER_UNIFORM_VEC4, 1);
		DrawRectangleRec(sv.rr, BLACK);
	EndShaderMode();

	DrawRectangleLinesEx(sv.rr, SLIDER_BORDER_WIDTH, SLIDER_BORDER_COLOUR);

	f32 radius = 4;
	v2  param  = {.x = colour.y, .y = 1 - colour.z};
	{
		/* TODO: move this to ctx */
		static f32 slider_scale = 1;
		f32 scale_delta  = (SLIDER_SCALE_TARGET - 1.0) * SLIDER_SCALE_SPEED * ctx->dt;
		b32 should_scale = (ctx->held_idx == -1 && hovering) ||
		                   (ctx->held_idx != -1 && ctx->held_idx == PM_RIGHT);
		slider_scale = move_towards_f32(slider_scale, should_scale?
		                                SLIDER_SCALE_TARGET : 1.0, scale_delta);

		/* NOTE: North-East */
		f32 line_len = 8;
		v2 start = {
			.x = sv.pos.x + (param.x * sv.size.w) + 0.5 * radius,
			.y = sv.pos.y + (param.y * sv.size.h) + 0.5 * radius,
		};
		v2 end   = start;
		end.x   += line_len * slider_scale;
		end.y   += line_len * slider_scale;
		DrawLineEx(start.rv, end.rv, 4, ctx->fg);

		/* NOTE: North-West */
		start.x -= radius;
		end      = start;
		end.x   -= line_len * slider_scale;
		end.y   += line_len * slider_scale;
		DrawLineEx(start.rv, end.rv, 4, ctx->fg);

		/* NOTE: South-West */
		start.y -= radius;
		end      = start;
		end.x   -= line_len * slider_scale;
		end.y   -= line_len * slider_scale;
		DrawLineEx(start.rv, end.rv, 4, ctx->fg);

		/* NOTE: South-East */
		start.x += radius;
		end      = start;
		end.x   += line_len * slider_scale;
		end.y   -= line_len * slider_scale;
		DrawLineEx(start.rv, end.rv, 4, ctx->fg);
	}

	EndTextureMode();

	if (IsMouseButtonUp(MOUSE_BUTTON_LEFT))
		ctx->held_idx = -1;

	store_formatted_colour(ctx, colour, CM_HSV);
}

DEBUG_EXPORT void
do_colour_picker(ColourPickerCtx *ctx, f32 dt, Vector2 window_pos, Vector2 mouse_pos)
{
	ctx->dt            = dt;
	ctx->mouse_pos.rv  = mouse_pos;
	ctx->window_pos.rv = window_pos;

	uv2 ws = ctx->window_size;

	DrawRectangle(ctx->window_pos.x, ctx->window_pos.y, ws.w, ws.h, ctx->bg);

	v2 pad = {.x = 0.05 * ws.w, .y = 0.05 * ws.h};
	Rect upper = {
		.pos  = {.x = ctx->window_pos.x + pad.x, .y = ctx->window_pos.y + pad.y},
		.size = {.w = ws.w - 2 * pad.x,          .h = ws.h * 0.6},
	};
	Rect lower = {
		.pos  = {.x = upper.pos.x,      .y = upper.pos.y + ws.h * 0.6},
		.size = {.w = ws.w - 2 * pad.x, .h = ws.h * 0.4 - 1 * pad.y},
	};

	Rect ma = cut_rect_left(upper, 0.84);
	Rect sa = cut_rect_right(upper, 0.84);
	do_colour_stack(ctx, sa);

	{
		if (ctx->picker_texture.texture.width != (i32)(ma.size.w)) {
			i32 w = ma.size.w;
			i32 h = ma.size.h;
			UnloadRenderTexture(ctx->picker_texture);
			ctx->picker_texture = LoadRenderTexture(w, h);
			if (ctx->mode != CPM_PICKER)
				do_picker_mode(ctx, ma.pos);
		}

		if (ctx->slider_texture.texture.width != (i32)(ma.size.w)) {
			i32 w = ma.size.w;
			i32 h = ma.size.h;
			UnloadRenderTexture(ctx->slider_texture);
			ctx->slider_texture = LoadRenderTexture(w, h);
			if (ctx->mode != CPM_SLIDERS)
				do_slider_mode(ctx, ma.pos);
		}
	}

	{
		Rect tr = {
			.size = {
				.w = ctx->slider_texture.texture.width,
				.h = -ctx->slider_texture.texture.height
			}
		};
		NPatchInfo tnp = {tr.rr, 0, 0, 0, 0, NPATCH_NINE_PATCH};
		switch (ctx->mode) {
		case CPM_SLIDERS:
			do_slider_mode(ctx, ma.pos);
			DrawTextureNPatch(ctx->slider_texture.texture, tnp, ma.rr, (Vector2){0},
			                  0, WHITE);
			break;
		case CPM_PICKER:
			do_picker_mode(ctx, ma.pos);
			DrawTextureNPatch(ctx->picker_texture.texture, tnp, ma.rr, (Vector2){0},
			                  0, WHITE);
			break;
		case CPM_LAST:
			ASSERT(0);
			break;
		}
		DrawRectangleRec(ma.rr, Fade(ctx->bg, 1 - ctx->mcs.mode_visible_t));
	}

	{
		Rect cb    = lower;
		cb.size.h *= 0.25;
		cb.pos.y  += 0.04 * lower.size.h;
		do_colour_selector(ctx, cb);

		f32 mode_x_pad = 0.04 * lower.size.w;

		Rect mb    = cb;
		mb.size.w *= (1.0 / (CPM_LAST + 1) - 0.1);
		mb.size.w -= 0.5 * mode_x_pad;
		mb.size.h  = mb.size.w;

		mb.pos.y  += lower.size.h * 0.75 / 2;

		f32 offset = lower.size.w - (CPM_LAST + 1) * (mb.size.w + 0.5 * mode_x_pad);
		mb.pos.x  += 0.5 * offset;

		Rect tr = {
			.size = {
				.w =  ctx->slider_texture.texture.width,
				.h = -ctx->slider_texture.texture.height
			}
		};

		NPatchInfo tnp = {tr.rr, 0, 0, 0, 0, NPATCH_NINE_PATCH };
		for (u32 i = 0; i < CPM_LAST; i++) {
			if (CheckCollisionPointRec(ctx->mouse_pos.rv, mb.rr)) {
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
					if (ctx->mode != i)
						ctx->mcs.next_mode = i;
				}
				ctx->mcs.scales[i] = move_towards_f32(ctx->mcs.scales[i], 1.1,
				                                      ctx->dt);
			} else {
				ctx->mcs.scales[i] = move_towards_f32(ctx->mcs.scales[i], 1,
				                                      ctx->dt);
			}

			f32 scaled_dt = 2 * ctx->dt;
			f32 mvt = ctx->mcs.mode_visible_t;
			if (ctx->mcs.next_mode != -1) {
				ctx->mcs.mode_visible_t = move_towards_f32(mvt, 0, scaled_dt);
				if (ctx->mcs.mode_visible_t == 0) {
					ctx->mode = ctx->mcs.next_mode;
					ctx->mcs.next_mode = -1;
					if (ctx->mode == CPM_PICKER) {
						v4 hsv = get_formatted_colour(ctx, CM_HSV);
						ctx->pms.base_hue       = hsv.x;
						ctx->pms.fractional_hue = 0;
					}
					ctx->flags |= CPF_REFILL_TEXTURE;
				}
			} else {
				ctx->mcs.mode_visible_t = move_towards_f32(mvt, 1, scaled_dt);
			}

			Texture *texture = NULL;
			switch (i) {
			case CPM_PICKER:  texture = &ctx->picker_texture.texture; break;
			case CPM_SLIDERS: texture = &ctx->slider_texture.texture; break;
			case CPM_LAST: break;
			}
			ASSERT(texture);

			f32 scale      = ctx->mcs.scales[i];
			Rect txt_out   = scale_rect_centered(mb, (v2){.x = 0.8 * scale,
			                                              .y = 0.8 * scale});
			Rect outline_r = scale_rect_centered(mb, (v2){.x = scale, .y = scale});

			DrawTextureNPatch(*texture, tnp, txt_out.rr, (Vector2){0}, 0, WHITE);
			DrawRectangleRoundedLinesEx(outline_r.rr, SELECTOR_ROUNDNESS, 0,
			                            SELECTOR_BORDER_WIDTH, SELECTOR_BORDER_COLOUR);

			mb.pos.x      += mb.size.w + mode_x_pad;
			txt_out.pos.x += mb.size.w + mode_x_pad;
		}

		static char *button_text[2] = {"Copy", "Paste"};
		static f32 button_colour_t[2];
		v4 fg         = normalize_colour(pack_rl_colour(ctx->fg));
		Color bg      = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
		Rect btn_r    = mb;
		btn_r.size.h *= 0.46;

		for (u32 i = 0; i < 2; i++) {
			if (CheckCollisionPointRec(ctx->mouse_pos.rv, btn_r.rr)) {
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
					char *txt;
					v4 new_colour;
					switch (i) {
					case 0:
						txt = (char *)TextFormat("%02x%02x%02x%02x",
						                         bg.r, bg.g, bg.b, bg.a);
						SetClipboardText(txt);
						break;
					case 1:
						txt        = (char *)GetClipboardText();
						new_colour = normalize_colour(parse_hex_u32(txt));
						store_formatted_colour(ctx, new_colour, CM_RGB);
						if (ctx->mode == CPM_PICKER) {
							f32 hue = rgb_to_hsv(new_colour).x;
							ctx->pms.base_hue       = hue;
							ctx->pms.fractional_hue = 0;
						}
						break;
					default:
						ASSERT(0);
						break;
					}
				}
				button_colour_t[i] += TEXT_HOVER_SPEED * ctx->dt;
			} else {
				button_colour_t[i] -= TEXT_HOVER_SPEED * ctx->dt;
			}
			CLAMP01(button_colour_t[i]);

			v2 tpos = center_align_text_in_rect(btn_r, button_text[i], ctx->font,
			                                    ctx->font_size);
			v2 spos = {.x = tpos.x + 1.75, .y = tpos.y + 2};

			v4 colour = lerp_v4(fg, ctx->hover_colour, button_colour_t[i]);

			DrawRectangleRounded(btn_r.rr, SELECTOR_ROUNDNESS, 0, bg);
			DrawRectangleRoundedLinesEx(btn_r.rr, SELECTOR_ROUNDNESS, 0,
			                            SELECTOR_BORDER_WIDTH, SELECTOR_BORDER_COLOUR);
			DrawTextEx(ctx->font, button_text[i], spos.rv, ctx->font_size, 0,
			           Fade(BLACK, 0.8));
			DrawTextEx(ctx->font, button_text[i], tpos.rv, ctx->font_size, 0,
			           colour_from_normalized(colour));

			btn_r.pos.y += 0.54 * mb.size.h;
		}
	}

#ifdef _DEBUG
	DrawFPS(20, 20);
#endif

}
