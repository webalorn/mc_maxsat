#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>

#include "maxsat.hpp"
#include "mc.hpp"

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
    srand(1);

    SatProblem problem = readSatProblem("data/input1.cnf");
    cout << "We have a problem with " << problem.nVars << " variables and " << problem.nClauses << " clauses" << endl;

    cout << "First solution has score=" << problem.score(problem.randomAssignment()) << endl;
    auto assign = problem.freeAssignment();

    assign = assignMostFrequentLitH3Static(problem, assign);
    cout << "After H3, score=" << problem.score(assign) << endl;

    assign = applyWalkSat(problem, assign, 200, 0.1);
    cout << "After WalkSat, score=" << problem.score(assign) << endl;

    // MCTS
    cout << endl << "Now applying MCTS" << endl;
    MCSettings settings{};
    settings.nodeNActionVars = 1;
    settings.nodeActionVarsHeuristic = 1;
    settings.ucbCExplo = 0.03;

    settings.nodeActionHeuristicDynamic = false;
    settings.rolloutHeuristic = 3;
    settings.dynamicHeuristic = false;
    settings.walkBudgetPerVar = 2;
    settings.walkEps = 0.3;

    MCTSInstance<> inst{settings, problem};
    runMCTS(inst, 10);
    cerr << "tree size " << inst.tree.size() << endl;

    cout << "After MC, score=" << inst.minUnverified << endl;
}