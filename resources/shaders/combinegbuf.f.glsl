#version 330 core

in vec2 uv;
out vec4 color;

uniform sampler2D gp_albedo_transparency, gp_specular, lp_ambdif, lp_specular, depth_buffer;
uniform float gamma = 2.2, exposure = 2.5;

void main()
{
	vec4 depth = texture(depth_buffer, uv);
	if (depth.r >= 1)
		discard;

	vec4 albedo_transparency = texture(gp_albedo_transparency, uv);
	vec3 albedo = pow(albedo_transparency.rgb, vec3(gamma)), ambdif = max(vec3(0.01), texture(lp_ambdif, uv).rgb);
	vec3 specfactor = texture(gp_specular, uv).rgb, specular = texture(lp_specular, uv).rgb;

	vec3 hdr_color = ambdif * albedo + max(vec3(0), specular * specfactor);
	vec3 mapped_color = vec3(1) - exp(-hdr_color * exposure);
	color = vec4(pow(mapped_color, 1 / vec3(gamma)), albedo_transparency.a);
}
