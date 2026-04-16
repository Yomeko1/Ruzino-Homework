#include <Eigen/Sparse>

#include "GCore/Components/MeshComponent.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"

/*
** @brief HW4_TutteParameterization
**
** This file contains two nodes whose primary function is to map the boundary of
*a mesh to a plain
** convex closed curve (circle of square), setting the stage for subsequent
*Laplacian equation
** solution and mesh parameterization tasks.
**
** Key to this node's implementation is the adept manipulation of half-edge data
*structures
** to identify and modify the boundary of the mesh.
**
** Task Overview:
** - The two execution functions (node_map_boundary_to_square_exec,
** node_map_boundary_to_circle_exec) require an update to accurately map the
*mesh boundary to a and
** circles. This entails identifying the boundary edges, evenly distributing
*boundary vertices along
** the square's perimeter, and ensuring the internal vertices' positions remain
*unchanged.
** - A focus on half-edge data structures to efficiently traverse and modify
*mesh boundaries.
*/

NODE_DEF_OPEN_SCOPE

/*
** HW4_TODO: Node to map the mesh boundary to a circle.
*/

NODE_DECLARATION_FUNCTION(hw5_circle_boundary_mapping)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");
    // Output-1: Processed 3D mesh whose boundary is mapped to a square and the
    // interior vertices remains the same
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_circle_boundary_mapping)
{
    auto input = params.get_input<Geometry>("Input");

    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Boundary Mapping: Need Geometry Input.");
    }

    auto halfedge_mesh = operand_to_openmesh(&input);
    int n_vertices = halfedge_mesh->n_vertices();

    std::vector<int> boundary_edges(n_vertices, -1);
    for (const auto& halfedge_handle : halfedge_mesh->halfedges()) {
        if (halfedge_handle.is_boundary()) {
            int from_idx = halfedge_handle.from().idx();
            int to_idx = halfedge_handle.to().idx();
            boundary_edges[from_idx] = to_idx;
        }
    }

    int start = -1;
    for (int i = 0; i < n_vertices; i++) {
        if (boundary_edges[i] >= 0) {
            start = i;
            break;
        }
    }

    if (start == -1) {
        throw std::runtime_error("No boundary found.");
    }

    std::vector<int> boundary_loop;
    int curr = start;
    do {
        boundary_loop.push_back(curr);
        curr = boundary_edges[curr];
    } while (curr != start && curr != -1);

    if (boundary_loop.empty()) {
        throw std::runtime_error("Failed to extract boundary loop.");
    }

    int n_boundary = boundary_loop.size();

    std::vector<double> edge_lengths(n_boundary);
    double total_length = 0;
    for (int i = 0; i < n_boundary; i++) {
        const auto& p1 = halfedge_mesh->point(halfedge_mesh->vertex_handle(boundary_loop[i]));
        const auto& p2 = halfedge_mesh->point(halfedge_mesh->vertex_handle(boundary_loop[(i + 1) % n_boundary]));
        edge_lengths[i] = (p2 - p1).length();
        total_length += edge_lengths[i];
    }

    double accumulated = 0;
    for (int i = 0; i < n_boundary; i++) {
        double ratio = (accumulated + edge_lengths[i] * 0.5) / total_length;
        double theta = 2.0 * M_PI * ratio;

        double x = 0.5 + 0.4 * cos(theta);
        double y = 0.5 + 0.4 * sin(theta);

        auto vh = halfedge_mesh->vertex_handle(boundary_loop[i]);
        halfedge_mesh->point(vh)[0] = x;
        halfedge_mesh->point(vh)[1] = y;
        halfedge_mesh->point(vh)[2] = 0;

        accumulated += edge_lengths[i];
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

/*
** HW4_TODO: Node to map the mesh boundary to a square.
*/

NODE_DECLARATION_FUNCTION(hw5_square_boundary_mapping)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");

    // Output-1: Processed 3D mesh whose boundary is mapped to a square and the
    // interior vertices remains the same
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_square_boundary_mapping)
{
    auto input = params.get_input<Geometry>("Input");

    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Input does not contain a mesh");
    }

    auto halfedge_mesh = operand_to_openmesh(&input);
    int n_vertices = halfedge_mesh->n_vertices();

    std::vector<int> boundary_edges(n_vertices, -1);
    for (const auto& halfedge_handle : halfedge_mesh->halfedges()) {
        if (halfedge_handle.is_boundary()) {
            int from_idx = halfedge_handle.from().idx();
            int to_idx = halfedge_handle.to().idx();
            boundary_edges[from_idx] = to_idx;
        }
    }

    int start = -1;
    for (int i = 0; i < n_vertices; i++) {
        if (boundary_edges[i] >= 0) {
            start = i;
            break;
        }
    }

    if (start == -1) {
        throw std::runtime_error("No boundary found.");
    }

    std::vector<int> boundary_loop;
    int curr = start;
    do {
        boundary_loop.push_back(curr);
        curr = boundary_edges[curr];
    } while (curr != start && curr != -1);

    if (boundary_loop.empty()) {
        throw std::runtime_error("Failed to extract boundary loop.");
    }

    int n_boundary = boundary_loop.size();

    std::vector<double> edge_lengths(n_boundary);
    double total_length = 0;
    for (int i = 0; i < n_boundary; i++) {
        const auto& p1 = halfedge_mesh->point(halfedge_mesh->vertex_handle(boundary_loop[i]));
        const auto& p2 = halfedge_mesh->point(halfedge_mesh->vertex_handle(boundary_loop[(i + 1) % n_boundary]));
        edge_lengths[i] = (p2 - p1).length();
        total_length += edge_lengths[i];
    }

    double side_length = total_length / 4.0;
    double accumulated = 0;
    int edge_idx = 0;

    for (int i = 0; i < n_boundary; i++) {
        double edge_pos = accumulated / total_length;
        double t = fmod(accumulated + edge_lengths[i] * 0.5, total_length) / total_length;

        double x, y;
        if (t < 0.25) {
            x = t * 4.0;
            y = 0.0;
        } else if (t < 0.5) {
            x = 1.0;
            y = (t - 0.25) * 4.0;
        } else if (t < 0.75) {
            x = 1.0 - (t - 0.5) * 4.0;
            y = 1.0;
        } else {
            x = 0.0;
            y = 1.0 - (t - 0.75) * 4.0;
        }

        auto vh = halfedge_mesh->vertex_handle(boundary_loop[i]);
        halfedge_mesh->point(vh)[0] = x;
        halfedge_mesh->point(vh)[1] = y;
        halfedge_mesh->point(vh)[2] = 0;

        accumulated += edge_lengths[i];
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(boundary_mapping);
NODE_DEF_CLOSE_SCOPE