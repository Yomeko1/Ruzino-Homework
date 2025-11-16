#include "GCore/Components/CurveComponent.h"
#include "GCore/Components/MeshComponent.h"
#include "geom_node_base.h"

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(tree_simple_branch)
{
    b.add_input<float>("Length").min(0.1f).max(10.0f).default_val(1.0f);
    b.add_input<float>("Radius").min(0.01f).max(1.0f).default_val(0.1f);
    b.add_input<int>("Subdivisions").min(1).max(20).default_val(5);
    
    b.add_output<Geometry>("Branch Curve");
}

NODE_EXECUTION_FUNCTION(tree_simple_branch)
{
    float length = params.get_input<float>("Length");
    float radius = params.get_input<float>("Radius");
    int subdivisions = params.get_input<int>("Subdivisions");
    
    // Create a simple upward-growing branch curve
    Geometry curve_geom;
    std::shared_ptr<CurveComponent> curve = 
        std::make_shared<CurveComponent>(&curve_geom);
    curve_geom.attach_component(curve);
    
    std::vector<glm::vec3> vertices;
    std::vector<float> widths;
    
    // Create vertices along Y axis
    for (int i = 0; i <= subdivisions; ++i) {
        float t = static_cast<float>(i) / subdivisions;
        float y = t * length;
        
        // Slight taper from base to tip
        float w = radius * (1.0f - 0.5f * t);
        
        vertices.push_back(glm::vec3(0.0f, y, 0.0f));
        widths.push_back(w);
    }
    
    curve->set_vertices(vertices);
    curve->set_width(widths);
    curve->set_vert_count({static_cast<int>(vertices.size())});
    curve->set_periodic(false);
    
    params.set_output("Branch Curve", curve_geom);
    return true;
}

NODE_DECLARATION_UI(tree_simple_branch);

NODE_DEF_CLOSE_SCOPE
