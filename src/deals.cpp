#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <iostream>

#include "deals.hpp"
#include "timing.hpp"

namespace deals {
/* ----------------------------------------------------------
**  DealsSearchQuery execute()    execute search process
** ----------------------------------------------------------*/
void DealsSearchQuery::execute() {
  if (filter_destination) {
    result_destinations_count = destination_values_set.size();
  } else {
    result_destinations_count = filter_result_limit;
  }

  current_time = timing::getTimestampSec();
  // run presearch in derived class
  pre_search();

  // table processor iterates table pages and call DealsSearchQuery::process_element()
  table.processRecords(*this);

  // run postsearch in derived class
  post_search();
};

//----------------------------------------------------------------
// DealsSearchQuery process_element()
// function that will be called by TableProcessor
// for iterating over all not expired pages in table
//----------------------------------------------------------------
void DealsSearchQuery::process_element(const i::DealInfo &deal) {
  // check if not expired
  if (deal.timestamp + DEALS_EXPIRES < current_time) {
    return;
  }

  if (filter_origin && origin_value != deal.origin) {
    return;
  }

  if (filter_timestamp && timestamp_value > deal.timestamp) {
    return;
  }

  if (filter_flight_by_roundtrip) {
    if (roundtrip_flight_flag == true) {
      if (deal.return_date == 0) {
        return;
      }
    } else {
      if (deal.return_date != 0) {
        return;
      }
    }
  }

  if (filter_destination) {
    if (destination_values_set.find(deal.destination) == destination_values_set.end()) {
      return;
    }
  }

  if (filter_destination_country) {
    if (destination_country_set.find(deal.destination_country) == destination_country_set.end()) {
      return;
    }
  }

  if (filter_departure_date && (deal.departure_date < departure_date_values.from ||
                                deal.departure_date > departure_date_values.to)) {
    return;
  }

  if (filter_return_date &&
      (deal.return_date < return_date_values.from || deal.return_date > return_date_values.to)) {
    return;
  }

  if (filter_stay_days &&
      (deal.stay_days < stay_days_values.from || deal.stay_days > stay_days_values.to)) {
    return;
  }

  if (filter_flight_by_stops && (direct_flights_flag != deal.direct)) {
    return;
  }

  if (filter_departure_weekdays && (deal.departure_day_of_week & departure_weekdays_bitmask) == 0) {
    return;
  }

  if (filter_return_weekdays && (deal.return_day_of_week & return_weekdays_bitmask) == 0) {
    return;
  }

  // Deal matched all selected filters -> process it @ derivered class
  process_deal(deal);
}

//------------------------------------------------------------------------------
//      ***************************************************
//                   Deals Database class
//      ***************************************************
DealsDatabase::DealsDatabase() {
  // 1k pages x 10k elements per page, 10m records total, expire 60 seconds
  db_index = new shared_mem::Table<i::DealInfo>(DEALINFO_TABLENAME, DEALINFO_PAGES /* pages */,
                                                DEALINFO_ELEMENTS /* elements in page */,
                                                DEALS_EXPIRES /* page expire */);

  // 10k pages x 3.2m per page = 32g bytes, expire 60 seconds
  db_data = new shared_mem::Table<i::DealData>(DEALDATA_TABLENAME, DEALDATA_PAGES /* pages */,
                                               DEALDATA_ELEMENTS /* elements in page */,
                                               DEALS_EXPIRES /* page expire */);
}

//---------------------------------------------------------
// DealsDatabase destructor
//---------------------------------------------------------
DealsDatabase::~DealsDatabase() {
  delete db_data;
  delete db_index;
}

//---------------------------------------------------------
//  DealsDatabase  truncate
//---------------------------------------------------------
void DealsDatabase::truncate() {
  db_data->cleanup();
  db_index->cleanup();
}

//---------------------------------------------------------
//  DealsDatabase  addDeal
//---------------------------------------------------------
void DealsDatabase::addDeal(const types::Required<types::IATACode> &origin,
                            const types::Required<types::IATACode> &destination,
                            const types::Required<types::CountryCode> &destination_country,
                            const types::Required<types::Date> &departure_date,
                            const types::Optional<types::Date> &return_date,
                            const types::Required<types::Boolean> &direct_flight,
                            const types::Required<types::Number> &price, const std::string &data) {
  // convert string to i::DealData (byte array)
  const auto data_pointer = (deals::i::DealData *)data.c_str();
  const uint32_t data_size = data.length();

  // 1) Add data and get data offset in db page --------------------------
  auto result = db_data->addRecord(data_pointer, data_size);
  if (result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cerr << "ERROR addRecord->DealData:" << (int)result.error << std::endl;
    throw types::Error("Internal Error: addRecord->DealData", types::ErrorCode::InternalError);
  }

  const types::Weekdays departure_day_of_week(departure_date);
  const types::Weekdays return_day_of_week(return_date);

  i::DealInfo info;
  info.timestamp = timing::getTimestampSec();
  info.origin = origin.get_code();
  info.destination = destination.get_code();
  info.destination_country = destination_country.get_code();
  info.departure_date = departure_date.get_code();
  info.overriden = false;
  info.direct = direct_flight.isTrue();
  info.departure_day_of_week = departure_day_of_week.get_bitmask();
  info.return_day_of_week = return_day_of_week.get_bitmask();
  info.price = price.get_value();

  strncpy(info.page_name, result.page_name.c_str(), MEMPAGE_NAME_MAX_LEN);
  info.index = result.index;
  info.size = result.size;

  if (return_date.isUndefined()) {
    info.stay_days = UINT8_MAX;
    info.return_date = 0;
  } else {
    info.return_date = return_date.get_code();
    uint32_t days = return_date.days_after(departure_date);
    info.stay_days = days > UINT8_MAX ? UINT8_MAX : days;
  }

  // 2) Add deal to index, with data position information --------------------------
  auto di_result = db_index->addRecord(&info);
  if (di_result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cerr << "ERROR addRecord->DealInfo:" << (int)di_result.error << std::endl;
    throw types::Error("Internal Error: addRecord->DealInfo", types::ErrorCode::InternalError);
  }
}

/*---------------------------------------------------------
* DealsDatabase  fill_deals_with_data
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::fill_deals_with_data(std::vector<i::DealInfo> i_deals) {
  std::vector<DealInfo> result;

  for (const auto &deal : i_deals) {
    auto deal_data = i::sharedDealData{*db_data, deal.page_name, deal.index, deal.size};
    auto data_pointer = (char *)deal_data.get_data();
    std::string data = {data_pointer, deal.size};

    if (0) {
      std::shared_ptr<DealInfoTest> testdata(new DealInfoTest{
          types::code_to_origin(deal.origin), types::code_to_origin(deal.destination),
          types::int_to_date(deal.departure_date), types::int_to_date(deal.return_date),
          deal.timestamp, deal.price, deal.stay_days, deal.departure_day_of_week,
          deal.return_day_of_week, deal.destination_country, deal.direct, deal.overriden});

      std::cout << testdata->origin << " " << testdata->destination << " "
                << testdata->departure_date << " " << testdata->return_date << std::endl;

      result.push_back({data, testdata});
    } else {
      result.push_back({data, nullptr});
    }
  }

  return result;
}

// ***************************************************
// CHEAPEST (simple std::unordered_map version)
// ***************************************************
/*---------------------------------------------------------
* DealsDatabase  searchForCheapest
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapest(
    const types::Required<types::IATACode> &origin,
    const types::Optional<types::IATACodes> &destinations,
    const types::Optional<types::CountryCodes> &destination_countries,
    const types::Optional<types::Date> &departure_date_from,
    const types::Optional<types::Date> &departure_date_to,
    const types::Optional<types::Weekdays> &departure_days_of_week,
    const types::Optional<types::Date> &return_date_from,
    const types::Optional<types::Date> &return_date_to,
    const types::Optional<types::Weekdays> &return_days_of_week,
    const types::Optional<types::Number> &stay_from,  // lf
    const types::Optional<types::Number> &stay_to,
    const types::Optional<types::Boolean> &direct_flights,
    const types::Optional<types::Number> &limit,
    const types::Optional<types::Number> &max_lifetime_sec,
    const types::Optional<types::Boolean> &roundtrip_flights) {
  //
  DealsCheapestByDatesSimple query(*db_index);  // <- table processed by search class
  query.origin(origin);
  query.destinations(destinations);
  query.destination_countries(destination_countries);
  query.departure_dates(departure_date_from, departure_date_to);
  query.return_dates(return_date_from, return_date_to);
  query.departure_weekdays(departure_days_of_week);
  query.return_weekdays(return_days_of_week);
  query.stay_days(stay_from, stay_to);
  query.direct_flights(direct_flights);
  query.roundtrip_flights(roundtrip_flights);
  query.result_limit(limit);
  query.max_lifetime_sec(max_lifetime_sec);

  query.execute();

  // load deals data from data pages (DealData shared memory pagers)
  return fill_deals_with_data(query.exec_result);
}

//----------------------------------------------------------------
// DealsCheapestByDatesSimple PRESEARCH
//----------------------------------------------------------------
void DealsCheapestByDatesSimple::pre_search() {
  grouped_max_price = 0;
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
void DealsCheapestByDatesSimple::process_deal(const i::DealInfo &deal) {
  if (grouped_destinations.size() > filter_result_limit) {
    if (grouped_max_price <= deal.price) {
      // deal price is far more expensive, skip grouping
      return;
    }
    grouped_max_price = deal.price;

  } else if (grouped_max_price < deal.price) {
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
// DealsCheapestByDatesSimple POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestByDatesSimple::post_search() {
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

//  ***************************************************
//         CHEAPEST DAY BY DAY (2nd version)
//  ***************************************************

/*---------------------------------------------------------
* DealsDatabase  searchForCheapestDayByDay
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapestDayByDay(
    const types::Required<types::IATACode> &origin,
    const types::Optional<types::IATACodes> &destinations,
    const types::Optional<types::CountryCodes> &destination_countries,
    const types::Optional<types::Date> &departure_date_from,
    const types::Optional<types::Date> &departure_date_to,
    const types::Optional<types::Weekdays> &departure_days_of_week,
    const types::Optional<types::Date> &return_date_from,
    const types::Optional<types::Date> &return_date_to,
    const types::Optional<types::Weekdays> &return_days_of_week,
    const types::Optional<types::Number> &stay_from,  // lf
    const types::Optional<types::Number> &stay_to,
    const types::Optional<types::Boolean> &direct_flights,
    const types::Optional<types::Number> &limit,
    const types::Optional<types::Number> &max_lifetime_sec,
    const types::Optional<types::Boolean> &roundtrip_flights) {
  //
  DealsCheapestDayByDay query(*db_index);
  query.origin(origin);
  query.destinations(destinations);
  query.destination_countries(destination_countries);
  query.departure_dates(departure_date_from, departure_date_to);
  query.return_dates(return_date_from, return_date_to);
  query.departure_weekdays(departure_days_of_week);
  query.return_weekdays(return_days_of_week);
  query.stay_days(stay_from, stay_to);
  query.direct_flights(direct_flights);
  query.roundtrip_flights(roundtrip_flights);
  query.result_limit(limit);
  query.max_lifetime_sec(max_lifetime_sec);

  query.execute();

  return fill_deals_with_data(query.exec_result);
}

//----------------------------------------------------------------
// DealsCheapestDayByDay PRESEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::pre_search() {
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
void DealsCheapestDayByDay::process_deal(const i::DealInfo &deal) {
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
// DealsCheapestDayByDay POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::post_search() {
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

//***********************************************************
//                   UTILS
//***********************************************************
namespace utils {
void print(const i::DealInfo &deal) {
  std::cout << "i::DEAL: (" << types::int_to_date(deal.departure_date) << ")"
            << types::code_to_origin(deal.origin) << "-" << types::code_to_origin(deal.destination)
            << "(" << types::int_to_date(deal.return_date) << ") : " << deal.price << " "
            << deal.page_name << ":" << deal.index << std::endl;
}
void print(const DealInfo &deal) {
  if (deal.test == nullptr) {
    std::cerr << "print() error: No test data. Rebuild code with testing enabled" << std::endl;
    return;
  }

  std::cout << "DEAL: (" << deal.test->departure_date << ")" << deal.test->origin << "-"
            << deal.test->destination << "(" << deal.test->return_date << ")"
            << (deal.test->overriden ? "w" : " ") << ": " << deal.test->price << std::endl;
}

std::string sprint(const DealInfo &deal) {
  if (deal.test == nullptr) {
    std::cerr << "print() error: No test data. Rebuild code with testing enabled" << std::endl;
    return "no test data\n";
  }

  return "(" + deal.test->departure_date + ")" + deal.test->origin + "-" + deal.test->destination +
         "(" + deal.test->return_date + ") : " + std::to_string(deal.test->price) + "|" +
         deal.data + "\n";
}
}  // utils namespace
}  // deals namespace
