/* See LICENSE for copyright details */
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 out_colour;

#define MODE_H   0
#define MODE_SV  1

uniform int  u_mode;
uniform vec4 u_hsv[2];
uniform vec2 u_offset;
uniform vec2 u_size;

/* input:  h [0,360] | s,v [0, 1] *
 * output: rgb [0,1]              */
vec3 hsv2rgb(vec3 hsv)
{
	vec3 k = mod(vec3(5, 3, 1) + hsv.x / 60, 6);
	k = max(min(min(k, 4 - k), 1), 0);
	return hsv.z - hsv.z * hsv.y * k;
}

void main()
{
	vec2 pos = (gl_FragCoord.xy - u_offset) / u_size;

	vec4 out_hsv;
	switch (u_mode) {
	case MODE_H:
		out_hsv = mix(u_hsv[0], u_hsv[1], pos.y);
		break;
	case MODE_SV:
		out_hsv = vec4(u_hsv[0].x, pos.x, pos.y, 1);
		break;
	}
	out_hsv.x *= 360;
	out_colour = vec4(hsv2rgb(out_hsv.xyz), 1);
}
