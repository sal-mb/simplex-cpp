#pragma once

#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "mps_reader.hpp"
#include "params.hpp"
using namespace Eigen;

struct EtaMatrix {
    VectorXd col;
    size_t index;
};

class Simplex {
   public:
    const mpsReader& mps;
    Simplex(const mpsReader& mps, const Params& p);

    double solve();

    const Params& p;
    const int m = mps.n_rows_inq + mps.n_rows_eq;
    const int n = mps.A.cols();
    const VectorXd& ub = mps.ub;
    const VectorXd& lb = mps.lb;

    SparseMatrix<double> A = mps.A.sparseView();
    const VectorXd c = -mps.c;
    SparseMatrix<double> B0;

    vector<size_t> x_b_idx;
    vector<size_t> x_n_idx;

    VectorXd x_b;
    VectorXd x_n;

    vector<EtaMatrix> eta_vector;

   private:
    tuple<bool, size_t, double> choose_entering_variable();
    tuple<double, size_t> choose_leaving_variable(const VectorXd& d, size_t entering_x_n_idx, double entering_value);

    VectorXd solve_LU(const SparseMatrix<double>& A, const VectorXd& b);
    RowVectorXd solve_btran(RowVectorXd b);
    VectorXd solve_ftran(VectorXd a);
};
