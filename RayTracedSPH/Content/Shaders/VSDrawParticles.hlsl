#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
StructuredBuffer<Particle> g_roParticles : register (t0);

cbuffer cbPerFrame : register (b1)
{
	matrix viewProj;
}

float4 main(uint vid : SV_VERTEXID) : SV_POSITION
{
	const Particle particle = g_roParticles[vid];
	return mul(float4(particle.Pos, 1.0), viewProj);
}
