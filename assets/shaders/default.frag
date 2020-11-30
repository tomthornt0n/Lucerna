#version 330 core

layout(location=0) out vec4 o_colour;

in vec4 v_colour;
in vec2 v_texture_coordinates;

uniform sampler2D u_texture;

void main()
{
    o_colour = texture2D(u_texture, v_texture_coordinates) * v_colour;
}

