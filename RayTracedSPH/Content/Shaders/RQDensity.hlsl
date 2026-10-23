//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RQCommon.hlsli"
#include "Density.hlsli"

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

			if (IsDensityNeighbor(r_sq)) density = AddDensity(density, r_sq);
		}
	}

	g_rwDensities[DTid] = density;
}
