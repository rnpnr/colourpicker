/* See LICENSE for copyright details */
#include <raylib.h>
#include <rlgl.h>

#include "util.c"

global f32 dt_for_frame;

#ifdef _DEBUG
enum clock_counts {
	CC_WHOLE_RUN,
	CC_DO_PICKER,
	CC_DO_SLIDER,
	CC_UPPER,
	CC_LOWER,
	CC_TEMP,
	CC_LAST
};
global struct {
	s64 cpu_cycles[CC_LAST];
	s64 total_cycles[CC_LAST];
	s64 hit_count[CC_LAST];
} g_debug_clock_counts;

#define BEGIN_CYCLE_COUNT(cc_name) \
	g_debug_clock_counts.cpu_cycles[cc_name] = rdtsc(); \
	g_debug_clock_counts.hit_count[cc_name]++

#define END_CYCLE_COUNT(cc_name) \
	g_debug_clock_counts.cpu_cycles[cc_name] = rdtsc() - g_debug_clock_counts.cpu_cycles[cc_name]; \
	g_debug_clock_counts.total_cycles[cc_name] += g_debug_clock_counts.cpu_cycles[cc_name]

#else
#define BEGIN_CYCLE_COUNT(a)
#define END_CYCLE_COUNT(a)
#endif

function void
mem_move(u8 *dest, u8 *src, sz n)
{
	if (dest < src) while (n) { *dest++ = *src++; n--; }
	else            while (n) { n--; dest[n] = src[n]; }
}

function b32
point_in_rect(v2 p, Rect r)
{
	v2  end    = add_v2(r.pos, r.size);
	b32 result = BETWEEN(p.x, r.pos.x, end.x) & BETWEEN(p.y, r.pos.y, end.y);
	return result;
}

function f32
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

function Color
fade(Color a, f32 alpha)
{
	a.a = (u8)((f32)a.a * alpha);
	return a;
}

function f32
lerp(f32 a, f32 b, f32 t)
{
	return a + t * (b - a);
}

function v4
lerp_v4(v4 a, v4 b, f32 t)
{
	v4 result;
	result.x = a.x + t * (b.x - a.x);
	result.y = a.y + t * (b.y - a.y);
	result.z = a.z + t * (b.z - a.z);
	result.w = a.w + t * (b.w - a.w);
	return result;
}

function v2
measure_text(Font font, str8 text)
{
	v2 result = {.y = font.baseSize};

	for (sz i = 0; i < text.len; i++) {
		/* NOTE: assumes font glyphs are ordered (they are in our embedded fonts) */
		s32 idx   = (s32)text.data[i] - 32;
		result.x += font.glyphs[idx].advanceX;
		if (font.glyphs[idx].advanceX == 0)
			result.x += (font.recs[idx].width + font.glyphs[idx].offsetX);
	}

	return result;
}

function void
draw_text(Font font, str8 text, v2 pos, Color colour)
{
	for (sz i = 0; i < text.len; i++) {
		/* NOTE: assumes font glyphs are ordered (they are in our embedded fonts) */
		s32 idx = text.data[i] - 32;
		Rectangle dst = {
			pos.x + font.glyphs[idx].offsetX - font.glyphPadding,
			pos.y + font.glyphs[idx].offsetY - font.glyphPadding,
			font.recs[idx].width  + 2.0f * font.glyphPadding,
			font.recs[idx].height + 2.0f * font.glyphPadding
		};
		Rectangle src = {
			font.recs[idx].x - font.glyphPadding,
			font.recs[idx].y - font.glyphPadding,
			font.recs[idx].width  + 2.0f * font.glyphPadding,
			font.recs[idx].height + 2.0f * font.glyphPadding
		};
		DrawTexturePro(font.texture, src, dst, (Vector2){0}, 0, colour);

		pos.x += font.glyphs[idx].advanceX;
		if (font.glyphs[idx].advanceX == 0)
			pos.x += font.recs[idx].width;
	}
}

function v2
left_align_text_in_rect(Rect r, str8 text, Font font)
{
	v2 ts    = measure_text(font, text);
	v2 delta = { .h = r.size.h - ts.h };
	return (v2) { .x = r.pos.x, .y = r.pos.y + 0.5 * delta.h, };
}

function Rect
scale_rect_centered(Rect r, v2 scale)
{
	Rect or   = r;
	r.size.w *= scale.x;
	r.size.h *= scale.y;
	r.pos.x  += (or.size.w - r.size.w) / 2;
	r.pos.y  += (or.size.h - r.size.h) / 2;
	return r;
}

function v2
center_align_text_in_rect(Rect r, str8 text, Font font)
{
	v2 ts    = measure_text(font, text);
	v2 delta = { .w = r.size.w - ts.w, .h = r.size.h - ts.h };
	return (v2) {
		.x = r.pos.x + 0.5 * delta.w,
	        .y = r.pos.y + 0.5 * delta.h,
	};
}

function Rect
cut_rect_middle(Rect r, f32 left, f32 right)
{
	ASSERT(left <= right);
	r.pos.x  += r.size.w * left;
	r.size.w  = r.size.w * (right - left);
	return r;
}

function Rect
cut_rect_left(Rect r, f32 fraction)
{
	r.size.w *= fraction;
	return r;
}

function Rect
cut_rect_right(Rect r, f32 fraction)
{
	r.pos.x  += fraction * r.size.w;
	r.size.w *= (1 - fraction);
	return r;
}

function b32
hover_rect(v2 mouse, Rect rect, f32 *hover_t)
{
	b32 result = point_in_rect(mouse, rect);
	if (result) *hover_t += HOVER_SPEED * dt_for_frame;
	else        *hover_t -= HOVER_SPEED * dt_for_frame;
	CLAMP01(*hover_t);
	return result;
}

function b32
hover_var(ColourPickerCtx *ctx, v2 mouse, Rect rect, Variable *var)
{
	b32 result = 0;
	if (ctx->interaction.kind != InteractionKind_Drag || var == ctx->interaction.active) {
		result = hover_rect(mouse, rect, &var->parameter);
		if (result) {
			ctx->interaction.next_hot = var;
			ctx->interaction.hot_rect = rect;
		}
	}
	return result;
}

function void
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

function v4
convert_colour(v4 colour, ColourKind current, ColourKind target)
{
	v4 result = colour;
	switch (current) {
	case ColourKind_RGB: {
		switch (target) {
		case ColourKind_RGB: break;
		case ColourKind_HSV: result = rgb_to_hsv(colour); break;
		InvalidDefaultCase;
		}
	} break;
	case ColourKind_HSV: {
		switch (target) {
		case ColourKind_RGB: result = hsv_to_rgb(colour); break;
		case ColourKind_HSV: break;
		InvalidDefaultCase;
		}
	} break;
	InvalidDefaultCase;
	}
	return result;
}

function v4
get_formatted_colour(ColourPickerCtx *ctx, ColourKind format)
{
	v4 result = convert_colour(ctx->colour, ctx->stored_colour_kind, format);
	return result;
}

function void
store_formatted_colour(ColourPickerCtx *ctx, v4 colour, ColourKind format)
{
	ctx->colour = convert_colour(colour, format, ctx->stored_colour_kind);

	/* TODO(rnp): what exactly was going on here? shouldn't we always redraw the texture ? */
	if (ctx->stored_colour_kind == ColourKind_HSV)
		ctx->flags |= ColourPickerFlag_RefillTexture;
}

function void
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

function void
parse_and_store_text_input(ColourPickerCtx *ctx)
{
	str8 input = {.len = ctx->text_input_state.count, .data = ctx->text_input_state.buf};
	v4   new_colour = {0};
	ColourKind new_mode = ColourKind_Last;
	if (ctx->text_input_state.idx == -1) {
		return;
	} else if (ctx->text_input_state.idx == INPUT_HEX) {
		new_colour = normalize_colour(parse_hex_u32(input));
		new_mode   = ColourKind_RGB;
	} else {
		new_mode   = ctx->stored_colour_kind;
		new_colour = ctx->colour;
		f32 fv = parse_f64(input);
		CLAMP01(fv);
		switch(ctx->text_input_state.idx) {
		case INPUT_R: new_colour.r = fv; break;
		case INPUT_G: new_colour.g = fv; break;
		case INPUT_B: new_colour.b = fv; break;
		case INPUT_A: new_colour.a = fv; break;
		default: break;
		}
	}

	if (new_mode != ColourKind_Last)
		store_formatted_colour(ctx, new_colour, new_mode);
}

function void
set_text_input_idx(ColourPickerCtx *ctx, enum input_indices idx, Rect r, v2 mouse)
{
	if (ctx->text_input_state.idx != (s32)idx)
		parse_and_store_text_input(ctx);

	Stream in = {.data = ctx->text_input_state.buf, .cap = countof(ctx->text_input_state.buf)};
	if (idx == INPUT_HEX) {
		stream_append_colour(&in, rl_colour_from_normalized(get_formatted_colour(ctx, ColourKind_RGB)));
	} else {
		f32 fv = 0;
		switch (idx) {
		case INPUT_R: fv = ctx->colour.r; break;
		case INPUT_G: fv = ctx->colour.g; break;
		case INPUT_B: fv = ctx->colour.b; break;
		case INPUT_A: fv = ctx->colour.a; break;
		default: break;
		}
		stream_append_f64(&in, fv, 100);
	}
	ctx->text_input_state.count = in.widx;

	ctx->text_input_state.idx    = idx;
	ctx->text_input_state.cursor = -1;
	CLAMP(ctx->text_input_state.idx, -1, INPUT_A);
	if (ctx->text_input_state.idx == -1)
		return;

	ASSERT(CheckCollisionPointRec(mouse.rv, r.rr));
	ctx->text_input_state.cursor_hover_p = (mouse.x - r.pos.x) / r.size.w;
	CLAMP01(ctx->text_input_state.cursor_hover_p);
}

function void
do_text_input(ColourPickerCtx *ctx, Rect r, Color colour, s32 max_disp_chars)
{
	TextInputState *is = &ctx->text_input_state;
	v2 ts  = measure_text(ctx->font, (str8){.len = is->count, .data = is->buf});
	v2 pos = {.x = r.pos.x, .y = r.pos.y + (r.size.y - ts.y) / 2};

	s32 buf_delta = is->count - max_disp_chars;
	if (buf_delta < 0) buf_delta = 0;
	str8 buf = {.len = is->count - buf_delta, .data = is->buf + buf_delta};
	{
		/* NOTE: drop a char if the subtext still doesn't fit */
		v2 nts = measure_text(ctx->font, buf);
		if (nts.w > 0.96 * r.size.w) {
			buf.data++;
			buf.len--;
		}
	}
	draw_text(ctx->font, buf, pos, colour);

	is->cursor_t = move_towards_f32(is->cursor_t, is->cursor_t_target, 1.5 * dt_for_frame);
	if (is->cursor_t == is->cursor_t_target) {
		if (is->cursor_t_target == 0) is->cursor_t_target = 1;
		else                          is->cursor_t_target = 0;
	}

	v4 bg = ctx->cursor_colour;
	bg.a  = 0;
	Color cursor_colour = rl_colour_from_normalized(lerp_v4(bg, ctx->cursor_colour, is->cursor_t));

	/* NOTE: guess a cursor position */
	if (is->cursor == -1) {
		/* NOTE: extra offset to help with putting a cursor at idx 0 */
		#define TEXT_HALF_CHAR_WIDTH 10
		f32 x_off = TEXT_HALF_CHAR_WIDTH, x_bounds = r.size.w * is->cursor_hover_p;
		s32 i;
		for (i = 0; i < is->count && x_off < x_bounds; i++) {
			/* NOTE: assumes font glyphs are ordered */
			s32 idx = is->buf[i] - 32;
			x_off  += ctx->font.glyphs[idx].advanceX;
			if (ctx->font.glyphs[idx].advanceX == 0)
				x_off += ctx->font.recs[idx].width;
		}
		is->cursor = i;
	}

	buf.len = is->cursor - buf_delta;
	v2 sts = measure_text(ctx->font, buf);
	f32 cursor_x = r.pos.x + sts.x;
	f32 cursor_width;
	if (is->cursor == is->count) cursor_width = MIN(ctx->window_size.w * 0.03, 20);
	else                         cursor_width = MIN(ctx->window_size.w * 0.01, 6);

	Rect cursor_r = {
		.pos  = {.x = cursor_x,     .y = pos.y},
		.size = {.w = cursor_width, .h = ts.h},
	};

	DrawRectangleRec(cursor_r.rr, cursor_colour);

	/* NOTE: handle multiple input keys on a single frame */
	for (s32 key = GetCharPressed();
	     is->count < (s32)countof(is->buf) && key > 0;
	     key = GetCharPressed())
	{
		mem_move(is->buf + is->cursor + 1,
		         is->buf + is->cursor,
		         is->count - is->cursor);

		is->buf[is->cursor++] = key;
		is->count++;
	}

	is->cursor -= (IsKeyPressed(KEY_LEFT)  || IsKeyPressedRepeat(KEY_LEFT))  && is->cursor > 0;
	is->cursor += (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) && is->cursor < is->count;

	if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && is->cursor > 0) {
		is->cursor--;
		if (is->cursor < countof(is->buf) - 1) {
			mem_move(is->buf + is->cursor,
			         is->buf + is->cursor + 1,
			         is->count - is->cursor - 1);
		}
		is->count--;
	}

	if ((IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) && is->cursor < is->count) {
		mem_move(is->buf + is->cursor,
		         is->buf + is->cursor + 1,
		         is->count - is->cursor - 1);
		is->count--;
	}

	if (IsKeyPressed(KEY_ENTER)) {
		parse_and_store_text_input(ctx);
		is->idx = -1;
	}
}

function s32
do_button(ButtonState *btn, v2 mouse, Rect r, f32 hover_speed)
{
	b32 hovered       = CheckCollisionPointRec(mouse.rv, r.rr);
	s32 pressed_mask  = 0;
	pressed_mask     |= MOUSE_LEFT  * (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
	pressed_mask     |= MOUSE_RIGHT * (hovered && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT));

	if (hovered) btn->hover_t += hover_speed * dt_for_frame;
	else         btn->hover_t -= hover_speed * dt_for_frame;
	CLAMP01(btn->hover_t);

	return pressed_mask;
}

function s32
do_rect_button(ButtonState *btn, v2 mouse, Rect r, Color bg, f32 hover_speed, f32 scale_target, f32 fade_t)
{
	s32 pressed_mask = do_button(btn, mouse, r, hover_speed);

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

function s32
do_text_button(ColourPickerCtx *ctx, ButtonState *btn, v2 mouse, Rect r, str8 text, v4 fg, Color bg)
{
	s32 pressed_mask = do_rect_button(btn, mouse, r, bg, HOVER_SPEED, 1, 1);

	v2 tpos   = center_align_text_in_rect(r, text, ctx->font);
	v2 spos   = {.x = tpos.x + 1.75, .y = tpos.y + 2};
	v4 colour = lerp_v4(fg, ctx->hover_colour, btn->hover_t);

	draw_text(ctx->font, text, spos, fade(BLACK, 0.8));
	draw_text(ctx->font, text, tpos, rl_colour_from_normalized(colour));

	return pressed_mask;
}

function void
do_slider(ColourPickerCtx *ctx, Rect r, s32 label_idx, v2 relative_mouse, str8 name)
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
		ctx->flags |= ColourPickerFlag_RefillTexture;
	}

	if (IsMouseButtonUp(MOUSE_BUTTON_LEFT))
		ctx->held_idx = -1;

	f32 current = ctx->colour.E[label_idx];

	{
		f32 scale_delta  = (SLIDER_SCALE_TARGET - 1.0) * SLIDER_SCALE_SPEED * dt_for_frame;
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
		b32 collides = CheckCollisionPointRec(relative_mouse.rv, vr.rr);
		if (collides && ctx->text_input_state.idx != (label_idx + 1)) {
			s->colour_t[label_idx] += HOVER_SPEED * dt_for_frame;
		} else {
			s->colour_t[label_idx] -= HOVER_SPEED * dt_for_frame;
		}
		CLAMP01(s->colour_t[label_idx]);

		if (!collides && ctx->text_input_state.idx == (label_idx + 1) &&
		    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			set_text_input_idx(ctx, -1, vr, relative_mouse);
			current = ctx->colour.E[label_idx];
		}

		v4 colour       = lerp_v4(normalize_colour(pack_rl_colour(ctx->fg)),
		                          ctx->hover_colour, s->colour_t[label_idx]);
		Color colour_rl = rl_colour_from_normalized(colour);

		if (collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
			set_text_input_idx(ctx, label_idx + 1, vr, relative_mouse);

		if (ctx->text_input_state.idx != (label_idx + 1)) {
			u8 vbuf[4];
			Stream vstream = {.data = vbuf, .cap = countof(vbuf)};
			stream_append_f64(&vstream, current, 100);
			str8 value = {.len = vstream.widx, .data = vbuf};
			draw_text(ctx->font, value, left_align_text_in_rect(vr, value, ctx->font), colour_rl);
		} else {
			do_text_input(ctx, vr, colour_rl, 4);
		}
	}
	draw_text(ctx->font, name, center_align_text_in_rect(lr, name, ctx->font), ctx->fg);
}

function void
do_status_bar(ColourPickerCtx *ctx, Rect r, v2 relative_mouse)
{
	Rect hex_r  = cut_rect_left(r, 0.5);
	Rect mode_r;
	get_slider_subrects(r, 0, 0, &mode_r);

	str8 mode_txt = str8("");
	switch (ctx->stored_colour_kind) {
	case ColourKind_RGB: mode_txt = str8("RGB"); break;
	case ColourKind_HSV: mode_txt = str8("HSV"); break;
	InvalidDefaultCase;
	}

	v2 mode_ts     = measure_text(ctx->font, mode_txt);
	mode_r.pos.y  += (mode_r.size.h - mode_ts.h) / 2;
	mode_r.size.w  = mode_ts.w;

	hover_var(ctx, relative_mouse, mode_r, &ctx->slider_mode_state.colour_kind_cycler);

	u8 hbuf[8];
	Stream hstream = {.data = hbuf, .cap = countof(hbuf)};
	stream_append_colour(&hstream, rl_colour_from_normalized(get_formatted_colour(ctx, ColourKind_RGB)));
	str8 hex   = {.len = hstream.widx, .data = hbuf};
	str8 label = str8("RGB: ");

	v2 label_size = measure_text(ctx->font, label);
	v2 hex_size   = measure_text(ctx->font, hex);

	f32 extra_input_scale = 1.07;
	hex_r.size.w = extra_input_scale * (label_size.w + hex_size.w);

	Rect label_r = cut_rect_left(hex_r,  label_size.x / hex_r.size.w);
	hex_r        = cut_rect_right(hex_r, label_size.x / hex_r.size.w);

	s32 hex_collides = CheckCollisionPointRec(relative_mouse.rv, hex_r.rr);

	if (!hex_collides && ctx->text_input_state.idx == INPUT_HEX &&
	    IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
		set_text_input_idx(ctx, -1, hex_r, relative_mouse);
		hstream.widx = 0;
		stream_append_colour(&hstream, rl_colour_from_normalized(get_formatted_colour(ctx, ColourKind_RGB)));
		hex.len = hstream.widx;
	}

	if (hex_collides && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		set_text_input_idx(ctx, INPUT_HEX, hex_r, relative_mouse);

	if (hex_collides && ctx->text_input_state.idx != INPUT_HEX)
		ctx->sbs.hex_hover_t += HOVER_SPEED * dt_for_frame;
	else
		ctx->sbs.hex_hover_t -= HOVER_SPEED * dt_for_frame;
	CLAMP01(ctx->sbs.hex_hover_t);

	v4 fg          = normalize_colour(pack_rl_colour(ctx->fg));
	v4 hex_colour  = lerp_v4(fg, ctx->hover_colour, ctx->sbs.hex_hover_t);
	v4 mode_colour = lerp_v4(fg, ctx->hover_colour, ctx->slider_mode_state.colour_kind_cycler.parameter);

	draw_text(ctx->font, label, left_align_text_in_rect(label_r, label, ctx->font), ctx->fg);

	Color hex_colour_rl = rl_colour_from_normalized(hex_colour);
	if (ctx->text_input_state.idx != INPUT_HEX) {
		draw_text(ctx->font, hex, left_align_text_in_rect(hex_r, hex, ctx->font), hex_colour_rl);
	} else {
		do_text_input(ctx, hex_r, hex_colour_rl, 8);
	}

	draw_text(ctx->font, mode_txt, mode_r.pos, rl_colour_from_normalized(mode_colour));
}

function void
do_colour_stack(ColourPickerCtx *ctx, Rect sa)
{
	ColourStackState *css = &ctx->colour_stack;

	/* NOTE: Small adjusment to align with mode text. TODO: Cleanup? */
	sa = scale_rect_centered(sa, (v2){.x = 1, .y = 0.98});
	sa.pos.y += 0.02 * sa.size.h;

	Rect r    = sa;
	r.size.h *= 1.0 / (countof(css->items) + 3);
	r.size.w *= 0.75;
	r.pos.x  += (sa.size.w - r.size.w) * 0.5;

	f32 y_pos_delta = r.size.h * 1.2;
	r.pos.y -= y_pos_delta * css->y_off_t;

	/* NOTE: Stack is moving up; draw last top item as it moves up and fades out */
	if (css->fade_param) {
		ButtonState btn;
		do_rect_button(&btn, ctx->mouse_pos, r, rl_colour_from_normalized(css->last),
		               0, 1, css->fade_param);
		r.pos.y += y_pos_delta;
	}

	f32 stack_scale_target = (f32)(countof(css->items) + 1) / countof(css->items);

	f32 fade_scale[countof(css->items)] = { [countof(css->items) - 1] = 1 };
	for (u32 i = 0; i < countof(css->items); i++) {
		s32 cidx    = (css->widx + i) % countof(css->items);
		Color bg    = rl_colour_from_normalized(css->items[cidx]);
		b32 pressed = do_rect_button(css->buttons + cidx, ctx->mouse_pos, r, bg,
		                             BUTTON_HOVER_SPEED, stack_scale_target,
		                             1 - css->fade_param * fade_scale[i]);
		if (pressed) {
			v4 hsv = rgb_to_hsv(css->items[cidx]);
			store_formatted_colour(ctx, hsv, ColourKind_HSV);
			if (ctx->mode == CPM_PICKER) {
				ctx->pms.base_hue       = hsv.x;
				ctx->pms.fractional_hue = 0;
			}
		}
		r.pos.y += y_pos_delta;
	}

	css->fade_param -= BUTTON_HOVER_SPEED * dt_for_frame;
	css->y_off_t    += BUTTON_HOVER_SPEED * dt_for_frame;
	if (css->fade_param < 0) {
		css->fade_param = 0;
		css->y_off_t    = 0;
	}

	r.pos.y   = sa.pos.y + sa.size.h - r.size.h;
	r.pos.x  += r.size.w * 0.1;
	r.size.w *= 0.8;

	b32 push_pressed = do_button(&css->tri_btn, ctx->mouse_pos, r, BUTTON_HOVER_SPEED);
	f32 param    = css->tri_btn.hover_t;
	v2 tri_size  = {.x = 0.25 * r.size.w,          .y = 0.5 * r.size.h};
	v2 tri_scale = {.x = 1 - 0.5 * param,          .y = 1 + 0.3 * param};
	v2 tri_mid   = {.x = r.pos.x + 0.5 * r.size.w, .y = r.pos.y - 0.3 * r.size.h * param};
	draw_cardinal_triangle(tri_mid, tri_size, tri_scale, NORTH, ctx->fg);

	if (push_pressed) {
		css->fade_param         = 1.0;
		css->last               = css->items[css->widx];
		css->items[css->widx++] = get_formatted_colour(ctx, ColourKind_RGB);
		if (css->widx == countof(css->items))
			css->widx = 0;
	}
}

function void
do_colour_selector(ColourPickerCtx *ctx, Rect r)
{
	Color colour  = rl_colour_from_normalized(get_formatted_colour(ctx, ColourKind_RGB));
	Color pcolour = rl_colour_from_normalized(ctx->previous_colour);

	Rect cs[2] = {cut_rect_left(r, 0.5), cut_rect_right(r, 0.5)};
	DrawRectangleRec(cs[0].rr, pcolour);
	DrawRectangleRec(cs[1].rr, colour);

	v4 fg        = normalize_colour(pack_rl_colour(ctx->fg));
	str8 labels[2] = {str8("Revert"), str8("Apply")};

	s32 pressed_idx = -1;
	for (u32 i = 0; i < countof(cs); i++) {
		if (CheckCollisionPointRec(ctx->mouse_pos.rv, cs[i].rr) && ctx->held_idx == -1) {
			ctx->selection_hover_t[i] += HOVER_SPEED * dt_for_frame;

			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
				pressed_idx = i;
		} else {
			ctx->selection_hover_t[i] -= HOVER_SPEED * dt_for_frame;
		}

		CLAMP01(ctx->selection_hover_t[i]);

		v4 colour = lerp_v4(fg, ctx->hover_colour, ctx->selection_hover_t[i]);

		v2 fpos = center_align_text_in_rect(cs[i], labels[i], ctx->font);
		v2 pos  = fpos;
		pos.x  += 1.75;
		pos.y  += 2;
		draw_text(ctx->font, labels[i], pos,  fade(BLACK, 0.8));
		draw_text(ctx->font, labels[i], fpos, rl_colour_from_normalized(colour));
	}

	DrawRectangleRoundedLinesEx(r.rr, SELECTOR_ROUNDNESS, 0, 4 * SELECTOR_BORDER_WIDTH, ctx->bg);
	DrawRectangleRoundedLinesEx(r.rr, SELECTOR_ROUNDNESS, 0, SELECTOR_BORDER_WIDTH,
	                            SELECTOR_BORDER_COLOUR);
	v2 start  = cs[1].pos;
	v2 end    = cs[1].pos;
	end.y    += cs[1].size.h;
	DrawLineEx(start.rv, end.rv, SELECTOR_BORDER_WIDTH, SELECTOR_BORDER_COLOUR);

	if      (pressed_idx == 0) store_formatted_colour(ctx, ctx->previous_colour, ColourKind_RGB);
	else if (pressed_idx == 1) ctx->previous_colour = get_formatted_colour(ctx, ColourKind_RGB);

	if (pressed_idx != -1) {
		ctx->pms.base_hue       = get_formatted_colour(ctx, ColourKind_HSV).x;
		ctx->pms.fractional_hue = 0;
	}
}

function void
do_slider_shader(ColourPickerCtx *ctx, Rect r, s32 colour_mode, f32 *regions, f32 *colours)
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
	rlSetUniform(ctx->colours_id,      colours,       RL_SHADER_UNIFORM_VEC4,  3);
	rlSetUniform(ctx->regions_id,      regions,       RL_SHADER_UNIFORM_VEC4,  4);
	DrawRectanglePro(r.rr, (Vector2){0}, 0, BLACK);
	EndShaderMode();
}

function void
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


	local_persist str8 colour_slider_labels[ColourKind_Last][4] = {
		[ColourKind_RGB] = { str8("R"), str8("G"), str8("B"), str8("A") },
		[ColourKind_HSV] = { str8("H"), str8("S"), str8("V"), str8("A") },
	};
	for (s32 i = 0; i < 4; i++) {
		str8 name = colour_slider_labels[ctx->stored_colour_kind][i];
		do_slider(ctx, ss, i, relative_mouse, name);
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
	do_slider_shader(ctx, tr, ctx->stored_colour_kind, regions, (f32 *)colours);

	EndTextureMode();

	END_CYCLE_COUNT(CC_DO_SLIDER);
}


#define PM_LEFT   0
#define PM_MIDDLE 1
#define PM_RIGHT  2

function v4
do_vertical_slider(ColourPickerCtx *ctx, v2 test_pos, Rect r, s32 idx,
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
		if (should_scale) ctx->pms.scale_t[idx] += SLIDER_SCALE_SPEED * dt_for_frame;
		else              ctx->pms.scale_t[idx] -= SLIDER_SCALE_SPEED * dt_for_frame;
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

function void
do_picker_mode(ColourPickerCtx *ctx, v2 relative_mouse)
{
	BEGIN_CYCLE_COUNT(CC_DO_PICKER);

	v4 colour = get_formatted_colour(ctx, ColourKind_HSV);
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
		do_slider_shader(ctx, tr, ColourKind_HSV, regions, (f32 *)hsv);
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
		if (should_scale) ctx->pms.scale_t[PM_RIGHT] += SLIDER_SCALE_SPEED * dt_for_frame;
		else              ctx->pms.scale_t[PM_RIGHT] -= SLIDER_SCALE_SPEED * dt_for_frame;
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

	store_formatted_colour(ctx, colour, ColourKind_HSV);

	END_CYCLE_COUNT(CC_DO_PICKER);
}


#ifdef _DEBUG
#include <stdio.h>
#endif
function void
debug_dump_info(ColourPickerCtx *ctx)
{
	(void)ctx;
#ifdef _DEBUG
	if (IsKeyPressed(KEY_F1))
		ctx->flags ^= ColourPickerFlag_PrintDebug;

	DrawFPS(20, 20);

	local_persist char *fmts[CC_LAST] = {
		[CC_WHOLE_RUN] = "Whole Run:   %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_DO_PICKER] = "Picker Mode: %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_DO_SLIDER] = "Slider Mode: %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_UPPER]     = "Upper:       %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_LOWER]     = "Lower:       %7ld cyc | %2d h | %7d cyc/h\n",
		[CC_TEMP]      = "Temp:        %7ld cyc | %2d h | %7d cyc/h\n",
	};

	s64 cycs[CC_LAST];
	s64 hits[CC_LAST];

	for (u32 i = 0; i < CC_LAST; i++) {
		cycs[i] = g_debug_clock_counts.total_cycles[i];
		hits[i] = g_debug_clock_counts.hit_count[i];
		g_debug_clock_counts.hit_count[i]  = 0;
		g_debug_clock_counts.total_cycles[i] = 0;
	}

	if (!(ctx->flags & ColourPickerFlag_PrintDebug))
		return;

	local_persist u32 fcount;
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

function void
colour_picker_begin_interact(ColourPickerCtx *ctx, b32 scroll)
{
	InteractionState *is = &ctx->interaction;
	if (is->hot) {
		switch (is->hot->kind) {
		case VariableKind_Cycler: {
			if (scroll) is->kind = InteractionKind_Scroll;
			else        is->kind = InteractionKind_Set;
		} break;
		InvalidDefaultCase;
		}
	}

	if (is->kind != InteractionKind_None) {
		is->active = is->hot;
		is->rect   = is->hot_rect;
	}
}

function void
colour_picker_end_interact(ColourPickerCtx *ctx, b32 mouse_left_pressed, b32 mouse_right_pressed)
{
	InteractionState *is = &ctx->interaction;
	switch (is->kind) {
	case InteractionKind_Scroll: {
		f32 delta = GetMouseWheelMoveV().y;
		switch (is->active->kind) {
		case VariableKind_Cycler: {
			is->active->cycler.state += delta;
			is->active->cycler.state %= is->active->cycler.length;
		} break;
		InvalidDefaultCase;
		}
	} break;
	case InteractionKind_Set: {
		switch (is->active->kind) {
		case VariableKind_Cycler: {
			s32 delta = (s32)mouse_left_pressed - (s32)mouse_right_pressed;
			is->active->cycler.state += delta;
			is->active->cycler.state %= is->active->cycler.length;
		} break;
		InvalidDefaultCase;
		}
	} break;
	InvalidDefaultCase;
	}

	if (is->active->flags & VariableFlag_UpdateStoredMode) {
		ASSERT(is->active->kind == VariableKind_Cycler);
		ctx->colour = convert_colour(ctx->colour, ctx->stored_colour_kind,
		                             is->active->cycler.state);
		ctx->stored_colour_kind = is->active->cycler.state;
	}

	is->kind   = InteractionKind_None;
	is->active = 0;
}

function void
colour_picker_interact(ColourPickerCtx *ctx, v2 mouse)
{
	InteractionState *is = &ctx->interaction;
	if (!is->active) is->hot = is->next_hot;
	is->next_hot = 0;

	b32 mouse_left_pressed  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
	b32 mouse_right_pressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
	b32 wheel_moved         = GetMouseWheelMoveV().y != 0;
	if (mouse_left_pressed || mouse_right_pressed || wheel_moved) {
		if (is->kind != InteractionKind_None)
			colour_picker_end_interact(ctx, mouse_left_pressed, mouse_right_pressed);
		colour_picker_begin_interact(ctx, wheel_moved);
	}

	switch (is->kind) {
	case InteractionKind_None: break;
	case InteractionKind_Scroll:
	case InteractionKind_Set: {
		colour_picker_end_interact(ctx, mouse_left_pressed, mouse_right_pressed);
	} break;
	InvalidDefaultCase;
	}

	ctx->last_mouse = mouse;
}

function void
colour_picker_init(ColourPickerCtx *ctx)
{
#ifdef _DEBUG
	ctx->picker_shader   = LoadShader(0, HSV_LERP_SHADER_NAME);
#else
	ctx->picker_shader   = LoadShaderFromMemory(0, _binary_slider_lerp_glsl_start);
#endif
	ctx->mode_id         = GetShaderLocation(ctx->picker_shader, "u_mode");
	ctx->colour_mode_id  = GetShaderLocation(ctx->picker_shader, "u_colour_mode");
	ctx->colours_id      = GetShaderLocation(ctx->picker_shader, "u_colours");
	ctx->regions_id      = GetShaderLocation(ctx->picker_shader, "u_regions");
	ctx->radius_id       = GetShaderLocation(ctx->picker_shader, "u_radius");
	ctx->border_thick_id = GetShaderLocation(ctx->picker_shader, "u_border_thick");

	local_persist str8 colour_kind_labels[ColourKind_Last] = {
		[ColourKind_RGB] = str8("RGB"),
		[ColourKind_HSV] = str8("HSV"),
	};
	ctx->slider_mode_state.colour_kind_cycler.kind  = VariableKind_Cycler;
	ctx->slider_mode_state.colour_kind_cycler.flags = VariableFlag_UpdateStoredMode;
	ctx->slider_mode_state.colour_kind_cycler.cycler.state  = ctx->stored_colour_kind;
	ctx->slider_mode_state.colour_kind_cycler.cycler.length = countof(colour_kind_labels);
	ctx->slider_mode_state.colour_kind_cycler.cycler.labels = colour_kind_labels;

	ctx->flags |= ColourPickerFlag_Ready;
}

DEBUG_EXPORT void
do_colour_picker(ColourPickerCtx *ctx, f32 dt, Vector2 window_pos, Vector2 mouse_pos)
{
	BEGIN_CYCLE_COUNT(CC_WHOLE_RUN);

	dt_for_frame = dt;

	if (IsWindowResized()) {
		ctx->window_size.h = GetScreenHeight();
		ctx->window_size.w = ctx->window_size.h / WINDOW_ASPECT_RATIO;
		SetWindowSize(ctx->window_size.w, ctx->window_size.h);

		UnloadTexture(ctx->font.texture);
		if (ctx->window_size.w < 480) ctx->font = LoadFont_lora_sb_1_inc();
		else                          ctx->font = LoadFont_lora_sb_0_inc();
	}

	if (!(ctx->flags & ColourPickerFlag_Ready))
		colour_picker_init(ctx);

	ctx->mouse_pos.rv  = mouse_pos;
	ctx->window_pos.rv = window_pos;

	colour_picker_interact(ctx, ctx->mouse_pos);

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
		/* TODO(rnp): move this into single resize function */
		if (ctx->picker_texture.texture.width != (s32)(ma.size.w)) {
			s32 w = ma.size.w;
			s32 h = ma.size.h;
			UnloadRenderTexture(ctx->picker_texture);
			ctx->picker_texture = LoadRenderTexture(w, h);
			if (ctx->mode != CPM_PICKER) {
				s32 mode  = ctx->mode;
				ctx->mode = CPM_PICKER;
				do_picker_mode(ctx, ma_relative_mouse);
				ctx->mode = mode;
			}
		}

		if (ctx->slider_texture.texture.width != (s32)(ma.size.w)) {
			s32 w = ma.size.w;
			s32 h = ma.size.h;
			UnloadRenderTexture(ctx->slider_texture);
			ctx->slider_texture = LoadRenderTexture(w, h);
			if (ctx->mode != CPM_SLIDERS) {
				s32 mode  = ctx->mode;
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

		Rect tr = {.size.w =  ctx->slider_texture.texture.width,
		           .size.h = -ctx->slider_texture.texture.height};

		NPatchInfo tnp = {tr.rr, 0, 0, 0, 0, NPATCH_NINE_PATCH };
		for (u32 i = 0; i < CPM_LAST; i++) {
			if (do_button(ctx->mcs.buttons + i, ctx->mouse_pos, mb, 10)) {
				if (ctx->mode != i)
					ctx->mcs.next_mode = i;
			}

			if (ctx->mcs.next_mode != -1) {
				ctx->mcs.mode_visible_t -= 2 * dt_for_frame;
				if (ctx->mcs.mode_visible_t < 0) {
					ctx->mode          = ctx->mcs.next_mode;
					ctx->mcs.next_mode = -1;
					if (ctx->mode == CPM_PICKER) {
						v4 hsv = get_formatted_colour(ctx, ColourKind_HSV);
						ctx->pms.base_hue       = hsv.x;
						ctx->pms.fractional_hue = 0;
					}
					ctx->flags |= ColourPickerFlag_RefillTexture;
				}
			} else {
				ctx->mcs.mode_visible_t += 2 * dt_for_frame;
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
		Color bg      = rl_colour_from_normalized(get_formatted_colour(ctx, ColourKind_RGB));
		Rect btn_r    = mb;
		btn_r.size.h *= 0.46;

		if (do_text_button(ctx, ctx->buttons + 0, ctx->mouse_pos, btn_r, str8("Copy"), fg, bg)) {
			/* NOTE: SetClipboardText needs a NUL terminated string */
			u8 cbuf[9] = {0};
			Stream cstream = {.data = cbuf, .cap = countof(cbuf) - 1};
			stream_append_colour(&cstream, bg);
			SetClipboardText((char *)cbuf);
		}
		btn_r.pos.y += 0.54 * mb.size.h;

		if (do_text_button(ctx, ctx->buttons + 1, ctx->mouse_pos, btn_r, str8("Paste"), fg, bg)) {
			str8 txt = str8_from_c_str((char *)GetClipboardText());
			if (txt.len) {
				v4 new_colour = normalize_colour(parse_hex_u32(txt));
				ctx->colour = convert_colour(new_colour, ColourKind_RGB, ctx->stored_colour_kind);
				if (ctx->mode == CPM_PICKER) {
					f32 hue = get_formatted_colour(ctx, ColourKind_HSV).x;
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
