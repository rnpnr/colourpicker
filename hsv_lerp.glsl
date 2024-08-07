/* See LICENSE for copyright details */
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 out_colour;

#define CM_RGB  0
#define CM_HSV  1

uniform int   u_colour_mode;
uniform float u_radius;
uniform float u_border_thick;
uniform vec4  u_colours[3];
uniform vec4  u_regions[4];
uniform vec2  u_size;

/* input:  h [0,360] | s,v [0, 1] *
 * output: rgb [0,1]              */
vec3 hsv2rgb(vec3 hsv)
{
	vec3 k = mod(vec3(5, 3, 1) + hsv.x / 60, 6);
	k = max(min(min(k, 4 - k), 1), 0);
	return hsv.z - hsv.z * hsv.y * k;
}

bool in_rounded_region(vec2 pos, vec4 min_max, float radius)
{
	vec2 min_xy = min_max.xy;
	vec2 max_xy = min_max.zw;
	vec2 ulp    = vec2(min_xy.x + radius, max_xy.y - radius);
	vec2 urp    = vec2(max_xy.x - radius, max_xy.y - radius);
	vec2 blp    = vec2(min_xy.x + radius, min_xy.y + radius);
	vec2 brp    = vec2(max_xy.x - radius, min_xy.y + radius);

	bool ulr  = length(ulp - pos) < radius;
	bool urr  = length(urp - pos) < radius;
	bool blr  = length(blp - pos) < radius;
	bool brr  = length(brp - pos) < radius;
	bool rect = (pos.x > min_xy.x) && (pos.x < max_xy.x) &&
	            (pos.y < max_xy.y - radius) && (pos.y > min_xy.y + radius);
	bool upr  = (pos.x > ulp.x) && (pos.x < urp.x) && (pos.y < max_xy.y) && (pos.y > min_xy.y);

	return rect || ulr || urr || blr || brr || upr;
}

void main()
{
	vec2 pos = gl_FragCoord.xy;

	vec4 colour = vec4(0);
	switch (u_colour_mode) {
	case CM_RGB:
		out_colour = vec4(0);
		break;
	case CM_HSV:
		vec4 shift = vec4(vec2(u_border_thick), -vec2(u_border_thick));

		if (in_rounded_region(pos, u_regions[0], u_radius + u_border_thick)) {
			colour.w = 1;
			if (in_rounded_region(pos, u_regions[0] + shift, u_radius)) {
				float scale = u_regions[0].w - u_regions[0].y;
				colour = mix(vec4(1, u_colours[0].yzw), vec4(0, u_colours[0].yzw),
				             (pos.y - u_regions[0].y) / scale);
			}
		} else if (in_rounded_region(pos, u_regions[1], u_radius + u_border_thick)) {
			colour.w = 1;
			if (in_rounded_region(pos, u_regions[1] + shift, u_radius)) {
				float scale = u_regions[1].w - u_regions[1].y;
				colour = mix(u_colours[2], u_colours[1],
				             (pos.y - u_regions[1].y) / scale);
			}
		} else if (in_rounded_region(pos, u_regions[2], u_radius + u_border_thick)) {
			colour.w = 1;
			if (in_rounded_region(pos, u_regions[2] + shift, u_radius)) {
				vec2 scale = u_regions[2].zw - u_regions[2].xy;
				vec2 t     = (pos - u_regions[2].xy) / scale;
				colour     = vec4(u_colours[0].x, t.x, t.y, 1);
			}
		}
		colour.x   *= 360;
		out_colour  = vec4(hsv2rgb(colour.xyz), colour.w);
		break;
	}
}
