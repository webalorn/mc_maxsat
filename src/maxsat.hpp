#ifndef MAXSAT_HPP
#define MAXSAT_HPP

#include <vector>
#include <array>

struct SatProblem;

struct Literal {
    int varId;
    bool isTrue;
    int id(const SatProblem&) const;
};

struct Clause {
    std::vector<Literal> literals;
    double weight;
};

bool operator==(const Literal&, const Literal&);
bool operator<(const Literal&, const Literal&);

using Value = signed char;
using Assignment = std::vector<Value>;

struct SatProblem {
    int nVars, nClauses;
    double totalWeight;
    std::vector<Clause> clauses;
    std::vector<std::vector<int>> clausesUsingVar;
    std::vector<std::array<std::vector<int>, 2>> clausesUsingLit;

    SatProblem(std::vector<Clause>& initClauses, int initNVars=0);

    Assignment freeAssignment() const;
    Assignment randomAssignment() const;
    std::vector<int> unverifiedClauses(const Assignment&) const;
    double score(const Assignment&) const;
};

const Value UNASSIGNED = -1;
const Value VAR_TRUE = 1;
const Value VAR_FALSE = 0;

/*
    Assignment Heuristics
*/

Assignment assignAtRandom(const SatProblem&, const Assignment&);

// Static versions assign using statistics from the whole SAT problem
Assignment assignInOrderH1Static(const SatProblem&, const Assignment&);
Assignment assignMostFrequentVarH2Static(const SatProblem&, const Assignment&);
Assignment assignMostFrequentLitH3Static(const SatProblem&, const Assignment&);

// Dynamic versions assign using statistics from all the untrue clauses
Assignment assignInOrderH1Dynamic(const SatProblem&, const Assignment&);
Assignment assignMostFrequentVarH2Dynamic(const SatProblem&, const Assignment&);
Assignment assignMostFrequentLitH3Dynamic(const SatProblem&, const Assignment&);

/*
    Algorithms
*/
Assignment applyWalkSat(const SatProblem&, const Assignment&, int, float, bool);
Assignment applyMaxWalkSat(const SatProblem&, const Assignment&, int, float, bool);

#endif