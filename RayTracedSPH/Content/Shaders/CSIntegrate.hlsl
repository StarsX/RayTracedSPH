//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

#define SIMULATION_BLOCK_SIZE 256

//--------------------------------------------------------------------------------------
// Buffer
//--------------------------------------------------------------------------------------
RWStructuredBuffer<Particle> g_rwParticles : register (u0);
Buffer<float3> g_roAccelerations : register (t0);

[numthreads(SIMULATION_BLOCK_SIZE, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint PID = DTid.x; // Particle ID to operate on

    float3 pos = g_rwParticles[PID].Pos;
    float3 velocity = g_rwParticles[PID].Velocity;
    float3 acceleration = g_roAccelerations[PID];

    // Apply the forces from the map walls
    [unroll]
    for (unsigned int i = 0; i < 6; i++)
    {
        float dist = dot(float4(pos, 1), g_planes[i]);
        acceleration += min(dist, 0) * -g_wallStiffness * g_planes[i].xyz;
    }

    // Apply gravity
    acceleration += g_gravity.xyx;

    // Integrate
    velocity += g_timeStep * acceleration;
    pos += g_timeStep * velocity;

    // Update
    g_rwParticles[PID].Pos = pos;
    g_rwParticles[PID].Velocity = velocity;
}