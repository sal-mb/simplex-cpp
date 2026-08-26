#include <limits>
#include <stdexcept>

#include "preprocess.hpp"

Preprocessor::Preprocessor(const Params& p, ProblemData data) : p(p), data(data), changed(false) {}

void Preprocessor::create_slack_variables() {
    const int m = data.m;
    const int n = data.n;

    if (static_cast<int>(data.row_types.size()) != m) {
        throw std::runtime_error("create_slack_variables: row_types size does not match m");
    }

    Eigen::MatrixXd A_ext = Eigen::MatrixXd::Zero(m, n + m);
    A_ext.leftCols(n) = data.A;

    Eigen::VectorXd c_ext = Eigen::VectorXd::Zero(n + m);
    c_ext.head(n) = data.c;

    Eigen::VectorXd lb_ext(n + m);
    Eigen::VectorXd ub_ext(n + m);
    lb_ext.head(n) = data.lb;
    ub_ext.head(n) = data.ub;

    const double inf = std::numeric_limits<double>::infinity();

    for (int i = 0; i < m; i++) {
        A_ext(i, n + i) = -1.0;

        switch (data.row_types[i]) {
            case 'L':
                lb_ext(n + i) = -inf;
                ub_ext(n + i) = data.b(i);
                break;
            case 'G':
                lb_ext(n + i) = data.b(i);
                ub_ext(n + i) = inf;
                break;
            case 'E':
                lb_ext(n + i) = data.b(i);
                ub_ext(n + i) = data.b(i);
                break;
            default:
                throw std::runtime_error("create_slack_variables: unknown row type");
        }
    }

    data.A = std::move(A_ext);
    data.c = std::move(c_ext);
    data.lb = std::move(lb_ext);
    data.ub = std::move(ub_ext);
    data.b = Eigen::VectorXd::Zero(m);
}

ProblemData Preprocessor::process() {
    check_contradicting_bounds();

    int pass = 0;
    constexpr int MAX_PASSES = 10;
    changed = true; // Initialized to enter the loop

    // Presolve Pass Loop: runs techniques until convergence or MAX_PASSES
    while (changed && pass < MAX_PASSES) {
        changed = false; // Reset changed flag at the beginning of each pass
        pass++;

        if (p.tighten_bounds) {
            tighten_individual_bounds();
        }

        if (p.remove_empty_rows) {
            remove_empty_rows();
        }

        if (p.remove_empty_cols) {
            remove_empty_columns();
        }

        if (p.remove_redundant_forcing) {
            remove_redundant_forcing_constraints();
        }

        if (p.remove_fixed) {
            remove_fixed_variables();
        }

        if (p.remove_singleton_rows) {
            remove_singleton_rows();
        }
    }

    // Post-presolve scaling: executed once outside the loop
    if (p.scaling) {
        apply_scaling();
        data.scaling_applied = true;
    }

    create_slack_variables();

    return data;
}
