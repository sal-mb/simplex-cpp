#pragma once

#include "mps_reader.hpp"
#include "params.hpp"
#include <vector>
class Simplex {
public:
  const mpsReader &mps;
  Simplex(const mpsReader &mps, const Params &p);

  double solve();

  const Params &p;
  const int m = mps.n_rows_inq + mps.n_rows_eq;
  const int n = mps.n_cols + m;

  vector<size_t> x_b_idx;
  vector<size_t> x_n_idx;
  const MatrixXd &A = mps.A;
  const VectorXd &c = mps.c;

  VectorXd x_b;
  VectorXd x_n;

  MatrixXd B;

  std::vector<MatrixXd> eta_vector;

private:
  std::tuple<bool, size_t> choose_entering_variable();
  std::tuple<double, size_t> choose_leaving_variable(const VectorXd &d);
};
