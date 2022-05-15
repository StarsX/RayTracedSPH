//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

typedef RaytracingAccelerationStructure RaytracingAS;

//--------------------------------------------------------------------------------------
// Constant
//--------------------------------------------------------------------------------------
static const float g_h_sq = g_smoothRadius * g_smoothRadius;

//--------------------------------------------------------------------------------------
// Buffer
//--------------------------------------------------------------------------------------
RaytracingAS g_bvhParticles : register(t0, space1);

//--------------------------------------------------------------------------------------
// Generate ray
//--------------------------------------------------------------------------------------
RayDesc GenerateRay(uint index)
{
	const Particle particle = g_roParticles[index];

	RayDesc ray;
	ray.Origin = particle.Pos;
	ray.Direction = float3(0.0.xx, 1.0);
	ray.TMin = 0.0;
#ifdef POINT_QUERY
	// 0-length ray for point query
	ray.TMax = 0.0;
#else
	// z-oriented ray segment with g_smoothRadius length
	ray.Origin.z -= g_smoothRadius * 0.5;
	ray.TMax = g_smoothRadius;
#endif

	return ray;
}
