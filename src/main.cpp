#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <filesystem>
#include <chrono>

#include "tclap/CmdLine.h"

#include "maxsat.hpp"
#include "mc.hpp"

using namespace std;
using namespace TCLAP;
namespace fs = std::filesystem;

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

                    clauses.back().weight = stof(parts.front()); // set weight value
                    parts.erase(parts.begin()); // remove weight value
                    
                    for (string& litStr : parts) {
                        int lit = stoi(litStr);
                        Literal litObj{abs(lit)-1, (lit > 0)};
                        clauses.back().literals.push_back(litObj);
                    }
                }
            }
        }
    }
    return SatProblem(clauses, nVars);
}

int main(int argc, char** argv) {
    cout << C_CYAN;
    for (const string& part : vector<string>(argv, argv+argc)) {
        cout << part << " ";
    }
    cout << C_RESET << endl;

    string method = "rollout";
    string dataPath = "data/test";
    MCSettings settings{};

    vector<string> methodsList{"rollout", "mcts", "nested_mc", "seq_halving"};
    ValuesConstraint<string> methodsConstraint(methodsList);
    vector<string> behaviors{"once", "full", "discounted"};
    ValuesConstraint<string> behaviorsConstraint(behaviors);
    vector<string> flipAlgorithms{"walksat", "novelty", "weighted_novelty", "maxwalksat"};
    ValuesConstraint<string> flipAlgorithmsConstraint(flipAlgorithms);
    vector<int> heuristicList{0, 1, 2, 3};
    ValuesConstraint<int> heuristicConstraint(heuristicList);

	try {
	    CmdLine cmd("Run MC experiment", ' ', "0.0");

        // Create the CMD arguments
    	ValueArg<string> dataPathArg("", "data", "Path to the data directory used",
            false, dataPath, "path", cmd);

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
        
    	ValueArg<string> flipAlgorithmArg("", "flip",
            "Flip algorithm used to optimize the solutions",
            false, settings.flipAlgorithm, &flipAlgorithmsConstraint, cmd);
        
    	ValueArg<int> walkBudgetPerVarArg("w", "w_var",
            "WalkSat budget per variable",
            false, settings.walkBudgetPerVar, "integer", cmd);
        
    	ValueArg<float> walkEpsArg("", "walk_eps",
            "WalkSat epsilon value",
            false, settings.walkEps, "float [0;1]", cmd);
        
    	ValueArg<float> amafArg("", "amaf",
            "AMAF coefficient (currently only for SH)",
            false, settings.amaf, "float", cmd);
        
    	ValueArg<float> amafBiasArg("", "amaf_bias",
            "AMAF bias coefficient (currently only for SH)",
            false, settings.amaf, "float", cmd);
        
    	ValueArg<float> ucbCExploArg("c", "ucb_explo",
            "UCB exploration parameter (c)",
            false, settings.ucbCExplo, "float", cmd);
        
    	ValueArg<string> behaviorArg("", "behavior",
            "MCTS behavior (once only run from the root, discounted is faster than full)",
            false, settings.behavior, &behaviorsConstraint, cmd);
        
    	ValueArg<int> stepsArg("n", "steps",
            "Number of steps for the MCTS algorithm, budget for the Sequential Halving algorithm, number of repeats of the NMCS or rollout algorithms",
            false, settings.steps, "integer", cmd);
        
    	ValueArg<int> nmcsDepthArg("d", "depth",
            "Depth for the nested MC algorithm",
            false, settings.nmcsDepth, "integer", cmd);
        

        // Parse the CMD arguments
	    cmd.parse(argc, argv);

        dataPath = dataPathArg.getValue();
        method = methodArg.getValue();
        settings.seed = seedArg.getValue();

        settings.nodeNActionVars = nodeNActionVarsArg.getValue();
        settings.nodeActionVarsHeuristic = nodeActionVarsHeuristicArg.getValue();
        settings.nodeActionHeuristicDynamic = nodeActionHeuristicDynamicSwitch.getValue();

        settings.rolloutHeuristic = rolloutHeuristicArg.getValue();
        settings.dynamicHeuristic = dynamicHeuristicSwitch.getValue();
        settings.flipAlgorithm = flipAlgorithmArg.getValue();
        settings.walkBudgetPerVar = walkBudgetPerVarArg.getValue();
        settings.walkEps = walkEpsArg.getValue();
        settings.amaf = amafArg.getValue();
        settings.amafBias = amafBiasArg.getValue();

        settings.ucbCExplo = ucbCExploArg.getValue();
        settings.behavior = behaviorArg.getValue();
        settings.steps = stepsArg.getValue();
        settings.nmcsDepth = nmcsDepthArg.getValue();

	} catch (ArgException &e) {
        cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
        return -1;
    }

    vector<string> dataFiles;
    for (const auto& dataFile : fs::directory_iterator(dataPath)) {
        string dataFilePath = dataFile.path();
        if (split(dataFilePath, '.').back() == "wcnf") {
            dataFiles.push_back(dataFilePath);
        }
    }

    cout << "Using data from " << dataPath << " (" << dataFiles.size() << " test files)" << endl;
    double totalScore = 0;
    double totalTime = 0;

    for (int iFile = 0; iFile < (int)dataFiles.size(); iFile++) {
        cout << "Running " << method << " on file " << (iFile+1) << "/" << dataFiles.size()
            << " [" << dataFiles[iFile] << "]" << endl;


        // Initialize the problem
        srand(settings.seed);
        SatProblem problem = readSatProblem(dataFiles[iFile]);
        MCTSInstance<> inst{settings, problem};

        auto startClock = chrono::high_resolution_clock::now();
        if (method == "rollout") {
            runRollout(inst);
        } else if (method == "mcts") {
            runMCTS(inst);
        } else if (method == "nested_mc") {
            runNMCS(inst);
        } else if (method == "seq_halving") {
            runSeqHalving(inst);
        }
        auto stopClock = chrono::high_resolution_clock::now();
        auto runDuration = duration_cast<chrono::milliseconds>(stopClock - startClock);
        double runTime = runDuration.count() / 1000.0;

        totalScore += inst.minUnverified;
        totalTime += runTime;
        cout << "score=" << inst.minUnverified
            << "  (avg=" << C_GREEN << setprecision(6) << (totalScore / (iFile+1)) << C_RESET
            << ", avg_time=" << C_CYAN << setprecision(3) << (totalTime / (iFile+1)) << "s" << C_RESET
            << ")" << endl;
    }
    cout << "Final average score is " << C_GREEN << setprecision(6) << (totalScore / dataFiles.size()) << C_RESET
        << "    (avg_time=" << C_CYAN << setprecision(3) << (totalTime / dataFiles.size()) << "s" << C_RESET
        << ", total_time=" << C_CYAN << ((int)totalTime) << "s" << C_RESET
        << ")" << endl;
    
}