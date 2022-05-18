//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#define POINT_QUERY

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
struct Particle
{
	float3 Pos;
	float3 Velocity;
};

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbSimulation : register(b0)
{
	uint	g_numParticles;
	float	g_smoothRadius;
	float	g_pressureStiffness;
	float	g_restDensity;
	float	g_densityCoef;
	float	g_pressureGradCoef;
	float	g_viscosityLaplaceCoef;
};

//--------------------------------------------------------------------------------------
// Buffer
//--------------------------------------------------------------------------------------
StructuredBuffer<Particle> g_roParticles : register(t0);
