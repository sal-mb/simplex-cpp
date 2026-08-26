#pragma once

#include <Eigen/Dense>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <string>
#include <vector>

struct ProblemData {
    int m{0};          // number of constraints (n_rows_inq + n_rows_eq)
    int n{0};          // number of original variables (n_cols only, never
                       // includes slacks -- even after slack creation,
                       // A.cols() grows to n + m but n itself stays fixed)
    int n_rows_eq{0};  // number of equality rows
    int n_rows_inq{0}; // number of inequality rows (L/G/E excluding objective)

    Eigen::MatrixXd A;  // Raw stage: m x n (no slacks yet).
                        // Preprocessed stage: m x (n + m), one slack
                        // column appended per row (see row_types).
    Eigen::VectorXd b;  // Raw stage: true RHS per row (size m).
                        // Preprocessed stage: always zero -- RHS is
                        // folded entirely into the slack columns'
                        // bounds instead (Ax_ext = 0 convention).
    Eigen::VectorXd lb; // Raw stage: size n. Preprocessed stage: size n + m.
    Eigen::VectorXd ub; // Raw stage: size n. Preprocessed stage: size n + m.
    Eigen::VectorXd c;  // Raw stage: size n. Preprocessed stage: size n + m
                        // (slack entries are always 0).

    // One entry per constraint row (size m), in the same row order as A/b:
    // 'L' (<=), 'G' (>=), or 'E' (=). Needed to know each row's slack
    // bound direction when slack columns are created. Populated by
    // MpsReader::read() and left untouched by remove_fixed_variables()
    // (which only ever removes columns, never rows) and apply_scaling()
    // (which doesn't touch row semantics either).
    std::vector<char> row_types;

    std::string name;      // problem name
    std::string file_name; // source file name

    // Populated by remove_fixed_variables(): the constant that must be
    // added back to x.dot(c) (i.e. Solution::cost) to recover the true
    // objective value, since eliminated fixed variables no longer
    // appear in x/c at all. Zero if remove_fixed_variables() was never
    // applied, or found nothing to remove.
    double obj_constant{0.0};

    // Output-state flags describing what preprocessing has actually
    // been applied to this ProblemData so far (set by Preprocessor).
    bool scaling_applied{false};
    bool fixed_removed{false};
    bool empty_rows_removed{false};
    bool empty_cols_removed{false};
    bool singleton_rows_removed{false};
    bool redundant_forcing_removed{false};
};

template <>
struct fmt::formatter<ProblemData> {
    // Parses format specifications; accepts default "{}"
    static constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) {
        const auto* it = ctx.begin();
        const auto* end = ctx.end();
        if (it != end && *it != '}') {
            throw fmt::format_error("invalid format specifier for ProblemData");
        }
        return it;
    }

    // Formats the ProblemData object
    template <typename FormatContext>
    auto format(const ProblemData& p, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(),
                              "ProblemData {{\n"
                              "  Name:                   {}\n"
                              "  File Name:              {}\n"
                              "  Constraints (m):        {}\n"
                              "  Variables (n):          {}\n"
                              "  Equality Rows:          {}\n"
                              "  Inequality Rows:        {}\n"
                              "  A Dimensions:           {} x {}\n"
                              "  b Size:                 {}\n"
                              "  lb Size:                {}\n"
                              "  ub Size:                {}\n"
                              "  c Size:                 {}\n"
                              "  Row Types:              [{}]\n"
                              "  Objective Constant:     {}\n"
                              "  Scaling Applied:        {}\n"
                              "  Fixed Removed:          {}\n"
                              "  Empty Rows Removed:     {}\n"
                              "  Empty Cols Removed:     {}\n"
                              "  Singleton Rows Removed: {}\n"
                              "}}",
                              p.name.empty() ? "<unnamed>" : p.name, p.file_name.empty() ? "<none>" : p.file_name, p.m,
                              p.n, p.n_rows_eq, p.n_rows_inq, p.A.rows(), p.A.cols(), p.b.size(), p.lb.size(),
                              p.ub.size(), p.c.size(), fmt::join(p.row_types, ", "), p.obj_constant, p.scaling_applied,
                              p.fixed_removed, p.empty_rows_removed, p.empty_cols_removed, p.singleton_rows_removed);
    }
};
