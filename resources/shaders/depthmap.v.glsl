#version 330 core

in vec3 vertex, v_texture;

uniform mat4 lightspace, model;

void main()
{
	gl_Position = lightspace * model * vec4(vertex, 1.0);
}
