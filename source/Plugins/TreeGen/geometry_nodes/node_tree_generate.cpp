#include "GCore/Components/CurveComponent.h"
#include "GCore/Components/MeshComponent.h"
#include "geom_node_base.h"
#include "TreeGen/TreeGrowth.h"
#include "TreeGen/TreeParameters.h"
#include "TreeGen/TreeStructure.h"

using namespace TreeGen;

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(tree_generate)
{
    // Tree parameters
    b.add_input<int>("Growth Years").min(1).max(50).default_val(10);
    b.add_input<int>("Random Seed").min(0).max(10000).default_val(42);
    
    // Geometric parameters
    b.add_input<float>("Apical Angle Variance").min(0.0f).max(90.0f).default_val(38.0f);
    b.add_input<int>("Lateral Buds").min(1).max(10).default_val(4);
    b.add_input<float>("Branch Angle").min(10.0f).max(90.0f).default_val(45.0f);
    b.add_input<float>("Growth Rate").min(0.5f).max(10.0f).default_val(3.0f);
    b.add_input<float>("Internode Length").min(0.05f).max(2.0f).default_val(0.3f);
    b.add_input<float>("Apical Control").min(0.5f).max(5.0f).default_val(2.0f);
    
    // Bud fate parameters
    b.add_input<float>("Apical Dominance").min(0.0f).max(5.0f).default_val(1.0f);
    b.add_input<float>("Light Factor").min(0.0f).max(1.0f).default_val(0.6f);
    
    // Environmental parameters
    b.add_input<float>("Phototropism").min(0.0f).max(1.0f).default_val(0.3f);
    b.add_input<float>("Gravitropism").min(0.0f).max(1.0f).default_val(0.2f);
    
    // Leaf parameters
    b.add_input<bool>("Generate Leaves").default_val(true);
    b.add_input<int>("Leaves Per Internode").min(0).max(10).default_val(3);
    b.add_input<float>("Leaf Size").min(0.01f).max(1.0f).default_val(0.15f);
    
    // Output
    b.add_output<Geometry>("Tree Branches");
    b.add_output<Geometry>("Leaves");
}

NODE_EXECUTION_FUNCTION(tree_generate)
{
    // Get parameters from inputs
    TreeParameters tree_params;
    
    tree_params.growth_time = params.get_input<int>("Growth Years");
    tree_params.random_seed = params.get_input<int>("Random Seed");
    tree_params.apical_angle_variance = params.get_input<float>("Apical Angle Variance");
    tree_params.num_lateral_buds = params.get_input<int>("Lateral Buds");
    tree_params.branching_angle_mean = params.get_input<float>("Branch Angle");
    tree_params.growth_rate = params.get_input<float>("Growth Rate");
    tree_params.internode_base_length = params.get_input<float>("Internode Length");
    tree_params.apical_control = params.get_input<float>("Apical Control");
    tree_params.apical_dominance_base = params.get_input<float>("Apical Dominance");
    tree_params.lateral_light_factor = params.get_input<float>("Light Factor");
    tree_params.phototropism = params.get_input<float>("Phototropism");
    tree_params.gravitropism = params.get_input<float>("Gravitropism");
    tree_params.generate_leaves = params.get_input<bool>("Generate Leaves");
    tree_params.leaves_per_internode = params.get_input<int>("Leaves Per Internode");
    tree_params.leaf_size_base = params.get_input<float>("Leaf Size");
    
    // Create tree growth system
    TreeGrowth growth(tree_params);
    
    // Initialize and grow tree
    TreeStructure tree = growth.initialize_tree();
    growth.grow_tree(tree, tree_params.growth_time);
    
    // Convert tree structure to curve geometry for visualization
    Geometry curve_geom = Geometry::CreateCurve();
    auto curve = curve_geom.get_component<CurveComponent>();
    
    std::vector<glm::vec3> curve_vertices;
    std::vector<int> curve_counts;
    std::vector<float> curve_widths;
    
    // Convert each branch to a curve segment
    for (const auto& branch : tree.all_branches) {
        if (!branch) continue;
        
        // Add branch start and end points
        curve_vertices.push_back(branch->start_position);
        curve_vertices.push_back(branch->end_position);
        curve_counts.push_back(2);  // Two points per branch segment
        
        // Add radius information as width
        curve_widths.push_back(branch->radius);
        curve_widths.push_back(branch->radius);
    }
    
    curve->set_vertices(curve_vertices);
    curve->set_curve_counts(curve_counts);
    curve->set_widths(curve_widths);
    curve->set_periodic(false);
    
    params.set_output("Tree Branches", curve_geom);
    
    // Generate leaf geometry as mesh
    Geometry leaf_geom = Geometry::CreateMesh();
    auto leaf_mesh = leaf_geom.get_component<MeshComponent>();
    
    std::vector<glm::vec3> leaf_vertices;
    std::vector<int> leaf_face_counts;
    std::vector<int> leaf_face_indices;
    std::vector<glm::vec3> leaf_normals;
    
    // Generate all leaves
    for (const auto& branch : tree.all_branches) {
        if (!branch) continue;
        
        for (const auto& leaf : branch->leaves) {
            if (!leaf) continue;
            
            int base_idx = leaf_vertices.size();
            
            // Create diamond-shaped leaf
            float hw = leaf->size * 0.5f;  // half width
            float hh = leaf->size * 0.7f;  // half height (slightly elongated)
            
            // Calculate leaf coordinate system
            glm::vec3 right = leaf->tangent;
            glm::vec3 up = leaf->normal;
            glm::vec3 forward = glm::normalize(glm::cross(right, up));
            
            // 4 vertices in diamond shape
            glm::vec3 v0 = leaf->position + up * hh;                    // top
            glm::vec3 v1 = leaf->position + right * hw;                 // right
            glm::vec3 v2 = leaf->position - up * hh;                    // bottom
            glm::vec3 v3 = leaf->position - right * hw;                 // left
            
            leaf_vertices.push_back(v0);
            leaf_vertices.push_back(v1);
            leaf_vertices.push_back(v2);
            leaf_vertices.push_back(v3);
            
            // Two triangular faces (front)
            leaf_face_indices.push_back(base_idx + 0);
            leaf_face_indices.push_back(base_idx + 1);
            leaf_face_indices.push_back(base_idx + 2);
            leaf_face_counts.push_back(3);
            
            leaf_face_indices.push_back(base_idx + 0);
            leaf_face_indices.push_back(base_idx + 2);
            leaf_face_indices.push_back(base_idx + 3);
            leaf_face_counts.push_back(3);
            
            // Normals (all point in leaf normal direction)
            for (int i = 0; i < 4; ++i) {
                leaf_normals.push_back(forward);
            }
        }
    }
    
    leaf_mesh->set_vertices(leaf_vertices);
    leaf_mesh->set_face_vertex_counts(leaf_face_counts);
    leaf_mesh->set_face_vertex_indices(leaf_face_indices);
    leaf_mesh->set_normals(leaf_normals);
    
    params.set_output("Leaves", leaf_geom);
    
    return true;
}

NODE_DECLARATION_UI(tree_generate);

NODE_DEF_CLOSE_SCOPE
