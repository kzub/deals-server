#include "deals_cheapest.hpp"

namespace deals {
//----------------------------------------------------------------
void SimplyCheapest::pre_search() {
  grouped_max_price = 0;
  if (filter_destination) {
    filter_result_limit = destination_values_set.size();
  }
}

//---------------------------------------------------------
void SimplyCheapest::process_deal(const i::DealInfo &deal) {
  if (grouped_destinations.size() >= filter_result_limit) {
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
}

//----------------------------------------------------------------
const std::vector<i::DealInfo> SimplyCheapest::get_result() const {
  return exec_result;
}
}  // namespace deals