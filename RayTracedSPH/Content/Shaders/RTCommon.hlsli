//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"
#include "SharedConst.h"

typedef RaytracingAccelerationStructure RaytracingAS;

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
StructuredBuffer<Particle> g_roParticles : register (t0);
RaytracingAS g_bvhParticles : register (t0, space1);


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
// Calculate distance between 2 particles
//--------------------------------------------------------------------------------------

float3 CalculateParticleDisplacement(float thit)
{
	const Particle hitParticle = g_roParticles[PrimitiveIndex()];

	float3 rayPos = WorldRayOrigin();
	rayPos.z += thit;

	return hitParticle.Pos - rayPos;
}


float CalculateParticleDistanceSqr(float thit)
{
	const float3 disp = CalculateParticleDisplacement(thit);
	
	return dot(disp, disp);
}
