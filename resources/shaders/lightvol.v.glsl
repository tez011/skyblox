#version 330 core

in vec3 vertex, light_color, light_strength;
in vec4 light_pos_size;
out vec3 f_light_pos, f_light_color, f_light_strength;

uniform mat4 vp;

void main()
{
	mat4 transl = mat4(1), scale = mat4(mat3(light_pos_size.w));
	transl[3].xyz = light_pos_size.xyz;
	mat4 model = transl * scale;

	f_light_pos = light_pos_size.xyz;
	f_light_strength = light_strength;
	f_light_color = light_color;
	gl_Position = vp * model * vec4(vertex, 1);
}
