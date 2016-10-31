#include "deals_cheapest.hpp"

namespace deals {
//----------------------------------------------------------------
void SimplyCheapest::pre_search() {
  grouped_max_price = 0;
}

//---------------------------------------------------------
void SimplyCheapest::process_deal(const i::DealInfo &deal) {
  if (grouped_destinations.size() > result_destinations_count) {
    if (grouped_max_price <= deal.price) {
      return;  // deal price is far more expensive, skip grouping
    }
  }
  if (grouped_max_price < deal.price) {
    grouped_max_price = deal.price;
  }

  auto &dst_deal = grouped_destinations[deal.destination];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    dst_deal = deal;
  }
  // if  not cheaper but same dates and direct/stops, replace with newer results
  else if (deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.direct == dst_deal.direct) {
    dst_deal = deal;
    dst_deal.overriden = true;
  }
}

//----------------------------------------------------------------
void SimplyCheapest::post_search() {
  for (const auto &v : grouped_destinations) {
    exec_result.push_back(v.second);
  }

  // sort by price ASC
  std::sort(exec_result.begin(), exec_result.end(),
            [](const i::DealInfo &a, const i::DealInfo &b) { return a.price < b.price; });

  // reduce output size
  if (exec_result.size() > result_destinations_count) {
    exec_result.resize(result_destinations_count);
  }
  if (filter_result_limit && exec_result.size() > filter_result_limit) {
    exec_result.resize(filter_result_limit);
  }
}

//----------------------------------------------------------------
std::vector<i::DealInfo> SimplyCheapest::get_result() {
  return exec_result;
}
}  // namespace deals