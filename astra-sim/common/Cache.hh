#ifndef ASTRASIM_COMMON_CACHE_HH
#define ASTRASIM_COMMON_CACHE_HH

#include <atomic>
#include <list>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace AstraSim {
namespace Utils {
template <typename K, typename T>
class Cache {
 public:
  Cache(size_t capacity) : capacity(capacity) {}

  void put(const K& key, const T& value);

  bool has(const K& key);

  const std::weak_ptr<const T> get(const K& key);

  const std::shared_ptr<const T> get_locked(const K& key);

  void remove(const K& key);

  ~Cache() {
    cache.clear();
    lru.clear();
  }

 private:
  size_t capacity;
  std::unordered_map<
      K,
      std::pair<std::shared_ptr<const T>, typename std::list<K>::iterator>>
      cache;
  std::list<K> lru;
  std::shared_mutex cache_mutex;
};
} // namespace Utils
} // namespace AstraSim

#endif /*ASTRASIM_COMMON_CACHE_HH*/
