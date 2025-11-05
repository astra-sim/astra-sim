#include "astra-sim/common/Util.hh"

using namespace AstraSim::Util;

inline uint64_t AstraSim::Util::HashUtil::fnv1a_64(const std::string& s) {
    uint64_t hash = 1469598103934665603ul;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= 1099511628211ul;
    }
    return hash;
}

inline uint64_t AstraSim::Util::HashUtil::hash_combine(uint64_t h, uint64_t v) {
    // Murmur-inspired mixing
    v ^= v >> 33;
    v *= 0xff51afd7ed558ccdul;
    v ^= v >> 33;
    v *= 0xc4ceb9fe1a85ec53ul;
    v ^= v >> 33;
    h ^= v;
    h *= 0xff51afd7ed558ccdul;
    return h;
}
