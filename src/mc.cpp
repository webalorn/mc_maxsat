#include <iostream>
#include <algorithm>
#include <cassert>

#include "mc.hpp"

using namespace std;

/*
    Settings
*/

MCSettings::MCSettings(){
    seed = 0;

    // Actions from nodes (MCTS, NMCS)
    nodeNActionVars = 2;
    nodeActionVarsHeuristic = 3;
    nodeActionHeuristicDynamic = false;

    // Rollout heuristic + algorithm (MCTS, NMCS)
    rolloutHeuristic = 3;
    dynamicHeuristic = false;
    flipAlgorithm = "walksat";
    walkBudgetPerVar = 2;
    walkEps = 0.2;

    steps = 100; // Steps for MCTS and budget of Sequential Halving ; number of repeats for NMCS or rollout
    behavior = "once"; // once for running only form root ; full for looping, discounted for looping faster
    amaf = 0; // AMAF coefficient (0 to disable ; currently, only for SH)
    amafBias = 0; // AMAF bias, allows to decrease the AMAF value quadraticly

    // MCTS only
    ucbCExplo = 0.05;

    // NMCS only
    nmcsDepth = 1;
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
            for (const Literal& lit : cls.literals) {
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
    bestActionId = -1;

    nextActions = nextActionsFrom(pb, assign, settings);
    nbSubExplorations = 0;
    actionsNExplorations = vector<int>(nextActions.size(), 0);
    bestScoresForActions = vector<double>(nextActions.size(), 0);
    for (double& score : bestScoresForActions) {
        score = INF - (rand() % 1000000); // Large random number
    }
    // actionsQValues = vector<double>(nextActions.size(), 0); // TODO: which starting value?
    actionsQValues = vector<double>(nextActions.size(), 1);
}

template<class S>
double MCState::rolloutValue(MCTSInstance<S>& inst) {
    auto nextAssign = applyHeuristic(inst.pb, stateAssign, inst.settings);
    nextAssign = applyFlipAlgorithm(inst, nextAssign, nbUnassigned);
    int score = inst.pb.score(nextAssign);
    inst.updateBest(nextAssign, score);

    return score;
}

template<class S>
Literal MCState::getUCBAction(MCTSInstance<S>& inst, bool allowExploration) {
    int ucbBestId = -1;
    double bestUCTVal = 0;
    double ucbCExplo = allowExploration ? inst.settings.ucbCExplo : 0;
    // cerr << "Actions: ";
    for (int iAction = 0; iAction < (int)nextActions.size(); iAction++) {
        double N_c = actionsNExplorations[iAction];
        double N_tot = max(nbSubExplorations, 1);
        double uctVal = (
            actionsQValues[iAction]
            + ucbCExplo * sqrt(log(N_tot) / (N_c + 1.))
        );
        if (ucbBestId == -1 || bestUCTVal < uctVal) {
            bestUCTVal = uctVal;
            ucbBestId = iAction;
        }
        // cerr << nextActions[iAction] << " (" << uctVal << ") ";
        // cerr << "[N_c=" << N_c << ", N_tot=" << N_tot << ", q=" << actionsQValues[iAction] << "] ";
    }
    // cerr << endl << endl;
    return nextActions[ucbBestId];
}

int MCState::getActionId(const Literal& action) {
    int actionId = -1;
    for (int i = 0; i < (int)nextActions.size(); i++) {
        if (action == nextActions[i]) {
            actionId = i;
        }
    }
    assert((actionId >= 0));
    return actionId;
}

template<class S>
void MCState::updateAfterAction(MCTSInstance<S>& inst, Literal action, int score) {
    int actionId = this->getActionId(action);
    for (int i = 0; i < (int)nextActions.size(); i++) {
        if (action == nextActions[i]) {
            actionId = i;
        }
    }
    assert((actionId >= 0));
    double N_c = actionsNExplorations[actionId];
    //double score_value = (double)(inst.pb.nClauses - score) / inst.pb.nClauses; // Fraction of OK clauses
    double score_value = (double)(inst.pb.totalWeight - score) / inst.pb.totalWeight; // Fraction of total Weight
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
    minUnverifiedWeight = pb.score(bestAssignment);
    amafCount = vector<int>(pb.nVars*2, 0);
    amafMin = vector<double>(pb.nVars*2, INF);
}

template<class S>
void MCTSInstance<S>::amafAddResult(const Literal& lit, double score, int count) {
    int litId = lit.id(pb);
    amafCount[litId] += count;
    amafMin[litId] = min(score, amafMin[litId]);
}

template<class S>
double MCTSInstance<S>::amafGet(const Literal& lit, double realValue, int count) {
    int litId = lit.id(pb);
    if (!amafCount[litId]) {
        return realValue;
    }
    double amafCoeff = 1 / (1. + (double)count / amafCount[litId] + (double)settings.amafBias * count);
    // cerr << "count=" << count << " amafCount[litId]=" << amafCount[litId] << " amafCoeff = " << amafCoeff << endl;
    amafCoeff = min(1., settings.amaf * amafCoeff);
    return realValue * (1 - amafCoeff) + amafMin[litId] * amafCoeff;
}

template<class S>
S* MCTSInstance<S>::get(Assignment& assign) {
    if (tree.find(assign) == tree.end()) {
        tree.insert({assign, unique_ptr<S>(new S{settings, pb, assign})});
    }
    return tree.at(assign).get();
}

template<class S>
void MCTSInstance<S>::updateBest(Assignment& assign, int unverifiedWeight) {
    if (unverifiedWeight < 0) {
        unverifiedWeight = pb.score(assign);
    }
    if (unverifiedWeight < minUnverifiedWeight) {
        minUnverifiedWeight = unverifiedWeight;
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

template<class S>
Assignment applyFlipAlgorithm(MCTSInstance<S>& inst, const Assignment& assign, int nbUnassigned) {
    Assignment nextAssign;
    int flipBudget = inst.pb.nVars * inst.settings.walkBudgetPerVar; // TODO: unassigned or total?

    if (inst.settings.flipAlgorithm == "novelty") {
        nextAssign = applyWalkSat(inst.pb, assign, flipBudget, inst.settings.walkEps, true);
    } else if (inst.settings.flipAlgorithm == "walksat") {
        nextAssign = applyWalkSat(inst.pb, assign, flipBudget, inst.settings.walkEps, false);
    } else if (inst.settings.flipAlgorithm == "weighted_novelty") {
        nextAssign = applyMaxWalkSat(inst.pb, assign, flipBudget, inst.settings.walkEps, true);
    } else if (inst.settings.flipAlgorithm == "maxwalksat") {
        nextAssign = applyMaxWalkSat(inst.pb, assign, flipBudget, inst.settings.walkEps, false);
    } else {
        assert((false));
    }
    return nextAssign;
}


void runRollout(MCTSInstance<>& inst) {
    auto assign = inst.pb.freeAssignment();
    int steps = inst.settings.steps;
    for (int iStep = 0; iStep < steps; iStep++) {
        MCState* state = inst.get(assign);
        state->rolloutValue(inst);
    }
}

int MCTSearchDfs(MCTSInstance<>& inst, Assignment& assign) {
    MCState* state = inst.get(assign);
    state->nbTimesSeen += 1;
    // cerr << "Assign " << assign << " x" << state->nbTimesSeen << endl;

    if (state->terminal || state->nbTimesSeen == 1) {
        return state->rolloutValue(inst);
    }
    Literal action = state->getUCBAction(inst);
    // cerr << "Take action " << action << endl;
    auto nextAssign = applyAction(assign, action);
    int score = MCTSearchDfs(inst, nextAssign);
    state->updateAfterAction(inst, action, score);
    return score;
}

void runMCTS(MCTSInstance<>& inst) {
    int steps = inst.settings.steps;
    auto assign = inst.pb.freeAssignment();
    bool discounted = (inst.settings.behavior == "discounted");
    bool once = (inst.settings.behavior == "once");

    MCState* state = inst.get(assign);
    while (!state->terminal) {
        // Compute number of steps to do if a discount is applied
        int nbPrevSteps = discounted ? state->nbTimesSeen : 0;
        for (int iStep = nbPrevSteps; iStep < steps; iStep++) {
            MCTSearchDfs(inst, assign);
        }
        if (once) {
            break;
        }
        // Take action
        Literal action = state->getUCBAction(inst, false); // No exploration
        assign = applyAction(assign, action);
        state = inst.get(assign);
    }
}

int NMCS(MCTSInstance<>& inst, Assignment assign, int level) {
    MCState* state = inst.get(assign);
    if (state->terminal || level <= 0) {
        return state->rolloutValue(inst);
    }
    int bestSeqScore = INF;
    while (!state->terminal) {
        int bestActionScore = INF;
        Assignment bestNextAssign;
        for (Literal action : state->nextActions) {
            Assignment nextAssign = applyAction(assign, action);
            int actionScore = NMCS(inst, nextAssign, level-1);
            if (actionScore < bestActionScore) {
                bestActionScore = actionScore;
                bestNextAssign = nextAssign;
            }
        }
        assign = bestNextAssign;
        bestSeqScore = min(bestSeqScore, bestActionScore);
        state = inst.get(assign);
    }
    return bestSeqScore;
}

void runNMCS(MCTSInstance<>& inst) {
    for (int step = 0; step < inst.settings.steps; step++) {
        NMCS(inst, inst.pb.freeAssignment(), inst.settings.nmcsDepth);
    }
}

double seqHalving(MCTSInstance<>& inst, Assignment assign, int budget) { 
    MCState* state = inst.get(assign);
    double bestScore = INF;

    // If no budget or terminal, use all the remaining budget on rollouts
    // if (state->terminal || budget <= 1) {
    if (state->terminal || budget < state->nextActions.size()) {
        // for (int step = 0; step < max(budget, 1); step++) {
        for (int step = 0; step < budget; step++) {
            bestScore = min(bestScore, state->rolloutValue(inst));
        }
        return bestScore;
    }
    state->nbTimesSeen += budget;
    
    // Else, split the budget between runs
    vector<pair<double, int>> movesScores;
    for (int iAction = 0; iAction < (int)state->nextActions.size(); iAction++) {
        movesScores.push_back({state->bestScoresForActions[iAction], iAction});
    }
    sort(rbegin(movesScores), rend(movesScores)); // Sort in reverse order to evaluate unseen moves first
    while (movesScores.size() > 1) {
        int loopBudget = budget / ceil(log2(movesScores.size()) + 1);
        budget -= loopBudget;
        for (int iMove = 0; iMove < (int)movesScores.size(); iMove++) {
            int callBudget = loopBudget / (movesScores.size() - iMove);
            loopBudget -= callBudget;

            int iAction = movesScores[iMove].second;
            state->actionsNExplorations[iAction] += callBudget;
            const Literal& action = state->nextActions[iAction];
            auto callAssign = applyAction(assign, action);
            double callScore = seqHalving(inst, callAssign, callBudget);
            
            // Update best scores
            double actionBestScore = min(callScore, state->bestScoresForActions[iAction]);
            // double amafScore = actionBestScore + inst.settings.amaf * inst.amafGet(action, inst.get(assign)->nbTimesSeen);
            double amafScore = inst.amafGet(action, actionBestScore, state->actionsNExplorations[iAction]);
            inst.amafAddResult(action, callScore, callBudget);
            movesScores[iMove].first = amafScore;
            state->bestScoresForActions[iAction] = actionBestScore;
            bestScore = min(bestScore, callScore);
        }
        sort(begin(movesScores), end(movesScores));
        int nKeep = movesScores.size()/2;
        while (movesScores.size() > nKeep) {
            movesScores.pop_back();
        }
    }
    auto nextAssign = applyAction(assign, state->nextActions[movesScores[0].second]);
    state->bestActionId = movesScores[0].second;
    bestScore = min(bestScore, seqHalving(inst,nextAssign, budget));
    return bestScore;
}

void runSeqHalving(MCTSInstance<>& inst) {
    int budget = inst.settings.steps;
    auto assign = inst.pb.freeAssignment();
    bool discounted = (inst.settings.behavior == "discounted");
    bool once = (inst.settings.behavior == "once");

    MCState* state = inst.get(assign);
    while (!state->terminal) {
        // Compute number of steps to do if a discount is applied
        int discount = discounted ? state->nbTimesSeen : 0;
        seqHalving(inst, assign, budget - discount);

        if (once) {
            break;
        }
        // Take action
        assert((state->bestActionId >= 0));
        Literal action = state->nextActions[state->bestActionId];
        assign = applyAction(assign, action);
        state = inst.get(assign);
    }
}

/*
    Template instantiation
*/

template struct MCTSInstance<MCState>;