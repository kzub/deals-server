#ifndef SRC_DEALS_STATS_HPP
#define SRC_DEALS_STATS_HPP

#include "deals_types.hpp"
#include "shared_memory.hpp"

#include <unordered_map>

namespace deals {
//------------------------------------------------------------
//
//------------------------------------------------------------
const std::string getStatsRoutine(shared_mem::Table<i::DealInfo>& table);

//------------------------------------------------------------
//
//------------------------------------------------------------
class StatsProcessor : public shared_mem::TableProcessor<i::DealInfo> {
 public:
  const std::string getStringResults();
  uint64_t elements = 0;
  uint64_t size = 0;
  uint32_t max = 0;
  uint32_t min = UINT32_MAX;

 protected:
  // function that will be called for iterating over all not expired pages in table
  void process_element(const i::DealInfo& element) final override;
  std::unordered_map<std::string, uint32_t> group_by_route;
};

}  // namespace deals
#endif