#include <cstdlib>
#include <algorithm>
#include <vector>
#include <array>
#include <numeric>
#include <string>
#include <queue>
#include <iostream>

#include "maxsat.hpp"

using namespace std;

bool operator==(const Literal& a, const Literal& b) {
    return a.varId == b.varId && a.isTrue == b.isTrue;
}

SatProblem::SatProblem(vector<Clause>& initClauses, int initNVars) {
    clauses = initClauses;
    nClauses = clauses.size();
    nVars = initNVars;
    for (const Clause& cls: clauses) {
        for (const Literal& lit : cls) {
            nVars = max(nVars, lit.varId+1);
        }
    }

    clausesUsingVar = vector<vector<int>>(nVars, vector<int>());
    clausesUsingLit = vector<array<vector<int>, 2>>(nVars, {vector<int>(), vector<int>()});
    for (int iCls = 0; iCls < nClauses; iCls++) {
        for (const Literal& lit : clauses[iCls]) {
            clausesUsingVar[lit.varId].push_back(iCls);
            clausesUsingLit[lit.varId][lit.isTrue].push_back(iCls);
        }
    }
}

Assignment SatProblem::freeAssignment() const {
    return Assignment(this->nVars, UNASSIGNED);
}

Assignment SatProblem::randomAssignment() const {
    auto assign = this->freeAssignment();
    assignAtRandom(*this, assign);
    return assign;
}

vector<int> SatProblem::unverifiedClauses(const Assignment& assign) const {
    vector<int> unverified;
    for (int iCls = 0; iCls < (int)this->nClauses; iCls++) {
        bool verified = false;
        for (const Literal& lit : this->clauses[iCls]) {
            if (assign[lit.varId] == lit.isTrue) {
                verified = true;
                break;
            }
        }
        if (!verified) {
            unverified.push_back(iCls);
        }
    }
    return unverified;
}

int SatProblem::score(const Assignment& assign) const {
    return this->unverifiedClauses(assign).size();
}


/*
    Assignment Heuristics
*/

Assignment assignAtRandom(const SatProblem& pb, const Assignment& prevAssign) {
    auto assign = prevAssign;
    for (int iVar = 0; iVar < pb.nVars; iVar++) {
        if (assign[iVar] == UNASSIGNED) {
            assign[iVar] = rand()%2;
        }
    }
    return assign;
}

Assignment assignStatic(const SatProblem& pb, const Assignment& prevAssign, string sortOrder) {
    auto assign = prevAssign;
    vector<array<int, 2>> nbTimesAs(pb.nVars, {0, 0});
    vector<int> varOrder(pb.nVars);
    iota(begin(varOrder), end(varOrder), 0);

    for (const Clause& cls : pb.clauses) {
        for (const Literal& lit : cls) {
            nbTimesAs[lit.varId][lit.isTrue]++;
        }
    }
    if (sortOrder == "max_literal") {
        sort(varOrder.begin(), varOrder.end(), [&nbTimesAs](int i1, int i2) -> bool { 
            return max(nbTimesAs[i1][0], nbTimesAs[i1][1]) >= max(nbTimesAs[i2][0], nbTimesAs[i2][1]); 
        });
    } else if (sortOrder == "max_var") {
        sort(varOrder.begin(), varOrder.end(), [&nbTimesAs](int i1, int i2) -> bool { 
            return nbTimesAs[i1][0] + nbTimesAs[i1][1] >= nbTimesAs[i2][0] + nbTimesAs[i2][1]; 
        });
    }
    // reverse(varOrder.begin(), varOrder.end());
    for (int iVar : varOrder) {
        if (assign[iVar] == UNASSIGNED) {
            assign[iVar] = (nbTimesAs[iVar][1] >= nbTimesAs[iVar][0]);
        }
    }
    return assign;
}

Assignment assignInOrderH1Static(const SatProblem& pb, const Assignment& assign) {
    return assignStatic(pb, assign, "None");
}

Assignment assignMostFrequentVarH2Static(const SatProblem& pb, const Assignment& assign) {
    return assignStatic(pb, assign, "max_var");
}

Assignment assignMostFrequentLitH3Static(const SatProblem& pb, const Assignment& assign) {
    return assignStatic(pb, assign, "max_literal");
}

Assignment assignDynamic(const SatProblem& pb, const Assignment& prevAssign, bool scoreIsId, bool scoreByLiteral) {
    auto assign = prevAssign;
    vector<array<int, 2>> nbOccLit(pb.nVars, {0, 0});
    vector<vector<int>> clausesUsingVar(pb.nVars);
    vector<bool> isClauseVerified(pb.nClauses, false);

    // Init
    for (int iCls = 0; iCls < (int)pb.nClauses; iCls++) {
        for (const Literal& lit : pb.clauses[iCls]) {
            nbOccLit[lit.varId][lit.isTrue] += 1;
            clausesUsingVar[lit.varId].push_back(iCls);
        }
    }
    // Compute initial score for variables
    vector<int> varScore(pb.nVars, 0);
    priority_queue<pair<int, int>> scoresWithVars; // (score, varId);
    for (int iVar = 0; iVar < pb.nVars; iVar++) {
        if (assign[iVar] == UNASSIGNED) {
            varScore[iVar] = scoreIsId ? (-iVar) : (
                scoreByLiteral ? max(nbOccLit[iVar][0], nbOccLit[iVar][1]) :
                    (nbOccLit[iVar][0] + nbOccLit[iVar][1])
            );
        } else {
            varScore[iVar] = pb.nClauses * 10; // Large score to ensure they will be first
        }
        scoresWithVars.push({varScore[iVar], iVar});
    }

    // Loop over variables in the order of their score
    while (!scoresWithVars.empty()) {
        int score = scoresWithVars.top().first;
        int iVar = scoresWithVars.top().second;
        scoresWithVars.pop();

        if (score == varScore[iVar]) { // Check if no update in between
            // Assign the variable
            if (assign[iVar] == UNASSIGNED) {
                assign[iVar] = (nbOccLit[iVar][1] >= nbOccLit[iVar][0]);
            }
            // Remove newly verified clauses using this literal
            for (int iCls : clausesUsingVar[iVar]) {
                if (!isClauseVerified[iCls]) {
                    isClauseVerified[iCls] = true;
                    // Update literals that are in this clause
                    for (const Literal& lit : pb.clauses[iCls]) {
                        nbOccLit[lit.varId][lit.isTrue] -= 1;
                        if (assign[lit.varId] == UNASSIGNED && !scoreIsId) {
                            // If the score changed for the lit variable, update it
                            int nextVarScore = scoreByLiteral ?
                                max(nbOccLit[lit.varId][0], nbOccLit[lit.varId][1])
                                : (nbOccLit[lit.varId][0] + nbOccLit[lit.varId][1])
                            ;
                            if (nextVarScore < varScore[lit.varId]) {
                                varScore[lit.varId] = nextVarScore;
                                scoresWithVars.push({nextVarScore, lit.varId});
                            }
                        }
                    }
                }
            }
        }
    }
    return assign;
}

Assignment assignInOrderH1Dynamic(const SatProblem& pb, const Assignment& assign) {
    return assignDynamic(pb, assign, true, false);
}

Assignment assignMostFrequentVarH2Dynamic(const SatProblem& pb, const Assignment& assign) {
    return assignDynamic(pb, assign, false, false);
}

Assignment assignMostFrequentLitH3Dynamic(const SatProblem& pb, const Assignment& assign) {
    return assignDynamic(pb, assign, false, true);
}


/*
    Algorithms
*/
Assignment applyWalkSat(const SatProblem& pb, const Assignment& prevAssign, int flipBudget, float randEps) {
    auto assign = prevAssign;
    auto bestAssign = assign;
    /* Every variable should be assigned prior to calling this function */
    vector<int> clsNbLitTrue(pb.nClauses, 0);
    int nbUnverified = 0;
    for (int iCls = 0; iCls < pb.nClauses; iCls++) {
        for (const Literal& lit : pb.clauses[iCls]) {
            if (assign[lit.varId] == lit.isTrue) {
                clsNbLitTrue[iCls] += 1;
            }
        }
        if (clsNbLitTrue[iCls] == 0) {
            nbUnverified += 1;
        }
    }
    int bestNbUnverified = nbUnverified;

    for (int iFlip = 0; iFlip < flipBudget; iFlip++) {
        vector<int> unverified;
        for (int iCls = 0; iCls < pb.nClauses; iCls++) {
            if (clsNbLitTrue[iCls] == 0) {
                unverified.push_back(iCls);
            }
        }
        if (unverified.empty()) {
            break;
        }
        const Clause& clsSwap = pb.clauses[unverified[rand() % unverified.size()]];
        vector<pair<int, int>> breakScoreVars; // (BreakScore, varId)
        for (const Literal& lit : clsSwap) {
            int nbBreaking = 0;
            for (int linkedClsId : pb.clausesUsingLit[lit.varId][assign[lit.varId]]) {
                if (clsNbLitTrue[linkedClsId] == 1) {
                    nbBreaking += 1;
                }
            }
            breakScoreVars.push_back({nbBreaking, lit.varId});
        }
        sort(begin(breakScoreVars), end(breakScoreVars));
        
        // Remove scores that don't contribute to the minimum break score (nbBreaking only)
        // while (breakScoreVars.size() > 1 &&
        //     breakScoreVars[breakScoreVars.size()-1].first > breakScoreVars[breakScoreVars.size()-2].first) {
        //     breakScoreVars.pop_back();
        // }
        // Choose the variable to flip
        int flipVar = -1;
        float randValue = rand() / (float)RAND_MAX;
        if (breakScoreVars[0].first == 0) { // If the first variable doesn't break anything
            flipVar = breakScoreVars[0].second;
        } else if (randValue < randEps) { // Sometimes, choose a random var
            flipVar = breakScoreVars[rand()%breakScoreVars.size()].second;
        } else { // Take the minimum breaking var
            flipVar = breakScoreVars[0].second;
        }

        // FLIP the variable
        for (int linkedClsId : pb.clausesUsingLit[flipVar][assign[flipVar]]) {
            clsNbLitTrue[linkedClsId] -= 1;
            if (clsNbLitTrue[linkedClsId] == 0) {
                nbUnverified += 1;
            }
        }
        assign[flipVar] = 1 - assign[flipVar];
        for (int linkedClsId : pb.clausesUsingLit[flipVar][assign[flipVar]]) {
            clsNbLitTrue[linkedClsId] += 1;
            if (clsNbLitTrue[linkedClsId] == 1) {
                nbUnverified -= 1;
            }
        }

        if (nbUnverified < bestNbUnverified) {
            bestNbUnverified = nbUnverified;
            bestAssign = assign;
        }
    }
    return bestAssign;
}