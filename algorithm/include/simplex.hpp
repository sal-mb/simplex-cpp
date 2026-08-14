#pragma once

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <Eigen/UmfPackSupport>
#include <optional>
#include <vector>

#include "mps_reader.hpp"
#include "params.hpp"

using namespace Eigen;
using namespace std;

struct EtaMatrix {
    VectorXd col;
    size_t index;
};

struct EnteringVariableInfo {
    bool optimal{true};
    int index{0};
    double reduced_cost{0.0};
};

struct LeavingVariableInfo {
    double step_length{0.0};
    optional<size_t> leaving_b_idx;
};

struct Solution {
    VectorXd x;
    double cost;
};
class Simplex {
   public:
    Simplex(const mpsReader& mps, const Params& p, int phase);

    std::tuple<VectorXd, double> solve();

    const mpsReader& mps;
    const Params& p;

    int m;
    int n;
    int iteration{0};
    int phase{1};

    SparseMatrix<double> A;
    SparseMatrix<double> B0;
    SparseMatrix<double> N;
    const VectorXd &ub, &lb;
    const VectorXd c;

    // SOLUTION
    VectorXd x;
    vector<size_t> basic_idx;
    vector<size_t> nonbasic_idx;

    // context cache for solving
    VectorXd c_b;
    RowVectorXd y;
    bool basis_changed{true};

    // ETA related
    vector<EtaMatrix> eta_vector;
    UmfPackLU<SparseMatrix<double>> B0_solver;
    RowVectorXd solve_btran(RowVectorXd b);
    VectorXd solve_ftran(VectorXd a);

    // Core function
    EnteringVariableInfo choose_entering_variable();
    LeavingVariableInfo choose_leaving_variable(const VectorXd& d, size_t entering_nonbasic_slot, double reduced_cost);
    // basis change
    void update_basis(size_t leaving_basis_idx, size_t entering_nonbasic_idx);

    void it_log() const;
};
