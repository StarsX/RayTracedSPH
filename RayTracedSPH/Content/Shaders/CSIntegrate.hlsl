//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"
#include "SharedConst.h"

//--------------------------------------------------------------------------------------
// Constant buffer
//--------------------------------------------------------------------------------------
[[vk::binding(1, 0)]]
cbuffer cbPerFrame : register(b1)
{
	float	g_timeStep;
	float3	g_gravity;
};

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
[[vk::binding(0, 2)]]
RWStructuredBuffer<Particle> g_rwParticles : register(u0);
[[vk::binding(1, 2)]]
RWStructuredBuffer<ParticleAABB> g_rwParticleAABBs : register(u1);

[[vk::binding(0, 1)]]
Buffer<float3> g_roAccelerations : register(t0);

[numthreads(GROUP_SIZE, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
	Particle particle = g_rwParticles[DTid];
	float3 acceleration = g_roAccelerations[DTid];

	// Apply the forces from the map walls
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		float dist = dot(float4(particle.Pos.xyz, 1.0), g_planes[i]);
		acceleration += min(dist, 0) * -g_wallStiffness * g_planes[i].xyz;
	}

	// Apply gravity
	acceleration += g_gravity;

	// Integrate
	particle.Velocity.xyz += g_timeStep * acceleration;
	particle.Pos.xyz += g_timeStep * particle.Velocity.xyz;

	// Update
	g_rwParticles[DTid] = particle;
	g_rwParticleAABBs[DTid] = MakeParticleAABB(particle.Pos.xyz, g_smoothRadius);
}
