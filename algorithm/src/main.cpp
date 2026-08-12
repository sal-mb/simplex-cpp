#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <Eigen/Dense>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>

#include <cstdio>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "constants.hpp"
#include "mps_reader.hpp"
#include "params.hpp"
#include "simplex.hpp"

using namespace fmt;
using namespace Eigen;

VectorXd simplex(const mpsReader &instance);

std::pair<SparseMatrix<double>, SparseMatrix<double>>
split_basis(const SparseMatrix<double> &A, const std::vector<int> &x_b,
            const std::vector<int> &x_n);

struct var {
  double value = 0;
  double lb = 0;
  double ub = pInf;
};

template <> struct fmt::formatter<var> {
  constexpr auto parse(fmt::format_parse_context &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const var &v, FormatContext &ctx) const {
    return fmt::format_to(ctx.out(), "value={}, lb={}, ub={}", v.value, v.lb,
                          v.ub);
  }
};

int main(int argc, char *argv[]) {
  Params::parse(argc, argv);
  auto &p = Params::get();

  mpsReader reader;
  reader.read(p.instance_file, p.preprocess);

  reader.A.block(0, reader.n_cols, reader.n_rows_inq, reader.n_rows_inq) =
      MatrixXd::Identity(reader.n_rows_inq, reader.n_rows_inq);

  const int m = reader.n_rows_inq + reader.n_rows_eq;
  const int n = reader.n_cols + m;

  println("Problem: {}", reader.Name);
  println("Rows: {}, Cols: {}", reader.n_rows, reader.n_cols);
  println("m: {}, n: {}", m, n);
  println("A:\n{}", streamed(reader.A));
  println("b:\n{}", streamed(reader.b.transpose()));
  println("c:\n{}", streamed(reader.c.transpose()));
  println("lb:\n{}", streamed(reader.lb.transpose()));
  println("ub:\n{}", streamed(reader.ub.transpose()));

  Simplex solver(reader, p);
  double cost = solver.solve();
  println("Objective cost: {}", cost);

  return 0;
}
