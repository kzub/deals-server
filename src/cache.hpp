#ifndef SRC_CACHE_HPP
#define SRC_CACHE_HPP

#include "timing.hpp"
namespace cache {

template <typename TYPE>
class Cache {
 public:
  Cache(const TYPE& val, uint32_t lifetime_sec);
  bool is_expired() const;
  TYPE get_value() const;

 private:
  uint32_t ts_live_until_sec;
  TYPE value;
};

//                             IMPLEMENTATIONS:
// constructor --------------------------------------------------
template <typename TYPE>
Cache<TYPE>::Cache(const TYPE& val, uint32_t lifetime_sec) : value(val) {
  ts_live_until_sec = lifetime_sec + timing::getTimestampSec();
};

// is_expired() --------------------------------------------------
template <typename TYPE>
bool Cache<TYPE>::is_expired() const {
  auto now = timing::getTimestampSec();
  // std::cout << "diff:" << std::to_string(ts_live_until_sec - now) << std::endl;
  return ts_live_until_sec < now;
}

// get_value() --------------------------------------------------
template <typename TYPE>
TYPE Cache<TYPE>::get_value() const {
  return value;
}

}  // namespace end

#endif