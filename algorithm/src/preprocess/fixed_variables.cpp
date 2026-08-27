#include <cmath>
#include <stdexcept>
#include <vector>

#include "fmt/core.h"
#include "fmt/ostream.h"
#include "fmt/ranges.h"
#include "preprocess.hpp"
using namespace fmt;

PresolveResult remove_fixed_variables(ProblemData& data, const Params& p) {
    const int n_orig = data.n;

    // Find columns with lb == ub (fixed variables)
    // These variables will be removed, and the constraint matrix adjusted

    // Count how many fixed variables we have
    std::vector<bool> is_fixed(n_orig, false);
    Eigen::VectorXd fixed_contribution = Eigen::VectorXd::Zero(n_orig);
    int fixed_count = 0;

    for (int j = 0; j < n_orig; j++) {
        if (std::abs(data.lb(j) - data.ub(j)) < p.eps) {
            is_fixed[j] = true;
            fixed_contribution(j) = data.lb(j); // == ub(j)
            fixed_count++;
        }
    }

    if (fixed_count == 0) {
        return {};
    }

    // Adjust RHS b: NOTE this is NOT "unchanged" -- each fixed column's
    // contribution must be folded in (b -= A.col(j) * value) before the
    // column is dropped, or the reduced system stops being equivalent
    // to the original one. Same for the objective: the constant
    // c(j) * value that a removed fixed variable used to contribute is
    // preserved in obj_constant so it can be added back to the final
    // objective later. This must happen here, while A still has its
    // original m x n_orig shape -- doing it after resizing A would have
    // nothing left to fold from.
    data.b -= data.A * fixed_contribution;
    data.obj_constant += data.c.dot(fixed_contribution);

    // If all variables are fixed, just return as-is
    const int n_new = n_orig - fixed_count;

    if (n_new == 0) {
        // Update n (number of original variables after removal)
        data.n = 0;
        data.A = Eigen::MatrixXd(data.m, 0);
        data.c = Eigen::VectorXd(0);
        data.lb = Eigen::VectorXd(0);
        data.ub = Eigen::VectorXd(0);
        return {true, 0, fixed_count};
    }

    // Adjust the constraint matrix A: keep only non-fixed columns.
    // At this stage A is still raw (m x n_orig, no slacks yet -- those
    // are only added later, by create_slack_variables), so there is no
    // slack-index bookkeeping to worry about here.
    Eigen::MatrixXd new_A(data.m, n_new);
    // Adjust bounds: only keep non-fixed variables
    Eigen::VectorXd new_lb(n_new);
    Eigen::VectorXd new_ub(n_new);
    // Adjust cost vector c
    Eigen::VectorXd new_c(n_new);

    int new_idx = 0;
    for (int j = 0; j < n_orig; j++) {
        if (!is_fixed[j]) {
            new_A.col(new_idx) = data.A.col(j);
            new_c(new_idx) = data.c(j);
            new_lb(new_idx) = data.lb(j);
            new_ub(new_idx) = data.ub(j);
            new_idx++;
        }
    }

    // Update n (number of original variables after removal)
    data.n = n_new;
    data.A = std::move(new_A);
    data.c = std::move(new_c);
    data.lb = std::move(new_lb);
    data.ub = std::move(new_ub);
    return {true, 0, fixed_count};
}

void check_contradicting_bounds(const ProblemData& data, const Params& p) {
    for (int i = 0; i < data.n; i++) {
        if (data.ub(i) < data.lb(i) - p.eps) {
            throw std::runtime_error("CONTRADICTING BOUNDS");
        }
    }
}
