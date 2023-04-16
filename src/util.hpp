#ifndef UTIL_HPP
#define UTIL_HPP

#include <ostream>
#include <iostream>
#include <vector>

#include "maxsat.hpp"

static const double INF = 1e9;

static const char* C_RESET = "\033[0m";
static const char* C_RED = "\033[31m";
static const char* C_GREEN = "\033[32m";
static const char* C_YELLOW = "\033[33m";
static const char* C_BLUE = "\033[34m";
static const char* C_MAGENTA = "\033[35m";
static const char* C_CYAN = "\033[36m";
static const char* C_BOLD = "\033[1m";
static const char* C_UNDERLINE = "\033[4m";
static const char* C_INVERSE = "\033[7m";

template<class T> std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    os << "[ ";
    for (auto& el : v) {
        os << el << ", ";
    }
    os << "]";
    return os;
}


template<> 
std::ostream& operator<<(std::ostream& os, const Assignment& v);
std::ostream& operator<<(std::ostream& os, const Literal& v);

#endif