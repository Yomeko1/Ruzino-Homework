#include <thrust/device_vector.h>
#include <thrust/distance.h>
#include <thrust/execution_policy.h>
#include <thrust/for_each.h>
#include <thrust/scan.h>
#include <thrust/sort.h>
#include <thrust/unique.h>

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
// For vertex v in triangle (v, a, b), store pair (a, b) with consistent
// orientation
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
    unsigned num_vertices =
        vertices->getDesc().element_count;  // glm::vec3 count
    unsigned num_triangles =
        triangles->getDesc().element_count / 3;  // 3 indices per triangle

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
        [] __device__(unsigned count) {
            return 1 + count * 2;
        });  // +1 for count field

    thrust::exclusive_scan(
        thrust::device,
        adjacency_sizes.begin(),
        adjacency_sizes.end(),
        offsets.begin());

    unsigned total_size = thrust::reduce(
        thrust::device, adjacency_sizes.begin(), adjacency_sizes.end());

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
        [adj_ptr, offsets_ptr, counts_ptr] __device__(unsigned v) {
            adj_ptr[offsets_ptr[v]] = counts_ptr[v];
        });

    // Track write positions for each vertex
    thrust::device_vector<unsigned> write_positions(num_vertices, 0);
    auto write_pos_ptr = thrust::raw_pointer_cast(write_positions.data());

    fill_surface_adjacency_kernel<<<blocks, threads>>>(
        triangle_ptr, num_triangles, offsets_ptr, write_pos_ptr, adj_ptr);
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
// Input: triangular faces, reconstruct tetrahedral topology
// ============================================================================

// Edge represented as sorted pair (min, max)
struct Edge {
    unsigned v0, v1;

    __host__ __device__ Edge() : v0(0), v1(0)
    {
    }

    __host__ __device__ Edge(unsigned a, unsigned b)
    {
        v0 = min(a, b);
        v1 = max(a, b);
    }

    __host__ __device__ bool operator<(const Edge& other) const
    {
        if (v0 != other.v0)
            return v0 < other.v0;
        return v1 < other.v1;
    }

    __host__ __device__ bool operator==(const Edge& other) const
    {
        return v0 == other.v0 && v1 == other.v1;
    }
};

// Device function to check if a vertex is in a triangle
__device__ bool
contains_vertex(unsigned v0, unsigned v1, unsigned v2, unsigned v)
{
    return (v == v0) || (v == v1) || (v == v2);
}

// Binary search for edge in sorted edge list
__device__ bool
has_edge_fast(const Edge* edges, unsigned num_edges, unsigned v1, unsigned v2)
{
    Edge target(v1, v2);

    unsigned left = 0;
    unsigned right = num_edges;

    while (left < right) {
        unsigned mid = (left + right) / 2;
        if (edges[mid] < target) {
            left = mid + 1;
        }
        else if (target < edges[mid]) {
            right = mid;
        }
        else {
            return true;
        }
    }
    return false;
}

// Triangle structure for deduplication (normalized form)
struct Triangle {
    unsigned v0, v1, v2;

    __host__ __device__ Triangle() : v0(0), v1(0), v2(0)
    {
    }

    __host__ __device__ Triangle(unsigned a, unsigned b, unsigned c)
    {
        // Normalize: sort vertices in ascending order (orientation-independent)
        // This treats (a,b,c) and (a,c,b) as the same triangle
        unsigned vmin = a < b ? (a < c ? a : c) : (b < c ? b : c);
        unsigned vmax = a > b ? (a > c ? a : c) : (b > c ? b : c);
        unsigned vmid =
            (a != vmin && a != vmax) ? a : ((b != vmin && b != vmax) ? b : c);

        v0 = vmin;
        v1 = vmid;
        v2 = vmax;
    }

    __host__ __device__ bool operator<(const Triangle& other) const
    {
        if (v0 != other.v0)
            return v0 < other.v0;
        if (v1 != other.v1)
            return v1 < other.v1;
        return v2 < other.v2;
    }

    __host__ __device__ bool operator==(const Triangle& other) const
    {
        return v0 == other.v0 && v1 == other.v1 && v2 == other.v2;
    }
};

// Kernel to normalize triangles for deduplication
__global__ void normalize_triangles_kernel(
    const unsigned* triangles,
    unsigned num_triangles,
    Triangle* normalized)
{
    unsigned tri_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (tri_idx >= num_triangles)
        return;

    unsigned v0 = triangles[tri_idx * 3 + 0];
    unsigned v1 = triangles[tri_idx * 3 + 1];
    unsigned v2 = triangles[tri_idx * 3 + 2];

    normalized[tri_idx] = Triangle(v0, v1, v2);
}

// Kernel to extract all edges from triangles
__global__ void extract_edges_kernel(
    const unsigned* triangles,
    unsigned num_triangles,
    Edge* edges)
{
    unsigned tri_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (tri_idx >= num_triangles)
        return;

    unsigned v0 = triangles[tri_idx * 3 + 0];
    unsigned v1 = triangles[tri_idx * 3 + 1];
    unsigned v2 = triangles[tri_idx * 3 + 2];

    // Each triangle contributes 3 edges
    edges[tri_idx * 3 + 0] = Edge(v0, v1);
    edges[tri_idx * 3 + 1] = Edge(v1, v2);
    edges[tri_idx * 3 + 2] = Edge(v2, v0);
}

// Check if vertex v and face (a,b,c) form a valid tetrahedron using edge list
__device__ bool forms_tetrahedron_fast(
    const Edge* edges,
    unsigned num_edges,
    unsigned v,
    unsigned a,
    unsigned b,
    unsigned c)
{
    // Check if the 3 edges from v to the opposite face vertices exist
    return has_edge_fast(edges, num_edges, v, a) &&
           has_edge_fast(edges, num_edges, v, b) &&
           has_edge_fast(edges, num_edges, v, c);
}

// Count valid opposite faces using 2D parallelization
__global__ void count_vertex_opposite_faces_kernel(
    const unsigned* triangles,
    unsigned num_triangles,
    const Edge* edges,
    unsigned num_edges,
    unsigned num_vertices,
    unsigned* face_counts)
{
    unsigned idx = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned total = num_triangles * num_vertices;

    if (idx >= total)
        return;

    unsigned tri_idx = idx / num_vertices;
    unsigned v = idx % num_vertices;

    unsigned v0 = triangles[tri_idx * 3 + 0];
    unsigned v1 = triangles[tri_idx * 3 + 1];
    unsigned v2 = triangles[tri_idx * 3 + 2];

    // Check if vertex v is not in this triangle and forms a tetrahedron
    if (!contains_vertex(v0, v1, v2, v) &&
        forms_tetrahedron_fast(edges, num_edges, v, v0, v1, v2)) {
        atomicAdd(&face_counts[v], 1);
    }
}

// Fill opposite face triplets using 2D parallelization
__global__ void fill_volume_adjacency_kernel(
    const unsigned* triangles,
    unsigned num_triangles,
    const Edge* edges,
    unsigned num_edges,
    unsigned num_vertices,
    const unsigned* offsets,
    unsigned* write_positions,
    unsigned* adjacency_list)
{
    unsigned idx = blockIdx.x * blockDim.x + threadIdx.x;
    unsigned total = num_triangles * num_vertices;

    if (idx >= total)
        return;

    unsigned tri_idx = idx / num_vertices;
    unsigned v = idx % num_vertices;

    unsigned v0 = triangles[tri_idx * 3 + 0];
    unsigned v1 = triangles[tri_idx * 3 + 1];
    unsigned v2 = triangles[tri_idx * 3 + 2];

    // Check if vertex v is not in this triangle and forms a tetrahedron
    if (!contains_vertex(v0, v1, v2, v) &&
        forms_tetrahedron_fast(edges, num_edges, v, v0, v1, v2)) {
        unsigned pos = atomicAdd(&write_positions[v], 3);
        adjacency_list[offsets[v] + 1 + pos + 0] = v0;
        adjacency_list[offsets[v] + 1 + pos + 1] = v1;
        adjacency_list[offsets[v] + 1 + pos + 2] = v2;
    }
}

std::tuple<cuda::CUDALinearBufferHandle, cuda::CUDALinearBufferHandle>
compute_volume_adjacency_gpu(
    cuda::CUDALinearBufferHandle vertices,
    cuda::CUDALinearBufferHandle triangles)
{
    unsigned num_vertices =
        vertices->getDesc().element_count;  // glm::vec3 count
    unsigned num_triangles =
        triangles->getDesc().element_count / 3;  // 3 indices per triangle

    auto tri_ptr = (unsigned*)triangles->get_device_ptr();

    int threads = 256;
    int blocks = (num_triangles + threads - 1) / threads;

    // Step 0: Deduplicate triangles (shared faces appear multiple times in
    // input)
    thrust::device_vector<Triangle> normalized_triangles(num_triangles);
    auto norm_tri_ptr = thrust::raw_pointer_cast(normalized_triangles.data());

    normalize_triangles_kernel<<<blocks, threads>>>(
        tri_ptr, num_triangles, norm_tri_ptr);
    cudaDeviceSynchronize();

    // Sort and remove duplicates
    thrust::sort(
        thrust::device,
        normalized_triangles.begin(),
        normalized_triangles.end());
    auto new_tri_end = thrust::unique(
        thrust::device,
        normalized_triangles.begin(),
        normalized_triangles.end());
    unsigned num_unique_triangles =
        thrust::distance(normalized_triangles.begin(), new_tri_end);
    normalized_triangles.resize(num_unique_triangles);

    // Convert back to flat buffer: copy triangle data in interleaved format
    thrust::device_vector<unsigned> unique_tri_buffer(num_unique_triangles * 3);
    auto unique_tri_ptr = thrust::raw_pointer_cast(unique_tri_buffer.data());
    auto norm_tri_ptr_const =
        thrust::raw_pointer_cast(normalized_triangles.data());

    // Use a kernel to properly copy the data
    blocks = (num_unique_triangles + threads - 1) / threads;
    thrust::for_each(
        thrust::device,
        thrust::counting_iterator<unsigned>(0),
        thrust::counting_iterator<unsigned>(num_unique_triangles),
        [unique_tri_ptr, norm_tri_ptr_const] __device__(unsigned i) {
            unique_tri_ptr[i * 3 + 0] = norm_tri_ptr_const[i].v0;
            unique_tri_ptr[i * 3 + 1] = norm_tri_ptr_const[i].v1;
            unique_tri_ptr[i * 3 + 2] = norm_tri_ptr_const[i].v2;
        });
    cudaDeviceSynchronize();

    // Update triangle count to unique count
    num_triangles = num_unique_triangles;

    // Step 1: Build sorted edge list for fast lookup
    thrust::device_vector<Edge> edges(num_triangles * 3);
    auto edges_ptr = thrust::raw_pointer_cast(edges.data());

    blocks = (num_triangles + threads - 1) / threads;

    extract_edges_kernel<<<blocks, threads>>>(
        unique_tri_ptr, num_triangles, edges_ptr);
    cudaDeviceSynchronize();

    // Sort and unique edges
    thrust::sort(thrust::device, edges.begin(), edges.end());
    auto new_end = thrust::unique(thrust::device, edges.begin(), edges.end());
    unsigned num_unique_edges = thrust::distance(edges.begin(), new_end);
    edges.resize(num_unique_edges);

    // Step 2: Count opposite faces per vertex using 2D parallelization
    thrust::device_vector<unsigned> face_counts(num_vertices, 0);
    auto counts_ptr = thrust::raw_pointer_cast(face_counts.data());

    unsigned total_pairs = num_triangles * num_vertices;
    blocks = (total_pairs + threads - 1) / threads;

    count_vertex_opposite_faces_kernel<<<blocks, threads>>>(
        unique_tri_ptr,
        num_triangles,
        edges_ptr,
        num_unique_edges,
        num_vertices,
        counts_ptr);
    cudaDeviceSynchronize();

    // Step 3: Build offset buffer
    // Each vertex stores: [count, triplet1_a, triplet1_b, triplet1_c, ...]
    thrust::device_vector<unsigned> offsets(num_vertices);
    thrust::device_vector<unsigned> adjacency_sizes(num_vertices);

    // Each opposite face contributes 3 values (one face triplet) per vertex
    thrust::transform(
        thrust::device,
        face_counts.begin(),
        face_counts.end(),
        adjacency_sizes.begin(),
        [] __device__(unsigned count) {
            return 1 + count * 3;
        });  // +1 for count field

    thrust::exclusive_scan(
        thrust::device,
        adjacency_sizes.begin(),
        adjacency_sizes.end(),
        offsets.begin());

    unsigned total_size = thrust::reduce(
        thrust::device, adjacency_sizes.begin(), adjacency_sizes.end());

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
        [adj_ptr, offsets_ptr, counts_ptr] __device__(unsigned v) {
            adj_ptr[offsets_ptr[v]] = counts_ptr[v];
        });

    // Track write positions for each vertex
    thrust::device_vector<unsigned> write_positions(num_vertices, 0);
    auto write_pos_ptr = thrust::raw_pointer_cast(write_positions.data());

    fill_volume_adjacency_kernel<<<blocks, threads>>>(
        unique_tri_ptr,
        num_triangles,
        edges_ptr,
        num_unique_edges,
        num_vertices,
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
