//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

// Shared by the ray-tracing pipeline and ray-query density shaders.
// Include after RTCommon.hlsli or RQCommon.hlsli.

[[vk::binding(0, 2)]]
RWBuffer<float> g_rwDensities : register(u0);

float CalculateDensity(float r_sq)
{
	// Implements this equation:
	// W_poly6(r, h) = 315 / (64 * pi * h^9) * (h^2 - r^2)^3
	// g_densityCoef = particleMass * 315.0f / (64.0f * PI * g_smoothRadius^9)
	const float d_sq = g_smoothRadius * g_smoothRadius - r_sq;

	return g_densityCoef * d_sq * d_sq * d_sq;
}

bool IsDensityNeighbor(float r_sq)
{
	return r_sq < g_smoothRadius * g_smoothRadius;
}

float AddDensity(float density, float r_sq)
{
	return density + CalculateDensity(r_sq);
}
