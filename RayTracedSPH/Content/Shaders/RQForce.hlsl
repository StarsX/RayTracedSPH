//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RQCommon.hlsli"
#include "Force.hlsli"

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
	if (DTid >= g_numParticles) return;

	const Particle particle = g_roParticles[DTid];
	const float density = g_roDensities[DTid];
	const RayDesc ray = GenerateRay(particle);
	const float pressure = CalculatePressure(density);
	const float3 velocity = particle.Velocity.xyz;

	// Trace the ray.
	RayQuery<RAY_FLAG_SKIP_CLOSEST_HIT_SHADER> query;
	query.TraceRayInline(g_bvhParticles, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, ray);

	float3 force = 0.0;
	while (query.Proceed())
	{
		if (query.CandidateType() == CANDIDATE_PROCEDURAL_PRIMITIVE)
		{
			const float thit = GetTHit();
			const uint hitIndex = query.CandidatePrimitiveIndex();
			const float3 disp = CalculateParticleDisplacement(hitIndex, query.CandidateObjectRayOrigin(), thit);
			const float r_sq = dot(disp, disp);

			if (IsForceNeighbor(r_sq, DTid, hitIndex))
				force = AddNeighborForce(force, disp, r_sq, pressure, velocity, hitIndex);
		}
	}

	g_rwAccelerations[DTid] = AccelerationFromForce(force, density);
}
