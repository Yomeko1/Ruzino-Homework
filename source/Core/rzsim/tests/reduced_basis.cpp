#include <GCore/Components/MeshComponent.h>
#include <GCore/GOP.h>
#include <GCore/create_geom.h>
#include <GCore/algorithms/tetgen_algorithm.h>
#include <GCore/util_openmesh_bind.h>
#include <gtest/gtest.h>
#include <rzsim/reduced_order_basis.h>

#include <iostream>
#include <iomanip>

using namespace Ruzino;

// Helper to print eigenvalues and some eigenvector statistics
void print_eigenmode_info(const ReducedOrderedBasis& rob, int max_modes = 10)
{
    std::cout << "\n=== Reduced Order Basis Information ===\n";
    std::cout << "Number of modes: " << rob.basis.size() << std::endl;
    std::cout << "Laplacian matrix size: " << rob.laplacian_matrix_.rows() 
              << " x " << rob.laplacian_matrix_.cols() << std::endl;
    std::cout << "Laplacian non-zeros: " << rob.laplacian_matrix_.nonZeros() << std::endl;
    
    int num_to_print = std::min(max_modes, static_cast<int>(rob.eigenvalues.size()));
    
    std::cout << "\nFirst " << num_to_print << " eigenvalues:\n";
    for (int i = 0; i < num_to_print; i++) {
        std::cout << "  λ[" << i << "] = " << std::setprecision(8) << rob.eigenvalues[i];
        
        // Print some statistics about the eigenvector
        if (i < rob.basis.size()) {
            float min_val = rob.basis[i].minCoeff();
            float max_val = rob.basis[i].maxCoeff();
            float norm = rob.basis[i].norm();
            std::cout << "  (min: " << min_val << ", max: " << max_val 
                      << ", norm: " << norm << ")";
        }
        std::cout << std::endl;
    }
}

// Test with a simple 2D grid mesh
TEST(ReducedOrderBasis, GridMesh)
{
    // Create a 2x2 grid
    Geometry mesh = create_grid(2, 1.0f);

    // Create reduced order basis
    ReducedOrderedBasis rob(mesh, 5);

    // Verify we got the expected number of modes
    EXPECT_GT(rob.basis.size(), 0);
    EXPECT_EQ(rob.eigenvalues.size(), rob.basis.size());
    
    // Verify matrix dimensions
    EXPECT_GT(rob.laplacian_matrix_.rows(), 0);
    EXPECT_GT(rob.laplacian_matrix_.cols(), 0);

    print_eigenmode_info(rob);
    
    // Eigenvalues should be non-negative for Laplacian
    for (size_t i = 0; i < rob.eigenvalues.size(); i++) {
        EXPECT_GE(rob.eigenvalues[i], -1e-6) << "Eigenvalue " << i << " is negative";
    }
}

// Test with a circle mesh
TEST(ReducedOrderBasis, CircleMesh)
{
    // Create a circle mesh
    Geometry mesh = create_circle_face(16, 1.0f);

    // Create reduced order basis with 8 modes
    ReducedOrderedBasis rob(mesh, 8);

    EXPECT_GT(rob.basis.size(), 0);
    EXPECT_EQ(rob.eigenvalues.size(), rob.basis.size());
    EXPECT_GT(rob.laplacian_matrix_.rows(), 0);

    print_eigenmode_info(rob);
    
    // Check eigenvalues are sorted in ascending order
    for (size_t i = 1; i < rob.eigenvalues.size(); i++) {
        EXPECT_LE(rob.eigenvalues[i-1], rob.eigenvalues[i]) 
            << "Eigenvalues should be sorted in ascending order";
    }
}

// Test with larger grid mesh
TEST(ReducedOrderBasis, LargerGridMesh)
{
    // Create a 5x5 grid mesh
    Geometry mesh = create_grid(5, 2.0f);

    // Create reduced order basis with 15 modes
    ReducedOrderedBasis rob(mesh, 15);

    EXPECT_GT(rob.basis.size(), 0);
    EXPECT_EQ(rob.eigenvalues.size(), rob.basis.size());
    EXPECT_GT(rob.laplacian_matrix_.rows(), 0);

    print_eigenmode_info(rob, 15);
    
    // Check eigenvalues are sorted in ascending order and non-negative
    for (size_t i = 0; i < rob.eigenvalues.size(); i++) {
        EXPECT_GE(rob.eigenvalues[i], -1e-6) << "Eigenvalue " << i << " is negative";
        if (i > 0) {
            EXPECT_LE(rob.eigenvalues[i-1], rob.eigenvalues[i]) 
                << "Eigenvalues should be sorted";
        }
    }
}

// Test with 3D tetrahedral mesh
TEST(ReducedOrderBasis, TetrahedralMesh)
{
    // Create a sphere (already triangulated)
    Geometry sphere = create_ico_sphere(2, 1.0f);
    
    // Tetrahedralize it
    geom_algorithm::TetgenParams params;
    params.max_volume = 0.05f;
    params.quality_ratio = 2.0f;
    
    Geometry tet_mesh = geom_algorithm::tetrahedralize(sphere, params);
    
    // Create reduced order basis
    ReducedOrderedBasis rob(tet_mesh, 10);

    EXPECT_GT(rob.basis.size(), 0);
    EXPECT_EQ(rob.basis.size(), rob.eigenvalues.size());
    EXPECT_GT(rob.laplacian_matrix_.rows(), 0);

    print_eigenmode_info(rob);
    
    // Verify eigenvalues are non-negative and sorted
    for (size_t i = 0; i < rob.eigenvalues.size(); i++) {
        EXPECT_GE(rob.eigenvalues[i], -1e-6) << "Eigenvalue " << i << " is negative";
        if (i > 0) {
            EXPECT_LE(rob.eigenvalues[i-1], rob.eigenvalues[i]) 
                << "Eigenvalues should be sorted";
        }
    }
    
    std::cout << "Tetrahedral mesh vertices: " << rob.laplacian_matrix_.rows() << std::endl;
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
