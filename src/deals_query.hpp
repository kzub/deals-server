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

  inline void reset_group_max_price() {
    group_max_price = 0;
  }
  inline void update_group_max_price(const uint32_t& price) {
    if (group_max_price < price) {
      group_max_price = price;
    }
  }
  inline bool more_than_group_max_price(const uint32_t& price) {
    return price > group_max_price;
  }

 private:
  // function that will be called by TableProcessor
  // for iterating over all not expired pages in table
  void process_element(const i::DealInfo& element) final override;

  // VIRTUALS:
  // if process_element() deside deals worth of processing
  // process_deal() will be called in derivered class
  virtual void process_deal(const i::DealInfo& deal) = 0;

  // before and after processing
  virtual void pre_search() = 0;
  virtual void post_search() = 0;
  virtual const std::vector<i::DealInfo> get_result() const = 0;

  uint32_t group_max_price = 0;
  shared_mem::Table<i::DealInfo>& table;
  uint32_t current_time = 0;

  friend class DealsDatabase;
};
}  // namespace deals
#endif