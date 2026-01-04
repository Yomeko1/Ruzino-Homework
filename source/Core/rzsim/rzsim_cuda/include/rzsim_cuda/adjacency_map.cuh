#pragma once

#include <vector>

#include "RHI/internal/cuda_extension.hpp"
#include "api.h"

RUZINO_NAMESPACE_OPEN_SCOPE

namespace rzsim_cuda {

// Pure CUDA implementation that doesn't depend on geometry library
// Returns two buffers:
// 1. adjacency_list: [count_v0, neighbor1, neighbor2, ... | count_v1, neighbor1, neighbor2, ... | ...]
// 2. offset_buffer: offset_buffer[vertex_id] = starting position of vertex_id in adjacency_list
//    (enables random access: vertex_id's neighbors are at adjacency_list[offset_buffer[vertex_id]+1 ... +count])
RZSIM_CUDA_API
    std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
    compute_adjacency_map_gpu(
        cuda::CUDALinearBufferHandle vertices,
        cuda::CUDALinearBufferHandle faceVertexCounts,
        cuda::CUDALinearBufferHandle faceVertexIndices);

}  // namespace rzsim_cuda

RUZINO_NAMESPACE_CLOSE_SCOPE
