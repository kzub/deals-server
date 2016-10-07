#ifndef SRC_CACHE_HPP
#define SRC_CACHE_HPP

#include "timing.hpp"
namespace cache {

template <typename TYPE>
class Cache {
 public:
  Cache(TYPE& val, uint32_t lifetime_sec);
  // Cache(const Cache<TYPE>& cache);
  // Cache(Cache<TYPE>&& cache);
  bool is_expired();
  TYPE& get_value();

 private:
  uint32_t ts_live_until_sec;
  TYPE value;
};

// IMPLEMENTATION:
// constructor
template <typename TYPE>
Cache<TYPE>::Cache(TYPE& val, uint32_t lifetime_sec) : value(val) {
  std::cout << "CONST" << std::endl;
  ts_live_until_sec = lifetime_sec + timing::getTimestampSec();
};

/*template <typename TYPE>
Cache<TYPE>::Cache(const Cache<TYPE>& cache) {
  std::cout << "COPYCONST" << std::endl;
  ts_live_until_sec = cache.ts_live_until_sec;
  value = cache.value;
};

template <typename TYPE>
Cache<TYPE>::Cache(Cache<TYPE>&& cache) {
  std::cout << "MOVECONST" << std::endl;
  ts_live_until_sec = cache.ts_live_until_sec;
  value = cache.value;
};*/

// is_expired()
template <typename TYPE>
bool Cache<TYPE>::is_expired() {
  auto now = timing::getTimestampSec();
  std::cout << "diff:" << std::to_string(ts_live_until_sec - now) << std::endl;
  return ts_live_until_sec < now;
}

// get()
template <typename TYPE>
TYPE& Cache<TYPE>::get_value() {
  return value;
}

}  // namespace end

#endif