#version 330 core

in vec4 vertex;
in vec4 v_texture;
out vec3 f_normal, f_texture, f_worldspace;

uniform mat4 model, vp;
const vec3 cube_normal[6] = vec3[](vec3(0, 0, 1), vec3(0, 0, -1), vec3(0, 1, 0), vec3(0, -1, 0), vec3(1, 0, 0), vec3(-1, 0, 0));

void main()
{
	vec4 pos_worldspace;

	f_texture = v_texture.xyz;
	f_normal = sign(vertex.w) * cube_normal[int(abs(vertex.w) - 1)];
	pos_worldspace = model * vec4(vertex.xyz, 1);
	f_worldspace = pos_worldspace.xyz;
	gl_Position = vp * pos_worldspace;
}
