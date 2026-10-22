//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RQCommon.hlsli"

//--------------------------------------------------------------------------------------
// Buffer
//--------------------------------------------------------------------------------------
[[vk::binding(0, 2)]]
RWBuffer<float> g_rwDensities : register(u0);

//--------------------------------------------------------------------------------------
// Density calculation
//--------------------------------------------------------------------------------------
float CalculateDensity(float r_sq)
{
	// Implements this equation:
	// W_poly6(r, h) = 315 / (64 * pi * h^9) * (h^2 - r^2)^3
	// g_densityCoef = particleMass * 315.0f / (64.0f * PI * g_smoothRadius^9)
	const float d_sq = g_smoothRadius * g_smoothRadius - r_sq;

	return g_densityCoef * d_sq * d_sq * d_sq;
}

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
	if (DTid >= g_numParticles) return;

	const Particle particle = g_roParticles[DTid];
	const RayDesc ray = GenerateRay(particle);

	// Trace the ray.
	RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
	query.TraceRayInline(g_bvhParticles, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, ray);

	float density = 0.0;
	while (query.Proceed())
	{
		if (query.CandidateType() == CANDIDATE_PROCEDURAL_PRIMITIVE)
		{
			const float thit = GetTHit();
			const float3 disp = CalculateParticleDisplacement(query.CandidatePrimitiveIndex(),
				query.CandidateObjectRayOrigin(), thit);
			const float r_sq = dot(disp, disp);

			if (r_sq < g_smoothRadius * g_smoothRadius)
				density += CalculateDensity(r_sq);
		}
	}

	g_rwDensities[DTid] = density;
}
