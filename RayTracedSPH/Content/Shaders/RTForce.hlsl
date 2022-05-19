//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RTCommon.hlsli"

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct RayPayload
{
	float3 Force;
};

struct HitAttributes
{
	float R_sq;
	float AdjDensity;
};

//--------------------------------------------------------------------------------------
// Buffer
//--------------------------------------------------------------------------------------
RWBuffer<float3> g_rwForces : register (u0);
Buffer<float> g_roDensities : register (t1);

//--------------------------------------------------------------------------------------
// Ray generation
//--------------------------------------------------------------------------------------
[shader("raygeneration")]
void raygenMain()
{
	const uint index = DispatchRaysIndex().x;
	const RayDesc ray = GenerateRay(index);
	const float density = g_roDensities[index];

	// Trace the ray.
	RayPayload payload;
	payload.Force = 0.0;
	TraceRay(g_bvhParticles, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 0, ray, payload);

	g_rwForces[index] = payload.Force / density;
}

//--------------------------------------------------------------------------------------
// Ray intersection
//--------------------------------------------------------------------------------------
[shader("intersection")]
void intersectionMain()
{
	const float thit = GetTHit();
	const float r_sq = CalculateParticleDistanceSqr(thit);
	const uint hitId = PrimitiveIndex();
	const uint selfId = DispatchRaysIndex().x;

	if (r_sq < g_smoothRadius * g_smoothRadius 
		&& selfId != hitId)
	{
		const HitAttributes attr = { r_sq, g_roDensities[hitId] };
		ReportHit(thit, /*hitKind*/ 0, attr);
	}
}

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

//--------------------------------------------------------------------------------------
// Ray any hit
//--------------------------------------------------------------------------------------
[shader("anyhit")]
void anyHitMain(inout RayPayload payload, HitAttributes attr)
{
	const uint index = DispatchRaysIndex().x;
	const float density = g_roDensities[index];
	const Particle particle = g_roParticles[index];

	const float r = sqrt(attr.R_sq);
	const float d = g_smoothRadius - r;
	const float pressure = CalculatePressure(density);
	const float adjPressure = CalculatePressure(attr.AdjDensity);

	const Particle adjParticle = g_roParticles[PrimitiveIndex()];
	float3 hitPos = WorldRayOrigin();
	hitPos.z += GetTHit();
	const float3 disp = adjParticle.Pos - hitPos;

	// Pressure term
	payload.Force += CalculateGradPressure(r, d, pressure, adjPressure, attr.AdjDensity, disp);

	// Viscosity term
	payload.Force += CalculateVelocityLaplace(d, particle.Velocity, adjParticle.Velocity, attr.AdjDensity);

	IgnoreHit();
}

//--------------------------------------------------------------------------------------
// Ray miss
//--------------------------------------------------------------------------------------
[shader("miss")]
void missMain(inout RayPayload payload)
{
}
