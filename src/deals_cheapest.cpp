#include "deals_cheapest.hpp"

namespace deals {
//----------------------------------------------------------------
void SimplyCheapest::pre_search() {
  if (filter_all_combinations) {
    std::cerr << "ERROR all_combinations not implemented for cheapest aggregation" << std::endl;
    throw types::Error("all_combinations flag not implemented for cheapest aggregation\n");
  }
}

//---------------------------------------------------------
void SimplyCheapest::process_deal(const i::DealInfo &deal) {
  auto &dst_deal = grouped_destinations[deal.destination];

  if (dst_deal.price == 0 || deal.price <= dst_deal.price) {
    // ignore same route and dates with lower price, but older timestamp
    if (utils::equal(deal, dst_deal) && dst_deal.timestamp > deal.timestamp) {
      return;
    }
    dst_deal = deal;
    // save for price history analysis
    grouped_destinations_hist[deal.destination].push_back(deal);
    return;
  }

  // if  not cheaper but same dates and direct/stops, replace with newer results
  if (utils::equal(deal, dst_deal) && dst_deal.timestamp < deal.timestamp) {
    dst_deal = deal;
    dst_deal.overriden = true;
    // save for price history analysis
    grouped_destinations_hist[deal.destination].push_back(deal);
  }
}

//----------------------------------------------------------------
void SimplyCheapest::post_search() {
  for (auto &v : grouped_destinations_hist) {
    exec_result.push_back(findCheapestAndLast(v.second));
  }

  // sort by price ASC
  std::sort(exec_result.begin(), exec_result.end(),
            [](const i::DealInfo &a, const i::DealInfo &b) { return a.price < b.price; });
}

//----------------------------------------------------------------
// check for result is not cheapest and outdated at the same time
//----------------------------------------------------------------
const i::DealInfo SimplyCheapest::findCheapestAndLast(std::vector<i::DealInfo> &history) {
  auto last_deal = history.back();
  history.pop_back();

  for (auto i = 0; i < history.size(); i++) {
    const auto &d = history[i];
    // when result contain equivalent deals with different timestamps
    if (utils::equal(d, last_deal)) {
      // if cheapest is outdated => select previous deal from history
      // and start from the begining
      if (d.timestamp > last_deal.timestamp) {
        last_deal = history.back();
        history.pop_back();
        i = 0;
        continue;
      }
    }
  }

  // when we rich this point last_deal - is cheapest and newest available
  return last_deal;
}

//----------------------------------------------------------------
const std::vector<i::DealInfo> SimplyCheapest::get_result() const {
  return exec_result;
}
}  // namespace deals