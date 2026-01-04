#include <GCore/Components/MeshComponent.h>
#include <GCore/GOP.h>
#include <gtest/gtest.h>
#include <rzsim/rzsim.h>


// Forward declarations for CUDA initialization
namespace Ruzino {
namespace cuda {
    extern int cuda_init();
    extern int cuda_shutdown();
}  // namespace cuda
}  // namespace Ruzino
#include <iostream>

using namespace Ruzino;

TEST(AdjacencyMap, SimpleTriangle)
{
    // Create a simple triangle mesh
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Triangle vertices: 0, 1, 2
    std::vector<glm::vec3> vertices = { glm::vec3(0.0f, 0.0f, 0.0f),
                                        glm::vec3(1.0f, 0.0f, 0.0f),
                                        glm::vec3(0.0f, 1.0f, 0.0f) };

    std::vector<int> faceVertexCounts = { 3 };
    std::vector<int> faceVertexIndices = { 0, 1, 2 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    // Test GPU version
    auto adjacencyGPU = get_adjcency_map_gpu(mesh);
    ASSERT_NE(adjacencyGPU, nullptr);

    // Test CPU version (just downloads from GPU)
    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for triangle:\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // For a single triangle [0, 1, 2]:
    // - Edge 0-1: 0->1, 1->0
    // - Edge 1-2: 1->2, 2->1
    // - Edge 2-0: 2->0, 0->2
    // Vertex 0 connects to: 1, 2
    // Vertex 1 connects to: 0, 2
    // Vertex 2 connects to: 1, 0
    std::vector<unsigned> expected = { 2, 1, 2, 2, 0, 2, 2, 1, 0 };
    ASSERT_EQ(adjacencyCPU.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(adjacencyCPU[i], expected[i]) << "Mismatch at index " << i;
    }
}

TEST(AdjacencyMap, Quad)
{
    // Create a quad mesh
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Quad vertices: 0--1
    //                |  |
    //                3--2
    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 1.0f, 0.0f),  // 0
        glm::vec3(1.0f, 1.0f, 0.0f),  // 1
        glm::vec3(1.0f, 0.0f, 0.0f),  // 2
        glm::vec3(0.0f, 0.0f, 0.0f)   // 3
    };

    std::vector<int> faceVertexCounts = { 4 };
    std::vector<int> faceVertexIndices = { 0, 1, 2, 3 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for quad:\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // For quad [0, 1, 2, 3]:
    // - Vertex 0 connects to 1 and 3
    // - Vertex 1 connects to 0 and 2
    // - Vertex 2 connects to 1 and 3
    // - Vertex 3 connects to 2 and 0
    std::vector<unsigned> expected = { 2, 1, 3, 2, 0, 2, 2, 1, 3, 2, 2, 0 };
    ASSERT_EQ(adjacencyCPU.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(adjacencyCPU[i], expected[i]) << "Mismatch at index " << i;
    }
}

TEST(AdjacencyMap, TwoTriangles)
{
    // Create two triangles sharing an edge
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Vertices:  0
    //           /|\
    //          / | \
    //         1--2--3
    // Faces: [0,1,2] and [0,2,3]
    std::vector<glm::vec3> vertices = {
        glm::vec3(0.5f, 1.0f, 0.0f),  // 0
        glm::vec3(0.0f, 0.0f, 0.0f),  // 1
        glm::vec3(0.5f, 0.0f, 0.0f),  // 2
        glm::vec3(1.0f, 0.0f, 0.0f)   // 3
    };

    std::vector<int> faceVertexCounts = { 3, 3 };
    std::vector<int> faceVertexIndices = { 0, 1, 2, 0, 2, 3 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for two triangles:\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // From face [0,1,2]: edges 0-1, 1-2, 2-0
    // From face [0,2,3]: edges 0-2, 2-3, 3-0
    // V0: from (0,1), (1,0), (0,2), (2,0), (3,0), (0,3) but no duplicates in
    // count = 4 neighbors Note: The order of neighbors might vary due to atomic
    // operations being unordered V0: neighbors could be [1, 2, 2, 3] or any
    // permutation with these values V1: [0, 2] V2: [0, 3, 1, 0] or similar
    // (order may vary) V3: [2, 0] Total size: (4+1) + (2+1) + (4+1) + (2+1) = 5
    // + 3 + 5 + 3 = 16
    std::vector<unsigned> expected = { 4, 1, 2, 2, 3, 2, 0, 2,
                                       4, 0, 3, 1, 0, 2, 2, 0 };
    ASSERT_EQ(adjacencyCPU.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(adjacencyCPU[i], expected[i]) << "Mismatch at index " << i;
    }
}

int main(int argc, char** argv)
{
    // Initialize CUDA - use forward declaration to avoid namespace conflicts
    Ruzino::cuda::cuda_init();

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    // Cleanup
    Ruzino::cuda::cuda_shutdown();

    return result;
}

// Complex test: Pyramid (4 triangular faces + 1 quad base)
TEST(AdjacencyMap, Pyramid)
{
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Pyramid vertices:
    //       4 (apex)
    //      /|\\ 
    //     / | \\
    //    0--1--2
    //    |  |  |
    //    3--+--+
    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 1.0f, 0.0f),   // 0
        glm::vec3(1.0f, 1.0f, 0.0f),   // 1
        glm::vec3(1.0f, 0.0f, 0.0f),   // 2
        glm::vec3(0.0f, 0.0f, 0.0f),   // 3
        glm::vec3(0.5f, 0.5f, 1.0f)    // 4 (apex)
    };

    // 4 triangular faces + 1 quad base
    std::vector<int> faceVertexCounts = { 3, 3, 3, 3, 4 };
    std::vector<int> faceVertexIndices = {
        0, 4, 1,  // Face 0: front triangle
        1, 4, 2,  // Face 1: right triangle
        2, 4, 3,  // Face 2: back triangle
        3, 4, 0,  // Face 3: left triangle
        0, 1, 2, 3 // Face 4: quad base
    };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for pyramid:\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // Verify total size is reasonable
    // V0: connected to 4,1 (face0) + 3,4 (face3) + 1,3 (face4) = 6 neighbors
    // V1: connected to 0,4 (face0) + 4,2 (face1) + 0,2 (face4) = 6 neighbors
    // V2: connected to 1,4 (face1) + 4,3 (face2) + 1,3 (face4) = 6 neighbors
    // V3: connected to 2,4 (face2) + 0,4 (face3) + 2,0 (face4) = 6 neighbors
    // V4: connected to 0,1 (face0) + 1,2 (face1) + 2,3 (face2) + 3,0 (face3) = 8 neighbors
    unsigned expected_total_size = (6+1) + (6+1) + (6+1) + (6+1) + (8+1);
    EXPECT_EQ(adjacencyCPU.size(), expected_total_size);
}

// Complex test: Two separate quads (non-connected mesh)
TEST(AdjacencyMap, TwoSeparateQuads)
{
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Two separate quads
    std::vector<glm::vec3> vertices = {
        // First quad
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        // Second quad
        glm::vec3(2.0f, 1.0f, 0.0f),
        glm::vec3(3.0f, 1.0f, 0.0f),
        glm::vec3(3.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 0.0f, 0.0f)
    };

    std::vector<int> faceVertexCounts = { 4, 4 };
    std::vector<int> faceVertexIndices = {
        0, 1, 2, 3,  // First quad
        4, 5, 6, 7   // Second quad
    };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for two separate quads:\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // Each vertex has exactly 2 neighbors
    // Total: 8 vertices * (2+1) = 24 elements
    EXPECT_EQ(adjacencyCPU.size(), 24);
}

// Complex test: Hexagon (6 triangles sharing a central vertex)
TEST(AdjacencyMap, Hexagon)
{
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Hexagon with center vertex
    std::vector<glm::vec3> vertices = {
        glm::vec3(0.5f, 0.5f, 0.0f),   // 0 (center)
        glm::vec3(1.0f, 0.5f, 0.0f),   // 1
        glm::vec3(0.75f, 1.0f, 0.0f),  // 2
        glm::vec3(0.0f, 1.0f, 0.0f),   // 3
        glm::vec3(-0.25f, 0.5f, 0.0f), // 4
        glm::vec3(0.0f, 0.0f, 0.0f),   // 5
        glm::vec3(0.75f, 0.0f, 0.0f)   // 6
    };

    // 6 triangles radiating from center
    std::vector<int> faceVertexCounts = { 3, 3, 3, 3, 3, 3 };
    std::vector<int> faceVertexIndices = {
        0, 1, 2,  // Triangle 0
        0, 2, 3,  // Triangle 1
        0, 3, 4,  // Triangle 2
        0, 4, 5,  // Triangle 3
        0, 5, 6,  // Triangle 4
        0, 6, 1   // Triangle 5
    };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for hexagon:\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // Center vertex (0) has 12 neighbors (2 per triangle * 6 triangles)
    // Boundary vertices (1-6) have 4 neighbors each (2 from adjacent triangles + 1 to center twice)
    // Actually: V0 = 12, others = 4 each
    // Total: (12+1) + 6*(4+1) = 13 + 30 = 43
    EXPECT_EQ(adjacencyCPU.size(), 43);
}

// Complex test: Cube (6 quads / 12 triangles)
TEST(AdjacencyMap, CubeQuads)
{
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Cube vertices
    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 0.0f, 0.0f),  // 0
        glm::vec3(1.0f, 0.0f, 0.0f),  // 1
        glm::vec3(1.0f, 1.0f, 0.0f),  // 2
        glm::vec3(0.0f, 1.0f, 0.0f),  // 3
        glm::vec3(0.0f, 0.0f, 1.0f),  // 4
        glm::vec3(1.0f, 0.0f, 1.0f),  // 5
        glm::vec3(1.0f, 1.0f, 1.0f),  // 6
        glm::vec3(0.0f, 1.0f, 1.0f)   // 7
    };

    // 6 quad faces
    std::vector<int> faceVertexCounts = { 4, 4, 4, 4, 4, 4 };
    std::vector<int> faceVertexIndices = {
        0, 1, 2, 3,  // Bottom
        4, 7, 6, 5,  // Top
        0, 4, 5, 1,  // Front
        2, 6, 7, 3,  // Back
        0, 3, 7, 4,  // Left
        1, 5, 6, 2   // Right
    };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for cube (quads):\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // Corner vertices have 6 neighbors each (shared by 3 faces)
    // Each corner appears in 3 faces, getting 6 edge connections
    // Total: 8 vertices * (6+1) = 56
    EXPECT_EQ(adjacencyCPU.size(), 56);
}

// Complex test: Pentagonal pyramid
TEST(AdjacencyMap, PentagonalPyramid)
{
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Pentagonal pyramid (5 triangle sides + 1 pentagon base)
    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 0.0f, 0.0f),    // 0 (apex)
        glm::vec3(1.0f, 0.0f, 0.0f),    // 1
        glm::vec3(0.31f, 0.95f, 0.0f),  // 2
        glm::vec3(-0.81f, 0.59f, 0.0f), // 3
        glm::vec3(-0.81f, -0.59f, 0.0f),// 4
        glm::vec3(0.31f, -0.95f, 0.0f), // 5
        glm::vec3(0.3f, 0.3f, 1.0f)     // 6 (apex)
    };

    std::vector<int> faceVertexCounts = { 3, 3, 3, 3, 3, 5 };
    std::vector<int> faceVertexIndices = {
        1, 6, 2,     // Triangle 0
        2, 6, 3,     // Triangle 1
        3, 6, 4,     // Triangle 2
        4, 6, 5,     // Triangle 3
        5, 6, 1,     // Triangle 4
        1, 2, 3, 4, 5// Pentagon base
    };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto adjacencyCPU = get_adjcency_map(mesh);
    ASSERT_GT(adjacencyCPU.size(), 0);

    std::cout << "Adjacency map for pentagonal pyramid:\n";
    for (size_t i = 0; i < adjacencyCPU.size(); i++) {
        std::cout << adjacencyCPU[i] << " ";
    }
    std::cout << std::endl;

    // Apex (0) is unused, V6 (top) has 10 neighbors, V1-5 have 6-8 each
    // This is a sanity check
    EXPECT_GT(adjacencyCPU.size(), 30);
}
// Test offset buffer for random access
TEST(AdjacencyMap, OffsetBufferRandomAccess)
{
    // Create a simple quad mesh for testing offsets
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    // Quad: 0--1
    //       |  |
    //       3--2
    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 1.0f, 0.0f),  // 0
        glm::vec3(1.0f, 1.0f, 0.0f),  // 1
        glm::vec3(1.0f, 0.0f, 0.0f),  // 2
        glm::vec3(0.0f, 0.0f, 0.0f)   // 3
    };

    std::vector<int> faceVertexCounts = { 4 };
    std::vector<int> faceVertexIndices = { 0, 1, 2, 3 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    // Get adjacency map with offsets
    auto result = get_adjcency_map_with_offsets_gpu(mesh);
    
    // Download both buffers
    auto adjacency_cpu = result.adjacency_list->get_host_vector<unsigned>();
    auto offset_cpu = result.offset_buffer->get_host_vector<unsigned>();

    std::cout << "Adjacency buffer for offset test:\n";
    for (size_t i = 0; i < adjacency_cpu.size(); i++) {
        std::cout << adjacency_cpu[i] << " ";
    }
    std::cout << "\nOffset buffer:\n";
    for (size_t i = 0; i < offset_cpu.size(); i++) {
        std::cout << "V" << i << "@" << offset_cpu[i] << " ";
    }
    std::cout << std::endl;

    // Verify offsets are in increasing order and point to valid positions
    ASSERT_EQ(offset_cpu.size(), 4);  // 4 vertices
    for (size_t i = 0; i < offset_cpu.size(); i++) {
        EXPECT_LT(offset_cpu[i], adjacency_cpu.size()) 
            << "Offset for vertex " << i << " is out of bounds";
        if (i > 0) {
            // Offsets should be monotonically increasing
            EXPECT_LE(offset_cpu[i-1], offset_cpu[i])
                << "Offsets are not monotonically increasing";
        }
    }

    // Verify we can read counts from each offset
    for (size_t i = 0; i < offset_cpu.size(); i++) {
        unsigned offset = offset_cpu[i];
        unsigned count = adjacency_cpu[offset];
        EXPECT_GT(count, 0) << "Vertex " << i << " has 0 neighbors";
        // Verify count fits within the buffer
        EXPECT_LT(offset + count, adjacency_cpu.size())
            << "Vertex " << i << " neighbors exceed buffer bounds";
    }
}