#ifndef SRC_DEALS_QUERY_HPP
#define SRC_DEALS_QUERY_HPP

#include "deals_types.hpp"
#include "search_query.hpp"
#include "shared_memory.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace deals {
//------------------------------------------------------------
// DealsSearchQuery
//------------------------------------------------------------
class DealsSearchQuery : public shared_mem::TableProcessor<i::DealInfo>, public query::SearchQuery {
 protected:
  DealsSearchQuery(shared_mem::Table<i::DealInfo>& table) : table(table) {
  }
  // preparations and actual processing
  std::vector<i::DealInfo> execute();

  // array size will be equal to filter_result_limit.
  // used for speed optimization, iteration throught vector is slower
  // uint32_t* destination_values = nullptr;  // <- array
  uint16_t result_destinations_count = 0;

 private:
  // function that will be called by TableProcessor
  // for iterating over all not expired pages in table
  void process_element(const i::DealInfo& element) final override;

  // VIRTUAL FUNCTIONS SECTION:
  virtual void process_deal(const i::DealInfo& deal) = 0;
  // if process_element() deside deals worth of processing
  // process_deal() will be called in parent class

  // before and after processing
  virtual void pre_search() = 0;
  virtual void post_search() = 0;
  virtual std::vector<i::DealInfo> get_result() = 0;

  shared_mem::Table<i::DealInfo>& table;
  uint32_t current_time = 0;
  friend class DealsDatabase;
};
}  // namespace deals
#endif