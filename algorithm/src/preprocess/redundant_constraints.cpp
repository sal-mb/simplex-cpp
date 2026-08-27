#include "constants.hpp"
#include "preprocess.hpp"

namespace {

    struct RowBounds {
        double L = 0.0;
        double U = 0.0;
        bool L_inf = false;
        bool U_inf = false;
    };

    enum class ConstraintAction { Keep, RemoveRedundant, ForcingMin, ForcingMax };

    ConstraintAction classify_constraint(char type, double b_i, const RowBounds& bounds, double eps) {
        if (type == 'L') {
            if (!bounds.L_inf && b_i < bounds.L - eps) {
                throw std::runtime_error("Presolve error: Infeasible constraint (b_i < L_i).");
            }
            if (!bounds.L_inf && std::abs(b_i - bounds.L) <= eps) {
                return ConstraintAction::ForcingMin;
            }
            if (!bounds.U_inf && b_i >= bounds.U - eps) {
                return ConstraintAction::RemoveRedundant;
            }
        } else if (type == 'G') {
            if (!bounds.U_inf && b_i > bounds.U + eps) {
                throw std::runtime_error("Presolve error: Infeasible constraint (b_i > U_i).");
            }
            if (!bounds.U_inf && std::abs(b_i - bounds.U) <= eps) {
                return ConstraintAction::ForcingMax;
            }
            if (!bounds.L_inf && b_i <= bounds.L + eps) {
                return ConstraintAction::RemoveRedundant;
            }
        } else if (type == 'E') {
            if ((!bounds.L_inf && b_i < bounds.L - eps) || (!bounds.U_inf && b_i > bounds.U + eps)) {
                throw std::runtime_error("Presolve error: Infeasible equality constraint.");
            }
            if (!bounds.L_inf && std::abs(b_i - bounds.L) <= eps) {
                return ConstraintAction::ForcingMin;
            }
            if (!bounds.U_inf && std::abs(b_i - bounds.U) <= eps) {
                return ConstraintAction::ForcingMax;
            }
        }
        return ConstraintAction::Keep;
    }

    void apply_forcing_bounds(ProblemData& data, int row_idx, ConstraintAction action, double eps) {
        for (int j = 0; j < data.n; ++j) {
            double a_ij = data.A(row_idx, j);
            if (std::abs(a_ij) <= eps) {
                continue;
            }

            if (action == ConstraintAction::ForcingMin) {
                if (a_ij > 0.0) {
                    data.ub(j) = data.lb(j);
                } else {
                    data.lb(j) = data.ub(j);
                }
            } else if (action == ConstraintAction::ForcingMax) {
                if (a_ij > 0.0) {
                    data.lb(j) = data.ub(j);
                } else {
                    data.ub(j) = data.lb(j);
                }
            }
        }
    }

} // anonymous namespace

PresolveResult remove_redundant_forcing_constraints(ProblemData& data, const Params& p) {
    std::vector<int> keep_rows;
    keep_rows.reserve(data.m);
    int removed = 0;

    for (int i = 0; i < data.m; ++i) {
        LimitSummary lower = compute_lower_limit(data, p, i);
        LimitSummary upper = compute_upper_limit(data, p, i);

        RowBounds bounds{.L = lower.sum, .U = upper.sum, .L_inf = lower.is_infinite(), .U_inf = upper.is_infinite()};

        ConstraintAction action = classify_constraint(data.row_types[i], data.b(i), bounds, p.eps);

        if (action == ConstraintAction::ForcingMin || action == ConstraintAction::ForcingMax) {
            apply_forcing_bounds(data, i, action, p.eps);

            // Defensive: forcing pins every variable touching this row to
            // one specific bound (lb := ub or ub := lb). If a different
            // row already pinned one of those same variables to an
            // incompatible value earlier in this pass, it can only show
            // up as lb > ub right here -- catch it immediately rather
            // than waiting for the next check_contradicting_bounds() call
            // (or, worse, letting Simplex trip over it).
            for (int j = 0; j < data.n; ++j) {
                if (std::abs(data.A(i, j)) > p.eps && data.lb(j) > data.ub(j) + p.eps) {
                    throw std::runtime_error("Presolve error: Infeasible bounds after forcing-constraint fix.");
                }
            }
        }

        if (action == ConstraintAction::Keep) {
            keep_rows.push_back(i);
        } else {
            removed++;
        }
    }

    bool changed = rebuild_rows(data, keep_rows);
    return {changed, removed, 0};
}
