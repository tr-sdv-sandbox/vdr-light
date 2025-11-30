/*
 * time_utils.cpp - Time utilities for VDR ecosystem
 */

#include "common/time_utils.hpp"

#include <iomanip>
#include <random>
#include <sstream>

namespace utils {

std::string generate_uuid() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937_64 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dist;

    uint64_t part1 = dist(gen);
    uint64_t part2 = dist(gen);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (part1 >> 32) << "-";
    oss << std::setw(4) << ((part1 >> 16) & 0xFFFF) << "-";
    oss << std::setw(4) << (part1 & 0xFFFF) << "-";
    oss << std::setw(4) << (part2 >> 48) << "-";
    oss << std::setw(12) << (part2 & 0xFFFFFFFFFFFF);

    return oss.str();
}

}  // namespace utils
