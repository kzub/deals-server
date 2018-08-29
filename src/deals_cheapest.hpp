#ifndef SRC_DEALS_CHEAPEST_HPP
#define SRC_DEALS_CHEAPEST_HPP

#include <unordered_map>
#include <unordered_set>
#include "deals_query.hpp"
#include "deals_types.hpp"
#include "search_query.hpp"

namespace deals {
//------------------------------------------------------------
// SimplyCheapest
//------------------------------------------------------------
class SimplyCheapest : public DealsSearchQuery {
 public:
  SimplyCheapest(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }
  // implement virtual functions:
  void process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;
  const std::vector<i::DealInfo> get_result() const final override;

 private:
  std::vector<i::DealInfo> exec_result;
  std::unordered_map<uint32_t, i::DealInfo> grouped_destinations;
  std::unordered_map<uint32_t, std::vector<i::DealInfo>> grouped_destinations_hist;
};
}  // namespace deals

#endif