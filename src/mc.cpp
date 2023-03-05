#include <iostream>
#include <algorithm>
#include <cassert>

#include "mc.hpp"

using namespace std;

/*
    Settings
*/

MCSettings::MCSettings(){
    nodeNActionVars = 2;
    nodeActionVarsHeuristic = 3;
    nodeActionHeuristicDynamic = false;
    rolloutHeuristic = 3;
    dynamicHeuristic = false;
    walkBudgetPerVar = 2;
    walkEps = 0.1;
    // ucbCExplo = sqrt(2);
    ucbCExplo = 0.2; // TODO: which c value?
}

Assignment applyHeuristic(SatProblem& pb, Assignment& assign, MCSettings& settings) {
    if (settings.rolloutHeuristic == 1 && !settings.dynamicHeuristic) {
        return assignInOrderH1Static(pb, assign);
    } else if (settings.rolloutHeuristic == 2 && !settings.dynamicHeuristic) {
        return assignMostFrequentVarH2Static(pb, assign);
    } else if (settings.rolloutHeuristic == 3 && !settings.dynamicHeuristic) {
        return assignMostFrequentLitH3Static(pb, assign);
    } else if (settings.rolloutHeuristic == 1 && settings.dynamicHeuristic) {
        return assignInOrderH1Dynamic(pb, assign);
    } else if (settings.rolloutHeuristic == 1 && settings.dynamicHeuristic) {
        return assignMostFrequentVarH2Dynamic(pb, assign);
    } else if (settings.rolloutHeuristic == 1 && settings.dynamicHeuristic) {
        return assignMostFrequentLitH3Dynamic(pb, assign);
    }
    return assignAtRandom(pb, assign);
}

/*
    MC Tree
*/

std::size_t AssignmentHash::operator()(const Assignment& assign) const {
    uint h = 0;
    for (const Value v : assign) {
        h = (h*3 + v+1) % HASH_MOD;
    }
    return h;
}

vector<Literal> nextActionsFrom(SatProblem& pb, const Assignment& assign, MCSettings& settings) {
    int sortHeuristic = settings.nodeActionVarsHeuristic;
    int limit = settings.nodeNActionVars;
    vector<pair<int, int>> scoresActions; // (score, action)
    vector<array<int, 2>> nbTimesAs;

    if (sortHeuristic >= 2) {
        nbTimesAs = vector<array<int, 2>>(pb.nVars, {0, 0});
        vector<Clause> clauses;
        if (settings.nodeActionHeuristicDynamic) {
            clauses.clear();
            for (int iCls : pb.unverifiedClauses(assign)) {
                clauses.push_back(pb.clauses[iCls]);
            }
        } else {
            clauses = pb.clauses;
        }
        for (const Clause& cls : clauses) {
            for (const Literal& lit : cls) {
                nbTimesAs[lit.varId][lit.isTrue]++;
            }
        }
    }

    for (int iVar = 0; iVar < pb.nVars; iVar++) {
        if (assign[iVar] == UNASSIGNED) {
            int score = iVar; // H1: in order
            if (sortHeuristic == 0) { // H0: Random
                score = rand();
            } else if (sortHeuristic == 3) { // H3: max literal 
                score = max(nbTimesAs[iVar][0], nbTimesAs[iVar][1]) * (-1); // -1 to put larger at the beginning
            } else if (sortHeuristic == 2) { // H2: max variable 
                score = (nbTimesAs[iVar][0] + nbTimesAs[iVar][1]) * (-1); // -1 to put larger at the beginning
            }
            scoresActions.push_back({score, iVar});
        }
    }
    if (sortHeuristic != 1) {
        sort(begin(scoresActions), end(scoresActions));
    }
    while (limit && scoresActions.size() > limit) {
        scoresActions.pop_back();
    }

    vector<Literal> actions;
    for (auto& scoreAction : scoresActions) {
        actions.push_back({scoreAction.second, true});
        actions.push_back({scoreAction.second, false});
    }
    return actions;
}

MCState::MCState(MCSettings& settings, SatProblem& pb, Assignment& assign) {
    stateAssign = assign;
    nbTimesSeen = 0;
    nbUnassigned = count(begin(stateAssign), end(stateAssign), UNASSIGNED);
    terminal = (nbUnassigned == 0);

    nextActions = nextActionsFrom(pb, assign, settings);
    nbSubExplorations = 0;
    actionsNExplorations = vector<int>(nextActions.size(), 0);
    // actionsQValues = vector<double>(nextActions.size(), 0); // TODO: which starting value?
    actionsQValues = vector<double>(nextActions.size(), 1);
}

template<class S>
int MCState::rolloutValue(MCTSInstance<S>& inst) {
    auto nextAssign = applyHeuristic(inst.pb, stateAssign, inst.settings);
    int walkSatBudget = nbUnassigned * inst.settings.walkBudgetPerVar;

    nextAssign = applyWalkSat(inst.pb, nextAssign, walkSatBudget, inst.settings.walkEps);
    int score = inst.pb.score(nextAssign);
    inst.updateBest(nextAssign, score);

    return score;
}

template<class S>
Literal MCState::getAction(MCTSInstance<S>& inst) {
    int bestActionId = -1;
    double bestUCTVal = 0;
    for (int iAction = 0; iAction < (int)nextActions.size(); iAction++) {
        double N_c = actionsNExplorations[iAction];
        double N_tot = max(nbSubExplorations, 1);
        double uctVal = (
            actionsQValues[iAction]
            + inst.settings.ucbCExplo * sqrt(log(N_tot) / (N_c + 1.))
        );
        if (bestActionId == -1 || bestUCTVal < uctVal) {
            uctVal = bestUCTVal;
            bestActionId = iAction;
        }
    }
    return nextActions[bestActionId];
}

template<class S>
void MCState::updateAfterAction(MCTSInstance<S>& inst, Literal action, int score) {
    int actionId = -1;
    for (int i = 0; i < (int)nextActions.size(); i++) {
        if (action == nextActions[i]) {
            actionId = i;
        }
    }
    assert((actionId >= 0));
    double N_c = actionsNExplorations[actionId];
    double score_value = (double)(inst.pb.nClauses - score) / inst.pb.nClauses; // Fraction of OK clauses
    actionsQValues[actionId] = (N_c * actionsQValues[actionId] + score_value) / (N_c + 1.);
    actionsNExplorations[actionId] += 1;
    nbSubExplorations += 1;
}


/*
    MC Instances
*/

template<class S>
MCTSInstance<S>::MCTSInstance(const MCSettings& _settings, const SatProblem& _pb)
    :settings(_settings), pb(_pb), tree() {
    bestAssignment = pb.randomAssignment();
    minUnverified = pb.score(bestAssignment);
}

template<class S>
S& MCTSInstance<S>::get(Assignment& assign) {
    if (tree.find(assign) == tree.end()) {
        tree.insert({assign, S{settings, pb, assign}});
    }
    return tree.at(assign);
}

template<class S>
void MCTSInstance<S>::updateBest(Assignment& assign, int nbUnverified) {
    if (nbUnverified < 0) {
        nbUnverified = pb.score(assign);
    }
    if (nbUnverified < minUnverified) {
        minUnverified = nbUnverified;
        bestAssignment = assign;
    }
}


/*
    MC Algorithms
*/

Assignment applyAction(const Assignment& assign, Literal action) {
    auto nextAssign = assign;
    nextAssign[action.varId] = action.isTrue;
    return nextAssign;
}

int MCTSearchDfs(MCTSInstance<>& inst, Assignment& assign) {
    MCState& state = inst.get(assign);
    state.nbTimesSeen += 1;

    if (state.terminal || state.nbTimesSeen == 1) {
        return state.rolloutValue(inst);
    }
    Literal action = state.getAction(inst);
    auto nextAssign = applyAction(assign, action);
    int score = MCTSearchDfs(inst, nextAssign);
    state.updateAfterAction(inst, action, score);
    return score;
}

void runMCTS(MCTSInstance<>& inst, int steps) {
    auto freeAssign = inst.pb.freeAssignment();
    for (int iStep = 0; iStep < steps; iStep++) {
        MCTSearchDfs(inst, freeAssign);
    }
}

/*
    Template instantiation
*/

template struct MCTSInstance<MCState>;