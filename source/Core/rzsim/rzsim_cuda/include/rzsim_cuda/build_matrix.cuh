#pragma once

#include <vector>

#include "RHI/internal/cuda_extension.hpp"
#include "api.h"

RUZINO_NAMESPACE_OPEN_SCOPE

namespace rzsim_cuda {

// Triplet format: (row, col, value) for sparse Laplace matrix L = D - A
// - Diagonal L[i][i] = sum of weights of incident edges (unweighted: degree(i))
// - Off-diagonal L[i][j] = -weight(i,j) if (i,j) is an edge, else 0

// Unweighted version (all edge weights = 1.0)
RZSIM_CUDA_API
cuda::CUDALinearBufferHandle build_laplace_matrix(
    std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
        adjacency_map,
    cuda::CUDALinearBufferHandle vertices);

// Weighted version (edge weights from edge_weights buffer)
// edge_weights: array of floats, one weight per directed edge in adjacency list
RZSIM_CUDA_API
cuda::CUDALinearBufferHandle build_laplace_matrix_weighted(
    std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
        adjacency_map,
    cuda::CUDALinearBufferHandle vertices,
    cuda::CUDALinearBufferHandle edge_weights);

// Compute cotangent weights for triangle mesh (FEM Laplace operator)
// For each edge (i,j), computes: w_ij = (cot α + cot β) / 2
// where α, β are the opposite angles in the two adjacent triangles
//
// vertices: 3D positions [x0,y0,z0, x1,y1,z1, ...]
// face_indices: triangle vertex indices [v0,v1,v2, v0,v1,v2, ...]
// adjacency_map: edge topology from compute_adjacency_map_gpu
//
// Returns: edge weights in same order as adjacency_map
RZSIM_CUDA_API
cuda::CUDALinearBufferHandle compute_cotangent_weights(
    cuda::CUDALinearBufferHandle vertices,
    cuda::CUDALinearBufferHandle face_indices,
    std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
        adjacency_map);

}  // namespace rzsim_cuda

RUZINO_NAMESPACE_CLOSE_SCOPE
