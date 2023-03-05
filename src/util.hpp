#ifndef UTIL_HPP
#define UTIL_HPP

#include <ostream>
#include <iostream>
#include <vector>

#include "maxsat.hpp"

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