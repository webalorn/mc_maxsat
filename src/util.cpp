#include "util.hpp"
#include "maxsat.hpp"

using namespace std;

template<> 
std::ostream& operator<<(std::ostream& os, const Assignment& v) {
    os << "[ ";
    for (auto& el : v) {
        os << (int)el << ", ";
    }
    os << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Literal& v) {
    os << v.varId << "_" << v.isTrue;
    return os;
}