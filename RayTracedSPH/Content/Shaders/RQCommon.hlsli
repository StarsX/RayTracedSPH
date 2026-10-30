//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"
#include "SharedConst.h"

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
[[vk::binding(0, 1)]]
StructuredBuffer<Particle> g_roParticles : register(t0);
RaytracingAccelerationStructure g_bvhParticles : register(t0, SPACE(AS_SPACE));

//--------------------------------------------------------------------------------------
// Generate ray
//--------------------------------------------------------------------------------------
RayDesc GenerateRay(Particle particle)
{
	RayDesc ray;
	ray.Origin = particle.Pos.xyz;
	ray.Direction = float3(0.0.xx, 1.0);
	ray.TMin = 0.0;
#if POINT_QUERY
	// 0-length ray for point query
	ray.TMax = 0.0;
#else
	// z-oriented ray segment with g_smoothRadius length
	ray.Origin.z -= g_smoothRadius * 0.5;
	ray.TMax = g_smoothRadius;
#endif

	return ray;
}

//--------------------------------------------------------------------------------------
// Get THit 
//--------------------------------------------------------------------------------------
float GetTHit()
{
#if POINT_QUERY
	// 0-length ray for point query
	return 0.0;
#else
	// z-oriented ray segment with g_smoothRadius length
	return g_smoothRadius * 0.5;
#endif
}

//--------------------------------------------------------------------------------------
// Calculate displacement between 2 particles
//--------------------------------------------------------------------------------------
float3 CalculateParticleDisplacement(uint primitiveIndex, float3 rayOrigin, float thit)
{
	const Particle hitParticle = g_roParticles[primitiveIndex];

	float3 rayPos = rayOrigin;
	rayPos.z += thit;

	return hitParticle.Pos.xyz - rayPos;
}
