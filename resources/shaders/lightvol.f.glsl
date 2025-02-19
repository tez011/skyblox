#version 330 core

in vec3 f_light_pos, f_light_color, f_light_strength;

layout(location = 0) out vec3 ambient_diffuse;
layout(location = 1) out vec3 specular;

uniform vec2 fbdims;
uniform vec3 view_pos;
uniform sampler2D gbuf_position, gbuf_normal, gbuf_specular_shininess;

void main()
{
	vec2 uv = gl_FragCoord.xy / fbdims;
	vec3 frag_pos = texture(gbuf_position, uv).xyz, normal = texture(gbuf_normal, uv).xyz;
	vec3 view_dir = normalize(view_pos - frag_pos);
	float shininess = texture(gbuf_specular_shininess, uv).w;

	vec3 light_dir = normalize(f_light_pos - frag_pos), halfway_dir = normalize(light_dir + view_dir);
	float frag_dist = max(0, length(f_light_pos - frag_pos) - 0.5),
	      inv_intensity = dot(f_light_strength, vec3(1, frag_dist, frag_dist * frag_dist));

	float ambient = 0.75 / inv_intensity;
	float diffuse = max(0, dot(normal, light_dir) / inv_intensity);
	float spec = pow(max(dot(normal, halfway_dir), 0), shininess);

	ambient_diffuse = (ambient + diffuse) * f_light_color;
	specular = spec * f_light_color;
}
