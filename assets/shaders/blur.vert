#version 330 core

layout(location=0) in vec2  a_position;
layout(location=1) in vec4  a_colour;
layout(location=2) in vec2  a_texture_coordinates;

out vec4 v_colour;
out vec2 v_texture_coordinates;

uniform mat4 u_projection_matrix;

void main()
{
    v_colour = a_colour;
    v_texture_coordinates = a_texture_coordinates;
    gl_Position = u_projection_matrix * vec4(a_position, 0.0, 1.0);
}

