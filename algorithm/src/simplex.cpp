#include "constants.hpp"
#include "fmt/core.h"
#include "fmt/ostream.h"
#include "fmt/ranges.h"
#include "mps_reader.hpp"
#include "params.hpp"
#include "simplex.hpp"
#include <tuple>

using namespace fmt;
using namespace Eigen;

Simplex::Simplex(const mpsReader &mps, const Params &p) : mps(mps), p(p) {
  for (int i = 0; i < n; i++) {
    if (i < mps.n_cols) {
      x_n_idx.push_back(i);
    } else {
      x_b_idx.push_back(i);
    }
  }

  MatrixXd first_b(m, m);
  eta_vector = {Eigen::MatrixXd::Identity(m, m)};

  B = eta_vector[0];
  x_b = VectorXd(m);
  for (int i = 0; i < m; i++) {
    x_b[i] = mps.ub[x_b_idx[i]];
  }
}
double Simplex::solve() {

  int iteration = 0;
  while (true) {
    println("=== Iteration {} ===", iteration++);
    println("new_b: {}", streamed(x_b.transpose()));
    println("x_b_idx: {}", x_b_idx);
    println("x_n_idx: {}", x_n_idx);
    println("B:\n{}", streamed(B));

    auto [optimality, entering_x_n_idx] = choose_entering_variable();

    if (optimality) {
      VectorXd c_b(m);

      for (int i = 0; i < m; ++i) {
        c_b[i] = c[x_b_idx[i]];
      }

      return x_b.dot(c_b);
    }

    auto entering_variable = x_n_idx[entering_x_n_idx];
    println("entering variable: {}", entering_variable);
    println("entering col: {}", streamed(A.col(entering_variable).transpose()));

    VectorXd d = B.colPivHouseholderQr().solve(A.col(entering_variable));
    println("d: {}", streamed(d.transpose()));

    auto [t, leaving_x_b_idx] = choose_leaving_variable(d);
    auto leaving_variable = x_b_idx[leaving_x_b_idx];

    println("leaving variable: {}", leaving_variable);
    println("leaving val: {}", t);

    x_b = x_b - d * t;
    x_b[leaving_x_b_idx] = t;
    println("b: {}", streamed(x_b.transpose()));

    MatrixXd E = MatrixXd::Identity(m, m);
    E.col(leaving_x_b_idx) = d;
    B = B * E;
    eta_vector.push_back(E);

    x_b_idx[leaving_x_b_idx] = entering_variable;
    x_n_idx[entering_x_n_idx] = leaving_variable;

    if (Params::get().verbose) {
      println("Press Enter to continue...");
      getchar();
    }
  }

  return nInf;
}

std::tuple<bool, size_t> Simplex::choose_entering_variable() {

  VectorXd c_b(m);

  for (int i = 0; i < m; ++i) {
    c_b[i] = c[x_b_idx[i]];
  }

  VectorXd y_col = B.transpose().colPivHouseholderQr().solve(c_b);
  RowVectorXd y = y_col.transpose();
  println("y: {}", streamed(y));

  vector<double> reduced_costs(x_n_idx.size());
  size_t entering_x_n_idx = 0;
  double entering_value = nInf;
  bool optimal = true;
  for (size_t i = 0; i < x_n_idx.size(); i++) {
    reduced_costs[i] = c[x_n_idx[i]] - y.dot(A.col(x_n_idx[i]));
    if (EPSILON_1 + reduced_costs[i] > entering_value) {
      entering_x_n_idx = i;
      entering_value = reduced_costs[i];
      if (entering_value > 0) {
        optimal = false;
      }
    }
  }
  println("reduced costs: {}", reduced_costs);
  return std::make_tuple(optimal, entering_x_n_idx);
}

std::tuple<double, size_t> Simplex::choose_leaving_variable(const VectorXd &d) {

  size_t leaving_x_b_idx = 0;
  double leaving_val = pInf;
  vector<double> leaving_costs(x_b_idx.size());

  for (size_t i = 0; i < x_b_idx.size(); i++) {
    if (d[i] < EPSILON_1) {
      leaving_costs[i] = 0;
    } else {
      leaving_costs[i] = x_b[i] / d[i];
    }
    if (leaving_costs[i] < leaving_val) {
      leaving_x_b_idx = i;
      leaving_val = leaving_costs[i];
    }
  }
  if (p.verbose) {
    println("leaving costs: {}", leaving_costs);
  }
  return std::make_tuple(leaving_val, leaving_x_b_idx);
}
