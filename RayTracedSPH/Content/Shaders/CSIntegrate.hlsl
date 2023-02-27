//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"
#include "SharedConst.h"

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
cbuffer cbPerFrame : register (b1)
{
	float	g_timeStep;
	float3	g_gravity;
};

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
RWStructuredBuffer<Particle> g_rwParticles : register (u0);
RWStructuredBuffer<ParticleAABB> g_rwParticleAABBs : register (u1);
Buffer<float3> g_roAccelerations : register (t0);

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
	Particle particle = g_rwParticles[DTid];
	float3 acceleration = g_roAccelerations[DTid];

	// Apply the forces from the map walls
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		float dist = dot(float4(particle.Pos, 1.0), g_planes[i]);
		acceleration += min(dist, 0) * -g_wallStiffness * g_planes[i].xyz;
	}

	// Apply gravity
	acceleration += g_gravity;

	// Integrate
	particle.Velocity += g_timeStep * acceleration;
	particle.Pos += g_timeStep * particle.Velocity;

	ParticleAABB aabb;
	aabb.Min = particle.Pos - g_smoothRadius;
	aabb.Max = particle.Pos + g_smoothRadius;

	// Update
	g_rwParticles[DTid] = particle;
	g_rwParticleAABBs[DTid] = aabb;
}
