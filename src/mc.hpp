#ifndef MC_HPP
#define MC_HPP

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>

#include "maxsat.hpp"
#include "util.hpp"

const uint HASH_MOD = 1e9+7;

struct MCState;
template<class S=MCState> struct MCTSInstance;

/*
    Settings
*/

struct MCSettings {
    int seed;
    int nodeNActionVars; // 0 for all variables, or > 0 for the number of vars
    int nodeActionVarsHeuristic; // 0 for random, or i in [1, 2, 3] for Hi
    bool nodeActionHeuristicDynamic;
    int rolloutHeuristic; // 0 for random, or i in [1, 2, 3] for Hi
    bool dynamicHeuristic;
    int walkBudgetPerVar;
    float walkEps;
    double ucbCExplo;
    int steps, nmcsDepth;
    std::string behavior;
    std::string flipAlgorithm; // walksat, novelty

    MCSettings();
};


/*
    MC Tree
*/

struct MCState {
    Assignment stateAssign;
    int nbTimesSeen;
    int nbUnassigned;
    bool terminal;
    int bestActionId;

    std::vector<Literal> nextActions;
    int nbSubExplorations;
    std::vector<int> actionsNExplorations;
    std::vector<double> actionsQValues;
    std::vector<int> bestScoresForActions;


    MCState(MCSettings&, SatProblem&, Assignment&);
    int getActionId(const Literal&);
    template<class S> int rolloutValue(MCTSInstance<S>&);
    template<class S> Literal getUCBAction(MCTSInstance<S>&, bool allowExploration=true);
    template<class S> void updateAfterAction(MCTSInstance<S>&, Literal, int);
};

struct AssignmentHash {
    std::size_t operator()(const Assignment& assign) const;
};

template<class T=MCState> using MCTree = std::unordered_map<Assignment, std::unique_ptr<T>, AssignmentHash>;


/*
    MC Instances
*/

template<class S>
struct MCTSInstance {
    MCSettings settings;
    SatProblem pb;
    MCTree<S> tree;

    int minUnverified;
    Assignment bestAssignment;

    MCTSInstance(const MCSettings&, const SatProblem&);
    S* get(Assignment&);
    void updateBest(Assignment& assign, int nbUnverified = -1);
};


/*
    MC Algorithms
*/

Assignment applyAction(const Assignment& assign, Literal action);
template<class S>
Assignment applyFlipAlgorithm(MCTSInstance<S>& inst, const Assignment& assign, int nbUnassigned);
void runRollout(MCTSInstance<>& inst, int steps);

void runRollout(MCTSInstance<>& inst);
void runMCTS(MCTSInstance<>& inst);
void runNMCS(MCTSInstance<>& inst);
void runSeqHalving(MCTSInstance<>& inst);

#endif