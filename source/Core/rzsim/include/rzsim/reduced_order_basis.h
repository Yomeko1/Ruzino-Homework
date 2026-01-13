#pragma once
#include <rzsim/api.h>

#include <Eigen/Eigen>
#include <Eigen/Sparse>
#include <vector>
#include <memory>


RUZINO_NAMESPACE_OPEN_SCOPE 

class Geometry;

struct RZSIM_API ReducedOrderedBasis {
    // Default constructor
    ReducedOrderedBasis() = default;
    
    // dimension: 2 for surface mesh (triangles/quads), 3 for volume mesh (tetrahedra)
    // use_libigl: if true, use libigl's cotmatrix; otherwise use custom implementation
    ReducedOrderedBasis(const Geometry& g, int num_modes = 10, int dimension = 2, bool use_libigl = false);

    // Compute eigenvalue decomposition and store the first N eigenvectors
    void compute_eigenmodes(int num_modes);

    std::vector<Eigen::VectorXf> basis;
    std::vector<float> eigenvalues;
    Eigen::SparseMatrix<float> laplacian_matrix_;

private:
    void assemble_laplacian_2d(void* mesh);
    void assemble_laplacian_3d(void* mesh);
    void assemble_laplacian_2d_libigl(void* mesh);
    void assemble_laplacian_3d_libigl(void* mesh);
};

RUZINO_NAMESPACE_CLOSE_SCOPE