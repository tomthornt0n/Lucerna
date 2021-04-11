#version 330 core

layout(location=0) out vec4 o_colour;

in vec2 v_texture_coordinates;

uniform sampler2D u_blur_texture;
uniform sampler2D u_screen_texture;
uniform float u_time;
uniform float u_exposure;

const vec3 bloom_tint = vec3(0.75);
const float noise_intensity = 0.02;
const float vignette_radius = 0.5;
const float gamma = 2.2;
const float overall_strength = 0.95;

float fmod(float x, float y)
{
	return x - y * (floor(x / y));
}

float rand(vec2 co)
{
	return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float vignette(vec2 uv, float radius, float smoothness)
{
	float diff = radius - distance(uv, vec2(0.5, 0.5));
	return smoothstep(-smoothness, smoothness, -diff);
}

void main()
{
	vec3 result;
 
 vec3 blur_col = texture(u_blur_texture, v_texture_coordinates).rgb;
	float blur_luminance = dot(blur_col, vec3(0.2125, 0.7154, 0.0721));
 
 vec3 original_col = texture(u_screen_texture, v_texture_coordinates).rgb;
 
 result = original_col;
 result *= blur_col;
 result *= blur_luminance * blur_luminance * bloom_tint;
	result += vec3(rand(v_texture_coordinates + vec2(fmod(u_time, 6.0)))) * noise_intensity;
 result = pow(result * u_exposure, vec3(1.0 / gamma));
 result -= vec3(vignette(v_texture_coordinates, vignette_radius, 0.5));
 
 o_colour = vec4(mix(original_col, result, overall_strength), 1.0);
}