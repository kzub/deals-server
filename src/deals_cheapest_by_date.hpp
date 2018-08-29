#ifndef SRC_DEALS_CHEAPEST_BY_DATE_HPP
#define SRC_DEALS_CHEAPEST_BY_DATE_HPP

#include <unordered_map>
#include <vector>
#include "deals_query.hpp"
#include "deals_types.hpp"
#include "search_query.hpp"

namespace deals {
//------------------------------------------------------------
// CheapestByDay
//------------------------------------------------------------
class CheapestByDay : public DealsSearchQuery {
 public:
  CheapestByDay(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }

  // implement virtual functions:
  void process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;
  const std::vector<i::DealInfo> get_result() const final override;

 private:
  void checkInputParams();
  query::DateValue getDateToGroup(const i::DealInfo& deal);

  std::vector<i::DealInfo> exec_result;
  std::unordered_map<uint32_t, i::DealInfo> grouped_by_date;
  std::unordered_map<uint32_t, std::vector<i::DealInfo>> grouped_by_date_hist;
  bool group_by_return_date = false;
};
}  // namespace deals
#endif