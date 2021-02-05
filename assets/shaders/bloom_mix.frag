#version 330 core

////
// NOTE(tbt): not really quite 'bloom'
////

layout(location=0) out vec4 o_colour;

in vec2 v_texture_coordinates;

uniform sampler2D u_blur_texture;
uniform sampler2D u_screen_texture;

const vec3 tint = vec3(0.7, 0.7, 0.7);
const float compensation = 1.5;

void main()
{
	vec3 blur_col = texture(u_blur_texture, v_texture_coordinates).rgb;
	float luminance = dot(blur_col, vec3(0.2125, 0.7154, 0.0721));
 o_colour = vec4((texture(u_screen_texture, v_texture_coordinates).rgb + blur_col * luminance * luminance * tint) * vec3(1.0 / compensation), 1.0);
}