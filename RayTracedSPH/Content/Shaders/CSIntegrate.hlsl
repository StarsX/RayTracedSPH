//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

#define SIMULATION_BLOCK_SIZE 64

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
RWStructuredBuffer<Particle> g_rwParticles : register (u0);
RWStructuredBuffer<ParticleAABB> g_rwParticleAABBs : register (u1);
Buffer<float3> g_roAccelerations : register (t0);

[numthreads(SIMULATION_BLOCK_SIZE, 1, 1)]
void main(uint DTid : SV_DispatchThreadID)
{
	Particle particle = g_rwParticles[DTid];
	float3 acceleration = g_roAccelerations[DTid];

	// Apply the forces from the map walls
	[unroll]
	for (unsigned int i = 0; i < 6; i++)
	{
		float dist = dot(float4(particle.Pos, 1), g_planes[i]);
		acceleration += min(dist, 0) * -g_wallStiffness * g_planes[i].xyz;
	}

	// Apply gravity
	acceleration += g_gravity;

	// Integrate
	particle.Velocity += g_timeStep * acceleration;
	particle.Pos += g_timeStep * particle.Velocity;

	// Update
	g_rwParticles[DTid] = particle;
	const float3 aabbExtent = { g_smoothRadius, g_smoothRadius, g_smoothRadius };
	g_rwParticleAABBs[DTid].Min = particle.Pos - aabbExtent;
	g_rwParticleAABBs[DTid].Max = particle.Pos + aabbExtent;
}