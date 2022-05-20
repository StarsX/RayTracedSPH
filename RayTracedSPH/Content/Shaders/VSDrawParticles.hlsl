//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Common.hlsli"

//--------------------------------------------------------------------------------------
// Buffers
//--------------------------------------------------------------------------------------
StructuredBuffer<Particle> g_roParticles : register (t0);

cbuffer cbVisualization : register (b0)
{
	matrix g_viewProj;
}

float4 main(uint vid : SV_VERTEXID) : SV_POSITION
{
	const Particle particle = g_roParticles[vid];

	return mul(float4(particle.Pos, 1.0), g_viewProj);
}
