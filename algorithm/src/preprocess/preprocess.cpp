#include <array>
#include <limits>
#include <stdexcept>

#include "preprocess.hpp"

Preprocessor::Preprocessor(const Params& p, ProblemData data) : p(p), data(data) {}

void create_slack_variables(ProblemData& data) {
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

namespace {

    // The presolve pipeline: each enabled technique runs once per pass, in
    // this order, until none of them report a change (or MAX_PASSES is
    // hit in Preprocessor::process()). This is the only place the set of
    // active techniques and their order is defined -- adding, removing,
    // reordering, or toggling a technique never requires touching
    // Preprocessor::process() itself.
    const std::array<PresolveTechnique, 6> K_PRESOLVE_TECHNIQUES{{
            {.name = "tighten_bounds",
             .removed_flag = nullptr,
             .enabled = [](const Params& p) { return p.tighten_bounds; },
             .apply = tighten_individual_bounds},
            {.name = "empty_rows",
             .removed_flag = &ProblemData::empty_rows_removed,
             .enabled = [](const Params& p) { return p.remove_empty_rows; },
             .apply = remove_empty_rows},
            {.name = "empty_cols",
             .removed_flag = &ProblemData::empty_cols_removed,
             .enabled = [](const Params& p) { return p.remove_empty_cols; },
             .apply = remove_empty_columns},
            {.name = "redundant_forcing",
             .removed_flag = &ProblemData::redundant_forcing_removed,
             .enabled = [](const Params& p) { return p.remove_redundant_forcing; },
             .apply = remove_redundant_forcing_constraints},
            {.name = "fixed_vars",
             .removed_flag = &ProblemData::fixed_removed,
             .enabled = [](const Params& p) { return p.remove_fixed; },
             .apply = remove_fixed_variables},
            {.name = "singleton_rows",
             .removed_flag = &ProblemData::singleton_rows_removed,
             .enabled = [](const Params& p) { return p.remove_singleton_rows; },
             .apply = remove_singleton_rows},
    }};

} // namespace

ProblemData Preprocessor::process() {
    check_contradicting_bounds(data, p);

    constexpr int MAX_PASSES = 10;
    int pass = 0;
    bool changed = true; // Initialized to enter the loop

    // Presolve Pass Loop: runs techniques until convergence or MAX_PASSES
    while (changed && pass < MAX_PASSES) {
        changed = false;
        pass++;

        for (const auto& technique : K_PRESOLVE_TECHNIQUES) {
            if (!technique.enabled(p)) {
                continue;
            }

            PresolveResult result = technique.apply(data, p);
            changed |= result.changed;

            if (technique.removed_flag != nullptr) {
                data.*(technique.removed_flag) = true;
            }
        }

        check_contradicting_bounds(data, p);
    }

    // Post-presolve scaling: executed once outside the loop
    if (p.scaling) {
        apply_scaling(data, p);
        data.scaling_applied = true;
    }

    create_slack_variables(data);

    return data;
}
