#ifndef SRC_DEALS_UNIQUE_ROUTES_HPP
#define SRC_DEALS_UNIQUE_ROUTES_HPP

#include "deals_types.hpp"
#include "shared_memory.hpp"

#include <unordered_map>

namespace deals {
//------------------------------------------------------------
// UniqueRoutes
//------------------------------------------------------------
const std::vector<DealInfo> getUniqueRoutesRoutine(shared_mem::Table<i::DealInfo>& table);

//------------------------------------------------------------
//
//------------------------------------------------------------
class UniqueProcessor : public shared_mem::TableProcessor<i::DealInfo> {
 public:
  std::vector<DealInfo> getResults();

 protected:
  // function that will be called for iterating over all not expired pages in table
  void process_element(const i::DealInfo& element) final override;
  std::unordered_map<uint64_t, i::DealInfo> grouped_by_routes;
};

}  // namespace deals
#endif