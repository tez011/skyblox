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
uniform vec3 sun, moon;

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

/*****************************************************************************/

in vec3 point;
out vec4 color;

const float sun_factor = 0.06, moon_factor = 0.055, g = 0.999;
const vec3 sun_color = vec3(1, 1, 0.95), moon_color = vec3(0.9, 0.9, 0.75);

uniform samplerCube night_sky_texture;
uniform float gamma = 2.2, exposure = 12.5;

void main()
{
	float sun_blur = sun_factor * (1 - g * g) / (2 + g * g) / pow(1 + g * g - 2 * g * dot(normalize(point), sun), 1.5) *
			 smoothstep(-0.03, 0.03, point.z),
	      moon_blur = moon_factor * (1 - g * g) / (2 + g * g) / pow(1 + g * g - 2 * g * dot(normalize(point), moon), 1.5) *
			  smoothstep(-0.03, 0.03, point.z);

	vec3 hdr_sky_color = max(vec3(0), GetSkyRadiance(normalize(point), sun));
	vec3 sky_color = pow(vec3(1.0) - exp(-hdr_sky_color * exposure), vec3(1.0 / gamma));
	vec3 day_color = step(sun_blur, 1) * sky_color + clamp(sun_blur, 0, 1) * sun_color;
	vec3 night_color = step(moon_blur, 1) * texture(night_sky_texture, point).rgb + clamp(moon_blur, 0, 1) * moon_color;

	color = vec4(mix(night_color, day_color, clamp(length(day_color) * 2, 0, 1)), 1);
	gl_FragDepth = 1 - 1e-6;
}
