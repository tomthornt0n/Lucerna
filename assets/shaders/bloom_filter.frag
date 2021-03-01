#version 330 core

layout(location=0) out vec4 o_colour;

in vec4 v_colour;
in vec2 v_texture_coordinates;

uniform sampler2D u_texture;

const float filter_threshold = 0.89;

void main()
{
	vec3 tex_col = texture2D(u_texture, v_texture_coordinates).rgb;
	float brightness = dot(tex_col, vec3(0.2126, 0.7152, 0.0722));
 if(brightness > filter_threshold)
	{
  o_colour = vec4(tex_col, 1.0);
	}
 else
	{
  o_colour = vec4(0.0, 0.0, 0.0, 1.0);
	}
}

