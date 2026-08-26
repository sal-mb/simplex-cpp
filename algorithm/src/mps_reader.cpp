#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>

#include "mps_reader.hpp"

ProblemData MpsReader::read(const string& file_name) {
    ifstream read_file(file_name);
    ProblemData data;

    if (!read_file.is_open()) {
        cout << "Error: MPSREADER - File not found" << endl;
        return data;
    }

    // get rid of comments or blank line
    find_pos2_start(read_file);

    // get problem dimention
    preproc_scan(read_file);

    // extract data
    data = extract_data(read_file);
    data.file_name = file_name;

    read_file.close();
    return data;
}

void MpsReader::find_pos2_start(ifstream& read_file) {
    long pos;
    string line;
    while (true) {
        pos = read_file.tellg();
        getline(read_file, line);
        if (line.empty())
            continue;
        else if (line.find('*') == 0)
            continue;
        else {
            read_file.seekg(pos, ios::beg); // go back one line
            break;
        }
    }
}

void MpsReader::preproc_scan(ifstream& read_file) {
    n_rows = 0;
    n_rows_eq = 0;
    n_rows_inq = 0;
    n_cols = 0;
    bnd_exist = false;

    string tmp = "_";
    string tmpItem;
    string firstWord;
    read_file >> firstWord;

    while (!read_file.eof()) {
        if (firstWord.empty() || firstWord.find('*') == 0) {
            continue;
        }

        int section = check_section_name(firstWord);

        if (section == 10) break; // ENDATA
        if (section == 8) {
            cout << "Error: MPSREADER - Currently cannot handle SOS" << endl;
            break;
        }
        if (section == 9) {
            cout << "Error: MPSREADER - Currently cannot handle RANGES" << endl;
            break;
        }
        if (section == 7) {
            cout << "Error: MPSREADER - Currently cannot handle OBJSENSE" << endl;
            break;
        }
        if (section == 6) {
            cout << "Error: MPSREADER - Currently cannot handle QUADOBJ" << endl;
            break;
        }

        if (section == 1) { // NAME
            // The problem name is optional; read only the rest of this
            // line so a blank NAME line never lets >> cross onto ROWS.
            string restOfLine;
            getline(read_file, restOfLine);
            istringstream nameStream(restOfLine);
            if (!(nameStream >> name)) name = "0";
            read_file >> firstWord;
        } else if (section == 2) { // ROWS
            // update firstWord and row_list
            read_file >> firstWord;
            while (check_section_name(firstWord) == -1) {
                // count n_rows, n_rows_eq, n_rows_inq
                if (firstWord == "L" || firstWord == "G" || firstWord == "E") {
                    n_rows++;
                    n_rows_inq++;
                } else if (firstWord == "N") {
                    n_rows++;
                }
                // store row labels
                row_labels.push_back(firstWord);

                // get row_list
                read_file >> tmpItem;
                row_list.push_back(tmpItem);
                next_line(read_file);
                read_file >> firstWord;
            }
        } else if (section == 3) { // COLUMNS
            // get position
            col_pos = read_file.tellg();
            read_file >> firstWord;
            while (check_section_name(firstWord) == -1) {
                if (firstWord != tmp) {
                    // update column list
                    col_list.push_back(firstWord);

                    // count columns
                    n_cols++;
                    tmp = firstWord;
                }
                next_line(read_file);
                read_file >> firstWord;
            }
        } else if (section == 4) { // RHS
            rhs_pos = read_file.tellg();
            next_line(read_file);
            read_file >> firstWord;
        } else if (section == 5) { // BOUNDS
            bnd_exist = true;
            bnd_pos = read_file.tellg();
            next_line(read_file);
            read_file >> firstWord;
        } else {
            next_line(read_file);
            read_file >> firstWord;
        }
    }
}

ProblemData MpsReader::extract_data(ifstream& read_file) {
    MatrixXd Araw = MatrixXd::Zero(n_rows, n_cols);
    VectorXd braw = VectorXd::Zero(n_rows);

    // get Araw
    get_araw(read_file, Araw);

    // get rhs
    get_braw(read_file, braw);

    // get c
    VectorXd c = VectorXd::Zero(n_cols);
    split_c(Araw, c);

    // get bounds
    VectorXd lb = VectorXd::Zero(n_cols);
    VectorXd ub = VectorXd::Constant(n_cols, numeric_limits<double>::infinity());

    if (bnd_exist) {
        get_bnds(read_file, lb, ub);
    }

    // split Araw to A, c and split braw to b, recording each kept row's type
    MatrixXd A(n_rows_inq + n_rows_eq, n_cols);
    VectorXd b(n_rows_inq + n_rows_eq);
    vector<char> row_types;
    row_types.reserve(n_rows_inq + n_rows_eq);
    split_raw(Araw, braw, A, b, row_types);

    ProblemData data;
    data.m = n_rows_inq + n_rows_eq;
    data.n = n_cols;
    data.n_rows_eq = n_rows_eq;
    data.n_rows_inq = n_rows_inq;
    data.A = A;
    data.b = b;
    data.lb = lb;
    data.ub = ub;
    data.c = c;
    data.row_types = row_types;
    data.name = name;
    return data;
}

void MpsReader::get_araw(ifstream& read_file, MatrixXd& Araw) {
    string line, colName, rowName;
    double value;
    int colIdx = 0, rowIdx;

    // go to the position of cols
    read_file.seekg(col_pos, ios::beg);
    next_line(read_file);
    do {
        // clear
        colName.clear();

        // read one line
        getline(read_file, line);
        istringstream thisLine(line);
        thisLine >> colName;

        // break if get to next section
        if (check_section_name(colName) != -1) break;

        // get col index
        colIdx = get_index(col_list, colName);

        while (thisLine >> rowName >> value) {
            // get row index
            rowIdx = get_index(row_list, rowName);
            if (rowIdx >= 0 && colIdx >= 0) Araw(rowIdx, colIdx) = value;
        }
    } while (true);
}

void MpsReader::get_braw(ifstream& read_file, VectorXd& braw) {
    string line, rowName;
    double value;
    int rowIdx;

    // go to the position of rhs
    read_file.seekg(rhs_pos, ios::beg);
    next_line(read_file);

    do {
        // read one line
        getline(read_file, line);
        istringstream thisLine(line);

        // Some MPS files (free-format variants) omit the optional RHS
        // vector-name field, giving lines that are just
        // "row value row value ...", while others include a leading
        // name "RHSNAME row value row value ...". Disambiguate by
        // parity of the token count: with the name field the line has
        // an odd number of tokens, without it, an even number.
        vector<string> tokens;
        string tok;
        while (thisLine >> tok)
            tokens.push_back(tok);

        if (tokens.empty()) continue;
        if (check_section_name(tokens[0]) != -1) break;

        size_t start = (tokens.size() % 2 == 1) ? 1 : 0;
        for (size_t i = start; i + 1 < tokens.size(); i += 2) {
            // get row index
            rowName = tokens[i];
            value = stod(tokens[i + 1]);
            rowIdx = get_index(row_list, rowName);
            if (rowIdx >= 0) braw(rowIdx) = value;
        }
    } while (true);
}

void MpsReader::get_bnds(ifstream& read_file, VectorXd& lb, VectorXd& ub) {
    string label, colName;
    double value;
    int colIdx;

    read_file.seekg(bnd_pos, ios::beg);
    next_line(read_file);

    do {
        string line;
        getline(read_file, line);
        istringstream iss(line);

        // As with RHS, the bound-set-name field is optional. A
        // value-bearing bound (LO/UP/FX) line therefore has either 4
        // tokens ("LO SETNAME COL VAL") or 3 ("LO COL VAL"); a bound
        // with no value (FR/MI/PL/BV) has either 3 tokens
        // ("FR SETNAME COL") or 2 ("FR COL").
        vector<string> tokens;
        string tok;
        while (iss >> tok)
            tokens.push_back(tok);

        if (tokens.empty()) continue;
        label = tokens[0];
        if (check_section_name(label) == 10) break; // ENDATA

        bool needsValue = (label == "LO" || label == "UP" || label == "FX");
        size_t ntok = tokens.size();
        value = 0.0;

        if (needsValue) {
            if (ntok == 4) {
                colName = tokens[2];
                value = stod(tokens[3]);
            } else if (ntok == 3) {
                colName = tokens[1];
                value = stod(tokens[2]);
            } else
                continue;
        } else {
            if (ntok == 3)
                colName = tokens[2];
            else if (ntok == 2)
                colName = tokens[1];
            else
                continue;
        }

        colIdx = get_index(col_list, colName);
        if (colIdx < 0) continue;

        // cout << "l: " << label << " " << colName << endl;
        if (label == "LO")
            lb(colIdx) = value;
        else if (label == "UP")
            ub(colIdx) = value;
        else if (label == "FR") {
            lb(colIdx) = -numeric_limits<double>::infinity();
            ub(colIdx) = numeric_limits<double>::infinity();
        } else if (label == "FX") {
            lb(colIdx) = value;
            ub(colIdx) = value;
        } else if (label == "MI")
            lb(colIdx) = -numeric_limits<double>::infinity();
        else if (label == "PL")
            ub(colIdx) = numeric_limits<double>::infinity();
        else if (label == "BV") {
            lb(colIdx) = 0.0;
            ub(colIdx) = 1.0;
        }
    } while (true);
}

void MpsReader::split_c(MatrixXd& Araw, VectorXd& c) {
    for (int i = n_rows - 1; i >= 0; i--) {
        if (row_labels[i] == "N") {
            c = Araw.row(i).transpose();
            row_labels[i] = "X";
            Araw.row(i).setZero();
        }
    }
}

void MpsReader::split_raw(const MatrixXd& Araw, const VectorXd& braw, MatrixXd& A, VectorXd& b,
                          vector<char>& row_types) {
    int counter = 0;
    for (int i = 0; i < n_rows; i++) {
        if (row_labels[i] != "X") {
            A.row(counter) = Araw.row(i);
            b(counter) = braw(i);
            row_types.push_back(row_labels[i][0]); // 'L', 'G', or 'E'
            counter++;
        }
    }
}

int MpsReader::check_section_name(const string& check_word) {
    if (check_word == "NAME") return 1;
    if (check_word == "ROWS") return 2;
    if (check_word == "COLUMNS") return 3;
    if (check_word == "RHS") return 4;
    if (check_word == "BOUNDS") return 5;
    if (check_word == "QUADOBJ") return 6;
    if (check_word == "OBJSENSE") return 7;
    if (check_word == "SOS") return 8;
    if (check_word == "RANGES") return 9;
    if (check_word == "ENDATA") return 10;
    if (check_word == "FR") return 11;
    return -1;
}

void MpsReader::next_line(ifstream& read_file) { read_file.ignore(numeric_limits<streamsize>::max(), '\n'); }

int MpsReader::get_index(const vector<string>& list, const string& item) {
    int idx = static_cast<int>(find(list.begin(), list.end(), item) - list.begin());
    if (idx >= static_cast<int>(list.size())) idx = -1;
    return idx;
}
