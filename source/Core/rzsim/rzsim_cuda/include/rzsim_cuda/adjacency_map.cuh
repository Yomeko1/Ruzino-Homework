#pragma once

#include <vector>

#include "RHI/internal/cuda_extension.hpp"
#include "api.h"

RUZINO_NAMESPACE_OPEN_SCOPE

namespace rzsim_cuda {

// Surface mesh (triangles): For each vertex, stores pairs of opposite edge
// vertices For vertex v in triangle (v, a, b), stores pair (a, b) Format:
// [count_v0, a1,b1, a2,b2, ... | count_v1, ... ]
// - adjacency_list: flattened pairs of opposite vertices
// - offset_buffer: starting position for each vertex
RZSIM_CUDA_API
std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
compute_surface_adjacency_gpu(
    cuda::CUDALinearBufferHandle vertices,
    cuda::CUDALinearBufferHandle
        triangles);  // triangle indices [v0,v1,v2, ...]

// Volume mesh (tetrahedra): For each vertex, stores triplets of opposite face
// vertices For vertex v in tetrahedron (v, a, b, c), stores triplet (a, b, c)
// Format: [count_v0, a1,b1,c1, a2,b2,c2, ... | count_v1, ... ]
// - adjacency_list: flattened triplets of opposite face vertices
// - offset_buffer: starting position for each vertex
RZSIM_CUDA_API
std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
compute_volume_adjacency_gpu(
    cuda::CUDALinearBufferHandle vertices,
    cuda::CUDALinearBufferHandle tetrahedra);  // tet indices [v0,v1,v2,v3, ...]

}  // namespace rzsim_cuda

RUZINO_NAMESPACE_CLOSE_SCOPE
