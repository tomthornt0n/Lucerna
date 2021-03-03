#version 330 core

layout(location=0) out vec4 o_colour;

in vec2 v_texture_coordinates;

uniform sampler2D u_blur_texture;
uniform sampler2D u_screen_texture;
uniform float u_time;
uniform float u_exposure;

const float warp_amount = 0.001;
const float warp_variance = 5.0;
const vec3 black_point = vec3(0.0);
const vec3 bloom_tint = vec3(0.9, 0.8, 1.0);
const float noise_intensity = 0.03;
const vec3 vignette_colour = vec3(0.9, 0.8, 1.0);

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
	vec2 warped_texture_coordinates = vec2(v_texture_coordinates.x - warp_amount * sin(u_time + v_texture_coordinates.y * warp_variance),
	                                       v_texture_coordinates.y - warp_amount * cos(u_time + v_texture_coordinates.x * warp_variance));
 
	vec3 screen_col = texture(u_screen_texture, warped_texture_coordinates).rgb;
	vec3 bloom_col = texture(u_blur_texture, warped_texture_coordinates).rgb * bloom_tint;
	// NOTE(tbt): wrap u_time at 6 seconds because the super jank noise function starts going a bit weird for large input numbers
	vec3 noise = vec3(rand(warped_texture_coordinates + vec2(fmod(u_time, 6.0)))) * noise_intensity;
	vec3	vignette = vec3(vignette(warped_texture_coordinates, 0.5, 0.5)) * vignette_colour;
 
 
	vec3 result = max(black_point,
	                  vec3(1.0) - exp(-(screen_col + bloom_col + noise + vignette) * u_exposure));
 
	o_colour = vec4(result, 1.0);
}