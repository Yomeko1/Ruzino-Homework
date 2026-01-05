#include <thrust/device_vector.h>
#include <thrust/execution_policy.h>
#include <thrust/for_each.h>
#include <thrust/scan.h>

#include <RHI/cuda.hpp>
#include <RHI/rhi.hpp>

#include "rzsim_cuda/adjacency_map.cuh"

RUZINO_NAMESPACE_OPEN_SCOPE

namespace rzsim_cuda {

// ============================================================================
// Surface Mesh (Triangles): Store opposite edge pairs for each vertex
// ============================================================================

// Count how many triangles each vertex belongs to
__global__ void count_vertex_triangles_kernel(
    const unsigned* triangles,
    unsigned num_triangles,
    unsigned* triangle_counts)
{
    unsigned tri_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (tri_idx >= num_triangles)
        return;
    
    unsigned v0 = triangles[tri_idx * 3 + 0];
    unsigned v1 = triangles[tri_idx * 3 + 1];
    unsigned v2 = triangles[tri_idx * 3 + 2];
    
    atomicAdd(&triangle_counts[v0], 1);
    atomicAdd(&triangle_counts[v1], 1);
    atomicAdd(&triangle_counts[v2], 1);
}

// Fill opposite edge pairs for each vertex
// For vertex v in triangle (v, a, b), store pair (a, b) with consistent orientation
__global__ void fill_surface_adjacency_kernel(
    const unsigned* triangles,
    unsigned num_triangles,
    const unsigned* offsets,
    unsigned* write_positions,
    unsigned* adjacency_list)
{
    unsigned tri_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (tri_idx >= num_triangles)
        return;
    
    unsigned v0 = triangles[tri_idx * 3 + 0];
    unsigned v1 = triangles[tri_idx * 3 + 1];
    unsigned v2 = triangles[tri_idx * 3 + 2];
    
    // For v0: opposite edge is (v1, v2)
    unsigned pos0 = atomicAdd(&write_positions[v0], 2);
    adjacency_list[offsets[v0] + 1 + pos0 + 0] = v1;
    adjacency_list[offsets[v0] + 1 + pos0 + 1] = v2;
    
    // For v1: opposite edge is (v2, v0) - maintaining CCW order
    unsigned pos1 = atomicAdd(&write_positions[v1], 2);
    adjacency_list[offsets[v1] + 1 + pos1 + 0] = v2;
    adjacency_list[offsets[v1] + 1 + pos1 + 1] = v0;
    
    // For v2: opposite edge is (v0, v1)
    unsigned pos2 = atomicAdd(&write_positions[v2], 2);
    adjacency_list[offsets[v2] + 1 + pos2 + 0] = v0;
    adjacency_list[offsets[v2] + 1 + pos2 + 1] = v1;
}

std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
compute_surface_adjacency_gpu(
    cuda::CUDALinearBufferHandle vertices,
    cuda::CUDALinearBufferHandle triangles)
{
    unsigned num_vertices = vertices->getDesc().element_count;  // glm::vec3 count
    unsigned num_triangles = triangles->getDesc().element_count / 3;  // 3 indices per triangle
    
    auto triangle_ptr = (unsigned*)triangles->get_device_ptr();
    
    // Step 1: Count triangles per vertex
    thrust::device_vector<unsigned> triangle_counts(num_vertices, 0);
    auto counts_ptr = thrust::raw_pointer_cast(triangle_counts.data());
    
    int threads = 256;
    int blocks = (num_triangles + threads - 1) / threads;
    
    count_vertex_triangles_kernel<<<blocks, threads>>>(
        triangle_ptr, num_triangles, counts_ptr);
    cudaDeviceSynchronize();
    
    // Step 2: Build offset buffer
    // Each vertex stores: [count, pair1_a, pair1_b, pair2_a, pair2_b, ...]
    thrust::device_vector<unsigned> offsets(num_vertices);
    thrust::device_vector<unsigned> adjacency_sizes(num_vertices);
    
    // Each triangle contributes 2 values (one edge pair) per vertex
    thrust::transform(
        thrust::device,
        triangle_counts.begin(),
        triangle_counts.end(),
        adjacency_sizes.begin(),
        [] __device__ (unsigned count) { return 1 + count * 2; });  // +1 for count field
    
    thrust::exclusive_scan(
        thrust::device,
        adjacency_sizes.begin(),
        adjacency_sizes.end(),
        offsets.begin());
    
    unsigned total_size = thrust::reduce(
        thrust::device,
        adjacency_sizes.begin(),
        adjacency_sizes.end());
    
    // Step 3: Allocate and fill adjacency list
    cuda::CUDALinearBufferDesc adj_desc;
    adj_desc.element_count = total_size;
    adj_desc.element_size = sizeof(unsigned);
    auto adjacency_buffer = cuda::create_cuda_linear_buffer(adj_desc);
    auto adj_ptr = (unsigned*)adjacency_buffer->get_device_ptr();
    
    // Initialize count fields
    auto offsets_ptr = thrust::raw_pointer_cast(offsets.data());
    thrust::for_each(
        thrust::device,
        thrust::counting_iterator<unsigned>(0),
        thrust::counting_iterator<unsigned>(num_vertices),
        [adj_ptr, offsets_ptr, counts_ptr] __device__ (unsigned v) {
            adj_ptr[offsets_ptr[v]] = counts_ptr[v];
        });
    
    // Track write positions for each vertex
    thrust::device_vector<unsigned> write_positions(num_vertices, 0);
    auto write_pos_ptr = thrust::raw_pointer_cast(write_positions.data());
    
    fill_surface_adjacency_kernel<<<blocks, threads>>>(
        triangle_ptr,
        num_triangles,
        offsets_ptr,
        write_pos_ptr,
        adj_ptr);
    cudaDeviceSynchronize();
    
    // Step 4: Create offset buffer for output
    cuda::CUDALinearBufferDesc offset_desc;
    offset_desc.element_count = num_vertices;
    offset_desc.element_size = sizeof(unsigned);
    auto offset_buffer = cuda::create_cuda_linear_buffer(offset_desc);
    
    cudaMemcpy(
        (void*)offset_buffer->get_device_ptr(),
        offsets_ptr,
        num_vertices * sizeof(unsigned),
        cudaMemcpyDeviceToDevice);
    
    return std::make_tuple(adjacency_buffer, offset_buffer);
}

// ============================================================================
// Volume Mesh (Tetrahedra): Store opposite face triplets for each vertex
// ============================================================================

// Count how many tetrahedra each vertex belongs to
__global__ void count_vertex_tets_kernel(
    const unsigned* tets,
    unsigned num_tets,
    unsigned* tet_counts)
{
    unsigned tet_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (tet_idx >= num_tets)
        return;
    
    unsigned v0 = tets[tet_idx * 4 + 0];
    unsigned v1 = tets[tet_idx * 4 + 1];
    unsigned v2 = tets[tet_idx * 4 + 2];
    unsigned v3 = tets[tet_idx * 4 + 3];
    
    atomicAdd(&tet_counts[v0], 1);
    atomicAdd(&tet_counts[v1], 1);
    atomicAdd(&tet_counts[v2], 1);
    atomicAdd(&tet_counts[v3], 1);
}

// Fill opposite face triplets for each vertex
// For vertex v in tet (v, a, b, c), store triplet (a, b, c) with consistent orientation
__global__ void fill_volume_adjacency_kernel(
    const unsigned* tets,
    unsigned num_tets,
    const unsigned* offsets,
    unsigned* write_positions,
    unsigned* adjacency_list)
{
    unsigned tet_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (tet_idx >= num_tets)
        return;
    
    unsigned v0 = tets[tet_idx * 4 + 0];
    unsigned v1 = tets[tet_idx * 4 + 1];
    unsigned v2 = tets[tet_idx * 4 + 2];
    unsigned v3 = tets[tet_idx * 4 + 3];
    
    // For v0: opposite face is (v1, v2, v3) with outward normal orientation
    unsigned pos0 = atomicAdd(&write_positions[v0], 3);
    adjacency_list[offsets[v0] + 1 + pos0 + 0] = v1;
    adjacency_list[offsets[v0] + 1 + pos0 + 1] = v2;
    adjacency_list[offsets[v0] + 1 + pos0 + 2] = v3;
    
    // For v1: opposite face is (v0, v3, v2) - CCW when viewed from v1
    unsigned pos1 = atomicAdd(&write_positions[v1], 3);
    adjacency_list[offsets[v1] + 1 + pos1 + 0] = v0;
    adjacency_list[offsets[v1] + 1 + pos1 + 1] = v3;
    adjacency_list[offsets[v1] + 1 + pos1 + 2] = v2;
    
    // For v2: opposite face is (v0, v1, v3)
    unsigned pos2 = atomicAdd(&write_positions[v2], 3);
    adjacency_list[offsets[v2] + 1 + pos2 + 0] = v0;
    adjacency_list[offsets[v2] + 1 + pos2 + 1] = v1;
    adjacency_list[offsets[v2] + 1 + pos2 + 2] = v3;
    
    // For v3: opposite face is (v0, v2, v1)
    unsigned pos3 = atomicAdd(&write_positions[v3], 3);
    adjacency_list[offsets[v3] + 1 + pos3 + 0] = v0;
    adjacency_list[offsets[v3] + 1 + pos3 + 1] = v2;
    adjacency_list[offsets[v3] + 1 + pos3 + 2] = v1;
}

std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
compute_volume_adjacency_gpu(
    cuda::CUDALinearBufferHandle vertices,
    cuda::CUDALinearBufferHandle tetrahedra)
{
    unsigned num_vertices = vertices->getDesc().element_count;  // glm::vec3 count
    unsigned num_tets = tetrahedra->getDesc().element_count / 4;  // 4 indices per tet
    
    auto tet_ptr = (unsigned*)tetrahedra->get_device_ptr();
    
    // Step 1: Count tets per vertex
    thrust::device_vector<unsigned> tet_counts(num_vertices, 0);
    auto counts_ptr = thrust::raw_pointer_cast(tet_counts.data());
    
    int threads = 256;
    int blocks = (num_tets + threads - 1) / threads;
    
    count_vertex_tets_kernel<<<blocks, threads>>>(
        tet_ptr, num_tets, counts_ptr);
    cudaDeviceSynchronize();
    
    // Step 2: Build offset buffer
    // Each vertex stores: [count, triplet1_a, triplet1_b, triplet1_c, ...]
    thrust::device_vector<unsigned> offsets(num_vertices);
    thrust::device_vector<unsigned> adjacency_sizes(num_vertices);
    
    // Each tet contributes 3 values (one face triplet) per vertex
    thrust::transform(
        thrust::device,
        tet_counts.begin(),
        tet_counts.end(),
        adjacency_sizes.begin(),
        [] __device__ (unsigned count) { return 1 + count * 3; });  // +1 for count field
    
    thrust::exclusive_scan(
        thrust::device,
        adjacency_sizes.begin(),
        adjacency_sizes.end(),
        offsets.begin());
    
    unsigned total_size = thrust::reduce(
        thrust::device,
        adjacency_sizes.begin(),
        adjacency_sizes.end());
    
    // Step 3: Allocate and fill adjacency list
    cuda::CUDALinearBufferDesc adj_desc;
    adj_desc.element_count = total_size;
    adj_desc.element_size = sizeof(unsigned);
    auto adjacency_buffer = cuda::create_cuda_linear_buffer(adj_desc);
    auto adj_ptr = (unsigned*)adjacency_buffer->get_device_ptr();
    
    // Initialize count fields
    auto offsets_ptr = thrust::raw_pointer_cast(offsets.data());
    thrust::for_each(
        thrust::device,
        thrust::counting_iterator<unsigned>(0),
        thrust::counting_iterator<unsigned>(num_vertices),
        [adj_ptr, offsets_ptr, counts_ptr] __device__ (unsigned v) {
            adj_ptr[offsets_ptr[v]] = counts_ptr[v];
        });
    
    // Track write positions for each vertex
    thrust::device_vector<unsigned> write_positions(num_vertices, 0);
    auto write_pos_ptr = thrust::raw_pointer_cast(write_positions.data());
    
    fill_volume_adjacency_kernel<<<blocks, threads>>>(
        tet_ptr,
        num_tets,
        offsets_ptr,
        write_pos_ptr,
        adj_ptr);
    cudaDeviceSynchronize();
    
    // Step 4: Create offset buffer for output
    cuda::CUDALinearBufferDesc offset_desc;
    offset_desc.element_count = num_vertices;
    offset_desc.element_size = sizeof(unsigned);
    auto offset_buffer = cuda::create_cuda_linear_buffer(offset_desc);
    
    cudaMemcpy(
        (void*)offset_buffer->get_device_ptr(),
        offsets_ptr,
        num_vertices * sizeof(unsigned),
        cudaMemcpyDeviceToDevice);
    
    return std::make_tuple(adjacency_buffer, offset_buffer);
}

}  // namespace rzsim_cuda

RUZINO_NAMESPACE_CLOSE_SCOPE
