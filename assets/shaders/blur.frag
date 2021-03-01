#version 330 core

layout(location=0) out vec4 o_colour;

in vec2 v_texture_coordinates;

uniform sampler2D u_texture;
uniform vec2 u_direction;

uniform float offset[3] = float[](0.0, 1.3846153846, 3.2307692308);
uniform float weight[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

void main()
{
 o_colour = texture2D(u_texture, v_texture_coordinates) * weight[0];

 for (int i = 1;
      i < 3;
      i++)
 {
  vec2 current_offset = vec2(offset[i]) * u_direction / textureSize(u_texture, 0);

  o_colour += texture2D(u_texture,
                        v_texture_coordinates + current_offset) * weight[i];

		o_colour += texture2D(u_texture,
                        v_texture_coordinates - current_offset) * weight[i];
 }
}

