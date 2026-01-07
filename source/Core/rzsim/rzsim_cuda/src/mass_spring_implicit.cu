#include <stdio.h>
#include <thrust/device_vector.h>
#include <thrust/reduce.h>
#include <thrust/remove.h>
#include <thrust/sort.h>
#include <thrust/transform.h>
#include <thrust/transform_reduce.h>
#include <thrust/unique.h>
#include <cusparse.h>

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <RHI/cuda.hpp>
#include <cstddef>
#include <map>
#include <set>

#include "RZSolver/Solver.hpp"
#include "rzsim_cuda/mass_spring_implicit.cuh"


RUZINO_NAMESPACE_OPEN_SCOPE

namespace rzsim_cuda {

// Kernel to extract edges from triangles
__global__ void
extract_edges_kernel(const int* triangles, int num_triangles, int* edge_pairs)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_triangles)
        return;

    int base_idx = tid * 3;
    int v0 = triangles[base_idx];
    int v1 = triangles[base_idx + 1];
    int v2 = triangles[base_idx + 2];

    // Each triangle produces 3 edges
    int output_base = tid * 6;

    // Edge 0-1
    edge_pairs[output_base + 0] = min(v0, v1);
    edge_pairs[output_base + 1] = max(v0, v1);

    // Edge 1-2
    edge_pairs[output_base + 2] = min(v1, v2);
    edge_pairs[output_base + 3] = max(v1, v2);

    // Edge 2-0
    edge_pairs[output_base + 4] = min(v2, v0);
    edge_pairs[output_base + 5] = max(v2, v0);
}

// Functor for comparing edge pairs
struct EdgePairEqual {
    __host__ __device__ bool operator()(
        const thrust::tuple<int, int>& a,
        const thrust::tuple<int, int>& b) const
    {
        return thrust::get<0>(a) == thrust::get<0>(b) &&
               thrust::get<1>(a) == thrust::get<1>(b);
    }
};

// Kernel to separate interleaved edges
__global__ void separate_edges_kernel(
    const int* interleaved,
    int* first,
    int* second,
    int num_edges)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_edges)
        return;

    first[tid] = interleaved[tid * 2];
    second[tid] = interleaved[tid * 2 + 1];
}

// Kernel to copy separated edges back to interleaved format
__global__ void interleave_edges_kernel(
    const int* edge_first,
    const int* edge_second,
    int* output,
    int num_edges)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_edges)
        return;

    output[tid * 2] = edge_first[tid];
    output[tid * 2 + 1] = edge_second[tid];
}

cuda::CUDALinearBufferHandle build_edge_set_gpu(
    cuda::CUDALinearBufferHandle positions,
    cuda::CUDALinearBufferHandle edges)
{
    // Get triangle count
    size_t num_triangles = edges->getDesc().element_count / 3;

    printf("[GPU] Building edge set from %zu triangles\n", num_triangles);

    // Allocate temporary buffer for all edges (3 edges per triangle)
    thrust::device_vector<int> all_edges(num_triangles * 6);

    // Launch kernel to extract edges
    int block_size = 256;
    int num_blocks = (num_triangles + block_size - 1) / block_size;

    extract_edges_kernel<<<num_blocks, block_size>>>(
        edges->get_device_ptr<int>(),
        num_triangles,
        thrust::raw_pointer_cast(all_edges.data()));

    cudaDeviceSynchronize();

    // Create vectors for edge pairs
    thrust::device_vector<int> edge_first(num_triangles * 3);
    thrust::device_vector<int> edge_second(num_triangles * 3);

    // Separate the interleaved edge data using kernel
    int sep_blocks = (num_triangles * 3 + block_size - 1) / block_size;
    separate_edges_kernel<<<sep_blocks, block_size>>>(
        thrust::raw_pointer_cast(all_edges.data()),
        thrust::raw_pointer_cast(edge_first.data()),
        thrust::raw_pointer_cast(edge_second.data()),
        num_triangles * 3);

    cudaDeviceSynchronize();

    // Create zip iterator
    auto edge_begin = thrust::make_zip_iterator(
        thrust::make_tuple(edge_first.begin(), edge_second.begin()));
    auto edge_end = thrust::make_zip_iterator(
        thrust::make_tuple(edge_first.end(), edge_second.end()));

    // Sort edges
    thrust::sort(
        edge_begin,
        edge_end,
        [] __device__(
            const thrust::tuple<int, int>& a,
            const thrust::tuple<int, int>& b) {
            if (thrust::get<0>(a) != thrust::get<0>(b))
                return thrust::get<0>(a) < thrust::get<0>(b);
            return thrust::get<1>(a) < thrust::get<1>(b);
        });

    // Remove duplicates
    auto new_end = thrust::unique(edge_begin, edge_end, EdgePairEqual());

    // Calculate unique edge count
    size_t num_unique_edges = new_end - edge_begin;

    printf("[GPU] Found %zu unique edges\n", num_unique_edges);

    // Copy unique edges to output buffer (interleaved format)
    auto output_buffer =
        cuda::create_cuda_linear_buffer<int>(num_unique_edges * 2);

    int* output_ptr = output_buffer->get_device_ptr<int>();

    // Use kernel to interleave the data
    int copy_blocks = (num_unique_edges + block_size - 1) / block_size;
    interleave_edges_kernel<<<copy_blocks, block_size>>>(
        thrust::raw_pointer_cast(edge_first.data()),
        thrust::raw_pointer_cast(edge_second.data()),
        output_ptr,
        num_unique_edges);

    cudaDeviceSynchronize();

    return output_buffer;
}

// Kernel to compute explicit step: x_tilde = x + dt * v
__global__ void explicit_step_kernel(
    const float* x,
    const float* v,
    float dt,
    int num_particles,
    float* x_tilde)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_particles)
        return;

    x_tilde[tid * 3 + 0] = x[tid * 3 + 0] + dt * v[tid * 3 + 0];
    x_tilde[tid * 3 + 1] = x[tid * 3 + 1] + dt * v[tid * 3 + 1];
    x_tilde[tid * 3 + 2] = x[tid * 3 + 2] + dt * v[tid * 3 + 2];
}

// Kernel to setup external forces (gravity)
__global__ void setup_external_forces_kernel(
    float mass,
    float gravity,
    int num_particles,
    float* f_ext)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_particles)
        return;

    f_ext[tid * 3 + 0] = 0.0f;
    f_ext[tid * 3 + 1] = 0.0f;
    f_ext[tid * 3 + 2] = mass * gravity;  // gravity in z direction
}

// Kernel to compute rest lengths from initial positions
__global__ void compute_rest_lengths_kernel(
    const float* positions,
    const int* springs,
    float* rest_lengths,
    int num_springs)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_springs)
        return;

    int i = springs[tid * 2];
    int j = springs[tid * 2 + 1];

    float dx = positions[i * 3 + 0] - positions[j * 3 + 0];
    float dy = positions[i * 3 + 1] - positions[j * 3 + 1];
    float dz = positions[i * 3 + 2] - positions[j * 3 + 2];

    rest_lengths[tid] = sqrtf(dx * dx + dy * dy + dz * dz);
}

void explicit_step_gpu(
    cuda::CUDALinearBufferHandle x,
    cuda::CUDALinearBufferHandle v,
    float dt,
    int num_particles,
    cuda::CUDALinearBufferHandle x_tilde)
{
    int block_size = 256;
    int num_blocks = (num_particles + block_size - 1) / block_size;

    explicit_step_kernel<<<num_blocks, block_size>>>(
        x->get_device_ptr<float>(),
        v->get_device_ptr<float>(),
        dt,
        num_particles,
        x_tilde->get_device_ptr<float>());

    cudaDeviceSynchronize();
}

void setup_external_forces_gpu(
    float mass,
    float gravity,
    int num_particles,
    cuda::CUDALinearBufferHandle f_ext)
{
    int block_size = 256;
    int num_blocks = (num_particles + block_size - 1) / block_size;

    setup_external_forces_kernel<<<num_blocks, block_size>>>(
        mass, gravity, num_particles, f_ext->get_device_ptr<float>());

    cudaDeviceSynchronize();
}

cuda::CUDALinearBufferHandle compute_rest_lengths_gpu(
    cuda::CUDALinearBufferHandle positions,
    cuda::CUDALinearBufferHandle springs)
{
    size_t num_springs = springs->getDesc().element_count / 2;

    auto rest_lengths_buffer =
        cuda::create_cuda_linear_buffer<float>(num_springs);

    int block_size = 256;
    int num_blocks = (num_springs + block_size - 1) / block_size;

    compute_rest_lengths_kernel<<<num_blocks, block_size>>>(
        positions->get_device_ptr<float>(),
        springs->get_device_ptr<int>(),
        rest_lengths_buffer->get_device_ptr<float>(),
        num_springs);

    cudaDeviceSynchronize();

    return rest_lengths_buffer;
}

// Kernel to compute gradient
__global__ void compute_gradient_kernel(
    const float* x_curr,
    const float* x_tilde,
    const float* M_diag,
    const float* f_ext,
    const int* springs,
    const float* rest_lengths,
    float stiffness,
    float dt,
    int num_particles,
    int num_springs,
    float* grad)
{
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= num_particles)
        return;

    // Initialize with inertial term: M * (x - x_tilde)
    grad[tid * 3 + 0] =
        M_diag[tid * 3 + 0] * (x_curr[tid * 3 + 0] - x_tilde[tid * 3 + 0]);
    grad[tid * 3 + 1] =
        M_diag[tid * 3 + 1] * (x_curr[tid * 3 + 1] - x_tilde[tid * 3 + 1]);
    grad[tid * 3 + 2] =
        M_diag[tid * 3 + 2] * (x_curr[tid * 3 + 2] - x_tilde[tid * 3 + 2]);

    // Add spring forces
    for (int s = 0; s < num_springs; ++s) {
        int i = springs[s * 2];
        int j = springs[s * 2 + 1];

        if (i != tid && j != tid)
            continue;

        float l0 = rest_lengths[s];
        float l0_sq = l0 * l0;

        float dx = x_curr[i * 3 + 0] - x_curr[j * 3 + 0];
        float dy = x_curr[i * 3 + 1] - x_curr[j * 3 + 1];
        float dz = x_curr[i * 3 + 2] - x_curr[j * 3 + 2];
        float diff_sq = dx * dx + dy * dy + dz * dz;

        float factor = 2.0f * stiffness * (diff_sq / l0_sq - 1.0f) * dt * dt;

        if (i == tid) {
            grad[tid * 3 + 0] += factor * dx;
            grad[tid * 3 + 1] += factor * dy;
            grad[tid * 3 + 2] += factor * dz;
        }
        else {  // j == tid
            grad[tid * 3 + 0] -= factor * dx;
            grad[tid * 3 + 1] -= factor * dy;
            grad[tid * 3 + 2] -= factor * dz;
        }
    }

    // Subtract external forces
    grad[tid * 3 + 0] -= dt * dt * f_ext[tid * 3 + 0];
    grad[tid * 3 + 1] -= dt * dt * f_ext[tid * 3 + 1];
    grad[tid * 3 + 2] -= dt * dt * f_ext[tid * 3 + 2];
}

void compute_gradient_gpu(
    cuda::CUDALinearBufferHandle x_curr,
    cuda::CUDALinearBufferHandle x_tilde,
    cuda::CUDALinearBufferHandle M_diag,
    cuda::CUDALinearBufferHandle f_ext,
    cuda::CUDALinearBufferHandle springs,
    cuda::CUDALinearBufferHandle rest_lengths,
    float stiffness,
    float dt,
    int num_particles,
    cuda::CUDALinearBufferHandle grad)
{
    int num_springs = springs->getDesc().element_count / 2;

    int block_size = 256;
    int num_blocks = (num_particles + block_size - 1) / block_size;

    compute_gradient_kernel<<<num_blocks, block_size>>>(
        x_curr->get_device_ptr<float>(),
        x_tilde->get_device_ptr<float>(),
        M_diag->get_device_ptr<float>(),
        f_ext->get_device_ptr<float>(),
        springs->get_device_ptr<int>(),
        rest_lengths->get_device_ptr<float>(),
        stiffness,
        dt,
        num_particles,
        num_springs,
        grad->get_device_ptr<float>());

    cudaDeviceSynchronize();

    // Debug: print first few gradient values
    std::vector<float> grad_host = grad->get_host_vector<float>();
    for (int i = 0; i < std::min(10, (int)grad_host.size()); i++) {
        printf("  grad[%d] = %.6e\n", i, grad_host[i]);
    }
}

// Kernel to compute 3x3 eigenvalues and eigenvectors using analytical solution
// PSD投影辅助函数 - 使用Eigen做特征分解
__device__ Eigen::Matrix3f project_psd(const Eigen::Matrix3f& H)
{
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> eigensolver(H);
    Eigen::Vector3f eigenvalues = eigensolver.eigenvalues();
    Eigen::Matrix3f eigenvectors = eigensolver.eigenvectors();

    // Clamp negative eigenvalues to zero
    for (int i = 0; i < 3; i++) {
        if (eigenvalues(i) < 0.0f)
            eigenvalues(i) = 0.0f;
    }

    // Reconstruct: H_psd = V * Lambda * V^T
    return eigenvectors * eigenvalues.asDiagonal() * eigenvectors.transpose();
}

// Kernel to assemble Hessian matrix contributions from springs
__global__ void assemble_hessian_kernel(
    const float* x_curr,
    const float* M_diag,
    const int* springs,
    const float* rest_lengths,
    float stiffness,
    float dt,
    int num_particles,
    int num_springs,
    float regularization,
    int* row_indices,  // Output triplet format
    int* col_indices,
    float* values,
    int* triplet_count)
{
    int s = blockIdx.x * blockDim.x + threadIdx.x;

    // First, add mass matrix diagonal
    if (s < num_particles * 3) {
        int idx = atomicAdd(triplet_count, 1);
        row_indices[idx] = s;
        col_indices[idx] = s;
        values[idx] = M_diag[s] + regularization;
        return;
    }

    if (s >= num_particles * 3 + num_springs)
        return;

    s = s - num_particles * 3;  // Adjust to spring index

    int vi = springs[s * 2];
    int vj = springs[s * 2 + 1];
    float l0 = rest_lengths[s];

    if (l0 < 1e-10f)
        return;

    float l0_sq = l0 * l0;

    float xi[3], xj[3], diff[3];
    xi[0] = x_curr[vi * 3 + 0];
    xi[1] = x_curr[vi * 3 + 1];
    xi[2] = x_curr[vi * 3 + 2];
    xj[0] = x_curr[vj * 3 + 0];
    xj[1] = x_curr[vj * 3 + 1];
    xj[2] = x_curr[vj * 3 + 2];

    diff[0] = xi[0] - xj[0];
    diff[1] = xi[1] - xj[1];
    diff[2] = xi[2] - xj[2];

    float diff_sq = diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2];

    // 使用Eigen计算H_diff和PSD投影
    Eigen::Vector3f diff_vec(diff[0], diff[1], diff[2]);
    float scale = 2.0f * stiffness / l0_sq;

    // H_diff = 2*k/l0^2 * (2*outer(diff,diff) + (diff_sq - l0^2)*I)
    Eigen::Matrix3f outer = diff_vec * diff_vec.transpose();
    Eigen::Matrix3f H_diff =
        scale *
        (2.0f * outer + (diff_sq - l0_sq) * Eigen::Matrix3f::Identity());

    // PSD投影：特征分解并钳位负特征值
    H_diff = project_psd(H_diff);

    // Add 6x6 block to triplet arrays
    float dt_sq = dt * dt;

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            float val = dt_sq * H_diff(r, c);

            // (vi, vi) block
            int idx = atomicAdd(triplet_count, 1);
            row_indices[idx] = vi * 3 + r;
            col_indices[idx] = vi * 3 + c;
            values[idx] = val;

            // (vi, vj) block
            idx = atomicAdd(triplet_count, 1);
            row_indices[idx] = vi * 3 + r;
            col_indices[idx] = vj * 3 + c;
            values[idx] = -val;

            // (vj, vi) block
            idx = atomicAdd(triplet_count, 1);
            row_indices[idx] = vj * 3 + r;
            col_indices[idx] = vi * 3 + c;
            values[idx] = -val;

            // (vj, vj) block
            idx = atomicAdd(triplet_count, 1);
            row_indices[idx] = vj * 3 + r;
            col_indices[idx] = vj * 3 + c;
            values[idx] = val;
        }
    }
}

// Simple COO to CSR conversion kernel
// Kernel to copy int buffer to thrust vector
__global__ void copy_int_buffer_kernel(const int* src, int* dst, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        dst[idx] = src[idx];
    }
}

// Kernel to copy float buffer to thrust vector
__global__ void copy_float_buffer_kernel(const float* src, float* dst, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        dst[idx] = src[idx];
    }
}

CSRMatrix assemble_hessian_gpu(
    cuda::CUDALinearBufferHandle x_curr,
    cuda::CUDALinearBufferHandle M_diag,
    cuda::CUDALinearBufferHandle springs,
    cuda::CUDALinearBufferHandle rest_lengths,
    float stiffness,
    float dt,
    int num_particles)
{
    int num_springs = springs->getDesc().element_count / 2;
    float regularization = 1e-6f;

    // Estimate maximum triplets: diagonal + springs * 36
    int max_triplets = num_particles * 3 + num_springs * 36;

    auto row_indices_buf = cuda::create_cuda_linear_buffer<int>(max_triplets);
    auto col_indices_buf = cuda::create_cuda_linear_buffer<int>(max_triplets);
    auto values_buf = cuda::create_cuda_linear_buffer<float>(max_triplets);
    auto triplet_count_buf = cuda::create_cuda_linear_buffer<int>(1);

    // Initialize count to 0
    int zero = 0;
    triplet_count_buf->assign_host_value(zero);

    int block_size = 256;
    int total_tasks = num_particles * 3 + num_springs;
    int num_blocks = (total_tasks + block_size - 1) / block_size;

    assemble_hessian_kernel<<<num_blocks, block_size>>>(
        x_curr->get_device_ptr<float>(),
        M_diag->get_device_ptr<float>(),
        springs->get_device_ptr<int>(),
        rest_lengths->get_device_ptr<float>(),
        stiffness,
        dt,
        num_particles,
        num_springs,
        regularization,
        row_indices_buf->get_device_ptr<int>(),
        col_indices_buf->get_device_ptr<int>(),
        values_buf->get_device_ptr<float>(),
        triplet_count_buf->get_device_ptr<int>());

    cudaDeviceSynchronize();

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        printf(
            "[GPU Error] Kernel launch failed: %s\n", cudaGetErrorString(err));
    }

    int nnz = triplet_count_buf->get_host_value<int>();
    printf(
        "[GPU] Assembled Hessian COO: %d triplets (before deduplication)\n",
        nnz);

    // Use thrust device_vectors and custom kernels to copy data
    thrust::device_vector<int> row_vec(nnz);
    thrust::device_vector<int> col_vec(nnz);
    thrust::device_vector<float> val_vec(nnz);

    int matrix_size = num_particles * 3;
    int copy_blocks = (nnz + 256 - 1) / 256;

    // Copy using custom kernels
    copy_int_buffer_kernel<<<copy_blocks, 256>>>(
        row_indices_buf->get_device_ptr<int>(),
        thrust::raw_pointer_cast(row_vec.data()),
        nnz);
    copy_int_buffer_kernel<<<copy_blocks, 256>>>(
        col_indices_buf->get_device_ptr<int>(),
        thrust::raw_pointer_cast(col_vec.data()),
        nnz);
    copy_float_buffer_kernel<<<copy_blocks, 256>>>(
        values_buf->get_device_ptr<float>(),
        thrust::raw_pointer_cast(val_vec.data()),
        nnz);

    cudaDeviceSynchronize();

    // Create keys: key = row * matrix_size + col
    thrust::device_vector<long long> keys(nnz);
    thrust::transform(
        thrust::make_zip_iterator(
            thrust::make_tuple(row_vec.begin(), col_vec.begin())),
        thrust::make_zip_iterator(
            thrust::make_tuple(row_vec.end(), col_vec.end())),
        keys.begin(),
        [matrix_size] __device__(const thrust::tuple<int, int>& t) {
            return (long long)thrust::get<0>(t) * (long long)matrix_size +
                   (long long)thrust::get<1>(t);
        });

    // Sort by keys
    thrust::sort_by_key(keys.begin(), keys.end(), val_vec.begin());

    // Sum duplicates using reduce_by_key
    thrust::device_vector<long long> unique_keys(nnz);
    thrust::device_vector<float> unique_vals(nnz);

    auto new_end = thrust::reduce_by_key(
        keys.begin(),
        keys.end(),
        val_vec.begin(),
        unique_keys.begin(),
        unique_vals.begin());

    int nnz_unique = new_end.first - unique_keys.begin();

    // Resize to only the unique entries
    unique_keys.resize(nnz_unique);
    unique_vals.resize(nnz_unique);

    // Extract row and col from unique keys
    thrust::device_vector<int> unique_rows(nnz_unique);
    thrust::device_vector<int> unique_cols(nnz_unique);
    thrust::transform(
        unique_keys.begin(),
        unique_keys.begin() + nnz_unique,
        thrust::make_zip_iterator(
            thrust::make_tuple(unique_rows.begin(), unique_cols.begin())),
        [matrix_size] __device__(long long key) {
            return thrust::make_tuple(
                (int)(key / matrix_size), (int)(key % matrix_size));
        });

    // Create new buffers from thrust vectors
    // Note: explicitly cast to size_t to avoid matching the wrong overload
    cuda::CUDALinearBufferHandle final_row_buf = cuda::create_cuda_linear_buffer<int>((size_t)nnz_unique);
    cuda::CUDALinearBufferHandle final_col_buf = cuda::create_cuda_linear_buffer<int>((size_t)nnz_unique);
    cuda::CUDALinearBufferHandle final_val_buf = cuda::create_cuda_linear_buffer<float>((size_t)nnz_unique);
    
    // Copy data from thrust vectors to buffers
    cudaMemcpy(
        final_row_buf->get_device_ptr<int>(),
        thrust::raw_pointer_cast(unique_rows.data()),
        nnz_unique * sizeof(int),
        cudaMemcpyDeviceToDevice);
    cudaMemcpy(
        final_col_buf->get_device_ptr<int>(),
        thrust::raw_pointer_cast(unique_cols.data()),
        nnz_unique * sizeof(int),
        cudaMemcpyDeviceToDevice);
    cudaMemcpy(
        final_val_buf->get_device_ptr<float>(),
        thrust::raw_pointer_cast(unique_vals.data()),
        nnz_unique * sizeof(float),
        cudaMemcpyDeviceToDevice);

    cudaDeviceSynchronize();

    nnz = nnz_unique;
    printf(
        "[GPU] Assembled Hessian: %d x %d with %d non-zeros (after "
        "deduplication)\n",
        matrix_size,
        matrix_size,
        nnz);

    // Convert COO to CSR format using cuSPARSE
    auto row_offsets_buf =
        cuda::create_cuda_linear_buffer<int>(matrix_size + 1);

    // Use cuSPARSE for COO to CSR conversion
    cusparseHandle_t handle;
    cusparseCreate(&handle);
    
    cusparseXcoo2csr(
        handle,
        final_row_buf->get_device_ptr<int>(),
        nnz,
        matrix_size,
        row_offsets_buf->get_device_ptr<int>(),
        CUSPARSE_INDEX_BASE_ZERO);
    
    cusparseDestroy(handle);
    cudaDeviceSynchronize();

    CSRMatrix result;
    result.row_offsets = row_offsets_buf;
    result.col_indices = final_col_buf;
    result.values = final_val_buf;
    result.num_rows = matrix_size;
    result.num_cols = matrix_size;
    result.nnz = nnz;

    return result;
}

}  // namespace rzsim_cuda

RUZINO_NAMESPACE_CLOSE_SCOPE
