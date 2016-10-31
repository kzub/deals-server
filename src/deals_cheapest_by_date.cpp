#include "deals_cheapest_by_date.hpp"

namespace deals {
//----------------------------------------------------------------
// CheapestByDay PRESEARCH
//----------------------------------------------------------------
void CheapestByDay::pre_search() {
  if (!filter_destination) {
    std::cerr << "ERROR no destinations specified" << std::endl;
    throw types::Error("destinations must be specified\n");
  }

  if (!filter_departure_date || !departure_date_values.duration) {
    std::cerr << "ERROR no departure_date range" << std::endl;
    throw types::Error("departure dates interval must be specified\n");
  }

  // 3 city * 365 days - is a limit
  if (result_destinations_count * departure_date_values.duration > 1098) {
    std::cerr << "ERROR result_destinations_count * departure_date_values.duration > 1098"
              << std::endl;
    throw types::Error("too much deals count requested, reduce destinations or dates range\n");
  }
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
void CheapestByDay::process_deal(const i::DealInfo &deal) {
  auto &dst_dates = grouped_destinations_and_dates[deal.destination];
  auto &dst_deal = dst_dates[deal.departure_date];

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
// CheapestByDay POSTSEARCH
//----------------------------------------------------------------
void CheapestByDay::post_search() {
  for (const auto &dates : grouped_destinations_and_dates) {
    for (const auto &deal : dates.second) {
      exec_result.push_back(deal.second);
    }
  }

  // sort by departure_date ASC
  std::sort(exec_result.begin(), exec_result.end(), [](const i::DealInfo &a, const i::DealInfo &b) {
    return a.departure_date < b.departure_date;
  });
}

//----------------------------------------------------------------
// CheapestByDay get_result
//----------------------------------------------------------------
std::vector<i::DealInfo> CheapestByDay::get_result() {
  return exec_result;
}
}  // namespace deals