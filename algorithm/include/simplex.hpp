#pragma once

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <Eigen/UmfPackSupport>
#include <optional>
#include <vector>

#include "mps_reader.hpp"
#include "params.hpp"

struct EtaMatrix {
    Eigen::VectorXd col;
    size_t index;
};

struct EnteringVariableInfo {
    bool optimal{true};
    int index{0};
    double reduced_cost{0.0};
};

struct LeavingVariableInfo {
    double step_length{0.0};
    std::optional<size_t> leaving_b_idx;
};

struct Solution {
    std::vector<size_t> basic_idx;
    std::vector<size_t> nonbasic_idx;
    Eigen::SparseMatrix<double> B;
    Eigen::SparseMatrix<double> N;
    Eigen::VectorXd x;
    double cost;
};

class Simplex {
   public:
    Simplex(const mpsReader& mps, const Params& p, std::optional<Solution> s, int phase);

    Solution solve();

    const mpsReader& mps;
    const Params& p;

    int m;
    int n;
    int iteration{0};
    int phase{1};

    Eigen::SparseMatrix<double> A;
    Eigen::SparseMatrix<double> B0;
    Eigen::SparseMatrix<double> B0T;
    Eigen::SparseMatrix<double> N;
    Eigen::VectorXd ub, lb;
    Eigen::VectorXd c;

    // SOLUTION
    Eigen::VectorXd x;
    std::vector<size_t> basic_idx;
    std::vector<size_t> nonbasic_idx;

    // Context cache for solving
    Eigen::VectorXd c_b;
    Eigen::RowVectorXd y;
    bool basis_changed{true};

    // ETA related
    std::vector<EtaMatrix> eta_vector;
    Eigen::UmfPackLU<Eigen::SparseMatrix<double>> B0_solver;
    Eigen::UmfPackLU<Eigen::SparseMatrix<double>> B0T_solver;

    Eigen::RowVectorXd solve_btran(Eigen::RowVectorXd b);
    Eigen::VectorXd solve_ftran(const Eigen::VectorXd& a);
    void refactorization();

    // Core functions
    EnteringVariableInfo choose_entering_variable();
    LeavingVariableInfo choose_leaving_variable(const Eigen::VectorXd& d, size_t entering_nonbasic_slot,
                                                double reduced_cost);

    // Basis change
    void update_basis(size_t leaving_basis_idx, size_t entering_nonbasic_idx);

    // Phase 0
    void init_phase_0();

    // sets c and ub/lb for the var
    void update_phase_0_costs(size_t var);
    double compute_infeasibility() const;

    // misc
    Solution get_solution() const;
    void it_log() const;
};
