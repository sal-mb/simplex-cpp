#include <Eigen/Dense>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include "mps_reader.hpp"
#include "params.hpp"
#include "simplex.hpp"

using namespace fmt;
using namespace Eigen;

VectorXd simplex(const mpsReader& instance);

int main(int argc, char* argv[]) {
    Params::parse(argc, argv);
    auto& p = Params::get();

    mpsReader reader;
    reader.read(p.instance_file, static_cast<int>(p.preprocess));

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

    Simplex solver_p0(reader, p, nullopt, 0);
    Solution s = solver_p0.solve();
    if (p.verbose) {
        getchar();
        println("Finished Phase 0");
        println("Starting Phase 1 with: \nbasic_idx: {}\nnonbasic_idx: {} \nB:\n{}\nN:\n{}\nx: {}\ncost: {}",
                s.basic_idx, s.nonbasic_idx, streamed(s.B.toDense()), streamed(s.N.toDense()), s.x, s.cost);
        getchar();
    }
    Simplex solver_p1(reader, p, s, 1);
    s = solver_p1.solve();
    println("Objective cost: {}", s.cost);

    return 0;
}
