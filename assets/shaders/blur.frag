#version 330 core

layout(location=0) out vec4 o_colour;

in vec2 v_texture_coordinates;

uniform sampler2D u_texture;
uniform vec2 u_direction;

const float weight[6] = float[] (0.382925,	0.24173,	0.060598,	0.005977,	0.000229,	0.000003);

void main()
{

    vec2 tex_offset = 1.0 / textureSize(u_texture, 0);
    vec3 result = texture(u_texture, v_texture_coordinates).rgb * weight[0];

    for(int i = 1; i < 6; ++i)
    {
        vec2 offset = tex_offset * u_direction * i;
        result += texture(u_texture, v_texture_coordinates + offset).rgb * weight[i];
        result += texture(u_texture, v_texture_coordinates - offset).rgb * weight[i];
    }

    o_colour = vec4(result, 1.0);
}

