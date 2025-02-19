#version 330 core

out vec3 point;
uniform mat3 V_t;
uniform mat4 P_inv;

void main()
{
	vec2 v_coord = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
	gl_Position = vec4(v_coord * 2.0 - 1.0, 0, 1);
	point = (V_t * (P_inv * gl_Position).xyz);
}
