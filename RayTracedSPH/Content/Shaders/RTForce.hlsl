//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "RTCommon.hlsli"

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
// Buffer
//--------------------------------------------------------------------------------------
RWBuffer<float3> g_rwForces : register (u0);
Buffer<float> g_roDensities : register (t1);

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
	payload.Pressure = CalculatePressure(density);
	payload.Velocity = g_roParticles[index].Velocity;
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
	const float3 disp = CalculateParticleDisplacement(thit);
	const float r_sq = dot(disp, disp);
	const uint hitIndex = PrimitiveIndex();
	const uint index = DispatchRaysIndex().x;

	if (r_sq < g_smoothRadius * g_smoothRadius 
		&& index != hitIndex)
	{
		const HitAttributes attr = { disp, r_sq };
		ReportHit(thit, /*hitKind*/ 0, attr);
	}
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
	const uint hitIndex = PrimitiveIndex();
	const Particle hitParticle = g_roParticles[hitIndex];

	const float r = sqrt(attr.R_sq);
	const float d = g_smoothRadius - r;
	const float hitDensity = g_roDensities[hitIndex];
	const float hitPressure = CalculatePressure(hitDensity);

	// Pressure term
	payload.Force += CalculateGradPressure(r, d, payload.Pressure, hitPressure, hitDensity, attr.Disp);

	// Viscosity term
	payload.Force += CalculateVelocityLaplace(d, payload.Velocity, hitParticle.Velocity, hitDensity);

	IgnoreHit();
}

//--------------------------------------------------------------------------------------
// Ray miss
//--------------------------------------------------------------------------------------
[shader("miss")]
void missMain(inout RayPayload payload)
{
}
