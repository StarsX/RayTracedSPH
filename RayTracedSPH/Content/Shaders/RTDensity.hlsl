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
	const RayDesc ray = GenerateRay(index);

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
	float3 pointPos = WorldRayOrigin();
#if !POINT_QUERY
	// z-oriented ray segment with g_smoothRadius length
	pointPos.z += g_smoothRadius * 0.5;
#endif

	const Particle particle = g_roParticles[PrimitiveIndex()];
	const float3 disp = particle.Pos - pointPos;
	const float r_sq = dot(disp, disp);
	if (r_sq < g_h_sq)
	{
#if POINT_QUERY
		const float thit = 0.0;
#else
		const float thit = g_smoothRadius * 0.5;
#endif
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
	// Implements this equation:
	// W_poly6(r, h) = 315 / (64 * pi * h^9) * (h^2 - r^2)^3
	// g_densityCoef = particleMass * 315.0f / (64.0f * PI * g_smoothRadius^9)
	const float d_sq = g_h_sq - attr.R_sq;

	payload.Density += g_densityCoef * d_sq * d_sq * d_sq;

	IgnoreHit();
}

//--------------------------------------------------------------------------------------
// Ray miss
//--------------------------------------------------------------------------------------
[shader("miss")]
void missMain(inout RayPayload payload)
{
}
