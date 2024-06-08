/* See LICENSE for copyright details */
#include <raylib.h>
#include "util.c"

void
do_colour_picker(ColourPickerCtx *ctx)
{
	ClearBackground(BLACK);
	DrawFPS(20, 20);

	uv2 ws = ctx->window_size;

	{
		v2  cc = { .x = (f32)ws.w / 2, .y = (f32)ws.h / 4 };
		DrawCircleSector(cc.rv, 0.6 * cc.x, 0, 360, 69, RED);
		DrawCircleSector(cc.rv, 0.58 * cc.x, 0, 360, 69, BLACK);
		DrawText("Hello World!", ws.w * 0.3, ws.h / 4, 48, BLUE);
	}

	{
		v2 start = { .x = 0, .y = (f32)ws.h / 2 };
		v2 end   = { .x = ws.h, .y = (f32)ws.h / 2 };
		DrawLineEx(start.rv, end.rv, 8, BLUE);
	}

	{
		v2 subregion    = {
			.x = (f32)ws.w * 0.9,
			.y = (f32)ws.h / 2 * 0.9,
		};

		v2 starting_pos = {
			.x = 0.05 * (f32)ws.w,
			.y = (f32)ws.h / 2 + 0.25 * ((f32)ws.h / 2),
		};

		v2 size = { .x = subregion.x, .y = 0.15 * subregion.y };

		DrawRectangleV(starting_pos.rv, size.rv, DARKGREEN);

		starting_pos.y += size.y + 0.1 * ((f32)ws.h / 2);
		DrawRectangleV(starting_pos.rv, size.rv, DARKGREEN);

		starting_pos.y += size.y + 0.1 * ((f32)ws.h / 2);
		DrawRectangleV(starting_pos.rv, size.rv, DARKGREEN);
	}
}
