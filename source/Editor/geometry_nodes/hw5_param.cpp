#include <time.h>

#include <Eigen/Sparse>
#include <cmath>

//#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include <pxr/usd/usdGeom/mesh.h>

#include <Eigen/Core>
#include <Eigen/Eigen>
#include <cfloat>
#include <cstdlib>
#include <unordered_set>
#include <vector>

#include "GCore/Components.h"
#include "GCore/Components/MeshComponent.h"
#include "GCore/GOP.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "nodes/core/def/node_def.hpp"
#include "geom_node_base.h"

/*
** @brief HW4_TutteParameterization
**
** This file presents the basic framework of a "node", which processes inputs
** received from the left and outputs specific variables for downstream nodes to
** use.
** - In the first function, node_declare, you can set up the node's input and
** output variables.
** - The second function, node_exec is the execution part of the node, where we
** need to implement the node's functionality.
** Your task is to fill in the required logic at the specified locations
** within this template, especially in node_exec.
*/

NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(hw5_param)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");

    // Input-2: Reference mesh for computing cotangent weights (optional)
    b.add_input<Geometry>("ReferenceMesh");

    // Output-1: Minimal surface with uniform weights
    b.add_output<Geometry>("Uniform");

    // Output-2: Minimal surface with cotangent weights
    b.add_output<Geometry>("Cotangent");

    // Output-3: Minimal surface with Floater shape-preserving weights
    b.add_output<Geometry>("Floater");
}

NODE_EXECUTION_FUNCTION(hw5_param)
{
    // Get the input from params
    auto input = params.get_input<Geometry>("Input");
    auto ref_mesh_input = params.get_input<Geometry>("ReferenceMesh");

    // Check if reference mesh is provided for cotangent weights
    bool use_cotangent = false;
    std::shared_ptr<PolyMesh> ref_mesh;
    if (ref_mesh_input.get_component<MeshComponent>() != nullptr) {
        use_cotangent = true;
        ref_mesh = operand_to_openmesh(&ref_mesh_input);
    }

    // (TO BE UPDATED) Avoid processing the node when there is no input
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Minimal Surface: Need Geometry Input.");
        return false;
    }

    /* ----------------------------- Preprocess -------------------------------
    ** Create a halfedge structure (using OpenMesh) for the input mesh. The
    ** half-edge data structure is a widely used data structure in geometric
    ** processing, offering convenient operations for traversing and modifying
    ** mesh elements.
    */
    auto halfedge_mesh = operand_to_openmesh(&input);

    int n_vertices = halfedge_mesh->n_vertices();
    int n_faces = halfedge_mesh->n_faces();
    int n_edges = halfedge_mesh->n_edges();

    /* ---------------- [HW4_TODO] TASK 1: Minimal Surface --------------------
    ** In this task, you are required to generate a 'minimal surface' mesh with
    ** the boundary of the input mesh as its boundary.
    **
    ** Specifically, the positions of the boundary vertices of the input mesh
    ** should be fixed. By solving a global Laplace equation on the mesh,
    ** recalculate the coordinates of the vertices inside the mesh to achieve
    ** the minimal surface configuration.
    **
    ** (Recall the Poisson equation with Dirichlet Boundary Condition in HW3)
    */

    std::vector<bool> is_boundary(n_vertices, false);
    std::vector<int> interior_indices;
    std::vector<int> boundary_indices;

    for (const auto& halfedge_handle : halfedge_mesh->halfedges()) {
        if (halfedge_handle.is_boundary()) {
            int from_idx = halfedge_handle.from().idx();
            int to_idx = halfedge_handle.to().idx();
            is_boundary[from_idx] = true;
            is_boundary[to_idx] = true;
        }
    }

    for (int i = 0; i < n_vertices; i++) {
        if (is_boundary[i])
            boundary_indices.push_back(i);
        else
            interior_indices.push_back(i);
    }

    int n_interior = interior_indices.size();
    if (n_interior == 0) {
        throw std::runtime_error("Minimal Surface: No interior vertices.");
        return false;
    }

    std::vector<int> vertex_to_mat(n_vertices, -1);
    for (int i = 0; i < n_interior; i++) {
        vertex_to_mat[interior_indices[i]] = i;
    }

    // ============================================================
    // Compute UNIFORM weights result
    // ============================================================
    std::vector<Eigen::Triplet<double>> triplets;
    Eigen::VectorXd b_x(n_interior), b_y(n_interior), b_z(n_interior);
    b_x.setZero();
    b_y.setZero();
    b_z.setZero();

    for (int i = 0; i < n_interior; i++) {
        int v_idx = interior_indices[i];
        
        std::vector<int> neighbors;
        for (const auto& he : halfedge_mesh->halfedges()) {
            if (he.from().idx() == v_idx) {
                neighbors.push_back(he.to().idx());
            }
        }
        
        int degree = neighbors.size();
        if (degree == 0) {
            throw std::runtime_error("Interior vertex has degree 0.");
        }

        double weight = 1.0 / degree;
        triplets.push_back(Eigen::Triplet<double>(i, i, 1.0));

        double b_x_contrib = 0, b_y_contrib = 0, b_z_contrib = 0;
        for (int neighbor_idx : neighbors) {
            if (is_boundary[neighbor_idx]) {
                auto neighbor_vh = halfedge_mesh->vertex_handle(neighbor_idx);
                const auto& p = halfedge_mesh->point(neighbor_vh);
                b_x_contrib += weight * p[0];
                b_y_contrib += weight * p[1];
                b_z_contrib += weight * p[2];
            } else {
                int neighbor_mat_idx = vertex_to_mat[neighbor_idx];
                if (neighbor_mat_idx >= 0) {
                    triplets.push_back(Eigen::Triplet<double>(i, neighbor_mat_idx, -weight));
                }
            }
        }

        b_x(i) = b_x_contrib;
        b_y(i) = b_y_contrib;
        b_z(i) = b_z_contrib;
    }

    Eigen::SparseMatrix<double> A(n_interior, n_interior);
    A.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::BiCGSTAB<Eigen::SparseMatrix<double>> solver;
    solver.setMaxIterations(2000);
    solver.setTolerance(1e-6);
    solver.compute(A);

    Eigen::VectorXd x = solver.solve(b_x);
    Eigen::VectorXd y = solver.solve(b_y);
    Eigen::VectorXd z = solver.solve(b_z);

    for (int i = 0; i < n_interior; i++) {
        int v_idx = interior_indices[i];
        auto vh = halfedge_mesh->vertex_handle(v_idx);
        halfedge_mesh->point(vh)[0] = x(i);
        halfedge_mesh->point(vh)[1] = y(i);
        halfedge_mesh->point(vh)[2] = z(i);
    }

    auto uniform_output = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Uniform", std::move(*uniform_output));

    // ============================================================
    // Compute COTANGENT weights result (if reference mesh provided)
    // ============================================================
    if (use_cotangent && ref_mesh) {
        // Compute cotangent weights using reference mesh
        // w_ij = cot(alpha_ij) + cot(beta_ij)
        // where alpha_ij and beta_ij are the angles opposite to edge (i,j) in the two adjacent triangles
        
        std::vector<std::vector<double>> cotan_weight(n_vertices);
        std::vector<std::vector<int>> cotan_neighbors(n_vertices);
        
        // Compute cotangent weights by iterating over all edges in reference mesh
        for (const auto& he : ref_mesh->halfedges()) {
            if (he.is_boundary()) continue;
            
            int vi = he.from().idx();
            int vj = he.to().idx();
            int third = -1;
            
            // Get the face containing this half-edge to find the third vertex
            auto fh = he.face();
            if (!fh.is_valid()) continue;
            
            std::vector<int> fv(3);
            int fcnt = 0;
            for (const auto& vh : fh.vertices()) {
                fv[fcnt++] = vh.idx();
            }
            
            // Find the third vertex (not vi or vj)
            for (int k = 0; k < 3; k++) {
                if (fv[k] != vi && fv[k] != vj) {
                    third = fv[k];
                    break;
                }
            }
            
            if (third == -1) continue;
            
            // Compute cotangent of angle at 'third' vertex
            // u = p(vi) - p(third), v = p(vj) - p(third)
            const auto& p1 = ref_mesh->point(ref_mesh->vertex_handle(vi));
            const auto& p2 = ref_mesh->point(ref_mesh->vertex_handle(vj));
            const auto& p3 = ref_mesh->point(ref_mesh->vertex_handle(third));
            
            auto u = p1 - p3;
            auto v = p2 - p3;
            
            double dot = u.dot(v);
            double cross = (u.cross(v)).norm();
            
            double cotan = 0;
            if (cross > 1e-10) {
                cotan = dot / cross;
            }
            
            // Only use positive cotangent weights (for Delaunay triangulation)
            if (cotan > 0) {
                // Add to both vertices of the edge
                bool found_i = false, found_j = false;
                for (size_t idx = 0; idx < cotan_neighbors[vi].size(); idx++) {
                    if (cotan_neighbors[vi][idx] == vj) {
                        cotan_weight[vi][idx] += cotan;
                        found_i = true;
                        break;
                    }
                }
                if (!found_i) {
                    cotan_neighbors[vi].push_back(vj);
                    cotan_weight[vi].push_back(cotan);
                }
                
                for (size_t idx = 0; idx < cotan_neighbors[vj].size(); idx++) {
                    if (cotan_neighbors[vj][idx] == vi) {
                        cotan_weight[vj][idx] += cotan;
                        found_j = true;
                        break;
                    }
                }
                if (!found_j) {
                    cotan_neighbors[vj].push_back(vi);
                    cotan_weight[vj].push_back(cotan);
                }
            }
        }
        
        // Normalize weights
        for (int v_idx = 0; v_idx < n_vertices; v_idx++) {
            double sum_w = 0;
            for (double w : cotan_weight[v_idx]) {
                sum_w += w;
            }
            if (sum_w > 0) {
                for (double& w : cotan_weight[v_idx]) {
                    w /= sum_w;
                }
            }
        }
        
        // Solve with cotangent weights
        std::vector<Eigen::Triplet<double>> cotan_triplets;
        Eigen::VectorXd cb_x(n_interior), cb_y(n_interior), cb_z(n_interior);
        cb_x.setZero();
        cb_y.setZero();
        cb_z.setZero();

        for (int i = 0; i < n_interior; i++) {
            int v_idx = interior_indices[i];
            
            std::vector<int> neighbors;
            for (const auto& he : halfedge_mesh->halfedges()) {
                if (he.from().idx() == v_idx) {
                    neighbors.push_back(he.to().idx());
                }
            }
            
            int degree = neighbors.size();
            if (degree == 0) continue;
            
            // Get cotangent weights for this vertex
            const auto& cw = cotan_weight[v_idx];
            const auto& cn = cotan_neighbors[v_idx];
            
            cotan_triplets.push_back(Eigen::Triplet<double>(i, i, 1.0));
            
            double b_xc = 0, b_yc = 0, b_zc = 0;
            for (int j = 0; j < degree; j++) {
                int neighbor_idx = neighbors[j];
                double w = 0;
                
                // Find the weight for this neighbor
                for (size_t k = 0; k < cn.size(); k++) {
                    if (cn[k] == neighbor_idx) {
                        w = cw[k];
                        break;
                    }
                }
                
                if (is_boundary[neighbor_idx]) {
                    auto neighbor_vh = halfedge_mesh->vertex_handle(neighbor_idx);
                    const auto& p = halfedge_mesh->point(neighbor_vh);
                    b_xc += w * p[0];
                    b_yc += w * p[1];
                    b_zc += w * p[2];
                } else {
                    int nm = vertex_to_mat[neighbor_idx];
                    if (nm >= 0 && w > 0) {
                        cotan_triplets.push_back(Eigen::Triplet<double>(i, nm, -w));
                    }
                }
            }
            
            cb_x(i) = b_xc;
            cb_y(i) = b_yc;
            cb_z(i) = b_zc;
        }

        Eigen::SparseMatrix<double> A_cotan(n_interior, n_interior);
        A_cotan.setFromTriplets(cotan_triplets.begin(), cotan_triplets.end());

        Eigen::BiCGSTAB<Eigen::SparseMatrix<double>> solver2;
        solver2.setMaxIterations(2000);
        solver2.setTolerance(1e-6);
        solver2.compute(A_cotan);

        Eigen::VectorXd x2 = solver2.solve(cb_x);
        Eigen::VectorXd y2 = solver2.solve(cb_y);
        Eigen::VectorXd z2 = solver2.solve(cb_z);

        for (int i = 0; i < n_interior; i++) {
            int v_idx = interior_indices[i];
            auto vh = halfedge_mesh->vertex_handle(v_idx);
            halfedge_mesh->point(vh)[0] = x2(i);
            halfedge_mesh->point(vh)[1] = y2(i);
            halfedge_mesh->point(vh)[2] = z2(i);
        }

        auto cotan_output = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Cotangent", std::move(*cotan_output));
    }
    
    // ============================================================
    // Compute FLOATER weights result (shape-preserving)
    // Floater weights based on edge length inverses
    // ============================================================
    if (use_cotangent && ref_mesh) {
        std::vector<std::vector<double>> floater_weight(n_vertices);
        std::vector<std::vector<int>> floater_neighbors(n_vertices);
        
        // Compute Floater weights using reference mesh
        // Based on inverse edge lengths
        for (int v_idx = 0; v_idx < n_vertices; v_idx++) {
            double sum_w = 0;
            
            // Find all neighbors and edge lengths
            std::vector<int> neighbors;
            std::vector<double> inv_lengths;
            
            for (const auto& he : ref_mesh->halfedges()) {
                if (he.from().idx() == v_idx) {
                    int neighbor = he.to().idx();
                    auto p1 = ref_mesh->point(ref_mesh->vertex_handle(v_idx));
                    auto p2 = ref_mesh->point(ref_mesh->vertex_handle(neighbor));
                    double len = (p2 - p1).norm();
                    if (len > 1e-10) {
                        neighbors.push_back(neighbor);
                        inv_lengths.push_back(1.0 / len);
                    }
                }
            }
            
            // Sum of inverse lengths
            for (double inv_len : inv_lengths) {
                sum_w += inv_len;
            }
            
            // Normalize to get weights
            if (sum_w > 0) {
                for (size_t i = 0; i < neighbors.size(); i++) {
                    floater_neighbors[v_idx].push_back(neighbors[i]);
                    floater_weight[v_idx].push_back(inv_lengths[i] / sum_w);
                }
            }
        }
        
        // Solve with Floater weights
        std::vector<Eigen::Triplet<double>> floater_triplets;
        Eigen::VectorXd fb_x(n_interior), fb_y(n_interior), fb_z(n_interior);
        fb_x.setZero();
        fb_y.setZero();
        fb_z.setZero();

        for (int i = 0; i < n_interior; i++) {
            int v_idx = interior_indices[i];
            
            std::vector<int> neighbors;
            for (const auto& he : halfedge_mesh->halfedges()) {
                if (he.from().idx() == v_idx) {
                    neighbors.push_back(he.to().idx());
                }
            }
            
            int degree = neighbors.size();
            if (degree == 0) continue;
            
            const auto& fw = floater_weight[v_idx];
            const auto& fn = floater_neighbors[v_idx];
            
            floater_triplets.push_back(Eigen::Triplet<double>(i, i, 1.0));
            
            double b_xc = 0, b_yc = 0, b_zc = 0;
            for (int j = 0; j < degree; j++) {
                int neighbor_idx = neighbors[j];
                double w = 0;
                
                for (size_t k = 0; k < fn.size(); k++) {
                    if (fn[k] == neighbor_idx) {
                        w = fw[k];
                        break;
                    }
                }
                
                if (is_boundary[neighbor_idx]) {
                    auto neighbor_vh = halfedge_mesh->vertex_handle(neighbor_idx);
                    const auto& p = halfedge_mesh->point(neighbor_vh);
                    b_xc += w * p[0];
                    b_yc += w * p[1];
                    b_zc += w * p[2];
                } else {
                    int nm = vertex_to_mat[neighbor_idx];
                    if (nm >= 0 && w > 0) {
                        floater_triplets.push_back(Eigen::Triplet<double>(i, nm, -w));
                    }
                }
            }
            
            fb_x(i) = b_xc;
            fb_y(i) = b_yc;
            fb_z(i) = b_zc;
        }

        Eigen::SparseMatrix<double> A_floater(n_interior, n_interior);
        A_floater.setFromTriplets(floater_triplets.begin(), floater_triplets.end());

        Eigen::BiCGSTAB<Eigen::SparseMatrix<double>> solver3;
        solver3.setMaxIterations(2000);
        solver3.setTolerance(1e-6);
        solver3.compute(A_floater);

        Eigen::VectorXd x3 = solver3.solve(fb_x);
        Eigen::VectorXd y3 = solver3.solve(fb_y);
        Eigen::VectorXd z3 = solver3.solve(fb_z);

        for (int i = 0; i < n_interior; i++) {
            int v_idx = interior_indices[i];
            auto vh = halfedge_mesh->vertex_handle(v_idx);
            halfedge_mesh->point(vh)[0] = x3(i);
            halfedge_mesh->point(vh)[1] = y3(i);
            halfedge_mesh->point(vh)[2] = z3(i);
        }

        auto floater_output = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Floater", std::move(*floater_output));
    }
    
    return true;
}

NODE_DECLARATION_UI(hw5_param);
NODE_DEF_CLOSE_SCOPE