#include "MassSpring.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace USTC_CG::mass_spring {
MassSpring::MassSpring(const Eigen::MatrixXd& X, const EdgeSet& E)
{
    this->X = this->init_X = X;
    this->vel = Eigen::MatrixXd::Zero(X.rows(), X.cols());
    this->E = E;

    std::cout << "number of edges: " << E.size() << std::endl;
    std::cout << "init mass spring" << std::endl;

    // Compute the rest pose edge length
    for (const auto& e : E) {
        Eigen::Vector3d x0 = X.row(e.first);
        Eigen::Vector3d x1 = X.row(e.second);
        this->E_rest_length.push_back((x0 - x1).norm());
    }

    // Initialize the mask for Dirichlet boundary condition
    dirichlet_bc_mask.resize(X.rows(), false);

    // (HW_TODO) Fix two vertices, feel free to modify this
    unsigned n_fix = sqrt(X.rows());  // Here we assume the cloth is square
    dirichlet_bc_mask[0] = true;
    dirichlet_bc_mask[n_fix - 1] = true;
}

void MassSpring::step()
{
    Eigen::Vector3d acceleration_ext = gravity + wind_ext_acc;

    unsigned n_vertices = X.rows();

    // The reason to not use 1.0 as mass per vertex: the cloth gets heavier as
    // we increase the resolution
    double mass_per_vertex = mass / n_vertices;

    //----------------------------------------------------
    // (HW Optional) Bonus part: Sphere collision
    Eigen::MatrixXd acceleration_collision =
        getSphereCollisionForce(sphere_center.cast<double>(), sphere_radius);
    //----------------------------------------------------

    if (time_integrator == IMPLICIT_EULER) {
        // Implicit Euler
        TIC(step)

        const Eigen::MatrixXd X_old = X;
        const double inertia_coeff = mass_per_vertex / (h * h);
        const double regularization = std::max(1e-8, 1e-6 * stiffness);
        const unsigned max_newton_iterations = 8;
        const double newton_tolerance = 1e-8;

        Eigen::MatrixXd Y = X + h * vel;
        Y.rowwise() += (h * h * acceleration_ext).transpose();
        if (enable_sphere_collision) {
            Y += h * h * acceleration_collision;
        }

        std::vector<int> full_to_reduced(n_vertices * 3, -1);
        std::vector<int> reduced_to_full;
        reduced_to_full.reserve(n_vertices * 3);
        for (unsigned i = 0; i < n_vertices; ++i) {
            if (dirichlet_bc_mask[i]) {
                X.row(i) = init_X.row(i);
                vel.row(i).setZero();
            }
            else {
                for (int d = 0; d < 3; ++d) {
                    full_to_reduced[3 * i + d] =
                        static_cast<int>(reduced_to_full.size());
                    reduced_to_full.push_back(3 * i + d);
                }
            }
        }
        const int n_free = static_cast<int>(reduced_to_full.size());

        for (unsigned iter = 0; iter < max_newton_iterations; ++iter) {
            auto H_elastic = computeHessianSparse(stiffness);
            SparseMatrix_d A = H_elastic;
            for (unsigned i = 0; i < 3 * n_vertices; ++i) {
                A.coeffRef(i, i) += inertia_coeff + regularization;
            }
            A.makeCompressed();

            Eigen::VectorXd grad_g =
                inertia_coeff * (flatten(X) - flatten(Y)) + flatten(computeGrad(stiffness));

            std::vector<Trip_d> A_reduced_triplets;
            A_reduced_triplets.reserve(A.nonZeros());
            for (int col = 0; col < A.outerSize(); ++col) {
                const int reduced_col = full_to_reduced[col];
                if (reduced_col < 0) {
                    continue;
                }
                for (SparseMatrix_d::InnerIterator it(A, col); it; ++it) {
                    const int reduced_row = full_to_reduced[it.row()];
                    if (reduced_row >= 0) {
                        A_reduced_triplets.emplace_back(
                            reduced_row, reduced_col, it.value());
                    }
                }
            }

            SparseMatrix_d A_reduced(n_free, n_free);
            A_reduced.setFromTriplets(
                A_reduced_triplets.begin(), A_reduced_triplets.end());
            A_reduced.makeCompressed();

            Eigen::VectorXd rhs_reduced(n_free);
            for (int idx = 0; idx < n_free; ++idx) {
                rhs_reduced[idx] = -grad_g[reduced_to_full[idx]];
            }

            if (enable_debug_output) {
                std::cout << "Newton iter " << iter
                          << ", rhs norm: " << rhs_reduced.norm()
                          << ", A_reduced size: " << A_reduced.rows() << "x"
                          << A_reduced.cols() << std::endl;
            }

            if (rhs_reduced.norm() < newton_tolerance) {
                break;
            }

            Eigen::SimplicialLDLT<SparseMatrix_d> solver;
            solver.compute(A_reduced);
            if (solver.info() != Eigen::Success) {
                std::cerr << "LDLT factorization failed, falling back to SparseLU."
                          << std::endl;
                Eigen::SparseLU<SparseMatrix_d> fallback_solver;
                fallback_solver.analyzePattern(A_reduced);
                fallback_solver.factorize(A_reduced);
                if (fallback_solver.info() != Eigen::Success) {
                    std::cerr << "Linear solver factorize failed!" << std::endl;
                    return;
                }
                Eigen::VectorXd fallback_delta = fallback_solver.solve(rhs_reduced);
                if (fallback_solver.info() != Eigen::Success ||
                    !fallback_delta.allFinite()) {
                    std::cerr << "Linear solve failed!" << std::endl;
                    return;
                }

                Eigen::VectorXd delta_X_flat =
                    Eigen::VectorXd::Zero(n_vertices * 3);
                for (int idx = 0; idx < n_free; ++idx) {
                    delta_X_flat[reduced_to_full[idx]] = fallback_delta[idx];
                }
                Eigen::MatrixXd delta_X = unflatten(delta_X_flat);
                X += delta_X;
                for (unsigned i = 0; i < n_vertices; ++i) {
                    if (dirichlet_bc_mask[i]) {
                        X.row(i) = init_X.row(i);
                    }
                }
                if (delta_X.norm() < newton_tolerance) {
                    break;
                }
                continue;
            }

            Eigen::VectorXd delta_X_reduced = solver.solve(rhs_reduced);
            if (solver.info() != Eigen::Success || !delta_X_reduced.allFinite()) {
                std::cerr << "Linear solve failed!" << std::endl;
                return;
            }

            Eigen::VectorXd delta_X_flat = Eigen::VectorXd::Zero(n_vertices * 3);
            for (int idx = 0; idx < n_free; ++idx) {
                delta_X_flat[reduced_to_full[idx]] = delta_X_reduced[idx];
            }

            Eigen::MatrixXd delta_X = unflatten(delta_X_flat);
            X += delta_X;
            for (unsigned i = 0; i < n_vertices; ++i) {
                if (dirichlet_bc_mask[i]) {
                    X.row(i) = init_X.row(i);
                }
            }

            if (delta_X.norm() < newton_tolerance) {
                break;
            }
        }

        vel = (X - X_old) / h;
        for (unsigned i = 0; i < n_vertices; ++i) {
            if (dirichlet_bc_mask[i]) {
                X.row(i) = init_X.row(i);
                vel.row(i).setZero();
            }
        }

        TOC(step)
    }
    else if (time_integrator == SEMI_IMPLICIT_EULER) {
        // Semi-implicit Euler
        Eigen::MatrixXd acceleration =
            -computeGrad(stiffness) / mass_per_vertex;
        acceleration.rowwise() += acceleration_ext.transpose();

        // -----------------------------------------------
        // (HW Optional)
        if (enable_sphere_collision) {
            acceleration += acceleration_collision;
        }
        // -----------------------------------------------

        vel += h * acceleration;
        if (enable_damping) {
            vel *= damping;
        }

        for (unsigned i = 0; i < n_vertices; ++i) {
            if (dirichlet_bc_mask[i]) {
                vel.row(i).setZero();
                X.row(i) = init_X.row(i);
            }
        }

        X += h * vel;

        for (unsigned i = 0; i < n_vertices; ++i) {
            if (dirichlet_bc_mask[i]) {
                X.row(i) = init_X.row(i);
            }
        }
    }
    else {
        std::cerr << "Unknown time integrator!" << std::endl;
        return;
    }
}

// There are different types of mass spring energy:
// For this homework we will adopt Prof. Huamin Wang's energy definition
// introduced in GAMES103 course Lecture 2 E = 0.5 * stiffness * sum_{i=1}^{n}
// (||x_i - x_j|| - l)^2 There exist other types of energy definition, e.g.,
// Prof. Minchen Li's energy definition
// https://www.cs.cmu.edu/~15769-f23/lec/3_Mass_Spring_Systems.pdf
double MassSpring::computeEnergy(double stiffness)
{
    double sum = 0.;
    unsigned i = 0;
    for (const auto& e : E) {
        auto diff = X.row(e.first) - X.row(e.second);
        auto l = E_rest_length[i];
        sum += 0.5 * stiffness * std::pow((diff.norm() - l), 2);
        i++;
    }
    return sum;
}

Eigen::MatrixXd MassSpring::computeGrad(double stiffness)
{
    Eigen::MatrixXd g = Eigen::MatrixXd::Zero(X.rows(), X.cols());
    unsigned i = 0;
    for (const auto& e : E) {
        Eigen::Vector3d diff = X.row(e.first) - X.row(e.second);
        double length = diff.norm();

        if (length > 1e-12) {
            Eigen::Vector3d grad =
                stiffness * (length - E_rest_length[i]) * diff / length;
            g.row(e.first) += grad.transpose();
            g.row(e.second) -= grad.transpose();
        }

        i++;
    }
    return g;
}

Eigen::SparseMatrix<double> MassSpring::computeHessianSparse(double stiffness)
{
    unsigned n_vertices = X.rows();
    Eigen::SparseMatrix<double> H(n_vertices * 3, n_vertices * 3);
    std::vector<Trip_d> triplets;
    triplets.reserve(E.size() * 36);

    unsigned i = 0;
    auto k = stiffness;
    const auto I = Eigen::Matrix3d::Identity();
    for (const auto& e : E) {
        Eigen::Vector3d diff = X.row(e.first) - X.row(e.second);
        double length = diff.norm();

        if (length > 1e-12) {
            Eigen::Matrix3d outer = diff * diff.transpose() / (length * length);
            Eigen::Matrix3d H_local;

            if (enable_make_SPD && E_rest_length[i] > length) {
                H_local = k * outer;
            }
            else {
                H_local = k * outer +
                          k * (1.0 - E_rest_length[i] / length) * (I - outer);
            }

            int v0 = e.first;
            int v1 = e.second;
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    double value = H_local(r, c);
                    int row0 = 3 * v0 + r;
                    int col0 = 3 * v0 + c;
                    int row1 = 3 * v1 + r;
                    int col1 = 3 * v1 + c;
                    triplets.emplace_back(row0, col0, value);
                    triplets.emplace_back(row1, col1, value);
                    triplets.emplace_back(row0, col1, -value);
                    triplets.emplace_back(row1, col0, -value);
                }
            }
        }

        i++;
    }

    H.setFromTriplets(triplets.begin(), triplets.end());
    H.makeCompressed();
    return H;
}

bool MassSpring::checkSPD(const Eigen::SparseMatrix<double>& A)
{
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(A);
    auto eigen_values = es.eigenvalues();
    return eigen_values.minCoeff() >= 1e-10;
}

void MassSpring::reset()
{
    std::cout << "reset" << std::endl;
    this->X = this->init_X;
    this->vel.setZero();
}

// ----------------------------------------------------------------------------------
// (HW Optional) Bonus part
Eigen::MatrixXd MassSpring::getSphereCollisionForce(
    Eigen::Vector3d center,
    double radius)
{
    Eigen::MatrixXd force = Eigen::MatrixXd::Zero(X.rows(), X.cols());
    for (int i = 0; i < X.rows(); i++) {
        // (HW Optional) Implement penalty-based force here
    }
    return force;
}
// ----------------------------------------------------------------------------------

bool MassSpring::set_dirichlet_bc_mask(const std::vector<bool>& mask)
{
    if (mask.size() == X.rows()) {
        dirichlet_bc_mask = mask;
        return true;
    }
    else
        return false;
}

bool MassSpring::update_dirichlet_bc_vertices(const MatrixXd& control_vertices)
{
    for (int i = 0; i < dirichlet_bc_control_pair.size(); i++) {
        int idx = dirichlet_bc_control_pair[i].first;
        int control_idx = dirichlet_bc_control_pair[i].second;
        X.row(idx) = control_vertices.row(control_idx);
    }

    return true;
}

bool MassSpring::init_dirichlet_bc_vertices_control_pair(
    const MatrixXd& control_vertices,
    const std::vector<bool>& control_mask)
{
    if (control_mask.size() != control_vertices.rows())
        return false;

    // TODO: optimize this part from O(n) to O(1)
    // First, get selected_control_vertices
    std::vector<VectorXd> selected_control_vertices;
    std::vector<int> selected_control_idx;
    for (int i = 0; i < control_mask.size(); i++) {
        if (control_mask[i]) {
            selected_control_vertices.push_back(control_vertices.row(i));
            selected_control_idx.push_back(i);
        }
    }

    // Then update mass spring fixed vertices
    for (int i = 0; i < dirichlet_bc_mask.size(); i++) {
        if (dirichlet_bc_mask[i]) {
            // O(n^2) nearest point search, can be optimized
            // -----------------------------------------
            int nearest_idx = 0;
            double nearst_dist = 1e6;
            VectorXd X_i = X.row(i);
            for (int j = 0; j < selected_control_vertices.size(); j++) {
                double dist = (X_i - selected_control_vertices[j]).norm();
                if (dist < nearst_dist) {
                    nearst_dist = dist;
                    nearest_idx = j;
                }
            }
            //-----------------------------------------

            X.row(i) = selected_control_vertices[nearest_idx];
            dirichlet_bc_control_pair.push_back(
                std::make_pair(i, selected_control_idx[nearest_idx]));
        }
    }

    return true;
}

}  // namespace USTC_CG::mass_spring