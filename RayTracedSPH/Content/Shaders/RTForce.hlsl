//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RTCommon.hlsli"
#include "Force.hlsli"

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct RayPayload
{
	float Pressure;
	float3 Force;
	float3 Velocity;
};

struct HitAttributes
{
	float3 Disp;
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
	const float density = g_roDensities[index];
	const RayDesc ray = GenerateRay(particle);

	// Trace the ray.
	RayPayload payload;
	payload.Pressure = CalculatePressure(density);
	payload.Velocity = particle.Velocity.xyz;
	payload.Force = 0.0;
	TraceRay(g_bvhParticles, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, payload);

	g_rwAccelerations[index] = AccelerationFromForce(payload.Force, density);
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
	const uint hitIndex = PrimitiveIndex();
	const uint index = DispatchRaysIndex().x;

	if (IsForceNeighbor(r_sq, index, hitIndex))
	{
		const HitAttributes attr = { disp, r_sq };
		ReportHit(thit, /*hitKind*/ 0, attr);
	}
}

//--------------------------------------------------------------------------------------
// Ray any hit
//--------------------------------------------------------------------------------------
[shader("anyhit")]
void anyHitMain(inout RayPayload payload, HitAttributes attr)
{
	payload.Force = AddNeighborForce(payload.Force, attr.Disp, attr.R_sq,
		payload.Pressure, payload.Velocity, PrimitiveIndex());

	IgnoreHit();
}
