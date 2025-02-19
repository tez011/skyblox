#version 330 core

in vec3 f_normal, f_texture, f_worldspace;
layout(location = 0) out vec3 position;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec4 color;
layout(location = 3) out vec4 specular;

uniform bool first_peel_pass;
uniform vec2 fbdims;
uniform sampler2D depth_texture;
uniform sampler2DArray block_textures;

void main()
{
	position = f_worldspace;
	normal = f_normal;
	color = texture(block_textures, f_texture).rgba;
	specular = vec4(0);
	// specular = vec4(0.633, 0.7278, 0.633, 0.6);
	if (color.a < 0.01)
		discard;

	if (first_peel_pass == false) {
		vec2 fbtexcoord = gl_FragCoord.xy / fbdims;
		float max_depth = texture(depth_texture, fbtexcoord).r;
		if (gl_FragCoord.z <= max_depth)
			discard;
	}
}
