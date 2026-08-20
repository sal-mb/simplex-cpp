#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
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

Simplex::Simplex(const mpsReader& mps, const Params& p, std::optional<Solution> s, int phase)
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
        // in case of no initial solution provided
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

    // init c_b
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

Solution Simplex::solve() { // NOLINT
    iteration = 0;
    basis_changed = true;

    if (phase == 0) {
        init_phase_0();
    }

    while (true) {
        if (p.verbose) {
            it_log();
        }

        EnteringVariableInfo entering = choose_entering_variable();

        if (entering.optimal) {
            if (phase == 0 && compute_infeasibility() > p.eps) {
                throw std::runtime_error("Problem is Infeasible in Phase 0");
            }
            if (p.verbose) {
                println("Finished Phase {} with {} iterations", phase, iteration);
                println("[Status] Optimal solution found.");
                getchar();
            }
            // refactoring before returning
            refactorization();
            return get_solution();
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

        LeavingVariableInfo leaving = choose_leaving_variable(d, entering.index, entering.reduced_cost);

        // leaving_b_idx is an optional, if it doesnt have a value it means bound snap
        if (leaving.leaving_b_idx) {
            if (p.verbose) {
                println("[Pivot] Step length t = {:.6f}", leaving.step_length);
            }
            update_basis(leaving.leaving_b_idx.value(), entering.index);
            eta_vector.push_back({.col = d, .index = leaving.leaving_b_idx.value()});
        } else if (p.verbose) {
            println("[Bound Hit] Entering variable x_{} hit its bound at t = {:.6f}; no pivot.", entering_var,
                    leaving.step_length);
        }

        // ETA refactorization
        if (!eta_vector.empty() && eta_vector.size() == p.refactor_period) {
            if (p.verbose) {
                println("[Refactorization] Refactorizing basis at iteration {}", iteration);
            }
            refactorization();
        }

        iteration++;
    }
}

EnteringVariableInfo Simplex::choose_entering_variable() {
    auto is_candidate = [this](double red_cost, double x_val, size_t x_i) {
        return (red_cost > p.eps && x_val < ub[x_i] - p.eps) || (red_cost < -p.eps && x_val > lb[x_i] + p.eps);
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
        // force bland's rule
        if (x_i > smallest_x) {
            continue;
        }

        double x_val = x[x_i];
        double red_cost = c[x_i] - A.col(x_i).dot(y.transpose());

        if (p.verbose) {
            println("  Nonbasic x_{} (slot {}): val = {}, red_cost = {}, lb = {}, ub = {}", x_i, i, x_val, red_cost,
                    lb[x_i], ub[x_i]);
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

LeavingVariableInfo Simplex::choose_leaving_variable(const VectorXd& d, size_t entering_nonbasic_slot, // NOLINT
                                                     double reduced_cost) {
    if (p.verbose) {
        println("\n--- Choosing Leaving Variable ---");
    }

    const size_t entering_var = nonbasic_idx[entering_nonbasic_slot];

    // entering_dir > 0: x_N has to increase. entering_dir < 0: x_N has to decrease.
    const double entering_dir = (reduced_cost > 0) ? 1.0 : -1.0;

    // How far the entering variable can move on its own before hitting its opposite bound.
    const double entering_self_limit =
            (entering_dir > 0) ? (ub[entering_var] - x[entering_var]) : (x[entering_var] - lb[entering_var]);

    // Basic variables update according to: x_B = x_B - d_i * (t * entering_dir).
    // When (d_i * entering_dir > 0): x_N INCREASES, so x_B DECREASES, moving toward its lb.
    // When (d_i * entering_dir < 0): x_N DECREASES, so x_B INCREASES, moving toward its ub.
    auto basic_variable_limit = [&](int i) -> double {
        const double d_i = d[i];
        if (std::abs(d_i) <= p.eps) {
            return pInf;
        }

        const size_t basic_var = basic_idx[i];
        const bool decreases = (d_i * entering_dir > 0);

        if (decreases) {
            if (lb[basic_var] <= nInf + p.eps) {
                return pInf; // unbounded below: never hits a lower bound
            }
            return std::max(0.0, (x[basic_var] - lb[basic_var]) / std::abs(d_i));
        }
        if (ub[basic_var] >= pInf - p.eps) {
            return pInf; // unbounded above: never hits an upper bound
        }
        return std::max(0.0, (ub[basic_var] - x[basic_var]) / std::abs(d_i));
    };

    double t = pInf;
    int leaving_b_idx = -1;
    size_t smallest_var_idx = std::numeric_limits<size_t>::max();

    for (int i = 0; i < m; i++) {
        double limit = basic_variable_limit(i);

        if (p.verbose) {
            println("  Basic x_{} (slot {}): val = {:.6f}, step_limit = {:.6f}", basic_idx[i], i, x[basic_idx[i]],
                    limit);
        }
        if (limit >= pInf - p.eps) {
            continue;
        }

        // if tie, break by bland's rule
        bool smaller = (limit < t - p.eps);
        bool tie = (std::abs(limit - t) < p.eps);

        if (smaller) {
            t = limit;
            leaving_b_idx = i;
            smallest_var_idx = basic_idx[i];
        } else if (tie && basic_idx[i] < smallest_var_idx) {
            // smallest index rule
            leaving_b_idx = i;
            smallest_var_idx = basic_idx[i];
        }
    }

    // bound flip leaving var
    if (entering_self_limit < t - p.eps) {
        t = entering_self_limit;
        leaving_b_idx = -1;
    }

    if (t >= pInf - p.eps) {
        throw std::runtime_error("Problem Unbounded");
    }

    // try to solve numerical issue
    auto snap_to_bound = [&](size_t var) {
        if (std::abs(x[var] - lb[var]) < p.eps) {
            x[var] = lb[var];
        } else if (std::abs(x[var] - ub[var]) < p.eps) {
            x[var] = ub[var];
        }
    };

    // updating vars
    x[entering_var] += t * entering_dir;
    snap_to_bound(entering_var);

    for (int i = 0; i < m; i++) {
        size_t basic_var = basic_idx[i];

        x[basic_var] -= d[i] * t * entering_dir;
        snap_to_bound(basic_var);

        if (p.verbose) {
            println("  Updated x_{}: val = {:.6f}, t = {:.6f}, d_i = {:.6f}, red_cost = {:.6f}", basic_var,
                    x[basic_var], t, d[i], reduced_cost);
        }
    }

    LeavingVariableInfo result;
    result.step_length = t;
    result.leaving_b_idx = (leaving_b_idx != -1) ? std::optional<size_t>(leaving_b_idx) : std::nullopt;
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
        // update bounds and c value of the entering and leaving var
        update_phase_0_costs(entering_var);
        for (int i = 0; i < m; i++) {
            update_phase_0_costs(basic_idx[i]);
        }
        update_phase_0_costs(leaving_var);
    }

    c_b[leaving_basis_idx] = c[entering_var];

    // mark basis change
    basis_changed = true;
}

RowVectorXd Simplex::solve_btran(RowVectorXd b) {
    for (int i = static_cast<int>(eta_vector.size()) - 1; i >= 0; i--) {
        const EtaMatrix& e = eta_vector[i];

        double b_eta = b(e.index);
        b(e.index) = 0;
        b(e.index) = (b_eta - b.dot(e.col)) / e.col(e.index);
    }
    return B0T_solver.solve(b.transpose()).transpose();
}

VectorXd Simplex::solve_ftran(const VectorXd& a) {
    VectorXd d = B0_solver.solve(a);

    for (const auto& e : eta_vector) {
        double d_eta = d(e.index) / e.col(e.index);

        d -= e.col * d_eta;
        d(e.index) = d_eta;
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

        FullPivLU<MatrixXd> lu(B0.toDense());
        println("rank(B0) = {}", lu.rank());
        println("rows(B0) = {}, cols(B0) = {}", B0.rows(), B0.cols());

        throw std::runtime_error("Refactorization failed");
    }

    eta_vector.clear();
}

void Simplex::init_phase_0() {
    // finding inital basic feasible solution
    if (p.verbose) {
        println("PHASE 0");
    }

    // assign the variables to its own bounds
    for (int i = 0; i < mps.n_cols; i++) {
        auto var = nonbasic_idx[i];

        if (lb[var] > nInf + p.eps) {
            x[var] = lb[var];
        } else if (ub[var] < pInf - p.eps) {
            x[var] = ub[var];
        } else if (lb[var] > nInf + p.eps && ub[var] < pInf - p.eps) {
            x[var] = lb[var];
        } else {
            x[var] = 0;
        }
    }

    // finding initial x_b
    VectorXd x_n = x.topRows(mps.n_cols);
    VectorXd rhs = mps.b - N * x_n;
    VectorXd x_b = B0_solver.solve(rhs);
    for (int i = 0; i < m; i++) {
        x[basic_idx[i]] = x_b[i];
    }

    // build c vector
    c = VectorXd::Zero(n);
    for (int i = 0; i < m; i++) {
        update_phase_0_costs(basic_idx[i]);
        c_b[i] = c[basic_idx[i]];
    }
}

void Simplex::update_phase_0_costs(size_t var) {
    double val = x[var];

    if (val < mps.lb[var] - p.eps) {
        // below its true lower bound: temporarily allow it to range up to lb, and reward
        // increasing it (that's the direction that restores feasibility).
        c[var] = 1;
        lb[var] = nInf;
        ub[var] = mps.lb[var];
    } else if (val > mps.ub[var] + p.eps) {
        // above its true upper bound, temporarily allow it to range down to ub, and
        // reward decreasing it.
        c[var] = -1;
        ub[var] = pInf;
        lb[var] = mps.ub[var];
    } else {
        // within its true bounds, so no penalty, restore to true bounds.
        c[var] = 0;
        lb[var] = mps.lb[var];
        ub[var] = mps.ub[var];
    }
}

double Simplex::compute_infeasibility() const {
    double infeasibility = 0.0;
    for (int i = 0; i < m; ++i) {
        size_t var = basic_idx[i];
        if (x[var] < mps.lb[var]) {
            infeasibility += (mps.lb[var] - x[var]);
        } else if (x[var] > mps.ub[var]) {
            infeasibility += (x[var] - mps.ub[var]);
        }
    }
    return infeasibility;
}

Solution Simplex::get_solution() const {
    return Solution{
            .basic_idx = basic_idx, .nonbasic_idx = nonbasic_idx, .B = B0, .N = N, .x = x, .cost = x.dot(mps.c)};
}
