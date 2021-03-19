#version 330 core

layout(location=0) out vec4 o_colour;

in vec2 v_texture_coordinates;

uniform sampler2D u_blur_texture;
uniform sampler2D u_screen_texture;
uniform float u_time;
uniform float u_exposure;

const vec3 bloom_tint = vec3(0.75);
const float noise_intensity = 0.03;
const float vignette_radius = 0.5;
const float gamma = 2.2;
const float overall_strength = 0.95;

const float desired_aspect = 9.0 / 16.0;

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
 vec2 screen_size = textureSize(u_screen_texture, 0);
 float correct_height = (screen_size.x * desired_aspect) / screen_size.y;
 float min_y = (1.0 - correct_height) / 2;
 float max_y = min_y + correct_height;
 
	vec3 screen_col = texture(u_screen_texture, v_texture_coordinates).rgb  * vec3(v_texture_coordinates.y > min_y && v_texture_coordinates.y < max_y);
	vec3 blur_col = texture(u_blur_texture, v_texture_coordinates).rgb;
 
	float blur_luminance_squared = dot(blur_col, vec3(0.2125, 0.7154, 0.0721));
	blur_luminance_squared = blur_luminance_squared * blur_luminance_squared;
	// NOTE(tbt): wrap u_time at 6 seconds because the super jank noise function starts going a bit weird for large input numbers
	vec3 noise = vec3(rand(v_texture_coordinates + vec2(fmod(u_time, 6.0)))) * noise_intensity;
 
 vec3 vignette_col = vec3(vignette(v_texture_coordinates, vignette_radius, 0.5));
 
	vec3 mixed_col = screen_col * blur_col * blur_luminance_squared * bloom_tint + noise;
	mixed_col = mix(screen_col, mixed_col, overall_strength);
 
	o_colour = vec4((pow(mixed_col * u_exposure, vec3(1 / gamma)) - vignette_col), 1.0);
}