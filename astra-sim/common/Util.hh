#ifndef __ASRTASIM_COMMON_UTIL__
#define __ASRTASIM_COMMON_UTIL__

#include <cstdint>
#include <string>

namespace AstraSim {
namespace Util {

class HashUtil {
    static inline uint64_t fnv1a_64(const std::string& s);
    static inline uint64_t hash_combine(uint64_t h, uint64_t v);
};

}  // namespace Util
}  // namespace AstraSim

#endif
