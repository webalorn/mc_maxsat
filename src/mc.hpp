#ifndef MC_HPP
#define MC_HPP

#include <vector>
#include <unordered_map>

#include "maxsat.hpp"

const uint HASH_MOD = 1e9+7;

struct MCState;
template<class S=MCState> struct MCTSInstance;

/*
    Settings
*/

struct MCSettings {
    int nodeNActionVars; // 0 for all variables, or > 0 for the number of vars
    int nodeActionVarsHeuristic; // 0 for random, or i in [1, 2, 3] for Hi
    bool nodeActionHeuristicDynamic;
    int rolloutHeuristic; // 0 for random, or i in [1, 2, 3] for Hi
    bool dynamicHeuristic;
    int walkBudgetPerVar;
    float walkEps;
    double ucbCExplo;

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

    std::vector<Literal> nextActions;
    int nbSubExplorations;
    std::vector<int> actionsNExplorations;
    std::vector<double> actionsQValues;


    MCState(MCSettings&, SatProblem&, Assignment&);
    template<class S> int rolloutValue(MCTSInstance<S>&);
    template<class S> Literal getAction(MCTSInstance<S>&);
    template<class S> void updateAfterAction(MCTSInstance<S>&, Literal, int);
};

struct AssignmentHash {
    std::size_t operator()(const Assignment& assign) const;
};

template<class T=MCState> using MCTree = std::unordered_map<Assignment, T, AssignmentHash>;


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
    S& get(Assignment&);
    void updateBest(Assignment& assign, int nbUnverified = -1);
};


/*
    MC Algorithms
*/

void runMCTS(MCTSInstance<>& inst, int steps);

#endif