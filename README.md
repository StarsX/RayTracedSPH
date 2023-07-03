# RayTracedSPH
Real-time fluid simulation using smoothed particle hydrodynamics (SPH) by taking advantage of GPU hardware ray tracing for particle neighbor search. A Zero-length ray represents a particle point and a particle AABB represent the impact range determined by the position and smoothing radius. Procerual mode of GPU ray tracing pipeline with any hit shader is leveraged to implement the neighbor search algorithm based on the sparse grid scheme of BVH. 

DXR fallback layer was expected to be supported for the legacy generations of GPUs, but it seems IgnoreHit() has some bug in the any hit shader on DXR fallback layer unfortunately.

Hot keys:

[F1] show/hide FPS

[Space] pause/play animation

Prerequisite: https://github.com/StarsX/XUSG
