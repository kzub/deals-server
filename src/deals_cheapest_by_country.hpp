#ifndef SRC_DEALS_CHEAPEST_BY_COUNTRY_HPP
#define SRC_DEALS_CHEAPEST_BY_COUNTRY_HPP

#include <unordered_map>
#include <vector>
#include "deals_query.hpp"
#include "deals_types.hpp"
#include "search_query.hpp"

namespace deals {
//------------------------------------------------------------
// CheapestByCountry
//------------------------------------------------------------
class CheapestByCountry : public DealsSearchQuery {
 public:
  CheapestByCountry(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }

  // implement virtual functions:
  void process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;
  const std::vector<i::DealInfo> get_result() const final override;

 private:
  std::vector<i::DealInfo> exec_result;
  std::unordered_map<uint32_t, i::DealInfo> grouped_by_country;
  uint32_t grouped_max_price = 0;
};
}  // namespace deals
#endif