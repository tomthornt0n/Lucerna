#version 330 core

layout(location=0) out vec4 o_colour;

in vec4 v_colour;
in vec2 v_texture_coordinates;

uniform sampler2D u_texture;
uniform vec2 u_direction;

const float radius = 1.0;

void main()
{
	vec4 sum = vec4(0.0);
	
	vec2 tc = v_texture_coordinates;
	
	float blur = radius / textureSize(u_texture, 0).x; 
    
	float hstep = u_direction.x;
	float vstep = u_direction.y;
    
	sum += texture2D(u_texture, vec2(tc.x - 4.0 * blur * hstep, tc.y - 4.0 * blur * vstep)) * 0.0162162162;
	sum += texture2D(u_texture, vec2(tc.x - 3.0 * blur * hstep, tc.y - 3.0 * blur * vstep)) * 0.0540540541;
	sum += texture2D(u_texture, vec2(tc.x - 2.0 * blur * hstep, tc.y - 2.0 * blur * vstep)) * 0.1216216216;
	sum += texture2D(u_texture, vec2(tc.x - 1.0 * blur * hstep, tc.y - 1.0 * blur * vstep)) * 0.1945945946;
	
	sum += texture2D(u_texture, vec2(tc.x, tc.y)) * 0.2270270270;
	
	sum += texture2D(u_texture, vec2(tc.x + 1.0 * blur * hstep, tc.y + 1.0 * blur * vstep)) * 0.1945945946;
	sum += texture2D(u_texture, vec2(tc.x + 2.0 * blur * hstep, tc.y + 2.0 * blur * vstep)) * 0.1216216216;
	sum += texture2D(u_texture, vec2(tc.x + 3.0 * blur * hstep, tc.y + 3.0 * blur * vstep)) * 0.0540540541;
	sum += texture2D(u_texture, vec2(tc.x + 4.0 * blur * hstep, tc.y + 4.0 * blur * vstep)) * 0.0162162162;

	gl_FragColor = v_colour * vec4(sum.rgb, 1.0);
}

