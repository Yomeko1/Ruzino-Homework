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
    
    // Output
    b.add_output<Geometry>("Tree Branches");
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
    return true;
}

NODE_DECLARATION_UI(tree_generate);

NODE_DEF_CLOSE_SCOPE
