#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

#include "maxsat.hpp"

using namespace std;

vector<string> split(const string &s, char delim) {
    vector<string> result{};
    stringstream ss(s);
    string item{};

    while (getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

SatProblem readSatProblem(string filepath) {
    vector<Clause> clauses{};
    int nVars;

    ifstream cnfFile;
    cnfFile.open(filepath);
    string line{};

    while (getline(cnfFile, line)) {
        if (line.size()) {
            if (line[0] == 'c') {
                continue;
            } else {
                auto parts = split(line, ' ');
                if (parts[0] == "p") {
                    nVars = stoi(parts[2]);
                } else {
                    clauses.push_back({});
                    parts.pop_back(); // Remove the 0 at the end
                    for (string& litStr : parts) {
                        int lit = stoi(litStr);
                        Literal litObj{abs(lit)-1, (lit > 0)};
                        clauses.back().push_back(litObj);
                    }
                }
            }
        }
    }
    return SatProblem(clauses, nVars);
}

int main() {
    srand(0);
    
    SatProblem problem = readSatProblem("data/input1.cnf");
    cout << "We have a problem with " << problem.nVars << " variables and " << problem.nClauses << " clauses" << endl;

    cout << "First solution has min=" << problem.minUnverified << endl;
    auto assign = problem.freeAssignment();

    assignMostFrequentLitH3Static(problem, assign);
    // assignMostFrequentLitH3Dynamic(problem, assign);
    cout << "After H3, min=" << problem.minUnverified << endl;
    cout << "score of " << problem.score(assign) << endl;

    applyWalkSat(problem, assign, 200, 0.1);
    cout << "After WalkSat, min=" << problem.minUnverified << endl;
}