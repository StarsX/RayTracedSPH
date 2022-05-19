//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

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
cbuffer cbSimulation : register (b0)
{
	float	g_timeStep;
	float	g_smoothRadius;
	float	g_pressureStiffness;
	float	g_restDensity;
	float	g_densityCoef;
	float	g_pressureGradCoef;
	float	g_viscosityLaplaceCoef;
	float	g_wallStiffness;

	float4	g_gravity;
	float4	g_planes[6];
};

