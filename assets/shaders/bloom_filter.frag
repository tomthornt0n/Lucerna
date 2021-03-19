#version 330 core

layout(location=0) out vec4 o_colour;

in vec4 v_colour;
in vec2 v_texture_coordinates;

uniform sampler2D u_texture;

const float filter_threshold = 0.89;

const float desired_aspect = 9.0 / 16.0;

void main()
{
	vec2 screen_size = textureSize(u_texture, 0);
 float correct_height = (screen_size.x * desired_aspect) / screen_size.y;
 float min_y = (1.0 - correct_height) / 2;
 float max_y = min_y + correct_height;
 
 vec3 tex_col = texture2D(u_texture, v_texture_coordinates).rgb;
	float brightness = dot(tex_col, vec3(0.2126, 0.7152, 0.0722));
 if(brightness > filter_threshold &&
    v_texture_coordinates.y > min_y &&
    v_texture_coordinates.y < max_y)
	{
  o_colour = vec4(tex_col, 1.0);
	}
 else
	{
  o_colour = vec4(0.0, 0.0, 0.0, 1.0);
	}
}

