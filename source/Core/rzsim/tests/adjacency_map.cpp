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
#include <set>
#include <tuple>

using namespace Ruzino;

// ============================================================================
// Surface Mesh Tests (Triangles)
// ============================================================================

// Helper to verify surface adjacency structure
void verify_surface_adjacency(
    const std::vector<unsigned>& adjacency_data,
    const std::vector<unsigned>& offset_buffer,
    size_t expected_vertex_count,
    const std::vector<std::vector<std::pair<unsigned, unsigned>>>&
        expected_pairs)
{
    ASSERT_EQ(offset_buffer.size(), expected_vertex_count);
    ASSERT_EQ(expected_pairs.size(), expected_vertex_count);

    for (size_t v = 0; v < expected_vertex_count; ++v) {
        unsigned offset = offset_buffer[v];
        unsigned count = adjacency_data[offset];

        ASSERT_EQ(count, expected_pairs[v].size())
            << "Vertex " << v << " should have " << expected_pairs[v].size()
            << " opposite edge pairs";

        // Collect actual pairs
        std::vector<std::pair<unsigned, unsigned>> actual_pairs;
        for (unsigned i = 0; i < count; ++i) {
            unsigned a = adjacency_data[offset + 1 + i * 2];
            unsigned b = adjacency_data[offset + 1 + i * 2 + 1];
            actual_pairs.push_back({ a, b });
        }

        // Verify all expected pairs exist (order may vary)
        for (const auto& expected_pair : expected_pairs[v]) {
            bool found = false;
            for (const auto& actual_pair : actual_pairs) {
                if (actual_pair == expected_pair) {
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found)
                << "Vertex " << v << " missing edge pair ("
                << expected_pair.first << ", " << expected_pair.second << ")";
        }
    }
}

TEST(SurfaceAdjacency, SingleTriangle)
{
    // Triangle (0, 1, 2)
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 0.0f, 0.0f),  // 0
        glm::vec3(1.0f, 0.0f, 0.0f),  // 1
        glm::vec3(0.0f, 1.0f, 0.0f)   // 2
    };

    std::vector<int> faceVertexCounts = { 3 };
    std::vector<int> faceVertexIndices = { 0, 1, 2 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto [adjacencyCPU, offsetCPU] = get_surface_adjacency(mesh);

    std::cout << "\n=== Single Triangle Surface Adjacency ===\n";
    for (size_t i = 0; i < offsetCPU.size(); ++i) {
        unsigned offset = offsetCPU[i];
        unsigned count = adjacencyCPU[offset];
        std::cout << "V" << i << " (count=" << count << "): ";
        for (unsigned j = 0; j < count; ++j) {
            unsigned a = adjacencyCPU[offset + 1 + j * 2];
            unsigned b = adjacencyCPU[offset + 1 + j * 2 + 1];
            std::cout << "(" << a << "," << b << ") ";
        }
        std::cout << "\n";
    }

    verify_surface_adjacency(
        adjacencyCPU,
        offsetCPU,
        3,
        {
            { { 1, 2 } },  // v0: opposite edge (v1, v2)
            { { 2, 0 } },  // v1: opposite edge (v2, v0)
            { { 0, 1 } }   // v2: opposite edge (v0, v1)
        });
}

TEST(SurfaceAdjacency, TwoTriangles)
{
    // Two triangles sharing edge: (0,1,2) and (0,2,3)
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

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

    auto [adjacencyCPU, offsetCPU] = get_surface_adjacency(mesh);

    std::cout << "\n=== Two Triangles Surface Adjacency ===\n";
    for (size_t i = 0; i < offsetCPU.size(); ++i) {
        unsigned offset = offsetCPU[i];
        unsigned count = adjacencyCPU[offset];
        std::cout << "V" << i << " (count=" << count << "): ";
        for (unsigned j = 0; j < count; ++j) {
            unsigned a = adjacencyCPU[offset + 1 + j * 2];
            unsigned b = adjacencyCPU[offset + 1 + j * 2 + 1];
            std::cout << "(" << a << "," << b << ") ";
        }
        std::cout << "\n";
    }

    verify_surface_adjacency(
        adjacencyCPU,
        offsetCPU,
        4,
        {
            { { 1, 2 }, { 2, 3 } },  // v0: in 2 triangles (0,1,2) and (0,2,3)
            { { 2, 0 } },  // v1: in triangle (0,1,2), opposite edge (2,0)
            { { 3, 0 }, { 0, 1 } },  // v2: in both triangles
            { { 0, 2 } }  // v3: in triangle (0,2,3), opposite edge (0,2)
        });
}

TEST(SurfaceAdjacency, TriangleFan)
{
    // 4 triangles sharing central vertex 0: (0,1,2), (0,2,3), (0,3,4), (0,4,1)
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    std::vector<glm::vec3> vertices = {
        glm::vec3(0.5f, 0.5f, 0.0f),  // 0 (center)
        glm::vec3(1.0f, 0.0f, 0.0f),  // 1
        glm::vec3(1.0f, 1.0f, 0.0f),  // 2
        glm::vec3(0.0f, 1.0f, 0.0f),  // 3
        glm::vec3(0.0f, 0.0f, 0.0f)   // 4
    };

    std::vector<int> faceVertexCounts = { 3, 3, 3, 3 };
    std::vector<int> faceVertexIndices = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto [adjacencyCPU, offsetCPU] = get_surface_adjacency(mesh);

    std::cout << "\n=== Triangle Fan Surface Adjacency ===\n";
    for (size_t i = 0; i < offsetCPU.size(); ++i) {
        unsigned offset = offsetCPU[i];
        unsigned count = adjacencyCPU[offset];
        std::cout << "V" << i << " (count=" << count << "): ";
        for (unsigned j = 0; j < count; ++j) {
            unsigned a = adjacencyCPU[offset + 1 + j * 2];
            unsigned b = adjacencyCPU[offset + 1 + j * 2 + 1];
            std::cout << "(" << a << "," << b << ") ";
        }
        std::cout << "\n";
    }

    // Vertex 0 is in 4 triangles
    unsigned offset0 = offsetCPU[0];
    unsigned count0 = adjacencyCPU[offset0];
    EXPECT_EQ(count0, 4) << "Central vertex should have 4 opposite edge pairs";
}

// ============================================================================
// Volume Mesh Tests (Tetrahedra)
// ============================================================================

// Helper to verify volume adjacency structure
void verify_volume_adjacency(
    const std::vector<unsigned>& adjacency_data,
    const std::vector<unsigned>& offset_buffer,
    size_t expected_vertex_count,
    const std::vector<std::vector<std::tuple<unsigned, unsigned, unsigned>>>&
        expected_triplets)
{
    ASSERT_EQ(offset_buffer.size(), expected_vertex_count);
    ASSERT_EQ(expected_triplets.size(), expected_vertex_count);

    for (size_t v = 0; v < expected_vertex_count; ++v) {
        unsigned offset = offset_buffer[v];
        unsigned count = adjacency_data[offset];

        ASSERT_EQ(count, expected_triplets[v].size())
            << "Vertex " << v << " should have " << expected_triplets[v].size()
            << " opposite face triplets";

        // Collect actual triplets
        std::vector<std::tuple<unsigned, unsigned, unsigned>> actual_triplets;
        for (unsigned i = 0; i < count; ++i) {
            unsigned a = adjacency_data[offset + 1 + i * 3];
            unsigned b = adjacency_data[offset + 1 + i * 3 + 1];
            unsigned c = adjacency_data[offset + 1 + i * 3 + 2];
            actual_triplets.push_back({ a, b, c });
        }

        // Verify all expected triplets exist
        for (const auto& expected_triplet : expected_triplets[v]) {
            bool found = false;
            for (const auto& actual_triplet : actual_triplets) {
                if (actual_triplet == expected_triplet) {
                    found = true;
                    break;
                }
            }
            auto [a, b, c] = expected_triplet;
            EXPECT_TRUE(found) << "Vertex " << v << " missing face triplet ("
                               << a << ", " << b << ", " << c << ")";
        }
    }
}

TEST(VolumeAdjacency, SingleTetrahedron)
{
    // Single tetrahedron (0, 1, 2, 3)
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 0.0f, 0.0f),  // 0
        glm::vec3(1.0f, 0.0f, 0.0f),  // 1
        glm::vec3(0.5f, 1.0f, 0.0f),  // 2
        glm::vec3(0.5f, 0.5f, 1.0f)   // 3
    };

    std::vector<int> faceVertexCounts = {
        4
    };  // Tetrahedron stored as single element
    std::vector<int> faceVertexIndices = { 0, 1, 2, 3 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto [adjacencyCPU, offsetCPU] = get_volume_adjacency(mesh);

    std::cout << "\n=== Single Tetrahedron Volume Adjacency ===\n";
    for (size_t i = 0; i < offsetCPU.size(); ++i) {
        unsigned offset = offsetCPU[i];
        unsigned count = adjacencyCPU[offset];
        std::cout << "V" << i << " (count=" << count << "): ";
        for (unsigned j = 0; j < count; ++j) {
            unsigned a = adjacencyCPU[offset + 1 + j * 3];
            unsigned b = adjacencyCPU[offset + 1 + j * 3 + 1];
            unsigned c = adjacencyCPU[offset + 1 + j * 3 + 2];
            std::cout << "(" << a << "," << b << "," << c << ") ";
        }
        std::cout << "\n";
    }

    verify_volume_adjacency(
        adjacencyCPU,
        offsetCPU,
        4,
        {
            { { 1, 2, 3 } },  // v0: opposite face (v1, v2, v3)
            { { 0, 3, 2 } },  // v1: opposite face (v0, v3, v2)
            { { 0, 1, 3 } },  // v2: opposite face (v0, v1, v3)
            { { 0, 2, 1 } }   // v3: opposite face (v0, v2, v1)
        });
}

TEST(VolumeAdjacency, TwoTetrahedra)
{
    // Two tets sharing a face: (0,1,2,3) and (0,2,1,4)
    Geometry mesh = Geometry::CreateMesh();
    auto meshComp = mesh.get_component<MeshComponent>();

    std::vector<glm::vec3> vertices = {
        glm::vec3(0.0f, 0.0f, 0.0f),  // 0
        glm::vec3(1.0f, 0.0f, 0.0f),  // 1
        glm::vec3(0.5f, 1.0f, 0.0f),  // 2
        glm::vec3(0.5f, 0.5f, 1.0f),  // 3
        glm::vec3(0.5f, 0.5f, -1.0f)  // 4
    };

    std::vector<int> faceVertexCounts = { 4, 4 };
    std::vector<int> faceVertexIndices = { 0, 1, 2, 3, 0, 2, 1, 4 };

    meshComp->set_vertices(vertices);
    meshComp->set_face_vertex_counts(faceVertexCounts);
    meshComp->set_face_vertex_indices(faceVertexIndices);

    auto [adjacencyCPU, offsetCPU] = get_volume_adjacency(mesh);

    std::cout << "\n=== Two Tetrahedra Volume Adjacency ===\n";
    for (size_t i = 0; i < offsetCPU.size(); ++i) {
        unsigned offset = offsetCPU[i];
        unsigned count = adjacencyCPU[offset];
        std::cout << "V" << i << " (count=" << count << "): ";
        for (unsigned j = 0; j < count; ++j) {
            unsigned a = adjacencyCPU[offset + 1 + j * 3];
            unsigned b = adjacencyCPU[offset + 1 + j * 3 + 1];
            unsigned c = adjacencyCPU[offset + 1 + j * 3 + 2];
            std::cout << "(" << a << "," << b << "," << c << ") ";
        }
        std::cout << "\n";
    }

    // Vertices 0, 1, 2 are in 2 tets each
    unsigned offset0 = offsetCPU[0];
    unsigned count0 = adjacencyCPU[offset0];
    EXPECT_EQ(count0, 2) << "Vertex 0 should be in 2 tetrahedra";
}

int main(int argc, char** argv)
{
    Ruzino::cuda::cuda_init();

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

    Ruzino::cuda::cuda_shutdown();

    return result;
}
