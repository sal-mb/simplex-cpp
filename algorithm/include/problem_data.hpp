#pragma once

#include <Eigen/Dense>
#include <string>

enum class ProblemStage { Raw, Preprocessed };

struct ProblemData {
    ProblemStage stage{ProblemStage::Raw};
    int m{0};
    int n{0};
    int n_rows_eq{0};
    int n_rows_inq{0};

    Eigen::MatrixXd A;
    Eigen::VectorXd b;
    Eigen::VectorXd lb;
    Eigen::VectorXd ub;
    Eigen::VectorXd c;

    std::string name;
    std::string file_name;

    bool remove_fixed{false};
    bool scaling{false};

    bool scaling_applied{false};
    bool fixed_removed{false};
};