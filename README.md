# Monte Carlo for MAXSat

Variables are between 0 and N-1. A clause is a list of integers and booleans, with `(i,b)` representing the variable `i`, and `b` being false if and only iff the literal is negated. A solution is a list of integers values for each variable, with `-1` meaning that the variable is unassigned, and `0` and `1` being the boolean values.