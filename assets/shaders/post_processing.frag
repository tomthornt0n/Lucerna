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
	vec3 blur_col = texture(u_blur_texture, v_texture_coordinates).rgb;
	vec3 screen_col = texture(u_screen_texture, v_texture_coordinates).rgb;

	float blur_luminance_squared = dot(blur_col, vec3(0.2125, 0.7154, 0.0721));
	blur_luminance_squared = blur_luminance_squared * blur_luminance_squared;

	vec3 noise = vec3(rand(v_texture_coordinates + vec2(u_time))) * noise_intensity;

 vec3 vignette_col = vec3(vignette(v_texture_coordinates, vignette_radius, 0.5));

	vec3 mixed_col = screen_col * blur_col * blur_luminance_squared * bloom_tint + noise;
	mixed_col = mix(screen_col, mixed_col, overall_strength);

	o_colour = vec4((pow(mixed_col * u_exposure, vec3(1 / gamma)) - vignette_col), 1.0);
}