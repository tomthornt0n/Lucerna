#version 330 core

out vec2 v_texture_coordinates;

void main() 
{
    float x = float(((uint(gl_VertexID) + 2) / 3) % 2); 
    float y = float(((uint(gl_VertexID) + 1) / 3) % 2); 

    gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);
    v_texture_coordinates = vec2(x, y);
}
