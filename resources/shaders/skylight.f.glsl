#version 330 core

/* Portions of, and the basis for, this code is Copyright (c) 2017 Eric Bruneton
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

https://github.com/ebruneton/precomputed_atmospheric_scattering/ */

const vec3 solar_irradiance = vec3(1.474000, 1.850400, 1.911980);
const int SCATTERING_TEXTURE_R_SIZE = 32;
const int SCATTERING_TEXTURE_MU_SIZE = 128;
const int SCATTERING_TEXTURE_MU_S_SIZE = 32;
const int SCATTERING_TEXTURE_NU_SIZE = 8;
const float solar_ang_radius = 0.004675;
const float sun_size = cos(0.00935 / 2.0);
const float PI = 3.14159265358979323846;
const float cos_neg_horizon_depth = -0.257912;
const float outer_radius = 6420.0, inner_radius = 6360.0, air_depth = outer_radius - inner_radius,
	    H = sqrt(outer_radius * outer_radius - inner_radius * inner_radius);

uniform sampler3D scattering_texture;

float DistanceToTopAtmosphereBoundary(float r, float mu)
{
	float discriminant = r * r * (mu * mu - 1.0) + outer_radius * outer_radius;
	return max(0.0, -r * mu + sqrt(max(discriminant, 0.0)));
}

float GetTextureCoordFromUnitRange(float x, int texture_size)
{
	return 0.5 / float(texture_size) + x * (1.0 - 1.0 / float(texture_size));
}

vec4 GetScatteringTextureUvwzFromRMuMuSNu(float r, float mu, float mu_s, float nu)
{
	float u_r = GetTextureCoordFromUnitRange(0.0, SCATTERING_TEXTURE_R_SIZE);
	float r_mu = r * mu;
	float discriminant = r_mu * r_mu - r * r + inner_radius * inner_radius;
	float d = -r_mu + sqrt(max(discriminant + H * H, 0.0));
	float u_mu = 0.5 + 0.5 * GetTextureCoordFromUnitRange((d - air_depth) / (H - air_depth), SCATTERING_TEXTURE_MU_SIZE / 2);
	d = DistanceToTopAtmosphereBoundary(inner_radius, mu_s);
	float a = (d - air_depth) / (H - air_depth);
	float A = -2.0 * cos_neg_horizon_depth * inner_radius / (H - air_depth);
	float u_mu_s = GetTextureCoordFromUnitRange(max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);
	float u_nu = (nu + 1.0) / 2.0;
	return vec4(u_nu, u_mu_s, u_mu, u_r);
}

vec3 GetExtrapolatedSingleMieScattering(vec4 scattering)
{
	const vec3 rayleigh_scattering = vec3(0.005802, 0.013558, 0.033100), mie_scattering = vec3(0.003996, 0.003996, 0.003996);

	return abs(sign(scattering.r)) * scattering.rgb * scattering.a / scattering.r * (rayleigh_scattering.r / mie_scattering.r) *
	       (mie_scattering / rayleigh_scattering);
}

vec3 GetCombinedScattering(float r, float mu, float mu_s, float nu, out vec3 single_mie_scattering)
{
	vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(r, mu, mu_s, nu);
	float tex_coord_x = uvwz.x * float(SCATTERING_TEXTURE_NU_SIZE - 1);
	float tex_x = floor(tex_coord_x);
	float lerp = tex_coord_x - tex_x;
	vec3 uvw0 = vec3((tex_x + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE), uvwz.z, uvwz.w);
	vec3 uvw1 = vec3((tex_x + 1.0 + uvwz.y) / float(SCATTERING_TEXTURE_NU_SIZE), uvwz.z, uvwz.w);
	vec4 combined_scattering = mix(texture(scattering_texture, uvw0), texture(scattering_texture, uvw1), lerp);
	vec3 scattering = vec3(combined_scattering);
	single_mie_scattering = GetExtrapolatedSingleMieScattering(combined_scattering);
	return scattering;
}

float RayleighPhaseFunction(float nu)
{
	float k = 3.0 / (16.0 * PI);
	return k * (1.0 + nu * nu);
}

float MiePhaseFunction(float nu)
{
	const float g = 0.8;
	float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
	return k * (1.0 + nu * nu) / pow(1.0 + g * g - 2.0 * g * nu, 1.5);
}

vec3 GetSkyRadiance(vec3 view_dir, vec3 sun_direction)
{
	float r = inner_radius;
	float rmu = inner_radius * view_dir.z; /* rmu := dot(<0, 0, inner_radius>, view_dir) */
	float mu = view_dir.z;
	float mu_s = sun_direction.z;
	float nu = dot(view_dir, sun_direction);
	vec3 single_mie_scattering,
		scattering = GetCombinedScattering(inner_radius, view_dir.z, sun_direction.z, nu, single_mie_scattering);

	return scattering * RayleighPhaseFunction(nu) + single_mie_scattering * MiePhaseFunction(nu);
}

in vec2 uv;
layout(location = 0) out vec3 ambient_diffuse;
layout(location = 1) out vec3 specular;

#define SUN_SHADOW_CASCADES 4
uniform sampler2D gbuf_position, gbuf_normal, gbuf_specular_shininess, gbuf_depth;
uniform sampler2DArray shadowmaps;
uniform float light_strength; /* 1 for sun, 0.138 for moon */
uniform vec2 fbdims;
uniform vec3 view_pos, skylight;
uniform float cascade_planes[SUN_SHADOW_CASCADES];
uniform mat4 lightspace[SUN_SHADOW_CASCADES];

uniform float gamma = 2.2, sky_exposure = 12.5;

const float BASE_LIGHT_STRENGTH = 1.8;

bool frag_in_shadow(vec3 frag_pos, float camera_depth)
{
	/* Use the depth of the fragment to determine which cascade to use. */
	int cascade = -1;
	for (int i = 0; i < SUN_SHADOW_CASCADES && cascade == -1; i++) {
		if (camera_depth < cascade_planes[i])
			cascade = i;
	}
	if (cascade == -1)
		return false;

	vec4 lt4 = lightspace[cascade] * vec4(frag_pos, 1.0);
	vec3 lt = (lt4.xyz / lt4.w) * 0.5 + 0.5;
	float light_depth = texture(shadowmaps, vec3(lt.xy, cascade)).r, frag_depth = lt.z;
	return light_depth < frag_depth && frag_depth <= 1.0;
}

void main() {
	vec2 uv = gl_FragCoord.xy / fbdims;
	vec3 frag_pos = texture(gbuf_position, uv).xyz, normal = texture(gbuf_normal, uv).xyz;
	float frag_depth = texture(gbuf_depth, uv).r, shininess = texture(gbuf_specular_shininess, uv).w;

	/* Directional light */
	vec3 view_dir = normalize(view_pos - frag_pos);
	vec3 light_dir = normalize(skylight);

	/* Physical sunlight parameters */
	vec3 sky_radiance_3 = pow(vec3(1.0) - exp(-GetSkyRadiance(vec3(0, 0, 1), light_dir) * sky_exposure), vec3(1.0 / gamma));
	float sky_radiance = light_strength * BASE_LIGHT_STRENGTH * max(0, dot(sky_radiance_3, vec3(0.2126, 0.7152, 0.0722)));

	float ambient = 0, diffuse = 0, spec = 0;
	if (!frag_in_shadow(frag_pos, frag_depth)) {
		diffuse = max(0, dot(normal, light_dir));
		spec = pow(max(dot(normal, normalize(light_dir + view_dir)), 0), shininess);
	}

	ambient_diffuse = vec3((ambient + diffuse) * sky_radiance);
	specular = vec3(spec * sky_radiance);
}
