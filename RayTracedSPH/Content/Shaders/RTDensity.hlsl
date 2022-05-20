//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RTCommon.hlsli"

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct RayPayload
{
	float Density;
};

struct HitAttributes
{
	float R_sq;
};

//--------------------------------------------------------------------------------------
// Buffer
//--------------------------------------------------------------------------------------
RWBuffer<float> g_rwDensities : register (u0);

//--------------------------------------------------------------------------------------
// Ray generation
//--------------------------------------------------------------------------------------
[shader("raygeneration")]
void raygenMain()
{
	const uint index = DispatchRaysIndex().x;
	const Particle particle = g_roParticles[index];
	const RayDesc ray = GenerateRay(particle);

	// Trace the ray.
	RayPayload payload;
	payload.Density = 0.0;
	TraceRay(g_bvhParticles, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, payload);

	g_rwDensities[index] = payload.Density;
}

//--------------------------------------------------------------------------------------
// Ray intersection
//--------------------------------------------------------------------------------------
[shader("intersection")]
void intersectionMain()
{
	const float thit = GetTHit();
	const float3 disp = CalculateParticleDisplacement(thit);
	const float r_sq = dot(disp, disp);

	if (r_sq < g_smoothRadius * g_smoothRadius)
	{
		const HitAttributes attr = { r_sq };
		ReportHit(thit, /*hitKind*/ 0, attr);
	}
}

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

//--------------------------------------------------------------------------------------
// Ray any hit
//--------------------------------------------------------------------------------------
[shader("anyhit")]
void anyHitMain(inout RayPayload payload, HitAttributes attr)
{
	payload.Density += CalculateDensity(attr.R_sq);

	IgnoreHit();
}

//--------------------------------------------------------------------------------------
// Ray miss
//--------------------------------------------------------------------------------------
[shader("miss")]
void missMain(inout RayPayload payload)
{
}
