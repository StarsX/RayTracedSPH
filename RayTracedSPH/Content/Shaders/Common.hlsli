//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------
// float4 keeps DXIL and default SPIR-V on the same 32-byte layout.
struct Particle
{
	float4 Pos;
	float4 Velocity;
};

// Min/Max stay six packed floats for the BLAS AABB. The tail pad rounds the
// element to 32 bytes, which both layouts agree on.
struct ParticleAABB
{
	float MinX, MinY, MinZ;
	float MaxX, MaxY, MaxZ;
	float2 _pad;
};

ParticleAABB MakeParticleAABB(float3 pos, float radius)
{
	ParticleAABB aabb;
	const float3 bmin = pos - radius;
	const float3 bmax = pos + radius;
	aabb.MinX = bmin.x;
	aabb.MinY = bmin.y;
	aabb.MinZ = bmin.z;
	aabb.MaxX = bmax.x;
	aabb.MaxY = bmax.y;
	aabb.MaxZ = bmax.z;
	aabb._pad = 0;

	return aabb;
}

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
[[vk::binding(0, 0)]]
cbuffer cbSimulation : register(b0)
{
	float	g_smoothRadius;
	float	g_pressureStiffness;
	float	g_restDensity;
	float	g_densityCoef;
	float	g_pressureGradCoef;
	float	g_viscosityLaplaceCoef;
	float	g_wallStiffness;
	uint	g_numParticles;

	float4	g_planes[6];
};
