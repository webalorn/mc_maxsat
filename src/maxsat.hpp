#include <vector>
#include <array>

struct Literal {
    int varId;
    bool isTrue;
};

using Value = signed char;
using Clause = std::vector<Literal>;
using Assignment = std::vector<Value>;

struct SatProblem {
    int nVars, nClauses;
    std::vector<Clause> clauses;
    std::vector<std::vector<int>> clausesUsingVar;
    std::vector<std::array<std::vector<int>, 2>> clausesUsingLit;

    int minUnverified;
    Assignment bestAssignment;

    SatProblem(std::vector<Clause>& initClauses, int initNVars=0);

    Assignment freeAssignment();
    Assignment randomAssignment();
    std::vector<int> unverifiedClauses(Assignment&);
    int score(Assignment&);
    void updateBest(Assignment& assign, int nbUnverified = -1);
};

const Value UNASSIGNED = -1;
const Value VAR_TRUE = 1;
const Value VAR_FALSE = 0;

/*
    AssignmentHeuristics
*/

void assignAtRandom(SatProblem&, Assignment&);

// Static versions assign using statistics from the whole SAT problem
void assignInOrderH1Static(SatProblem&, Assignment&);
void assignMostFrequentVarH2Static(SatProblem&, Assignment&);
void assignMostFrequentLitH3Static(SatProblem&, Assignment&);

// Dynamic versions assign using statistics from all the untrue clauses
void assignInOrderH1Dynamic(SatProblem&, Assignment&);
void assignMostFrequentVarH2Dynamic(SatProblem&, Assignment&);
void assignMostFrequentLitH3Dynamic(SatProblem&, Assignment&);

/*
    Algorithms
*/
void applyWalkSat(SatProblem&, Assignment&, int, float);