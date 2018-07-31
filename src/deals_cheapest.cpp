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

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    // ignore same route and dates with lower price, but older timestamp
    if (deal.destination == dst_deal.destination && deal.return_date == dst_deal.return_date &&
        deal.direct == dst_deal.direct && dst_deal.timestamp > deal.timestamp) {
      return;
    }
    dst_deal = deal;
  }
  // if  not cheaper but same dates and direct/stops, replace with newer results
  else if (deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.direct == dst_deal.direct &&
           dst_deal.timestamp < deal.timestamp) {
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