#include <limits>
#include <stdexcept>

#include "preprocess.hpp"

ProblemData create_slack_variables(ProblemData data) {
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
    return data;
}

ProblemData Preprocessor::process(const ProblemData& data) const {
    ProblemData result = data;

    // Step 1: Remove fixed variables if requested
    if (remove_fixed) {
        result = remove_fixed_variables(std::move(result));
        result.fixed_removed = true;
    }

    // Step 2: Apply scaling if requested
    if (scaling) {
        apply_scaling(result);
        result.scaling_applied = true;
    }

    // Step 3: Add slack variables
    result = create_slack_variables(std::move(result));

    return result;
}
