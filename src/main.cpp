#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "tclap/CmdLine.h"

#include "maxsat.hpp"
#include "mc.hpp"

using namespace std;
using namespace TCLAP;

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

int main_test() {
    srand(1);

    SatProblem problem = readSatProblem("data/input1.cnf");
    cout << "We have a problem with " << problem.nVars << " variables and " << problem.nClauses << " clauses" << endl;

    cout << "First solution has score=" << problem.score(problem.randomAssignment()) << endl;
    auto assign = problem.freeAssignment();

    assign = assignMostFrequentLitH3Static(problem, assign);
    cout << "After H3, score=" << problem.score(assign) << endl;

    assign = applyWalkSat(problem, assign, 200, 0.1);
    cout << "After WalkSat, score=" << problem.score(assign) << endl;

    /*
        MCTS
    */
    cout << endl << "Now applying MCTS" << endl;
    
    MCSettings settings{};
    settings.nodeNActionVars = 10;
    settings.nodeActionVarsHeuristic = 3;
    settings.ucbCExplo = 0.03;

    settings.nodeActionHeuristicDynamic = false;
    settings.rolloutHeuristic = 3;
    settings.dynamicHeuristic = false;
    settings.walkBudgetPerVar = 2;
    settings.walkEps = 0.3;

    MCTSInstance<> inst1{settings, problem};
    runMCTS(inst1, 200);

    cerr << "tree size " << inst1.tree.size() << endl;
    cout << "After MCTS, score=" << inst1.minUnverified << endl;

    /*
        NMCS
    */
    cout << endl << "Now applying Nested MCS" << endl;

    MCTSInstance<> inst2{settings, problem};
    runNMCS(inst2, inst2.pb.freeAssignment(), 1);

    cerr << "tree size " << inst2.tree.size() << endl;
    cout << "After NMCS, score=" << inst2.minUnverified << endl;
    
    return 0;
}

int main(int argc, char** argv) {
    // return main_test();

    string method = "rollout";
    MCSettings settings{};

    vector<string> methodsList{"rollout", "mcts", "nested_mc"};
    ValuesConstraint<string> methodsConstraint(methodsList);
    vector<int> heuristicList{0, 1, 2, 3};
    ValuesConstraint<int> heuristicConstraint(heuristicList);

	try {
	    CmdLine cmd("Run MC experiment", ' ', "0.0");

        // Create the CMD arguments
    	ValueArg<string> methodArg("m", "method", "Method used (algorithm)",
            true, method, &methodsConstraint, cmd);
        
    	ValueArg<int> seedArg("s", "seed", "Random generator seed", false, settings.seed, "integer", cmd);
        
    	ValueArg<int> nodeNActionVarsArg("", "node_actions",
            "Number of actionable variable for each node (0 for all variables)",
            false, settings.nodeNActionVars, "integer", cmd);

    	ValueArg<int> nodeActionVarsHeuristicArg("", "node_h",
            "Heuristic used to sort the next actions from a node (0=random, 1,2,3=H1,H2,H3)",
            false, settings.nodeActionVarsHeuristic, &heuristicConstraint, cmd);

        SwitchArg nodeActionHeuristicDynamicSwitch("", "node_h_dyn",
            "Use the dynamic heuristic for the node action heuristic", cmd, false);

    	ValueArg<int> rolloutHeuristicArg("", "rollout_h",
            "Heuristic used for the rollout initialization (0=random, 1,2,3=H1,H2,H3)",
            false, settings.rolloutHeuristic, &heuristicConstraint, cmd);

        SwitchArg dynamicHeuristicSwitch("", "rollout_h_dyn",
            "Use the dynamic heuristic for the rollout heuristic", cmd, false);
        
    	ValueArg<int> walkBudgetPerVarArg("w", "w_var",
            "Walk2Sat budget per variable",
            false, settings.walkBudgetPerVar, "integer", cmd);
        
    	ValueArg<float> walkEpsArg("", "walk_eps",
            "Walk2Sat epsilon value",
            false, settings.walkEps, "float [0;1]", cmd);
        
    	ValueArg<float> ucbCExploArg("c", "ucb_explo",
            "UCB exploration parameter (c)",
            false, settings.ucbCExplo, "float", cmd);
        
    	ValueArg<int> stepsArg("n", "steps",
            "Number of steps for the MCTS algorithm",
            false, settings.steps, "integer", cmd);
        
    	ValueArg<int> nmcsDepthArg("d", "n_depth",
            "Depth for the nested MC algorithm",
            false, settings.nmcsDepth, "integer", cmd);
        

        // Parse the CMD arguments
	    cmd.parse(argc, argv);

        method = methodArg.getValue();
        settings.seed = seedArg.getValue();

        settings.nodeNActionVars = nodeNActionVarsArg.getValue();
        settings.nodeActionVarsHeuristic = nodeActionVarsHeuristicArg.getValue();
        settings.nodeActionHeuristicDynamic = nodeActionHeuristicDynamicSwitch.getValue();

        settings.rolloutHeuristic = rolloutHeuristicArg.getValue();
        settings.dynamicHeuristic = dynamicHeuristicSwitch.getValue();
        settings.walkBudgetPerVar = walkBudgetPerVarArg.getValue();
        settings.walkEps = walkEpsArg.getValue();

        settings.ucbCExplo = ucbCExploArg.getValue();
        settings.steps = stepsArg.getValue();
        settings.nmcsDepth = nmcsDepthArg.getValue();

	} catch (ArgException &e) {
        cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
        return -1;
    }

    // Initialize the problem
    srand(settings.seed); // TODO: call for EACH problem

    SatProblem problem = readSatProblem("data/input1.cnf");
    MCTSInstance<> inst{settings, problem};

    if (method == "rollout") {
        runRollout(inst, 1);
    } else if (method == "mcts") {
        runMCTS(inst, settings.steps);
    } else if (method == "nested_mc") {
        runNMCS(inst, inst.pb.freeAssignment(), settings.nmcsDepth);
    }
    cout << "Found score of " << inst.minUnverified << endl;
    
}