#pragma once

#include <RHI/cuda.hpp>
#include <memory>
#include <vector>

#include "api.h"

RUZINO_NAMESPACE_OPEN_SCOPE

// Forward declarations
class Geometry;

// Surface mesh adjacency (triangles)
// For each vertex, stores pairs of opposite edge vertices
// Format: [count, a1,b1, a2,b2, ...]
RZSIM_API std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
get_surface_adjacency_gpu(const Geometry& g);

RZSIM_API std::tuple<std::vector<unsigned>, std::vector<unsigned>>
get_surface_adjacency(const Geometry& g);

// Volume mesh adjacency (tetrahedra)
// For each vertex, stores triplets of opposite face vertices
// Format: [count, a1,b1,c1, a2,b2,c2, ...]
// Returns: (adjacency_buffer, offset_buffer, num_tetrahedra)
RZSIM_API std::
    tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle, unsigned>
    get_volume_adjacency_gpu(const Geometry& g);

RZSIM_API std::tuple<std::vector<unsigned>, std::vector<unsigned>, unsigned>
get_volume_adjacency(const Geometry& g);

RUZINO_NAMESPACE_CLOSE_SCOPE
