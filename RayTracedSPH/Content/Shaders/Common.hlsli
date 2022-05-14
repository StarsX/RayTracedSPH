//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define POINT_QUERY

typedef RaytracingAccelerationStructure RaytracingAS;

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct Particle
{
	float3 Pos;
	float3 Velocity;
};

struct Simulation
{
	uint	NumParticles;
	float	SmoothRadius;
	float	PressureStiffness;
	float	RestDensity;
	float	DensityCoef;
	float	PressureGradCoef;
	float	ViscosityLaplaceCoef;
};

//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------
ConstantBuffer<Simulation> g_cbSim : register(b0);

//--------------------------------------------------------------------------------------
// Constant
//--------------------------------------------------------------------------------------
static const float g_h_sq = g_cbSim.SmoothRadius * g_cbSim.SmoothRadius;

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
StructuredBuffer<Particle>	g_roParticles	: register(t0, space0);
RaytracingAS				g_bvhParticles : register(t0, space1);

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
	ray.Origin.z -= g_cbSim.SmoothRadius * 0.5;
	ray.TMax = g_cbSim.SmoothRadius;
#endif

	return ray;
}
