#include <GCore/util_openmesh_bind.h>
#include <GCore/Components/MeshComponent.h>
#include <rzsim/reduced_order_basis.h>

#include <Eigen/Sparse>
#include <Eigen/Eigenvalues>
#include <iostream>

RUZINO_NAMESPACE_OPEN_SCOPE

using PolyMesh = OpenMesh::PolyMesh_ArrayKernelT<>;
using VolumeMesh = OpenVolumeMesh::GeometricTetrahedralMeshV3d;

ReducedOrderedBasis::ReducedOrderedBasis(const Geometry& g, int num_modes)
{
    // Get mesh component
    auto mesh_comp = g.get_component<MeshComponent>();
    if (!mesh_comp) {
        throw std::runtime_error("Geometry must have MeshComponent");
    }

    // Try to get volume mesh first (3D)
    auto geom_ptr = const_cast<Geometry*>(&g);
    auto volumemesh = operand_to_openvolumemesh(geom_ptr);
    
    if (volumemesh && volumemesh->n_vertices() > 0 && volumemesh->n_cells() > 0) {
        // 3D case: assemble Laplace operator for tetrahedral mesh
        assemble_laplacian_3d(volumemesh.get());
    } else {
        // 2D case: assemble Laplace operator for triangular mesh
        auto openmesh = operand_to_openmesh(geom_ptr);
        if (!openmesh || openmesh->n_vertices() == 0) {
            throw std::runtime_error("Invalid mesh for reduced order basis");
        }
        assemble_laplacian_2d(openmesh.get());
    }
    
    // Compute eigenmodes
    compute_eigenmodes(num_modes);
}

void ReducedOrderedBasis::assemble_laplacian_2d(void* mesh_ptr)
{
    auto mesh = static_cast<PolyMesh*>(mesh_ptr);
    int n_vertices = mesh->n_vertices();
    
    // Triplets for sparse matrix assembly
    std::vector<Eigen::Triplet<float>> triplets;
    
    // Iterate over all faces (triangles)
    for (auto f_it : mesh->faces()) {
        // Get the three vertices of the triangle
        std::vector<int> vertex_ids;
        std::vector<pxr::GfVec3d> positions;
        
        for (auto fv_it : mesh->fv_range(f_it)) {
            vertex_ids.push_back(fv_it.idx());
            auto pt = mesh->point(fv_it);
            positions.push_back(pxr::GfVec3d(pt[0], pt[1], pt[2]));
        }
        
        if (vertex_ids.size() != 3) continue; // Skip non-triangular faces
        
        // Compute element stiffness matrix (3x3 for triangle)
        auto v0 = positions[0];
        auto v1 = positions[1];
        auto v2 = positions[2];
        
        // Edge vectors
        auto d1 = v1 - v0;
        auto d2 = v2 - v0;
        
        // Area of triangle
        double area = 0.5 * std::abs(d1[0] * d2[1] - d1[1] * d2[0]);
        
        if (area < 1e-12) continue; // Skip degenerate triangles
        
        // Shape function gradients in physical coordinates
        // For linear triangle: grad(N_i) is constant
        double det = d1[0] * d2[1] - d1[1] * d2[0];
        
        // Gradients of shape functions
        double grad_N[3][2];
        grad_N[0][0] = (d2[1] - d1[1]) / det;
        grad_N[0][1] = (d1[0] - d2[0]) / det;
        grad_N[1][0] = -d2[1] / det;
        grad_N[1][1] = d2[0] / det;
        grad_N[2][0] = d1[1] / det;
        grad_N[2][1] = -d1[0] / det;
        
        // Element stiffness matrix K_e = area * grad_N^T * grad_N
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                double k_ij = area * (grad_N[i][0] * grad_N[j][0] + 
                                     grad_N[i][1] * grad_N[j][1]);
                triplets.emplace_back(vertex_ids[i], vertex_ids[j], k_ij);
            }
        }
    }
    
    // Assemble global stiffness matrix
    laplacian_matrix_.resize(n_vertices, n_vertices);
    laplacian_matrix_.setFromTriplets(triplets.begin(), triplets.end());
    laplacian_matrix_.makeCompressed();
    
    std::cout << "Assembled 2D Laplacian matrix: " << n_vertices 
              << " x " << n_vertices << " with " 
              << laplacian_matrix_.nonZeros() << " non-zeros" << std::endl;
}

void ReducedOrderedBasis::assemble_laplacian_3d(void* mesh_ptr)
{
    auto mesh = static_cast<VolumeMesh*>(mesh_ptr);
    int n_vertices = mesh->n_vertices();
    
    // Triplets for sparse matrix assembly
    std::vector<Eigen::Triplet<float>> triplets;
    
    // Iterate over all cells (tetrahedra)
    for (auto c_it = mesh->cells_begin(); c_it != mesh->cells_end(); ++c_it) {
        // Get the four vertices of the tetrahedron
        std::vector<int> vertex_ids;
        std::vector<pxr::GfVec3d> positions;
        
        for (auto cv_it = mesh->cv_iter(*c_it); cv_it.valid(); ++cv_it) {
            vertex_ids.push_back((*cv_it).idx());
            auto pt = mesh->vertex(*cv_it);
            positions.push_back(pxr::GfVec3d(pt[0], pt[1], pt[2]));
        }
        
        if (vertex_ids.size() != 4) continue; // Skip non-tetrahedral cells
        
        // Compute element stiffness matrix (4x4 for tetrahedron)
        auto v0 = positions[0];
        auto v1 = positions[1];
        auto v2 = positions[2];
        auto v3 = positions[3];
        
        // Edge vectors from v0
        auto d1 = v1 - v0;
        auto d2 = v2 - v0;
        auto d3 = v3 - v0;
        
        // Volume of tetrahedron
        double det = d1[0] * (d2[1] * d3[2] - d2[2] * d3[1]) -
                     d1[1] * (d2[0] * d3[2] - d2[2] * d3[0]) +
                     d1[2] * (d2[0] * d3[1] - d2[1] * d3[0]);
        double volume = std::abs(det) / 6.0;
        
        if (volume < 1e-12) continue; // Skip degenerate tetrahedra
        
        // Shape function gradients in physical coordinates
        // Build Jacobian matrix J = [d1 d2 d3]
        Eigen::Matrix3d J;
        J << d1[0], d2[0], d3[0],
             d1[1], d2[1], d3[1],
             d1[2], d2[2], d3[2];
        
        Eigen::Matrix3d J_inv = J.inverse();
        
        // Gradients of shape functions in reference element
        // grad_hat_N0 = [-1, -1, -1], grad_hat_N1 = [1, 0, 0], etc.
        Eigen::Matrix<double, 3, 4> grad_N;
        grad_N.col(0) = J_inv * Eigen::Vector3d(-1, -1, -1);
        grad_N.col(1) = J_inv * Eigen::Vector3d(1, 0, 0);
        grad_N.col(2) = J_inv * Eigen::Vector3d(0, 1, 0);
        grad_N.col(3) = J_inv * Eigen::Vector3d(0, 0, 1);
        
        // Element stiffness matrix K_e = volume * grad_N^T * grad_N
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                double k_ij = volume * grad_N.col(i).dot(grad_N.col(j));
                triplets.emplace_back(vertex_ids[i], vertex_ids[j], k_ij);
            }
        }
    }
    
    // Assemble global stiffness matrix
    laplacian_matrix_.resize(n_vertices, n_vertices);
    laplacian_matrix_.setFromTriplets(triplets.begin(), triplets.end());
    laplacian_matrix_.makeCompressed();
    
    std::cout << "Assembled 3D Laplacian matrix: " << n_vertices 
              << " x " << n_vertices << " with " 
              << laplacian_matrix_.nonZeros() << " non-zeros" << std::endl;
}

void ReducedOrderedBasis::compute_eigenmodes(int num_modes)
{
    int n = laplacian_matrix_.rows();
    
    if (num_modes > n) {
        std::cout << "Warning: Requested " << num_modes 
                  << " modes but matrix only has " << n 
                  << " dimensions. Using " << n << " modes instead." << std::endl;
        num_modes = n;
    }
    
    std::cout << "Computing " << num_modes << " eigenmodes..." << std::endl;
    
    // Convert sparse matrix to dense for eigenvalue decomposition
    // For large matrices, consider using iterative methods (Spectra/Arpack)
    Eigen::MatrixXf dense_laplacian = Eigen::MatrixXf(laplacian_matrix_);
    
    // Use SelfAdjointEigenSolver for symmetric matrices
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXf> eigensolver(dense_laplacian);
    
    if (eigensolver.info() != Eigen::Success) {
        throw std::runtime_error("Eigenvalue decomposition failed");
    }
    
    // Get eigenvalues and eigenvectors (already sorted in ascending order)
    Eigen::VectorXf all_eigenvalues = eigensolver.eigenvalues();
    Eigen::MatrixXf all_eigenvectors = eigensolver.eigenvectors();
    
    // Store the first num_modes eigenvalues and eigenvectors
    basis.clear();
    eigenvalues.clear();
    
    for (int i = 0; i < num_modes; i++) {
        eigenvalues.push_back(all_eigenvalues(i));
        basis.push_back(all_eigenvectors.col(i));
        
        std::cout << "  Mode " << i << ": eigenvalue = " << all_eigenvalues(i) << std::endl;
    }
    
    std::cout << "Eigenmode computation complete. Stored " << basis.size() 
              << " basis vectors." << std::endl;
}

RUZINO_NAMESPACE_CLOSE_SCOPE
