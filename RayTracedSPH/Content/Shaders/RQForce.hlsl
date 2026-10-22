//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RQCommon.hlsli"

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
[[vk::binding(0, 2)]]
RWBuffer<float3> g_rwAccelerations : register(u0);

[[vk::binding(1, 1)]]
Buffer<float> g_roDensities : register(t1);

//--------------------------------------------------------------------------------------
// Pressure calculation
//--------------------------------------------------------------------------------------
float CalculatePressure(float density)
{
	// Implements this equation:
	// Pressure = B * ((rho / rho_0)^y - 1)
	const float rhoRatio = density / g_restDensity;

	return g_pressureStiffness * max(rhoRatio * rhoRatio * rhoRatio - 1.0, 0.0);
}

//--------------------------------------------------------------------------------------
// Pressure gradient calculation
//--------------------------------------------------------------------------------------
float3 CalculateGradPressure(float r, float d, float pressure, float adjPressure, float adjDensity, float3 disp)
{
	const float avgPressure = 0.5 * (adjPressure + pressure);
	// Implements this equation:
	// W_spkiey(r, h) = 15 / (pi * h^6) * (h - r)^3
	// GRAD(W_spikey(r, h)) = -45 / (pi * h^6) * (h - r)^2
	// g_pressureGradCoef = particleMass * -45.0f / (PI * g_smoothRadius^6)

	return g_pressureGradCoef * avgPressure * d * d * disp / (adjDensity * r);
}

//--------------------------------------------------------------------------------------
// Velocity Laplacian calculation
//--------------------------------------------------------------------------------------
float3 CalculateVelocityLaplace(float d, float3 velocity, float3 adjVelocity, float adjDensity)
{
	float3 velDisp = (adjVelocity - velocity);
	// Implements this equation:
	// W_viscosity(r, h) = 15 / (2 * pi * h^3) * (-r^3 / (2 * h^3) + r^2 / h^2 + h / (2 * r) - 1)
	// LAPLACIAN(W_viscosity(r, h)) = 45 / (pi * h^6) * (h - r)
	// g_viscosityLaplaceCoef = particleMass * viscosity * 45.0f / (PI * g_smoothRadius^6)

	return g_viscosityLaplaceCoef * d * velDisp / adjDensity;
}

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

			if (r_sq < g_smoothRadius * g_smoothRadius && DTid != hitIndex)
			{
				const Particle hitParticle = g_roParticles[hitIndex];
				const float r = sqrt(r_sq);
				const float d = g_smoothRadius - r;
				const float hitDensity = g_roDensities[hitIndex];
				const float hitPressure = CalculatePressure(hitDensity);

				// Pressure term
				force += CalculateGradPressure(r, d, pressure, hitPressure, hitDensity, disp);

				// Viscosity term
				force += CalculateVelocityLaplace(d, velocity, hitParticle.Velocity.xyz, hitDensity);
			}
		}
	}

	g_rwAccelerations[DTid] = density > 0.0 ? force / density : 0.0;
}
