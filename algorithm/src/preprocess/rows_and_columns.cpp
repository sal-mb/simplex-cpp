#include <cmath>
#include <stdexcept>
#include <vector>

#include "constants.hpp"
#include "preprocess.hpp"

bool rebuild_rows(ProblemData& data, const std::vector<int>& keep_rows) {
    if (keep_rows.size() == static_cast<size_t>(data.m)) {
        return false;
    }

    int new_m = static_cast<int>(keep_rows.size());
    Eigen::MatrixXd new_A(new_m, data.n);
    Eigen::VectorXd new_b(new_m);
    std::vector<char> new_row_types;
    new_row_types.reserve(new_m);

    for (int r = 0; r < new_m; ++r) {
        int orig_i = keep_rows[r];
        new_A.row(r) = data.A.row(orig_i);
        new_b(r) = data.b(orig_i);
        new_row_types.push_back(data.row_types[orig_i]);
    }

    data.A = std::move(new_A);
    data.b = std::move(new_b);
    data.row_types = std::move(new_row_types);
    data.m = new_m;
    return true;
}

bool rebuild_columns(ProblemData& data, const std::vector<int>& keep_cols) {
    if (keep_cols.size() == static_cast<size_t>(data.n)) {
        return false;
    }

    int new_n = static_cast<int>(keep_cols.size());
    Eigen::MatrixXd new_A(data.m, new_n);
    Eigen::VectorXd new_c(new_n);
    Eigen::VectorXd new_lb(new_n);
    Eigen::VectorXd new_ub(new_n);

    for (int c_idx = 0; c_idx < new_n; ++c_idx) {
        int orig_j = keep_cols[c_idx];
        new_A.col(c_idx) = data.A.col(orig_j);
        new_c(c_idx) = data.c(orig_j);
        new_lb(c_idx) = data.lb(orig_j);
        new_ub(c_idx) = data.ub(orig_j);
    }

    data.A = std::move(new_A);
    data.c = std::move(new_c);
    data.lb = std::move(new_lb);
    data.ub = std::move(new_ub);
    data.n = new_n;
    return true;
}

PresolveResult remove_empty_rows(ProblemData& data, const Params& p) {
    std::vector<int> keep_rows;
    keep_rows.reserve(data.m);
    int removed = 0;

    for (int i = 0; i < data.m; ++i) {
        bool is_empty = true;
        for (int j = 0; j < data.n; ++j) {
            if (std::abs(data.A(i, j)) > p.eps) {
                is_empty = false;
                break;
            }
        }

        if (is_empty) {
            char type = data.row_types[i];
            double rhs = data.b(i);
            if ((type == 'E' && std::abs(rhs) > p.eps) || (type == 'L' && rhs < -p.eps) ||
                (type == 'G' && rhs > p.eps)) {
                throw std::runtime_error("Presolve error: Infeasible empty row detected.");
            }
            removed++;
            continue; // Constraint is redundant -> drop row
        }

        keep_rows.push_back(i);
    }

    bool changed = rebuild_rows(data, keep_rows);
    return {changed, removed, 0};
}

PresolveResult remove_empty_columns(ProblemData& data, const Params& p) {
    std::vector<int> keep_cols;
    keep_cols.reserve(data.n);
    int removed = 0;

    for (int j = 0; j < data.n; ++j) {
        bool is_empty = true;
        for (int i = 0; i < data.m; ++i) {
            if (std::abs(data.A(i, j)) > p.eps) {
                is_empty = false;
                break;
            }
        }

        if (is_empty) {
            double c_j = data.c(j);
            double fix_val = 0.0;

            // This is a minimization problem (min c'x -- see
            // mps_reader.hpp and Simplex::get_solution(), which reports
            // x.dot(data.c) directly as the true cost; Simplex's own
            // internal maximization of -data.c is just its pivoting
            // engine's implementation detail and doesn't change what
            // data.c itself means here). An empty column has no row
            // pulling it either way, so it's fixed at whichever of its
            // own bounds minimizes c_j * x_j.
            if (std::abs(c_j) <= p.eps) {
                // c_j = 0: any feasible value works, pick a finite one.
                if (data.lb(j) > nInf) {
                    fix_val = data.lb(j);
                } else if (data.ub(j) < pInf) {
                    fix_val = data.ub(j);
                } else {
                    fix_val = 0.0;
                }
            } else if (c_j > 0) {
                // c_j > 0: minimizing c_j*x_j means taking x_j as small
                // as possible.
                if (data.lb(j) <= nInf) {
                    throw std::runtime_error("Presolve error: Unbounded problem (empty col c_j > 0, lb = -inf)");
                }
                fix_val = data.lb(j);
            } else {
                // c_j < 0: minimizing c_j*x_j means taking x_j as large
                // as possible.
                if (data.ub(j) >= pInf) {
                    throw std::runtime_error("Presolve error: Unbounded problem (empty col c_j < 0, ub = +inf)");
                }
                fix_val = data.ub(j);
            }

            // Record constant objective shift
            data.obj_constant += c_j * fix_val;
            removed++;
            continue; // Drop empty column
        }

        keep_cols.push_back(j);
    }

    bool changed = rebuild_columns(data, keep_cols);
    return {changed, 0, removed};
}

PresolveResult remove_singleton_rows(ProblemData& data, const Params& p) {
    std::vector<int> keep_rows;
    keep_rows.reserve(data.m);
    int removed = 0;

    for (int i = 0; i < data.m; ++i) {
        int non_zero_col = -1;
        int non_zero_count = 0;

        for (int j = 0; j < data.n; ++j) {
            if (std::abs(data.A(i, j)) > p.eps) {
                non_zero_count++;
                non_zero_col = j;
            }
        }

        if (non_zero_count == 1) {
            int j = non_zero_col;
            double a_ij = data.A(i, j);
            double val = data.b(i) / a_ij;
            char type = data.row_types[i];

            if (type == 'E') {
                data.lb(j) = std::max(data.lb(j), val);
                data.ub(j) = std::min(data.ub(j), val);
            } else if ((type == 'L' && a_ij > 0) || (type == 'G' && a_ij < 0)) {
                data.ub(j) = std::min(data.ub(j), val);
            } else { // (type == 'L' && a_ij < 0) || (type == 'G' && a_ij > 0)
                data.lb(j) = std::max(data.lb(j), val);
            }

            if (data.lb(j) > data.ub(j) + p.eps) {
                throw std::runtime_error("Presolve error: Infeasible bound on singleton row.");
            }
            removed++;
            continue; // Drop row after folding bound into variable limits
        }

        keep_rows.push_back(i);
    }

    bool changed = rebuild_rows(data, keep_rows);
    return {changed, removed, 0};
}
