#version 330 core

in vec3 f_texture;
out vec4 color;

void main()
{
	if (abs(f_texture.s - 0.5) < 0.48 && abs(f_texture.t - 0.5) < 0.48)
		discard;

	color = vec4(0, 0, 0, 0.75);
}
