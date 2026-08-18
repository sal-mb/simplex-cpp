#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "constants.hpp"
#include "fmt/core.h"
#include "fmt/ostream.h"
#include "fmt/ranges.h"
#include "simplex.hpp"

using namespace Eigen;
using namespace fmt;

Simplex::Simplex(const mpsReader& mps, const Params& p, optional<Solution> s, int phase)
    : mps(mps)
    , p(p)
    , m(mps.n_rows_inq + mps.n_rows_eq)
    , n(mps.A.cols())
    , phase(phase)
    , A(mps.A.sparseView())
    , ub(mps.ub)
    , lb(mps.lb)
    , c(-mps.c)
    , x(VectorXd::Zero(n))
    , c_b(VectorXd::Zero(m))
    , y(RowVectorXd::Zero(m)) {
    if (s.has_value()) {
        basic_idx = s->basic_idx;
        nonbasic_idx = s->nonbasic_idx;
        B0 = s->B;
        N = s->N;
        x = s->x;

    } else {
        for (int i = 0; i < n; i++) {
            if (i < mps.n_cols) {
                nonbasic_idx.push_back(i);
            } else {
                basic_idx.push_back(i);
            }
        }
        B0 = A.rightCols(m);
        N = A.leftCols(mps.n_cols);
    }

    // Initialize persistent basic cost vector c_b
    for (int i = 0; i < m; ++i) {
        c_b[i] = c[basic_idx[i]];
    }

    B0T = B0.transpose();
    B0_solver.compute(B0);
    B0T_solver.compute(B0T);
    if (B0_solver.info() != Eigen::Success) {
        throw std::runtime_error("Initial UMFPACK factorization of B0 failed");
    }
}

Solution Simplex::solve() {
    iteration = 0;
    basis_changed = true;

    if (phase == 0) {
        init_phase_0();
    }

    while (true) {
        if (p.verbose) {
            it_log();
        }

        auto entering = choose_entering_variable();
        if (entering.optimal) {
            if (phase == 0) {
                double infeasibility = 0.0;
                for (int i = 0; i < m; ++i) {
                    size_t var = basic_idx[i];
                    if (x[var] < mps.lb[var] - EPSILON_1) {
                        infeasibility += (mps.lb[var] - x[var]);
                    } else if (x[var] > mps.ub[var] + EPSILON_1) {
                        infeasibility += (x[var] - mps.ub[var]);
                    }
                }
                if (infeasibility > EPSILON_1) {
                    throw std::runtime_error("Problem is Infeasible in Phase 0");
                }
            }
            if (p.verbose) {
                println("Finished Phase {} with {} iterations", phase, iteration);
                println("[Status] Optimal solution found.");
                getchar();
            }
            refactorization();
            Solution s = {.basic_idx = basic_idx,
                          .nonbasic_idx = nonbasic_idx,
                          .B = B0,
                          .N = N,
                          .x = x,
                          .cost = -(x.dot(mps.c))};
            return s;
        }

        const size_t entering_var = nonbasic_idx[entering.index];
        if (p.verbose) {
            println("[Entering] Selected variable: x_{} (slot: {}, reduced cost: {:.6f})", entering_var, entering.index,
                    entering.reduced_cost);
            println("  Entering column: {}", streamed(A.col(entering_var).toDense().transpose()));
        }

        VectorXd a_col = A.col(entering_var).toDense();
        VectorXd d = solve_ftran(a_col);

        if (p.verbose) {
            println("  FTRAN direction d: {}", streamed(d.transpose()));
        }

        auto leaving = choose_leaving_variable(d, entering.index, entering.reduced_cost);

        if (leaving.leaving_b_idx) {
            if (p.verbose) {
                println("[Pivot] Step length t = {:.6f}", leaving.step_length);
            }
            update_basis(leaving.leaving_b_idx.value(), entering.index);
            eta_vector.push_back({.col = d, .index = leaving.leaving_b_idx.value()});

        } else if (p.verbose) {
            basis_changed = false;
            if (p.verbose) {
                println("[Bound Hit] Entering variable x_{} hit its bound at t = {:.6f}; no pivot.", entering_var,
                        leaving.step_length);
            }
        }
        iteration++;

        if ((iteration % REFACTOR_PERIOD) == 0) {
            if (p.verbose) {
                println("[Refactorization] Refactorizing basis at iteration {}", iteration);
            }

            refactorization();
        }

        if (p.verbose) {
            println("\nPress Enter to continue...");
            // getchar();
        }
    }

    return {};
}

EnteringVariableInfo Simplex::choose_entering_variable() {
    auto is_candidate = [this](double red_cost, double x_val, size_t x_i) {
        return (red_cost > EPSILON_1 && x_val < ub[x_i] - EPSILON_1) ||
               (red_cost < -EPSILON_1 && x_val > lb[x_i] + EPSILON_1);
        // candidate conditions:
        // if reduced cost > 0, and x_val can increase (increases obj value)
        // if reduced cost < 0, and x_val can decrease (also increases obj value)
    };

    if (basis_changed) {
        y = solve_btran(c_b);
        basis_changed = false;
    } else if (p.verbose) {
        println("[BTRAN] Basis unchanged. Skipping y B = c_B solve and reusing cached y.");
    }

    if (p.verbose) {
        println("[BTRAN] Dual vector y: {}", streamed(y));
        println("\n--- Evaluating Entering Variable Candidates ---");
    }

    EnteringVariableInfo result;
    size_t smallest_x = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < nonbasic_idx.size(); i++) {
        auto x_i = nonbasic_idx[i];
        if (x_i > smallest_x) {
            continue;
        }

        double x_val = x[x_i];
        double red_cost = c[x_i] - A.col(x_i).dot(y.transpose());

        if (p.verbose) {
            println("  Nonbasic x_{} (slot {}): val = {:.6f}, red_cost = {:.6f}", x_i, i, x_val, red_cost);
        }

        if (is_candidate(red_cost, x_val, x_i)) {
            result.index = i;
            result.reduced_cost = red_cost;
            result.optimal = false;
            smallest_x = x_i;
        }
    }

    return result;
}

LeavingVariableInfo Simplex::choose_leaving_variable(const VectorXd& d, size_t entering_nonbasic_slot,
                                                     double reduced_cost) {
    if (p.verbose) {
        println("\n--- Choosing Leaving Variable ---");
    }

    LeavingVariableInfo result;
    size_t entering_var = nonbasic_idx[entering_nonbasic_slot];

    // CALCULATING STEP(T) OF ENTERING NON BASIC VARIABLE X_N
    double entering_dir = (reduced_cost > 0) ? 1.0 : -1.0;
    double x_n_t = pInf;

    // If entering_dir or reduced_cost > 0
    // x_N has to increase
    // else x_N has to decrease
    if (entering_dir > EPSILON_1) {
        x_n_t = ub[entering_var] - x[entering_var];
    } else {
        x_n_t = x[entering_var] - lb[entering_var];
    }
    x_n_t = std::max(0.0, x_n_t);

    // CALCULATING STEP(T) OF BASIC VARIABLES X_B
    auto get_basic_t_limit = [&](int i) -> double {
        // Basic variables update according to: x_B = x_B - d_i * (t * entering_dir).
        // When (d_i * entering_dir > 0): x_n INCREASES, so x_B DECREASES, so it moves toward its lower bound (lb).
        // When (d_i * entering_dir < 0): x_n DECREASES, so x_B INCREASES, so it moves toward its upper bound (ub).
        double d_i = d[i];
        if (std::abs(d_i) <= EPSILON_1) {
            return pInf;
        }

        size_t basic_var = basic_idx[i];

        bool decreases = (d_i * entering_dir > EPSILON_1);
        double t = 0;
        if (decreases) {
            if (lb[basic_var] > nInf + EPSILON_1) {
                t = (x[basic_var] - lb[basic_var]) / std::abs(d_i);
                return std::max(0.0, t);
            }
        } else if (ub[basic_var] < pInf - EPSILON_1) {
            t = (ub[basic_var] - x[basic_var]) / std::abs(d_i);
            return std::max(0.0, t);
        }
        return pInf;
    };

    double t = pInf;
    int leaving_b_idx = -1;
    size_t smallest_var_idx = std::numeric_limits<size_t>::max();

    for (int i = 0; i < m; i++) {
        double x_b_t = get_basic_t_limit(i);

        if (p.verbose) {
            println("  Basic x_{} (slot {}): val = {:.6f}, step_limit = {:.6f}", basic_idx[i], i, x[basic_idx[i]],
                    x_b_t);
        }

        bool smaller = (x_b_t < t - EPSILON_1);
        bool tie = (std::abs(x_b_t - t) < EPSILON_1);

        if (smaller) {
            t = x_b_t;
            leaving_b_idx = i;
            smallest_var_idx = basic_idx[i];
        } else if (tie && basic_idx[i] < smallest_var_idx) {
            // smallest index rule
            leaving_b_idx = i;
            smallest_var_idx = basic_idx[i];
        }
    }
    println("x_n_t: {}", x_n_t);
    println("t: {}", t);
    // UPDATING VARIABLES
    if (x_n_t < t - EPSILON_1) {
        t = x_n_t;
        leaving_b_idx = -1;
    }
    if (t >= pInf - EPSILON_1) {
        throw std::runtime_error("Problem Unbounded");
    }

    auto force_lb_ub = [&](int entering_var) {
        if (std::abs(x[entering_var] - lb[entering_var]) < EPSILON_1) {
            x[entering_var] = lb[entering_var];
        } else if (std::abs(x[entering_var] - ub[entering_var]) < EPSILON_1) {
            x[entering_var] = ub[entering_var];
        }
    };
    x[entering_var] += t * entering_dir;
    force_lb_ub(entering_var);

    for (int i = 0; i < m; i++) {
        size_t basic_var = basic_idx[i];

        x[basic_var] -= d[i] * t * entering_dir;
        force_lb_ub(basic_var);

        if (p.verbose) {
            println("  Updated x_{}: val = {:.6f}, t = {:.6f}, d_i = {:.6f}, red_cost = {:.6f}", basic_var,
                    x[basic_var], t, d[i], reduced_cost);
        }
    }

    result.step_length = t;
    result.leaving_b_idx = (leaving_b_idx != -1) ? std::optional<size_t>(leaving_b_idx) : std::nullopt;

    // getchar();
    return result;
}

void Simplex::update_basis(size_t leaving_basis_idx, size_t entering_nonbasic_idx) {
    size_t leaving_var = basic_idx[leaving_basis_idx];
    size_t entering_var = nonbasic_idx[entering_nonbasic_idx];

    if (p.verbose) {
        println("[Update Basis] Leaving: x_{} (slot {}), Entering: x_{} (slot {})", leaving_var, leaving_basis_idx,
                entering_var, entering_nonbasic_idx);

        VectorXd x_b(m);
        for (int i = 0; i < m; ++i) {
            x_b[i] = x[basic_idx[i]];
        }
        println("  Current basic values x_b: {}", streamed(x_b.transpose()));
    }

    basic_idx[leaving_basis_idx] = entering_var;
    nonbasic_idx[entering_nonbasic_idx] = leaving_var;

    N.col(entering_nonbasic_idx) = A.col(leaving_var);

    if (phase == 0) {
        auto entering_val = x[entering_var];
        if (entering_val < mps.lb[entering_var] - EPSILON_1) {
            c[entering_var] = 1;
            auto temp = mps.lb[entering_var];
            lb[entering_var] = nInf;
            ub[entering_var] = temp;
        } else if (entering_val > mps.ub[entering_var] + EPSILON_1) {
            c[entering_var] = -1;
            auto temp = mps.ub[entering_var];
            ub[entering_var] = pInf;
            lb[entering_var] = temp;
        }

        c[leaving_var] = 0;
        ub[leaving_var] = mps.ub[leaving_var];
        lb[leaving_var] = mps.lb[leaving_var];
    }
    // println("entenring var: {}, leaving var {}", entering_var, leaving_var);
    // getchar();

    c_b[leaving_basis_idx] = c[entering_var];

    // mark basis change
    basis_changed = true;
}

RowVectorXd Simplex::solve_btran(RowVectorXd b) {
    for (int i = eta_vector.size() - 1; i >= 0; i--) {
        const EtaMatrix& e = eta_vector[i];

        double b_eta = b(e.index);
        b(e.index) = 0;
        double new_b_eta = (b_eta - b.dot(e.col)) / e.col(e.index);
        b(e.index) = abs(new_b_eta) < EPSILON_1 ? 0.0 : new_b_eta;
    }
    y = B0T_solver.solve(b.transpose()).transpose();
    return (y.array().abs() < EPSILON_1).select(0.0, y);
}

VectorXd Simplex::solve_ftran(const VectorXd& a) {
    VectorXd d = B0_solver.solve(a);

    d = (d.array().abs() < EPSILON_1).select(0.0, d);
    for (const auto& e : eta_vector) {
        double d_eta = d(e.index) / e.col(e.index);

        d -= e.col * d_eta;
        d(e.index) = d_eta;

        d = (d.array().abs() < EPSILON_1).select(0.0, d);
    }

    return d;
}

void Simplex::it_log() const {
    println("\n=================== Iteration {} ===================", iteration);

    VectorXd x_b(m);
    for (int i = 0; i < m; ++i) {
        x_b[i] = x[basic_idx[i]];
    }

    VectorXd x_n(nonbasic_idx.size());
    for (size_t i = 0; i < nonbasic_idx.size(); ++i) {
        x_n[i] = x[nonbasic_idx[i]];
    }

    if (phase == 0) {
        println("c: {}", streamed(c.transpose()));
    }
    println("x_b: {}", streamed(x_b.transpose()));
    println("x_n: {}", streamed(x_n.transpose()));
    println("basic_idx: {}", basic_idx);
    println("nonbasic_idx: {}", nonbasic_idx);
    getchar();
}

void Simplex::refactorization() {
    B0.resize(m, m);
    B0.setZero();

    for (int col = 0; col < m; ++col) {
        B0.col(col) = A.col(basic_idx[col]);
    }

    B0T = B0.transpose();
    B0_solver.compute(B0);
    B0T_solver.compute(B0T);
    if (B0_solver.info() != Eigen::Success) {
        println("\n!!! UMFPACK FAILED !!!");

        // teste rápido para confirmar singularidade
        FullPivLU<MatrixXd> lu(B0.toDense());
        println("rank(B0) = {}", lu.rank());
        println("rows(B0) = {}, cols(B0) = {}", B0.rows(), B0.cols());

        throw std::runtime_error("Refactorization failed");
    }

    eta_vector.clear();
}

void Simplex::init_phase_0() {
    if (p.verbose) {
        println("PHASE 0");
    }
    // initializng x
    for (int i = 0; i < mps.n_cols; i++) {
        auto var = nonbasic_idx[i];
        // var with upper and lower bound
        if (ub[var] < pInf - EPSILON_1 && lb[var] > nInf + EPSILON_1) {
            x[var] = ub[var];
            continue;
        }

        // var with upper bound
        if (ub[var] < pInf - EPSILON_1) {
            x[var] = ub[var];
            continue;
        }

        // var with lower bound
        if (lb[var] > nInf + EPSILON_1) {
            x[var] = lb[var];
            continue;
        }
        // free var
        x[var] = 0;
    }

    VectorXd x_n = x.topRows(mps.n_cols);

    VectorXd rhs = mps.b - N * x_n;
    VectorXd x_b = B0_solver.solve(rhs);

    // Assign computed basic values back into x
    for (int i = 0; i < m; i++) {
        x[basic_idx[i]] = x_b[i];
    }

    // initializing c vector
    c = VectorXd::Zero(n);

    for (int i = 0; i < m; i++) {
        auto x_i = basic_idx[i];
        auto x_val = x[x_i];

        // if x_b < lb we have to increase the variable until lb
        // so the new lb will be -inf and the ub will be the lb
        if (x_val < mps.lb[x_i] - EPSILON_1) {
            c[x_i] = 1;
            auto temp = mps.lb[x_i];
            lb[x_i] = nInf;
            ub[x_i] = temp;
        } else if (x_val > mps.ub[x_i] + EPSILON_1) {
            // if x_b > ub we have to decrease the variable until ub
            // so the new lb will be the ub and the ub will be inf
            c[x_i] = -1;
            auto temp = mps.ub[x_i];
            ub[x_i] = pInf;
            lb[x_i] = temp;
        }
        c_b[i] = c[x_i];
    }
}
