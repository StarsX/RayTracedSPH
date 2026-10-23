//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

// Shared by the ray-tracing pipeline and ray-query force shaders.
// Include after RTCommon.hlsli or RQCommon.hlsli.

[[vk::binding(0, 2)]]
RWBuffer<float3> g_rwAccelerations : register(u0);

[[vk::binding(1, 1)]]
Buffer<float> g_roDensities : register(t1);

float CalculatePressure(float density)
{
	// Implements this equation:
	// Pressure = B * ((rho / rho_0)^y - 1)
	const float rhoRatio = density / g_restDensity;

	return g_pressureStiffness * max(rhoRatio * rhoRatio * rhoRatio - 1.0, 0.0);
}

float3 CalculateGradPressure(float r, float d, float pressure, float adjPressure, float adjDensity, float3 disp)
{
	const float avgPressure = 0.5 * (adjPressure + pressure);
	// Implements this equation:
	// W_spkiey(r, h) = 15 / (pi * h^6) * (h - r)^3
	// GRAD(W_spikey(r, h)) = -45 / (pi * h^6) * (h - r)^2
	// g_pressureGradCoef = particleMass * -45.0f / (PI * g_smoothRadius^6)

	return g_pressureGradCoef * avgPressure * d * d * disp / (adjDensity * r);
}

float3 CalculateVelocityLaplace(float d, float3 velocity, float3 adjVelocity, float adjDensity)
{
	float3 velDisp = (adjVelocity - velocity);
	// Implements this equation:
	// W_viscosity(r, h) = 15 / (2 * pi * h^3) * (-r^3 / (2 * h^3) + r^2 / h^2 + h / (2 * r) - 1)
	// LAPLACIAN(W_viscosity(r, h)) = 45 / (pi * h^6) * (h - r)
	// g_viscosityLaplaceCoef = particleMass * viscosity * 45.0f / (PI * g_smoothRadius^6)

	return g_viscosityLaplaceCoef * d * velDisp / adjDensity;
}

bool IsForceNeighbor(float r_sq, uint index, uint hitIndex)
{
	return r_sq < g_smoothRadius * g_smoothRadius && index != hitIndex;
}

float3 AddNeighborForce(float3 force, float3 disp, float r_sq, float pressure, float3 velocity, uint hitIndex)
{
	const Particle hitParticle = g_roParticles[hitIndex];
	const float r = sqrt(r_sq);
	const float d = g_smoothRadius - r;
	const float hitDensity = g_roDensities[hitIndex];
	const float hitPressure = CalculatePressure(hitDensity);

	force += CalculateGradPressure(r, d, pressure, hitPressure, hitDensity, disp);
	force += CalculateVelocityLaplace(d, velocity, hitParticle.Velocity.xyz, hitDensity);

	return force;
}

float3 AccelerationFromForce(float3 force, float density)
{
	return density > 0.0 ? force / density : 0.0;
}
