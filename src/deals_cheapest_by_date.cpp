#include "deals_cheapest_by_date.hpp"

namespace deals {
//----------------------------------------------------------------
void CheapestByDay::pre_search() {
  checkInputParams();

  if (filter_exact_date) {
    if (departure_return_max_duration) {
      filter_result_limit = departure_return_max_duration;
    }
    return;
  }

  if (filter_all_combinations) {
    filter_result_limit = departure_date_values.duration * return_date_values.duration;
    return;
  }

  if (departure_date_values.duration <= 1) {
    group_by_return_date = true;
    if (return_date_values.duration) {
      filter_result_limit = return_date_values.duration;
    }
    return;
  }

  if (departure_date_values.duration > 1) {
    filter_result_limit = departure_date_values.duration;
  } else {
    filter_result_limit = return_date_values.duration;
  }
}

void CheapestByDay::checkInputParams() {
  if (!departure_date_values.duration && !return_date_values.duration) {
    std::cerr << "ERROR no departure or return dates interval" << std::endl;
    throw types::Error("Departure or return dates interval must be specified\n");
  }
  if ((filter_departure_date && departure_date_values.duration > 367) ||
      (filter_return_date && return_date_values.duration > 367)) {
    std::cerr << "ERROR departure or return dates interval > 366."
              << " Dep:" << std::to_string(departure_date_values.duration)
              << " Ret:" << std::to_string(return_date_values.duration) << std::endl;
    throw types::Error("Date interval to large. 365 days is maximum\n");
  }
  if (filter_all_combinations) {
    if (!departure_date_values.duration || !return_date_values.duration) {
      std::cerr << "ERROR all_combination: Departure and return dates interval must be specified"
                << std::endl;
      throw types::Error(
          "Departure and return dates must be specified, if all_combination flag is set\n");
    }
    if (departure_date_values.duration > 11 || return_date_values.duration > 11) {
      std::cerr << "ERROR all_combination: departure and return dates interval must be <= 10"
                << std::endl;
      throw types::Error(
          "Departure and return dates interval must be <= 10, if all_combination flag is set\n");
    }
  }
}

//---------------------------------------------------------
query::DateValue CheapestByDay::getDateToGroup(const i::DealInfo &deal) {
  if (filter_exact_date) {
    if (exact_date_value == deal.return_date) {
      return deal.departure_date;
    }

    // exact_date_value == deal.departure_date
    // set first bit to 1 for group return dates separately
    return deal.return_date | 0x80000000;
  }

  if (filter_all_combinations) {
    return (deal.departure_date << 18) ^ (deal.return_date);
  }

  if (group_by_return_date) {
    return deal.return_date;
  }

  return deal.departure_date;
}

//---------------------------------------------------------
void CheapestByDay::process_deal(const i::DealInfo &deal) {
  //
  const auto &date = getDateToGroup(deal);
  auto &dst_deal = grouped_by_date[date];

  if (dst_deal.price == 0 || deal.price <= dst_deal.price) {
    // ignore same route and dates with lower price, but older timestamp
    if (utils::equal(deal, dst_deal) && dst_deal.timestamp > deal.timestamp) {
      return;
    }
    dst_deal = deal;
    grouped_by_date_hist[date].push_back(deal);
    return;
  }

  // if  not cheaper but same destination & dates, replace with newer results
  if (utils::equal(deal, dst_deal) && dst_deal.timestamp < deal.timestamp) {
    dst_deal = deal;
    grouped_by_date_hist[date].push_back(deal);
    dst_deal.overriden = true;  // it is used in tests
  }
}

//----------------------------------------------------------------
void CheapestByDay::post_search() {
  for (const auto &deal : grouped_by_date_hist) {
    exec_result.push_back(utils::findCheapestAndLast(deal.second));
  }
  const auto exact_date = exact_date_value;

  if (filter_exact_date) {
    std::sort(exec_result.begin(), exec_result.end(),
              [&exact_date](const i::DealInfo &a, const i::DealInfo &b) {
                if (exact_date == a.return_date && exact_date == b.return_date) {
                  return a.departure_date < b.departure_date;
                }
                if (exact_date == a.departure_date && exact_date == b.departure_date) {
                  return a.return_date < b.return_date;
                }
                if (exact_date == a.return_date) {
                  return true;
                }
                return false;
              });
  } else if (group_by_return_date) {
    std::sort(
        exec_result.begin(), exec_result.end(),
        [](const i::DealInfo &a, const i::DealInfo &b) { return a.return_date < b.return_date; });
  } else {
    std::sort(exec_result.begin(), exec_result.end(),
              [](const i::DealInfo &a, const i::DealInfo &b) {
                if (a.departure_date == b.departure_date) {
                  return a.return_date < b.return_date;
                }
                return a.departure_date < b.departure_date;
              });
  }
}

//----------------------------------------------------------------
const std::vector<i::DealInfo> CheapestByDay::get_result() const {
  return exec_result;
}
}  // namespace deals