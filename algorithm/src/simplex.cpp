#include <Eigen/Sparse>
#include <Eigen/UmfPackSupport>
#include <limits>
#include <tuple>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "constants.hpp"
#include "fmt/core.h"
#include "fmt/ostream.h"
#include "fmt/ranges.h"
#include "mps_reader.hpp"
#include "params.hpp"
#include "simplex.hpp"

using namespace fmt;
using namespace Eigen;

Simplex::Simplex(const mpsReader& mps, const Params& p) : mps(mps), p(p) {
    for (int i = 0; i < n; i++) {
        if (i < mps.n_cols) {
            x_n_idx.push_back(i);
        } else {
            x_b_idx.push_back(i);
        }
    }

    B0 = A.rightCols(m);

    x_b = VectorXd::Zero(m);
    x_n = VectorXd::Zero(mps.n_cols);
}
double Simplex::solve() {
    int iteration = 0;

    while (true) {
        println("=== Iteration {} ===", iteration++);
        println("x_b: {}", streamed(x_b.transpose()));
        println("x_n: {}", streamed(x_n.transpose()));
        println("x_b_idx: {}", x_b_idx);
        println("x_n_idx: {}", x_n_idx);

        auto [optimality, entering_x_n_idx, entering_value] = choose_entering_variable();

        if (optimality) {
            VectorXd c_b(m);

            for (int i = 0; i < m; ++i) {
                c_b[i] = c[x_b_idx[i]];
            }

            return x_b.dot(c_b);
        }

        auto entering_variable = x_n_idx[entering_x_n_idx];
        println("entering variable: {}", entering_variable);
        println("entering col: {}", streamed(A.col(entering_variable).toDense().transpose()));

        VectorXd a_col = A.col(entering_variable).toDense();
        VectorXd d = solve_ftran(a_col);
        println("d: {}", streamed(d.transpose()));

        auto [t, leaving_x_b_idx] = choose_leaving_variable(d, entering_x_n_idx, entering_value);

        if (leaving_x_b_idx != -1) {
            auto leaving_variable = x_b_idx[leaving_x_b_idx];

            println("leaving variable: {}", leaving_variable);
            println("leaving val: {}", t);

            println("b: {}", streamed(x_b.transpose()));

            eta_vector.push_back({.col = d, .index = leaving_x_b_idx});

            x_b_idx[leaving_x_b_idx] = entering_variable;
            x_n_idx[entering_x_n_idx] = leaving_variable;

            double temp = x_b[leaving_x_b_idx];
            x_b[leaving_x_b_idx] = x_n[entering_x_n_idx];
            x_n[entering_x_n_idx] = temp;
        } else {
            println("entering variable {} hit its bound; no pivot", entering_variable);
        }

        if (Params::get().verbose) {
            println("Press Enter to continue...");
            getchar();
        }
    }

    return nInf;
}

std::tuple<bool, size_t, double> Simplex::choose_entering_variable() {
    VectorXd c_b(m);

    for (int i = 0; i < m; ++i) {
        c_b[i] = c[x_b_idx[i]];
    }

    RowVectorXd y = solve_btran(c_b);
    println("y: {}", streamed(y));

    vector<double> reduced_costs(x_n_idx.size());
    size_t entering_x_n_idx = 0;
    double entering_value = nInf;
    bool optimal = true;
    size_t smallest_x = 99999999;
    for (size_t i = 0; i < x_n_idx.size(); i++) {
        auto x_i = x_n_idx[i];
        if (x_i > smallest_x) {
            continue;
        }
        auto x_value = x_n[i];

        reduced_costs[i] = c[x_i] - A.col(x_i).dot(y.transpose());
        println("cost: {}, x_{}: {}", reduced_costs[i], x_i, x_value);

        if (reduced_costs[i] > EPSILON_1 && x_value < ub[x_i] - EPSILON_1) {
            entering_x_n_idx = i;
            entering_value = reduced_costs[i];
            smallest_x = x_i;
            optimal = false;

        } else if (reduced_costs[i] < -EPSILON_1 && x_value > lb[x_i] + EPSILON_1) {
            entering_x_n_idx = i;
            entering_value = reduced_costs[i];
            smallest_x = x_i;
            optimal = false;
        }
    }
    println("reduced costs: {}", reduced_costs);
    return std::make_tuple(optimal, entering_x_n_idx, entering_value);
}

tuple<double, size_t> Simplex::choose_leaving_variable(const VectorXd& d, size_t entering_x_n_idx,
                                                       double entering_value) {
    println("CHOOSING LEAVING VARIABLE\n\n");
    auto x_j = x_n_idx[entering_x_n_idx];
    int leaving_x_b_idx = 0;
    double leaving_val = pInf;
    vector<double> leaving_costs(x_b_idx.size());

    double x_n_t = 0;

    if (entering_value > 0) {
        x_n_t = ub[x_j] - x_n[entering_x_n_idx];
    } else {
        x_n_t = x_n[entering_x_n_idx] - lb[x_j];
    }
    println("t: {}, x_{}: {}", x_n_t, x_j, x_n[entering_x_n_idx]);

    auto smallest_x_b_idx = 0;
    auto best_x_b_idx = 0;
    double t = pInf;
    for (int i = 0; i < m; i++) {
        auto x_b_i = x_b_idx[i];
        auto x = x_b[i];
        double x_b_t = -239;
        println("x_{}: {}", x_b_i, x);

        if (entering_value > 0 && d[i] > 0) {
            x_b_t = (x - lb[x_b_i]) / abs(d[i]);

        } else if (entering_value > 0 && d[i] < 0) {
            x_b_t = (ub[x_b_i] - x) / abs(d[i]);

        } else if (entering_value < 0 && d[i] > 0) {
            x_b_t = (ub[x_b_i] - x) / abs(d[i]);

        } else if (entering_value < 0 && d[i] < 0) {
            x_b_t = (x - lb[x_b_i]) / abs(d[i]);
        }

        println("curr t: {}", x_b_t);

        if (x_b_t < t - EPSILON_1) {
            leaving_x_b_idx = i;
            t = x_b_t;
        }
    }

    if (x_n_t < t - EPSILON_1) {
        t = x_n_t;
        leaving_x_b_idx = -1;
    }

    if (entering_value > 0) {
        x_n[entering_x_n_idx] = x_n[entering_x_n_idx] + t;
    } else {
        x_n[entering_x_n_idx] = x_n[entering_x_n_idx] - t;
    }
    println("entering x_{}: {}", x_j, x_n[entering_x_n_idx]);

    // updating xs
    for (int i = 0; i < m; i++) {
        if (entering_value > 0 && d[i] > 0) {
            x_b[i] = x_b[i] - t * abs(d[i]);
        } else if (entering_value > 0 && d[i] < 0) {
            x_b[i] = x_b[i] + t * abs(d[i]);
        } else if (entering_value < 0 && d[i] > 0) {
            x_b[i] = x_b[i] + t * abs(d[i]);
        } else if (entering_value < 0 && d[i] < 0) {
            x_b[i] = x_b[i] - t * abs(d[i]);
        }
        println("x_{}: {}, t: {}, d: {}, rc: {}", i, x_b[i], t, d[i], entering_value);
    }

    return std::make_tuple(t, leaving_x_b_idx);
}

RowVectorXd Simplex::solve_btran(RowVectorXd b) {
    VectorXd y(m);
    for (size_t i = eta_vector.size(); i-- > 0;) {
        EtaMatrix e = eta_vector[i];
        double sum = b[e.index];
        y = b;
        for (size_t j = 0; j < m; j++) {
            if (j != e.index) {
                sum -= e.col[j] * y[j];
            }
        }
        y[e.index] = sum / e.col[e.index];
        b = y;
    }

    y = solve_LU(B0.transpose(), b.transpose());

    return y.transpose();
}

VectorXd Simplex::solve_ftran(VectorXd a) {
    VectorXd d = solve_LU(B0, a);

    for (size_t i = 0; i < eta_vector.size(); i++) {
        a = d;

        EtaMatrix e = eta_vector[i];
        auto d_eta = a[e.index] / e.col(e.index);
        d[e.index] = d_eta;
        for (size_t j = 0; j < m; j++) {
            if (j != e.index) {
                d[j] = a[j] - e.col(j) * d_eta;
            }
        }
    }

    return d;
}

VectorXd Simplex::solve_LU(const SparseMatrix<double>& A, const VectorXd& b) {
    UmfPackLU<SparseMatrix<double>> solver;

    solver.compute(A);

    if (solver.info() != Eigen::Success) {
        throw std::runtime_error("UMFPACK factorization failed");
    }

    return solver.solve(b);
}
