#version 330 core

layout(location=0) out vec4 o_colour;

in vec2 v_texture_coordinates;

uniform sampler2D u_blur_texture;
uniform sampler2D u_screen_texture;
uniform float u_time;
uniform float u_exposure;

const float warp_amount = 0.001;
const float warp_variance = 5.0;
const vec3 bloom_tint = vec3(0.9, 0.8, 1.0);
const float bloom_amount = 0.8;
const float noise_intensity = 0.03;
const vec3 vignette_colour = vec3(0.9, 0.8, 1.0);

float vignette(vec2 uv, float radius, float smoothness)
{
	float diff = radius - distance(uv, vec2(0.5, 0.5));
	return smoothstep(-smoothness, smoothness, -diff);
}

float fmod(float x, float y)
{
	return x - y * (floor(x / y));
}

float rand(vec2 co)
{
	return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
 vec3 result;
 
 // NOTE(tbt): distort texture coordinates over time
	vec2 warped_texture_coordinates = vec2(v_texture_coordinates.x - warp_amount * sin(u_time + v_texture_coordinates.y * warp_variance),
	                                       v_texture_coordinates.y - warp_amount * cos(u_time + v_texture_coordinates.x * warp_variance));
 
 // NOTE(tbt): sample the original screen colour at the distorted coordinates
 result = texture(u_screen_texture, warped_texture_coordinates).rgb;
 
 // NOTE(tbt): sample from the blurred texture and tint
 vec3 bloom = texture(u_blur_texture, warped_texture_coordinates).rgb * bloom_tint;
	
 // NOTE(tbt): generate noise
	vec3 noise = vec3(rand(warped_texture_coordinates + vec2(sin(u_time) * 0.001)));
 
 // NOTE(tbt): generate a vignette
 vec3	vignette = vec3(1.0) + vec3(vignette(warped_texture_coordinates, 0.5, 0.5)) * vignette_colour;
 
 // NOTE(tbt): add bloom
 result += bloom * bloom_amount;
 
 // NOTE(tbt): add noise
 result += noise * noise_intensity;
 
 // NOTE(tbt): multiply by vignette
 result *= vignette;
 
 // NOTE(tbt): adjust for exposure
 result = vec3(1.0) - exp((-result) * u_exposure);
 
 /*
 vec3 result = max(black_point,
                   vec3(1.0) - exp(-((screen_col + bloom_col + noise) * vignette) * u_exposure));
*/
 o_colour = vec4(result, 1.0);
}