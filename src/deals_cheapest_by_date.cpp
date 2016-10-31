#include "deals_cheapest_by_date.hpp"

namespace deals {
//----------------------------------------------------------------
void CheapestByDay::pre_search() {
  if (!filter_departure_date || !departure_date_values.duration) {
    std::cerr << "ERROR no departure_date range" << std::endl;
    throw types::Error("departure dates interval must be specified\n");
  }

  // 365 days - is a limit
  if (departure_date_values.duration > 365) {
    std::cerr << "ERROR departure_date_values.duration > 365" << std::endl;
    throw types::Error("Date interval to large. 365 days is maximum\n");
  }

  grouped_max_price = 0;
  filter_result_limit = departure_date_values.duration;
}

//---------------------------------------------------------
void CheapestByDay::process_deal(const i::DealInfo &deal) {
  if (grouped_by_date.size() >= departure_date_values.duration) {
    if (grouped_max_price <= deal.price) {
      return;  // deal price is far more expensive, skip grouping
    }
  }
  if (grouped_max_price < deal.price) {
    grouped_max_price = deal.price;
  }

  auto &dst_deal = grouped_by_date[deal.departure_date];

  if (dst_deal.price == 0 || dst_deal.price >= deal.price) {
    dst_deal = deal;
  }
  // if  not cheaper but same dates, replace with newer results
  else if (deal.departure_date == dst_deal.departure_date &&
           deal.return_date == dst_deal.return_date && deal.direct == dst_deal.direct) {
    dst_deal = deal;
    dst_deal.overriden = true;  // it is used in tests
  }
}

//----------------------------------------------------------------
void CheapestByDay::post_search() {
  for (const auto &deal : grouped_by_date) {
    exec_result.push_back(deal.second);
  }

  // sort by departure_date ASC
  std::sort(exec_result.begin(), exec_result.end(), [](const i::DealInfo &a, const i::DealInfo &b) {
    return a.departure_date < b.departure_date;
  });
}

//----------------------------------------------------------------
const std::vector<i::DealInfo> CheapestByDay::get_result() const {
  return exec_result;
}
}  // namespace deals