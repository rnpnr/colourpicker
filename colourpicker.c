/* See LICENSE for copyright details */
#include <raylib.h>
#include <rlgl.h>
#include <stdio.h>

#include "util.c"

static s8 mode_labels[CM_LAST][4] = {
	[CM_RGB] = { s8("R"), s8("G"), s8("B"), s8("A") },
	[CM_HSV] = { s8("H"), s8("S"), s8("V"), s8("A") },
};

static void
mem_move(char *src, char *dest, size n)
{
	if (dest < src) while (n) { *dest++ = *src++; n--; }
	else            while (n) { n--; dest[n] = src[n]; }
}

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

static Color
fade(Color a, f32 alpha)
{
	a.a = (u8)((f32)a.a * alpha);
	return a;
}

static f32
lerp(f32 a, f32 b, f32 t)
{
	return a + t * (b - a);
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
measure_text(Font font, s8 text)
{
	v2 result = {.y = font.baseSize};

	for (size i = 0; i < text.len; i++) {
		/* NOTE: assumes font glyphs are ordered (they are in our embedded fonts) */
		i32 idx   = (i32)text.data[i] - 32;
		result.x += font.glyphs[idx].advanceX;
		if (font.glyphs[idx].advanceX == 0)
			result.x += (font.recs[idx].width + font.glyphs[idx].offsetX);
	}

	return result;
}

static v2
left_align_text_in_rect(Rect r, s8 text, Font font)
{
	v2 ts    = measure_text(font, text);
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
center_align_text_in_rect(Rect r, s8 text, Font font)
{
	v2 ts    = measure_text(font, text);
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
	if (ctx->is.idx == -1) {
		return;
	} else if (ctx->is.idx == INPUT_HEX) {
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
	if (ctx->is.idx != (i32)idx)
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
do_text_input(ColourPickerCtx *ctx, Rect r, Color colour, i32 max_disp_chars)
{
	v2 ts  = measure_text(ctx->font, (s8){.len = ctx->is.buf_len, .data = ctx->is.buf});
	v2 pos = {.x = r.pos.x, .y = r.pos.y + (r.size.y - ts.y) / 2};

	i32 buf_delta = ctx->is.buf_len - max_disp_chars;
	if (buf_delta < 0) buf_delta = 0;
	char *buf     = ctx->is.buf + buf_delta;
	{
		/* NOTE: drop a char if the subtext still doesn't fit */
		v2 nts = measure_text(ctx->font, (s8){.len = ctx->is.buf_len - buf_delta,
		                                      .data = buf});
		if (nts.w > 0.96 * r.size.w)
			buf++;
	}
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
		i32 i;
		for (i = 0; i < ctx->is.buf_len && x_off < x_bounds; i++) {
			/* NOTE: assumes font glyphs are ordered */
			i32 idx = ctx->is.buf[i] - 32;
			x_off  += ctx->font.glyphs[idx].advanceX;
			if (ctx->font.glyphs[idx].advanceX == 0)
				x_off += ctx->font.recs[idx].width;
		}
		ctx->is.cursor = i;
	}

	v2 sts = measure_text(ctx->font, (s8){.len = ctx->is.cursor - buf_delta, .data = buf});
	f32 cursor_x = r.pos.x + sts.x;
	f32 cursor_width;
	if (ctx->is.cursor == ctx->is.buf_len) cursor_width = MIN(ctx->window_size.w * 0.03, 20);
	else                                   cursor_width = MIN(ctx->window_size.w * 0.01, 6);

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

		mem_move(ctx->is.buf + ctx->is.cursor,
		         ctx->is.buf + ctx->is.cursor + 1,
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
		ctx->is.cursor--;
		mem_move(ctx->is.buf + ctx->is.cursor + 1,
		         ctx->is.buf + ctx->is.cursor,
		         ctx->is.buf_len - ctx->is.cursor);
		ctx->is.buf_len--;
	}

	if (IsKeyPressed(KEY_ENTER)) {
		parse_and_store_text_input(ctx);
		ctx->is.idx = -1;
	}
}

static i32
do_button(ButtonState *btn, v2 mouse, Rect r, f32 dt, f32 hover_speed)
{
	b32 hovered       = CheckCollisionPointRec(mouse.rv, r.rr);
	i32 pressed_mask  = 0;
	pressed_mask     |= MOUSE_LEFT  * (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
	pressed_mask     |= MOUSE_RIGHT * (hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));

	if (hovered) btn->hover_t += hover_speed * dt;
	else         btn->hover_t -= hover_speed * dt;
	CLAMP01(btn->hover_t);

	return pressed_mask;
}

static i32
do_rect_button(ButtonState *btn, v2 mouse, Rect r, Color bg, f32 dt, f32 hover_speed, f32 scale_target, f32 fade_t)
{
	i32 pressed_mask = do_button(btn, mouse, r, dt, hover_speed);

	f32 param  = lerp(1, scale_target, btn->hover_t);
	v2  bscale = (v2){
		.x = param + RECT_BTN_BORDER_WIDTH / r.size.w,
		.y = param + RECT_BTN_BORDER_WIDTH / r.size.h,
	};
	Rect sr    = scale_rect_centered(r, (v2){.x = param, .y = param});
	Rect sb    = scale_rect_centered(r, bscale);
	DrawRectangleRounded(sb.rr, SELECTOR_ROUNDNESS, 0, fade(SELECTOR_BORDER_COLOUR, fade_t));
	DrawRectangleRounded(sr.rr, SELECTOR_ROUNDNESS, 0, fade(bg, fade_t));

	return pressed_mask;
}

static i32
do_text_button(ColourPickerCtx *ctx, ButtonState *btn, v2 mouse, Rect r, s8 text, v4 fg, Color bg)
{
	i32 pressed_mask = do_rect_button(btn, mouse, r, bg, ctx->dt, TEXT_HOVER_SPEED, 1, 1);

	v2 tpos   = center_align_text_in_rect(r, text, ctx->font);
	v2 spos   = {.x = tpos.x + 1.75, .y = tpos.y + 2};
	v4 colour = lerp_v4(fg, ctx->hover_colour, btn->hover_t);

	DrawTextEx(ctx->font, text.data, spos.rv, ctx->font_size, 0, fade(BLACK, 0.8));
	DrawTextEx(ctx->font, text.data, tpos.rv, ctx->font_size, 0, colour_from_normalized(colour));

	return pressed_mask;
}

static void
do_slider(ColourPickerCtx *ctx, Rect r, i32 label_idx, v2 relative_mouse)
{
	Rect lr, sr, vr;
	get_slider_subrects(r, &lr, &sr, &vr);

	b32 hovering = CheckCollisionPointRec(relative_mouse.rv, sr.rr);

	if (hovering && ctx->held_idx == -1)
		ctx->held_idx = label_idx;

	if (ctx->held_idx != -1) {
		f32 current = ctx->colour.E[ctx->held_idx];
		f32 wheel = GetMouseWheelMove();
		if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
			current = (relative_mouse.x - sr.pos.x) / sr.size.w;
		current += wheel / 255;
		CLAMP01(current);
		ctx->colour.E[ctx->held_idx] = current;
		ctx->flags |= CPF_REFILL_TEXTURE;
	}

	if (IsMouseButtonUp(MOUSE_BUTTON_LEFT))
		ctx->held_idx = -1;

	f32 current = ctx->colour.E[label_idx];

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

	v2 fpos;
	{
		SliderState *s = &ctx->ss;
		b32 collides = CheckCollisionPointRec(relative_mouse.rv, vr.rr);
		if (collides && ctx->is.idx != (label_idx + 1)) {
			s->colour_t[label_idx] += TEXT_HOVER_SPEED * ctx->dt;
		} else {
			s->colour_t[label_idx] -= TEXT_HOVER_SPEED * ctx->dt;
		}
		CLAMP01(s->colour_t[label_idx]);

		if (!collides && ctx->is.idx == (label_idx + 1) &&
		    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			set_text_input_idx(ctx, -1, vr, relative_mouse);
			current = ctx->colour.E[label_idx];
		}

		v4 colour       = lerp_v4(normalize_colour(pack_rl_colour(ctx->fg)),
		                          ctx->hover_colour, s->colour_t[label_idx]);
		Color colour_rl = colour_from_normalized(colour);

		if (collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			set_text_input_idx(ctx, label_idx + 1, vr, relative_mouse);

		if (ctx->is.idx != (label_idx + 1)) {
			s8 value = {.len = 4, .data = (char *)TextFormat("%0.02f", current)};
			fpos = left_align_text_in_rect(vr, value, ctx->font);
			DrawTextEx(ctx->font, value.data, fpos.rv, ctx->font_size, 0, colour_rl);
		} else {
			do_text_input(ctx, vr, colour_rl, 4);
		}
	}

	s8 label = mode_labels[ctx->colour_mode][label_idx];
	fpos = center_align_text_in_rect(lr, label, ctx->font);
	DrawTextEx(ctx->font, label.data, fpos.rv, ctx->font_size, 0, ctx->fg);
}

static void
do_status_bar(ColourPickerCtx *ctx, Rect r, v2 relative_mouse)
{
	Rect hex_r  = cut_rect_left(r, 0.5);
	Rect mode_r;
	get_slider_subrects(r, 0, 0, &mode_r);

	s8 mode_txt = s8("");
	switch (ctx->colour_mode) {
	case CM_RGB:  mode_txt = s8("RGB"); break;
	case CM_HSV:  mode_txt = s8("HSV"); break;
	case CM_LAST: ASSERT(0);            break;
	}

	v2 mode_ts     = measure_text(ctx->font, mode_txt);
	mode_r.pos.y  += (mode_r.size.h - mode_ts.h) / 2;
	mode_r.size.w  = mode_ts.w;

	i32 mouse_mask = do_button(&ctx->sbs.mode, relative_mouse, mode_r, ctx->dt, TEXT_HOVER_SPEED);
	if (mouse_mask & MOUSE_LEFT)  step_colour_mode(ctx, 1);
	if (mouse_mask & MOUSE_RIGHT) step_colour_mode(ctx, -1);

	Color hc = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
	s8 hex   = {.len = 8, .data = (char *)TextFormat("%02x%02x%02x%02x", hc.r, hc.g, hc.b, hc.a)};
	s8 label = s8("RGB: ");

	v2 label_size = measure_text(ctx->font, label);
	v2 hex_size   = measure_text(ctx->font, hex);

	f32 extra_input_scale = 1.07;
	hex_r.size.w = extra_input_scale * (label_size.w + hex_size.w);

	Rect label_r = cut_rect_left(hex_r,  label_size.x / hex_r.size.w);
	hex_r        = cut_rect_right(hex_r, label_size.x / hex_r.size.w);

	i32 hex_collides = CheckCollisionPointRec(relative_mouse.rv, hex_r.rr);

	if (!hex_collides && ctx->is.idx == INPUT_HEX &&
	    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		set_text_input_idx(ctx, -1, hex_r, relative_mouse);
		hc       = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
		hex.data = (char *)TextFormat("%02x%02x%02x%02x", hc.r, hc.g, hc.b, hc.a);
	}

	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		set_text_input_idx(ctx, INPUT_HEX, hex_r, relative_mouse);

	if (hex_collides && ctx->is.idx != INPUT_HEX)
		ctx->sbs.hex_hover_t += TEXT_HOVER_SPEED * ctx->dt;
	else
		ctx->sbs.hex_hover_t -= TEXT_HOVER_SPEED * ctx->dt;
	CLAMP01(ctx->sbs.hex_hover_t);

	v4 fg          = normalize_colour(pack_rl_colour(ctx->fg));
	v4 hex_colour  = lerp_v4(fg, ctx->hover_colour, ctx->sbs.hex_hover_t);
	v4 mode_colour = lerp_v4(fg, ctx->hover_colour, ctx->sbs.mode.hover_t);

	v2 fpos = left_align_text_in_rect(label_r, label, ctx->font);
	DrawTextEx(ctx->font, (char *)label.data, fpos.rv, ctx->font_size, 0, ctx->fg);

	Color hex_colour_rl = colour_from_normalized(hex_colour);
	if (ctx->is.idx != INPUT_HEX) {
		fpos = left_align_text_in_rect(hex_r, hex, ctx->font);
		DrawTextEx(ctx->font, hex.data, fpos.rv, ctx->font_size, 0, hex_colour_rl);
	} else {
		do_text_input(ctx, hex_r, hex_colour_rl, 8);
	}

	DrawTextEx(ctx->font, mode_txt.data, mode_r.pos.rv, ctx->font_size, 0,
	           colour_from_normalized(mode_colour));
}

static void
do_colour_stack(ColourPickerCtx *ctx, Rect sa)
{
	ColourStackState *css = &ctx->colour_stack;

	/* NOTE: Small adjusment to align with mode text. TODO: Cleanup? */
	sa = scale_rect_centered(sa, (v2){.x = 1, .y = 0.98});
	sa.pos.y += 0.02 * sa.size.h;

	Rect r    = sa;
	r.size.h *= 1.0 / (ARRAY_COUNT(css->items) + 3);
	r.size.w *= 0.75;
	r.pos.x  += (sa.size.w - r.size.w) * 0.5;

	f32 y_pos_delta = r.size.h * 1.2;
	r.pos.y -= y_pos_delta * css->y_off_t;

	/* NOTE: Stack is moving up; draw last top item as it moves up and fades out */
	if (css->fade_param) {
		ButtonState btn;
		do_rect_button(&btn, ctx->mouse_pos, r, colour_from_normalized(css->last), ctx->dt,
		               0, 1, css->fade_param);
		r.pos.y += y_pos_delta;
	}

	f32 stack_scale_target = (f32)(ARRAY_COUNT(css->items) + 1) / ARRAY_COUNT(css->items);

	f32 fade_scale[ARRAY_COUNT(css->items)] = { [ARRAY_COUNT(css->items) - 1] = 1 };
	for (u32 i = 0; i < ARRAY_COUNT(css->items); i++) {
		i32 cidx    = (css->widx + i) % ARRAY_COUNT(css->items);
		Color bg    = colour_from_normalized(css->items[cidx]);
		b32 pressed = do_rect_button(css->buttons + cidx, ctx->mouse_pos, r, bg, ctx->dt,
		                             BUTTON_HOVER_SPEED, stack_scale_target,
		                             1 - css->fade_param * fade_scale[i]);
		if (pressed) {
			v4 hsv = rgb_to_hsv(css->items[cidx]);
			store_formatted_colour(ctx, hsv, CM_HSV);
			if (ctx->mode == CPM_PICKER) {
				ctx->pms.base_hue       = hsv.x;
				ctx->pms.fractional_hue = 0;
			}
		}
		r.pos.y += y_pos_delta;
	}

	css->fade_param -= BUTTON_HOVER_SPEED * ctx->dt;
	css->y_off_t    += BUTTON_HOVER_SPEED * ctx->dt;
	if (css->fade_param < 0) {
		css->fade_param = 0;
		css->y_off_t    = 0;
	}

	r.pos.y   = sa.pos.y + sa.size.h - r.size.h;
	r.pos.x  += r.size.w * 0.1;
	r.size.w *= 0.8;

	b32 push_pressed = do_button(&css->tri_btn, ctx->mouse_pos, r, ctx->dt, BUTTON_HOVER_SPEED);
	f32 param    = css->tri_btn.hover_t;
	v2 tri_size  = {.x = 0.25 * r.size.w,          .y = 0.5 * r.size.h};
	v2 tri_scale = {.x = 1 - 0.5 * param,          .y = 1 + 0.3 * param};
	v2 tri_mid   = {.x = r.pos.x + 0.5 * r.size.w, .y = r.pos.y - 0.3 * r.size.h * param};
	draw_cardinal_triangle(tri_mid, tri_size, tri_scale, NORTH, ctx->fg);

	if (push_pressed) {
		css->fade_param         = 1.0;
		css->last               = css->items[css->widx];
		css->items[css->widx++] = get_formatted_colour(ctx, CM_RGB);
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

	v4 fg        = normalize_colour(pack_rl_colour(ctx->fg));
	s8 labels[2] = {s8("Revert"), s8("Apply")};

	i32 pressed_idx = -1;
	for (u32 i = 0; i < ARRAY_COUNT(cs); i++) {
		if (CheckCollisionPointRec(ctx->mouse_pos.rv, cs[i].rr) && ctx->held_idx == -1) {
			ctx->selection_hover_t[i] += TEXT_HOVER_SPEED * ctx->dt;

			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
				pressed_idx = i;
		} else {
			ctx->selection_hover_t[i] -= TEXT_HOVER_SPEED * ctx->dt;
		}

		CLAMP01(ctx->selection_hover_t[i]);

		v4 colour = lerp_v4(fg, ctx->hover_colour, ctx->selection_hover_t[i]);

		v2 fpos = center_align_text_in_rect(cs[i], labels[i], ctx->font);
		v2 pos  = fpos;
		pos.x  += 1.75;
		pos.y  += 2;
		DrawTextEx(ctx->font, labels[i].data, pos.rv, ctx->font_size, 0, fade(BLACK, 0.8));
		DrawTextEx(ctx->font, labels[i].data, fpos.rv, ctx->font_size, 0,
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
do_slider_shader(ColourPickerCtx *ctx, Rect r, i32 colour_mode, f32 *regions, f32 *colours)
{
	f32 border_thick = SLIDER_BORDER_WIDTH;
	f32 radius       = SLIDER_ROUNDNESS / 2;
	/* NOTE: scale radius by rect width or height to adapt to window scaling */
	radius          *= (r.size.w > r.size.h)? r.size.h : r.size.w;

	BeginShaderMode(ctx->picker_shader);
	rlEnableShader(ctx->picker_shader.id);
	rlSetUniform(ctx->mode_id,         &ctx->mode,    RL_SHADER_UNIFORM_INT,   1);
	rlSetUniform(ctx->radius_id,       &radius,       RL_SHADER_UNIFORM_FLOAT, 1);
	rlSetUniform(ctx->border_thick_id, &border_thick, RL_SHADER_UNIFORM_FLOAT, 1);
	rlSetUniform(ctx->colour_mode_id,  &colour_mode,  RL_SHADER_UNIFORM_INT,   1);
	rlSetUniform(ctx->size_id,         r.size.E,      RL_SHADER_UNIFORM_VEC2,  1);
	rlSetUniform(ctx->colours_id,      colours,       RL_SHADER_UNIFORM_VEC4,  3);
	rlSetUniform(ctx->regions_id,      regions,       RL_SHADER_UNIFORM_VEC4,  4);
	DrawRectangleRec(r.rr, BLACK);
	EndShaderMode();
}

static void
do_slider_mode(ColourPickerCtx *ctx, v2 relative_mouse)
{
	BEGIN_CYCLE_COUNT(CC_DO_SLIDER);

	Rect tr  = {
		.size = {
			.w = ctx->picker_texture.texture.width,
			.h = ctx->picker_texture.texture.height
		}
	};
	Rect sb    = tr;
	Rect ss    = tr;
	sb.size.h *= 0.1;
	ss.size.h *= 0.15;
	ss.pos.y  += 1.2 * sb.size.h;

	BeginTextureMode(ctx->slider_texture);
	ClearBackground(ctx->bg);

	do_status_bar(ctx, sb, relative_mouse);

	Rect sr;
	get_slider_subrects(ss, 0, &sr, 0);
	f32 r_bound = sr.pos.x + sr.size.w;
	f32 y_step  = 1.525 * ss.size.h;

	for (i32 i = 0; i < 4; i++) {
		do_slider(ctx, ss, i, relative_mouse);
		ss.pos.y += y_step;
	}

	f32 start_y = sr.pos.y - 0.5 * ss.size.h;
	f32 end_y   = start_y + sr.size.h;
	f32 regions[16] = {
		sr.pos.x, start_y + 3 * y_step, r_bound, end_y + 3 * y_step,
		sr.pos.x, start_y + 2 * y_step, r_bound, end_y + 2 * y_step,
		sr.pos.x, start_y + 1 * y_step, r_bound, end_y + 1 * y_step,
		sr.pos.x, start_y + 0 * y_step, r_bound, end_y + 0 * y_step
	};
	v4 colours[3] = {ctx->colour};
	do_slider_shader(ctx, tr, ctx->colour_mode, regions, (f32 *)colours);

	EndTextureMode();

	END_CYCLE_COUNT(CC_DO_SLIDER);
}


#define PM_LEFT   0
#define PM_MIDDLE 1
#define PM_RIGHT  2

static v4
do_vertical_slider(ColourPickerCtx *ctx, v2 test_pos, Rect r, i32 idx,
                   v4 bot_colour, v4 top_colour, v4 colour)
{
	b32 hovering = CheckCollisionPointRec(test_pos.rv, r.rr);

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

	f32 param = (colour.x - top_colour.x) / (bot_colour.x - top_colour.x);
	{
		b32 should_scale = (ctx->held_idx == -1 && hovering) ||
		                   (ctx->held_idx != -1 && ctx->held_idx == idx);
		if (should_scale) ctx->pms.scale_t[idx] += SLIDER_SCALE_SPEED * ctx->dt;
		else              ctx->pms.scale_t[idx] -= SLIDER_SCALE_SPEED * ctx->dt;
		CLAMP01(ctx->pms.scale_t[idx]);

		f32 scale    = lerp(1, SLIDER_SCALE_TARGET, ctx->pms.scale_t[idx]);
		v2 tri_scale = {.x = scale, .y = scale};
		v2 tri_mid   = {.x = r.pos.x, .y = r.pos.y + (param * r.size.h)};
		draw_cardinal_triangle(tri_mid, SLIDER_TRI_SIZE, tri_scale, EAST, ctx->fg);
		tri_mid.x   += r.size.w;
		draw_cardinal_triangle(tri_mid, SLIDER_TRI_SIZE, tri_scale, WEST, ctx->fg);
	}

	return colour;
}

static void
do_picker_mode(ColourPickerCtx *ctx, v2 relative_mouse)
{
	BEGIN_CYCLE_COUNT(CC_DO_PICKER);

	v4 colour = get_formatted_colour(ctx, CM_HSV);
	colour.x  = ctx->pms.base_hue + ctx->pms.fractional_hue;

	Rect tr  = {
		.size = {
			.w = ctx->picker_texture.texture.width,
			.h = ctx->picker_texture.texture.height
		}
	};

	Rect hs1 = scale_rect_centered(cut_rect_left(tr, 0.2),        (v2){.x = 0.5, .y = 0.95});
	Rect hs2 = scale_rect_centered(cut_rect_middle(tr, 0.2, 0.4), (v2){.x = 0.5, .y = 0.95});
	Rect sv  = scale_rect_centered(cut_rect_right(tr, 0.4),       (v2){.x = 1.0, .y = 0.95});

	BeginTextureMode(ctx->picker_texture);
	ClearBackground(ctx->bg);

	v4 hsv[3] = {colour, colour, colour};
	hsv[1].x = 0;
	hsv[2].x = 1;
	f32 last_hue = colour.x;
	colour = do_vertical_slider(ctx, relative_mouse, hs1, PM_LEFT, hsv[2], hsv[1], colour);
	if (colour.x != last_hue)
		ctx->pms.base_hue = colour.x - ctx->pms.fractional_hue;

	f32 fraction = 0.1;
	if (ctx->pms.base_hue - 0.5 * fraction < 0) {
		hsv[1].x = 0;
		hsv[2].x = fraction;
	} else if (ctx->pms.base_hue + 0.5 * fraction > 1) {
		hsv[1].x = 1 - fraction;
		hsv[2].x = 1;
	} else {
		hsv[1].x = ctx->pms.base_hue - 0.5 * fraction;
		hsv[2].x = ctx->pms.base_hue + 0.5 * fraction;
	}

	colour = do_vertical_slider(ctx, relative_mouse, hs2, PM_MIDDLE, hsv[2], hsv[1], colour);
	ctx->pms.fractional_hue = colour.x - ctx->pms.base_hue;

	{
		f32 regions[16] = {
			hs1.pos.x, hs1.pos.y, hs1.pos.x + hs1.size.w, hs1.pos.y + hs1.size.h,
			hs2.pos.x, hs2.pos.y, hs2.pos.x + hs2.size.w, hs2.pos.y + hs2.size.h,
			sv.pos.x,  sv.pos.y,  sv.pos.x  + sv.size.w,  sv.pos.y  + sv.size.h
		};
		do_slider_shader(ctx, tr, CM_HSV, regions, (f32 *)hsv);
	}

	b32 hovering = CheckCollisionPointRec(relative_mouse.rv, sv.rr);
	if (hovering && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && ctx->held_idx == -1)
		ctx->held_idx = PM_RIGHT;

	if (ctx->held_idx == PM_RIGHT) {
		CLAMP(relative_mouse.x, sv.pos.x, sv.pos.x + sv.size.w);
		CLAMP(relative_mouse.y, sv.pos.y, sv.pos.y + sv.size.h);
		colour.y = (relative_mouse.x - sv.pos.x) / sv.size.w;
		colour.z = (sv.pos.y + sv.size.h - relative_mouse.y) / sv.size.h;
	}

	f32 radius = 4;
	v2  param  = {.x = colour.y, .y = 1 - colour.z};
	{
		b32 should_scale = (ctx->held_idx == -1 && hovering) ||
		                   (ctx->held_idx != -1 && ctx->held_idx == PM_RIGHT);
		if (should_scale) ctx->pms.scale_t[PM_RIGHT] += SLIDER_SCALE_SPEED * ctx->dt;
		else              ctx->pms.scale_t[PM_RIGHT] -= SLIDER_SCALE_SPEED * ctx->dt;
		CLAMP01(ctx->pms.scale_t[PM_RIGHT]);

		f32 slider_scale = lerp(1, SLIDER_SCALE_TARGET, ctx->pms.scale_t[PM_RIGHT]);
		f32 line_len     = 8;

		/* NOTE: North-East */
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

	END_CYCLE_COUNT(CC_DO_PICKER);
}

static void
debug_dump_info(ColourPickerCtx *ctx)
{
	(void)ctx;
#ifdef _DEBUG
	if (IsKeyPressed(KEY_F1))
		ctx->flags ^= CPF_PRINT_DEBUG;

	DrawFPS(20, 20);

	static char *fmts[CC_LAST] = {
		[CC_WHOLE_RUN] = "Whole Run:   %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_DO_PICKER] = "Picker Mode: %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_DO_SLIDER] = "Slider Mode: %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_UPPER]     = "Upper:       %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_LOWER]     = "Lower:       %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_TEMP]      = "Temp:        %7ld cyc | %2d h | %7d cyc/h\n",
	};

	i64 cycs[CC_LAST];
	i64 hits[CC_LAST];

	for (u32 i = 0; i < CC_LAST; i++) {
		cycs[i] = g_debug_clock_counts.total_cycles[i];
		hits[i] = g_debug_clock_counts.hit_count[i];
		g_debug_clock_counts.hit_count[i]  = 0;
		g_debug_clock_counts.total_cycles[i] = 0;
	}

	if (!(ctx->flags & CPF_PRINT_DEBUG))
		return;

	static u32 fcount;
	fcount++;
	if (fcount != 60)
		return;
	fcount = 0;

	printf("Begin Cycle Dump\n");
	for (u32 i = 0; i < CC_LAST; i++) {
		if (hits[i] == 0)
			continue;
		printf(fmts[i], cycs[i], hits[i], cycs[i]/hits[i]);
	}
#endif
}

DEBUG_EXPORT void
do_colour_picker(ColourPickerCtx *ctx, f32 dt, Vector2 window_pos, Vector2 mouse_pos)
{
	BEGIN_CYCLE_COUNT(CC_WHOLE_RUN);

	if (IsWindowResized()) {
		ctx->window_size.h = GetScreenHeight();
		ctx->window_size.w = ctx->window_size.h / WINDOW_ASPECT_RATIO;
		SetWindowSize(ctx->window_size.w, ctx->window_size.h);

		UnloadTexture(ctx->font.texture);
		if (ctx->window_size.w < 480) ctx->font = LoadFont_lora_sb_1_inc();
		else                          ctx->font = LoadFont_lora_sb_0_inc();
		ctx->font_size = ctx->font.baseSize;
	}

	if (!IsShaderReady(ctx->picker_shader)) {
#ifdef _DEBUG
		ctx->picker_shader   = LoadShader(0, HSV_LERP_SHADER_NAME);
#else
		ctx->picker_shader   = LoadShaderFromMemory(0, g_hsv_shader_text);
#endif
		ctx->mode_id         = GetShaderLocation(ctx->picker_shader, "u_mode");
		ctx->colour_mode_id  = GetShaderLocation(ctx->picker_shader, "u_colour_mode");
		ctx->colours_id      = GetShaderLocation(ctx->picker_shader, "u_colours");
		ctx->regions_id      = GetShaderLocation(ctx->picker_shader, "u_regions");
		ctx->size_id         = GetShaderLocation(ctx->picker_shader, "u_size");
		ctx->radius_id       = GetShaderLocation(ctx->picker_shader, "u_radius");
		ctx->border_thick_id = GetShaderLocation(ctx->picker_shader, "u_border_thick");
	}

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

	BEGIN_CYCLE_COUNT(CC_UPPER);

	Rect ma = cut_rect_left(upper, 0.84);
	Rect sa = cut_rect_right(upper, 0.84);
	do_colour_stack(ctx, sa);

	v2 ma_relative_mouse  = ctx->mouse_pos;
	ma_relative_mouse.x  -= ma.pos.x;
	ma_relative_mouse.y  -= ma.pos.y;

	{
		if (ctx->picker_texture.texture.width != (i32)(ma.size.w)) {
			i32 w = ma.size.w;
			i32 h = ma.size.h;
			UnloadRenderTexture(ctx->picker_texture);
			ctx->picker_texture = LoadRenderTexture(w, h);
			if (ctx->mode != CPM_PICKER) {
				i32 mode  = ctx->mode;
				ctx->mode = CPM_PICKER;
				do_picker_mode(ctx, ma_relative_mouse);
				ctx->mode = mode;
			}
		}

		if (ctx->slider_texture.texture.width != (i32)(ma.size.w)) {
			i32 w = ma.size.w;
			i32 h = ma.size.h;
			UnloadRenderTexture(ctx->slider_texture);
			ctx->slider_texture = LoadRenderTexture(w, h);
			if (ctx->mode != CPM_SLIDERS) {
				i32 mode  = ctx->mode;
				ctx->mode = CPM_SLIDERS;
				do_slider_mode(ctx, ma_relative_mouse);
				ctx->mode = mode;
			}
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
			do_slider_mode(ctx, ma_relative_mouse);
			DrawTextureNPatch(ctx->slider_texture.texture, tnp, ma.rr, (Vector2){0},
			                  0, WHITE);
			break;
		case CPM_PICKER:
			do_picker_mode(ctx, ma_relative_mouse);
			DrawTextureNPatch(ctx->picker_texture.texture, tnp, ma.rr, (Vector2){0},
			                  0, WHITE);
			break;
		case CPM_LAST:
			ASSERT(0);
			break;
		}
		DrawRectangleRec(ma.rr, fade(ctx->bg, 1 - ctx->mcs.mode_visible_t));
	}

	END_CYCLE_COUNT(CC_UPPER);

	{
		BEGIN_CYCLE_COUNT(CC_LOWER);
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
			if (do_button(ctx->mcs.buttons + i, ctx->mouse_pos, mb, ctx->dt, 10)) {
				if (ctx->mode != i)
					ctx->mcs.next_mode = i;
			}

			if (ctx->mcs.next_mode != -1) {
				ctx->mcs.mode_visible_t -= 2 * ctx->dt;
				if (ctx->mcs.mode_visible_t < 0) {
					ctx->mode          = ctx->mcs.next_mode;
					ctx->mcs.next_mode = -1;
					if (ctx->mode == CPM_PICKER) {
						v4 hsv = get_formatted_colour(ctx, CM_HSV);
						ctx->pms.base_hue       = hsv.x;
						ctx->pms.fractional_hue = 0;
					}
					ctx->flags |= CPF_REFILL_TEXTURE;
				}
			} else {
				ctx->mcs.mode_visible_t += 2 * ctx->dt;
			}
			CLAMP01(ctx->mcs.mode_visible_t);

			Texture *texture = NULL;
			switch (i) {
			case CPM_PICKER:  texture = &ctx->picker_texture.texture; break;
			case CPM_SLIDERS: texture = &ctx->slider_texture.texture; break;
			case CPM_LAST: break;
			}
			ASSERT(texture);

			f32 scale      = lerp(1, 1.1, ctx->mcs.buttons[i].hover_t);
			Rect txt_out   = scale_rect_centered(mb, (v2){.x = 0.8 * scale,
			                                              .y = 0.8 * scale});
			Rect outline_r = scale_rect_centered(mb, (v2){.x = scale, .y = scale});

			DrawTextureNPatch(*texture, tnp, txt_out.rr, (Vector2){0}, 0, WHITE);
			DrawRectangleRoundedLinesEx(outline_r.rr, SELECTOR_ROUNDNESS, 0,
			                            SELECTOR_BORDER_WIDTH, SELECTOR_BORDER_COLOUR);

			mb.pos.x      += mb.size.w + mode_x_pad;
			txt_out.pos.x += mb.size.w + mode_x_pad;
		}

		v4 fg         = normalize_colour(pack_rl_colour(ctx->fg));
		Color bg      = colour_from_normalized(get_formatted_colour(ctx, CM_RGB));
		Rect btn_r    = mb;
		btn_r.size.h *= 0.46;

		if (do_text_button(ctx, ctx->buttons + 0, ctx->mouse_pos, btn_r, s8("Copy"), fg, bg))
			SetClipboardText(TextFormat("%02x%02x%02x%02x", bg.r, bg.g, bg.b, bg.a));
		btn_r.pos.y += 0.54 * mb.size.h;

		if (do_text_button(ctx, ctx->buttons + 1, ctx->mouse_pos, btn_r, s8("Paste"), fg, bg)) {
			char *txt = (char *)GetClipboardText();
			if (txt) {
				v4 new_colour = normalize_colour(parse_hex_u32(txt));
				store_formatted_colour(ctx, new_colour, CM_RGB);
				if (ctx->mode == CPM_PICKER) {
					f32 hue = rgb_to_hsv(new_colour).x;
					ctx->pms.base_hue       = hue;
					ctx->pms.fractional_hue = 0;
				}
			}
		}

		END_CYCLE_COUNT(CC_LOWER);
	}

	END_CYCLE_COUNT(CC_WHOLE_RUN);

	debug_dump_info(ctx);
}
