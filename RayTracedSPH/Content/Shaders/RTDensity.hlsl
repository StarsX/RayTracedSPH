//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RTCommon.hlsli"
#include "Density.hlsli"

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

	if (IsDensityNeighbor(r_sq))
	{
		const HitAttributes attr = { r_sq };
		ReportHit(thit, /*hitKind*/ 0, attr);
	}
}

//--------------------------------------------------------------------------------------
// Ray any hit
//--------------------------------------------------------------------------------------
[shader("anyhit")]
void anyHitMain(inout RayPayload payload, HitAttributes attr)
{
	payload.Density = AddDensity(payload.Density, attr.R_sq);

	IgnoreHit();
}
