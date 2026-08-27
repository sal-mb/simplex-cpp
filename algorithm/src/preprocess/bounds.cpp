#include <cmath>
#include <stdexcept>

#include "constants.hpp"
#include "preprocess.hpp"

namespace {

    // Applies equations (7.19), (7.20), (7.23), and (7.24) for '<=' ('L') or '=' ('E') constraints
    void process_lower_limit_tightening(ProblemData& data, int i, const LimitSummary& limit, double eps,
                                        bool& changed) {
        double b_i = data.b(i);

        if (limit.inf_count == 0) {
            double L_i = limit.sum;
            for (int j = 0; j < data.n; ++j) {
                double a_ij = data.A(i, j);
                if (std::abs(a_ij) <= eps) continue;

                if (a_ij > 0.0) { // j in P_i -> update upper bound u_k (7.19)
                    double new_ub = data.lb(j) + (b_i - L_i) / a_ij;
                    if (new_ub < data.ub(j) - eps) {
                        data.ub(j) = new_ub;
                        changed = true;
                    }
                } else { // j in M_i -> update lower bound l_k (7.20)
                    double new_lb = data.ub(j) + (b_i - L_i) / a_ij;
                    if (new_lb > data.lb(j) + eps) {
                        data.lb(j) = new_lb;
                        changed = true;
                    }
                }

                if (data.lb(j) > data.ub(j) + eps) {
                    throw std::runtime_error("Presolve error: Infeasible bounds detected during tightening.");
                }
            }
        } else if (limit.inf_count == 1) { // Gondzio extension (7.23 & 7.24)
            int k = limit.inf_var_idx;
            double a_ik = data.A(i, k);
            double S_k = limit.sum;

            if (a_ik > 0.0) { // k in P_i (l_k = -inf) -> derive new upper bound u_k (7.23)
                double new_ub = (b_i - S_k) / a_ik;
                if (new_ub < data.ub(k) - eps) {
                    data.ub(k) = new_ub;
                    changed = true;
                }
            } else { // k in M_i (u_k = +inf) -> derive new lower bound l_k (7.24)
                double new_lb = (b_i - S_k) / a_ik;
                if (new_lb > data.lb(k) + eps) {
                    data.lb(k) = new_lb;
                    changed = true;
                }
            }

            if (data.lb(k) > data.ub(k) + eps) {
                throw std::runtime_error("Presolve error: Infeasible bounds during Gondzio tightening.");
            }
        }
    }

    // Applies dual tightening rules for '>=' ('G') or '=' ('E') constraints
    void process_upper_limit_tightening(ProblemData& data, int i, const LimitSummary& limit, double eps,
                                        bool& changed) {
        double b_i = data.b(i);

        if (limit.inf_count == 0) {
            double U_i = limit.sum;
            for (int j = 0; j < data.n; ++j) {
                double a_ij = data.A(i, j);
                if (std::abs(a_ij) <= eps) continue;

                if (a_ij > 0.0) { // j in P_i -> update lower bound l_k
                    double new_lb = data.ub(j) + (b_i - U_i) / a_ij;
                    if (new_lb > data.lb(j) + eps) {
                        data.lb(j) = new_lb;
                        changed = true;
                    }
                } else { // j in M_i -> update upper bound u_k
                    double new_ub = data.lb(j) + (b_i - U_i) / a_ij;
                    if (new_ub < data.ub(j) - eps) {
                        data.ub(j) = new_ub;
                        changed = true;
                    }
                }

                if (data.lb(j) > data.ub(j) + eps) {
                    throw std::runtime_error("Presolve error: Infeasible bounds detected during tightening.");
                }
            }
        } else if (limit.inf_count == 1) { // Gondzio extension for '>='
            int k = limit.inf_var_idx;
            double a_ik = data.A(i, k);
            double S_k = limit.sum;

            if (a_ik > 0.0) { // k in P_i (u_k = +inf) -> derive new lower bound l_k
                double new_lb = (b_i - S_k) / a_ik;
                if (new_lb > data.lb(k) + eps) {
                    data.lb(k) = new_lb;
                    changed = true;
                }
            } else { // k in M_i (l_k = -inf) -> derive new upper bound u_k
                double new_ub = (b_i - S_k) / a_ik;
                if (new_ub < data.ub(k) - eps) {
                    data.ub(k) = new_ub;
                    changed = true;
                }
            }

            if (data.lb(k) > data.ub(k) + eps) {
                throw std::runtime_error("Presolve error: Infeasible bounds during Gondzio tightening.");
            }
        }
    }

} // anonymous namespace

PresolveResult tighten_individual_bounds(ProblemData& data, const Params& p) {
    bool changed = false;

    for (int i = 0; i < data.m; ++i) {
        char type = data.row_types[i];

        if (type == 'L' || type == 'E') {
            LimitSummary lower = compute_lower_limit(data, p, i);
            process_lower_limit_tightening(data, i, lower, p.eps, changed);
        }
        if (type == 'G' || type == 'E') {
            LimitSummary upper = compute_upper_limit(data, p, i);
            process_upper_limit_tightening(data, i, upper, p.eps, changed);
        }
    }

    return {changed, 0, 0};
}
