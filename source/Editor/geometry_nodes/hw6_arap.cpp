#include <Eigen/Dense>
#include <Eigen/SVD>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <limits>
#include <stdexcept>
#include <vector>

#include "GCore/Components.h"
#include "GCore/Components/MeshComponent.h"
#include "GCore/GOP.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "nodes/core/def/node_def.hpp"

using namespace Ruzino;

namespace {

constexpr double kEps = 1e-8;
constexpr double kPi = 3.14159265358979323846;
constexpr double kPenalty = 1e8;

template<typename MeshT>
bool same_topology(const MeshT& lhs, const MeshT& rhs)
{
    if (lhs.n_vertices() != rhs.n_vertices() ||
        lhs.n_faces() != rhs.n_faces()) {
        return false;
    }

    for (const auto& fh : lhs.faces()) {
        std::array<int, 3> lhs_ids{};
        std::array<int, 3> rhs_ids{};
        int k = 0;
        for (const auto& vh : lhs.fv_range(fh)) {
            if (k >= 3) {
                return false;
            }
            lhs_ids[k++] = vh.idx();
        }
        if (k != 3) {
            return false;
        }

        k = 0;
        for (const auto& vh : rhs.fv_range(rhs.face_handle(fh.idx()))) {
            if (k >= 3) {
                return false;
            }
            rhs_ids[k++] = vh.idx();
        }
        if (k != 3 || lhs_ids != rhs_ids) {
            return false;
        }
    }
    return true;
}

template<typename MeshT>
std::vector<int> collect_boundary_vertices(const MeshT& mesh)
{
    std::vector<int> boundary;
    boundary.reserve(mesh.n_vertices());
    for (const auto& vh : mesh.vertices()) {
        if (mesh.is_boundary(vh)) {
            boundary.push_back(vh.idx());
        }
    }
    return boundary;
}

std::vector<glm::vec2> normalize_uv(
    const std::vector<Eigen::Vector2d>& uv_coords)
{
    if (uv_coords.empty()) {
        return {};
    }

    double min_u = uv_coords[0].x();
    double max_u = uv_coords[0].x();
    double min_v = uv_coords[0].y();
    double max_v = uv_coords[0].y();

    for (size_t i = 1; i < uv_coords.size(); ++i) {
        min_u = std::min(min_u, uv_coords[i].x());
        max_u = std::max(max_u, uv_coords[i].x());
        min_v = std::min(min_v, uv_coords[i].y());
        max_v = std::max(max_v, uv_coords[i].y());
    }

    const double scale =
        1.0 / std::max(kEps, std::max(max_u - min_u, max_v - min_v));
    std::vector<glm::vec2> output_uv(uv_coords.size());
    for (size_t i = 0; i < uv_coords.size(); ++i) {
        output_uv[i] = glm::vec2(
            static_cast<float>((uv_coords[i].x() - min_u) * scale),
            static_cast<float>((uv_coords[i].y() - min_v) * scale));
    }
    return output_uv;
}

Geometry make_flattened_geometry(
    const Geometry& input,
    const std::vector<glm::vec2>& uv)
{
    auto output = input;
    auto mesh_component = output.get_component<MeshComponent>();
    if (!mesh_component) {
        throw std::runtime_error(
            "HW6 ARAP: flattened output requires mesh component.");
    }

    std::vector<glm::vec3> vertices;
    vertices.reserve(uv.size());
    for (const auto& texcoord : uv) {
        vertices.emplace_back(texcoord.x, texcoord.y, 0.0f);
    }

    mesh_component->set_vertices(vertices);
    mesh_component->set_texcoords_array(uv);
    return output;
}

}  // namespace

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(hw6_arap)
{
    b.add_input<Geometry>("Input");
    b.add_input<Geometry>("InitGeometry");
    b.add_input<int>("Iterations").default_val(10).min(1).max(30);

    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw6_arap)
{
    try {
        auto input_geom = params.get_input<Geometry>("Input");
        auto init_geom = params.get_input<Geometry>("InitGeometry");
        const int max_iters = params.get_input<int>("Iterations");

        if (!input_geom.get_component<MeshComponent>() ||
            !init_geom.get_component<MeshComponent>()) {
            throw std::runtime_error(
                "HW6 ARAP: Need both Input and InitGeometry.");
        }

        auto original_mesh = operand_to_openmesh(&input_geom);
        auto init_mesh = operand_to_openmesh(&init_geom);
        if (!same_topology(*original_mesh, *init_mesh)) {
            throw std::runtime_error(
                "HW6 ARAP: Input and InitGeometry topology mismatch.");
        }

        const int num_vertices = static_cast<int>(original_mesh->n_vertices());
        const int num_faces = static_cast<int>(original_mesh->n_faces());
        if (num_vertices < 3 || num_faces == 0) {
            throw std::runtime_error(
                "HW6 ARAP: input mesh must contain triangles.");
        }

        std::vector<Eigen::Vector3d> original_positions(num_vertices);
        std::vector<Eigen::Vector2d> initial_uv(num_vertices);

        for (const auto& vh : original_mesh->vertices()) {
            const auto& p = original_mesh->point(vh);
            original_positions[vh.idx()] = Eigen::Vector3d(p[0], p[1], p[2]);
        }
        for (const auto& vh : init_mesh->vertices()) {
            const auto& p = init_mesh->point(vh);
            initial_uv[vh.idx()] = Eigen::Vector2d(p[0], p[1]);
        }

        std::vector<std::array<Eigen::Vector2d, 3>> face_local_coords(
            static_cast<size_t>(num_faces));
        std::vector<std::array<int, 3>> face_vertex_ids(
            static_cast<size_t>(num_faces));
        std::vector<std::array<double, 3>> face_cots(
            static_cast<size_t>(num_faces));

        for (const auto& fh : original_mesh->faces()) {
            const int face_id = fh.idx();
            int k = 0;
            for (const auto& vh : original_mesh->fv_range(fh)) {
                if (k >= 3) {
                    throw std::runtime_error(
                        "HW6 ARAP: only triangular meshes are supported.");
                }
                face_vertex_ids[face_id][k++] = vh.idx();
            }
            if (k != 3) {
                throw std::runtime_error(
                    "HW6 ARAP: only triangular meshes are supported.");
            }

            const Eigen::Vector3d p0 =
                original_positions[face_vertex_ids[face_id][0]];
            const Eigen::Vector3d p1 =
                original_positions[face_vertex_ids[face_id][1]];
            const Eigen::Vector3d p2 =
                original_positions[face_vertex_ids[face_id][2]];

            const double l01 = (p1 - p0).norm();
            const double l12 = (p2 - p1).norm();
            const double l20 = (p0 - p2).norm();

            face_local_coords[face_id][0] = Eigen::Vector2d(0.0, 0.0);
            face_local_coords[face_id][1] = Eigen::Vector2d(l01, 0.0);
            const double x2 = (l01 * l01 + l20 * l20 - l12 * l12) /
                              (2.0 * std::max(kEps, l01));
            const double y2 = std::sqrt(std::max(0.0, l20 * l20 - x2 * x2));
            face_local_coords[face_id][2] = Eigen::Vector2d(x2, y2);

            auto calc_cotangent_2d = [](const Eigen::Vector2d& p,
                                        const Eigen::Vector2d& a,
                                        const Eigen::Vector2d& b) {
                const Eigen::Vector2d v1 = a - p;
                const Eigen::Vector2d v2 = b - p;
                const double dot = v1.dot(v2);
                const double cross =
                    std::abs(v1.x() * v2.y() - v1.y() * v2.x());
                return dot / std::max(kEps, cross);
            };

            for (int i = 0; i < 3; ++i) {
                const int j = (i + 1) % 3;
                const int k2 = (i + 2) % 3;
                face_cots[face_id][i] = calc_cotangent_2d(
                    face_local_coords[face_id][k2],
                    face_local_coords[face_id][i],
                    face_local_coords[face_id][j]);
            }
        }

        double signed_area_sum = 0.0;
        for (int face_id = 0; face_id < num_faces; ++face_id) {
            const Eigen::Vector2d& u0 = initial_uv[face_vertex_ids[face_id][0]];
            const Eigen::Vector2d& u1 = initial_uv[face_vertex_ids[face_id][1]];
            const Eigen::Vector2d& u2 = initial_uv[face_vertex_ids[face_id][2]];
            signed_area_sum += (u1.x() - u0.x()) * (u2.y() - u0.y()) -
                               (u2.x() - u0.x()) * (u1.y() - u0.y());
        }
        if (signed_area_sum < 0.0) {
            for (auto& uv : initial_uv) {
                uv.y() = -uv.y();
            }
        }

        const auto boundary_vertices =
            collect_boundary_vertices(*original_mesh);
        if (boundary_vertices.size() < 2) {
            throw std::runtime_error(
                "HW6 ARAP: Need at least two boundary vertices.");
        }

        int fixed_vertex0 = boundary_vertices[0];
        int fixed_vertex1 = boundary_vertices[1];
        double best_dist_sq = -1.0;
        for (size_t i = 0; i < boundary_vertices.size(); ++i) {
            for (size_t j = i + 1; j < boundary_vertices.size(); ++j) {
                const double dist_sq = (initial_uv[boundary_vertices[i]] -
                                        initial_uv[boundary_vertices[j]])
                                           .squaredNorm();
                if (dist_sq > best_dist_sq) {
                    best_dist_sq = dist_sq;
                    fixed_vertex0 = boundary_vertices[i];
                    fixed_vertex1 = boundary_vertices[j];
                }
            }
        }
        if (best_dist_sq <= kEps) {
            throw std::runtime_error(
                "HW6 ARAP: InitGeometry boundary is degenerate.");
        }

        const Eigen::Vector2d fixed_target_uv0 = initial_uv[fixed_vertex0];
        const Eigen::Vector2d fixed_target_uv1 = initial_uv[fixed_vertex1];

        Eigen::SparseMatrix<double> laplace_matrix(num_vertices, num_vertices);
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(static_cast<size_t>(num_faces) * 12 + 2);

        for (int face_id = 0; face_id < num_faces; ++face_id) {
            for (int i = 0; i < 3; ++i) {
                const int j = (i + 1) % 3;
                const int vi = face_vertex_ids[face_id][i];
                const int vj = face_vertex_ids[face_id][j];
                const double cot = face_cots[face_id][i];

                triplets.emplace_back(vi, vj, -cot);
                triplets.emplace_back(vj, vi, -cot);
                triplets.emplace_back(vi, vi, cot);
                triplets.emplace_back(vj, vj, cot);
            }
        }
        triplets.emplace_back(fixed_vertex0, fixed_vertex0, kPenalty);
        triplets.emplace_back(fixed_vertex1, fixed_vertex1, kPenalty);

        laplace_matrix.setFromTriplets(triplets.begin(), triplets.end());

        Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> solver(
            laplace_matrix);
        if (solver.info() != Eigen::Success) {
            throw std::runtime_error(
                "HW6 ARAP: Laplace matrix factorization failed.");
        }

        // ARAP solver (local/global iterations)
        auto solve_arap = [&]() {
            std::vector<Eigen::Vector2d> uv_coords = initial_uv;
            std::vector<Eigen::Matrix2d> face_transforms(
                static_cast<size_t>(num_faces));
            Eigen::MatrixXd rhs(num_vertices, 2);

            for (int iter = 0; iter < max_iters; ++iter) {
                // Local phase: compute best-fit rotation per triangle
                for (int face_id = 0; face_id < num_faces; ++face_id) {
                    Eigen::Matrix2d covariance = Eigen::Matrix2d::Zero();
                    for (int i = 0; i < 3; ++i) {
                        const int j = (i + 1) % 3;
                        const Eigen::Vector2d edge_u =
                            uv_coords[face_vertex_ids[face_id][i]] -
                            uv_coords[face_vertex_ids[face_id][j]];
                        const Eigen::Vector2d edge_x =
                            face_local_coords[face_id][i] -
                            face_local_coords[face_id][j];
                        covariance += face_cots[face_id][i] *
                                      (edge_u * edge_x.transpose());
                    }

                    Eigen::JacobiSVD<Eigen::Matrix2d> svd(
                        covariance, Eigen::ComputeFullU | Eigen::ComputeFullV);
                    Eigen::Matrix2d rotation =
                        svd.matrixU() * svd.matrixV().transpose();
                    if (rotation.determinant() < 0.0) {
                        Eigen::Matrix2d corrected_u = svd.matrixU();
                        corrected_u.col(1) *= -1.0;
                        rotation = corrected_u * svd.matrixV().transpose();
                    }
                    face_transforms[face_id] = rotation;
                }

                // Global phase: solve for new vertex positions
                rhs.setZero();
                for (int face_id = 0; face_id < num_faces; ++face_id) {
                    for (int i = 0; i < 3; ++i) {
                        const int j = (i + 1) % 3;
                        const Eigen::Vector2d edge_x =
                            face_local_coords[face_id][i] -
                            face_local_coords[face_id][j];
                        const Eigen::Vector2d contrib =
                            face_cots[face_id][i] * face_transforms[face_id] *
                            edge_x;
                        rhs.row(face_vertex_ids[face_id][i]) +=
                            contrib.transpose();
                        rhs.row(face_vertex_ids[face_id][j]) -=
                            contrib.transpose();
                    }
                }

                rhs.row(fixed_vertex0) +=
                    kPenalty * fixed_target_uv0.transpose();
                rhs.row(fixed_vertex1) +=
                    kPenalty * fixed_target_uv1.transpose();

                const Eigen::MatrixXd solved_uv = solver.solve(rhs);
                for (int i = 0; i < num_vertices; ++i) {
                    uv_coords[i] = solved_uv.row(i).transpose();
                }
            }
            return uv_coords;
        };

        std::vector<glm::vec2> output_uv = normalize_uv(solve_arap());

        auto output_mesh = make_flattened_geometry(input_geom, output_uv);

        params.set_output("Output", std::move(output_mesh));
        return true;
    }
    catch (const std::exception& e) {
        params.set_error(e.what());
        return false;
    }
}

NODE_DECLARATION_UI(hw6_arap);
NODE_DEF_CLOSE_SCOPE