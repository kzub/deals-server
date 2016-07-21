#include <sys/mman.h>
#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <iostream>

#include "deals.hpp"
#include "timing.hpp"

namespace deals {
//      ***************************************************
//                   Deals search base class
//      ***************************************************

//-------------------------------------------------------
// DealsSearchQuery destructor
//-------------------------------------------------------
DealsSearchQuery::~DealsSearchQuery() {
  if (destination_values != nullptr) {
    delete destination_values;
  }
}

/* ----------------------------------------------------------
**  execute search process
** ----------------------------------------------------------*/
void DealsSearchQuery::execute() {
  // if there was bad parameters -> no processing required
  if (query_is_broken) {
    std::cout << "ERROR: query has inconsistent parameters" << std::endl;
    return;
  }

  // define destinations count we will look for
  // -------------------------------------------
  if (filter_destination) {
    destination_values_size = destination_values_vector.size();
  } else {
    destination_values_size = filter_limit;
  }

  // release mem if multiple execute() called per instance
  // --------------------------------------------
  if (destination_values != nullptr) {
    delete destination_values;
  }
  destination_values = new uint32_t[destination_values_size];

  // fill destination array
  // -------------------------------------------
  if (filter_destination) {
    uint16_t counter = 0;
    for (auto &dst : destination_values_vector) {
      destination_values[counter++] = dst;
    }
  }

  // run presearch in child class context
  pre_search();

  // table processor iterates table pages and call DealsSearchQuery::process_function()
  table.processRecords(*this);

  // run postsearch in child class context
  post_search();
};

//----------------------------------------------------------------
// process_function()
// function that will be called by TableProcessor
// for iterating over all not expired pages in table
//----------------------------------------------------------------
bool DealsSearchQuery::process_function(i::DealInfo *elements, uint32_t size) {
  if (size == 0) {
    std::cout << "ERROR DealsSearchQuery::process_function size == 0" << std::endl;
    return false;
  }

  if (filter_timestamp) {
    const i::DealInfo &lastdeal = elements[size - 1];

    if (timestamp_value > lastdeal.timestamp) {
      // std::cout << "whole page is expired" << std::endl;
      return true;
    }
  }

  for (uint32_t idx = 0; idx < size; ++idx) {
    const i::DealInfo &deal = elements[idx];

    // if origin is provided let's look only for this origin
    // --------------------------------
    if (filter_origin && origin_value != deal.origin) {
      // std::cout << "filter_origin" << std::endl;
      continue;
    }

    if (filter_timestamp && timestamp_value > deal.timestamp) {
      // std::cout << "filter_timestamp" << std::endl;
      continue;
    }

    // if destanations are provided let's look only for this destinations
    // --------------------------------
    if (filter_destination) {
      bool condition_matched = false;
      for (uint16_t dst_idx = 0; dst_idx < destination_values_size; ++dst_idx) {
        if (destination_values[dst_idx] == deal.destination) {
          condition_matched = true;
          break;
        }
      }

      if (!condition_matched) {
        // std::cout << "filter_destination !condition_matched" << std::endl;
        continue;
      }
    }

    // if departure date interval provided let's look it matches
    // --------------------------------
    if (filter_departure_date) {
      if (deal.departure_date < departure_date_values.from ||
          deal.departure_date > departure_date_values.to) {
        // std::cout << "filter_departure_date" << std::endl;
        continue;
      }
    }

    // if return date interval provided let's look it matches
    // --------------------------------
    if (filter_return_date) {
      if (deal.return_date < return_date_values.from || deal.return_date > return_date_values.to) {
        // std::cout << "filter_return_date" << return_date_values.from << " " <<
        // return_date_values.to
        //           << std::endl;
        continue;
      }
    }

    // filter desired departure week days
    //------------------------------------
    if (filter_stay_days && deal.return_date) {
      if (deal.stay_days < stay_days_values.from || deal.stay_days > stay_days_values.to) {
        // std::cout << "filter_stay_days" << std::endl;
        continue;
      }
    }

    // direct & flight with stops
    // --------------------------------
    if (filter_flight_by_stops) {
      if (direct_flights_flag != deal.flags.direct) {
        // std::cout << "filter_flight_by_stops" << std::endl;
        continue;
      }
    }

    // roundtrips
    // --------------------------------
    if (filter_flight_by_roundtrip) {
      if (roundtrip_flight_flag == true) {
        if (deal.return_date == 0) {
          // std::cout << "filter_flight_by_roundtrip (this is ow)" << roundtrip_flight_flag << " "
          // << deal.return_date << std::endl;
          continue;
        }
      } else {
        if (deal.return_date != 0) {
          // std::cout << "filter_flight_by_roundtrip (this is rt)" << roundtrip_flight_flag << " "
          // << deal.return_date << std::endl;
          continue;
        }
      }
    }

    // filter deal price
    // ----------------------------------------
    if (filter_price) {
      if (deal.price < price_from_value) {
        // std::cout << "filter_price_from:" << deal.price << std::endl;
        continue;
      }
      if (deal.price > price_to_value) {
        // std::cout << "filter_price_to:" << deal.price << std::endl;
        continue;
      }
    }

    // filter desired departure week days
    //------------------------------------
    if (filter_departure_weekdays) {
      if (((1 << deal.flags.departure_day_of_week) & departure_weekdays_bitmask) == 0) {
        // std::cout << "filter_departure_weekdays" << std::endl;
        continue;
      }
    }

    // filter desired return week days
    //------------------------------------
    if (filter_return_weekdays && deal.return_date) {
      if (((1 << deal.flags.return_day_of_week) & return_weekdays_bitmask) == 0) {
        // std::cout << "filter_return_weekdays" << std::endl;
        continue;
      }
    }

    // **********************************************************************
    // Deal match for selected filters -> process it @ child class
    // **********************************************************************
    if (!process_deal(deal)) {
      // stop here if function return false
      return false;
    }
  }  // end of page elements iteration loop

  // go on with next table page
  // true - means continue to iterate
  return true;
}

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
bool DealsDatabase::addDeal(std::string origin, std::string destination, std::string departure_date,
                            std::string return_date, bool direct_flight, uint32_t price,
                            std::string data) {
  if (origin.length() != 3) {
    std::cout << "wrong origin length:" << origin << std::endl;
    return false;
  }

  if (destination.length() != 3) {
    std::cout << "wrong destination length:" << destination << std::endl;
    return false;
  }

  uint32_t departure_date_int = query::date_to_int(departure_date);
  if (departure_date_int == 0) {
    std::cout << "wrong departure date:" << departure_date << std::endl;
    return false;
  }

  uint32_t return_date_int = query::date_to_int(return_date);

  // Firstly add data and get data position at db
  deals::i::DealData *data_pointer = (deals::i::DealData *)data.c_str();
  uint32_t data_size = data.length();

  shared_mem::ElementPointer<i::DealData> result = db_data->addRecord(data_pointer, data_size);
  if (result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cout << "ERROR DealsDatabase::addDeal 1:" << (int)result.error << std::endl;
    return false;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;

  i::DealInfo info;
  info.timestamp = timing::getTimestampSec();
  info.origin = query::origin_to_code(origin);
  info.destination = query::origin_to_code(destination);
  info.departure_date = departure_date_int;
  info.return_date = return_date_int;
  info.flags.overriden = false;
  info.flags.direct = direct_flight;
  info.flags.departure_day_of_week = ::utils::day_of_week_from_date(departure_date);
  info.flags.return_day_of_week = ::utils::day_of_week_from_date(return_date);
  info.price = price;
  strncpy(info.page_name, result.page_name.c_str(), MEMPAGE_NAME_MAX_LEN);
  info.index = result.index;
  info.size = result.size;

  if (return_date_int) {
    uint32_t days = ::utils::days_between_dates(departure_date, return_date);
    info.stay_days = days > UINT8_MAX ? UINT8_MAX : days;
  } else {
    info.stay_days = UINT8_MAX;
  }

  // Secondly add deal to index, include data position information
  shared_mem::ElementPointer<i::DealInfo> di_result = db_index->addRecord(&info);
  if (di_result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cout << "ERROR DealsDatabase::addDeal 2:" << (int)di_result.error << std::endl;
    return false;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;
  // std::cout << "addDeal OK" << std::endl;
  return true;
}

/*---------------------------------------------------------
* DealsDatabase  fill_deals_with_data
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::fill_deals_with_data(std::vector<i::DealInfo> i_deals) {
  // internal <i::DealInfo> contain shared memory page name and
  // information offsets. It's not useful anywhere outside
  // so lets transform internal format to external <DealInfo>
  std::vector<DealInfo> result;

  for (auto &deal : i_deals) {
    // std::cout << "DEAL> page:(" << deal->page_name << " " << deal->index << "
    // " << deal->size << ")" << std::endl;
    shared_mem::ElementPointer<i::DealData> deal_data(*db_data, deal.page_name, deal.index,
                                                      deal.size);
    i::DealData *data_pointer = deal_data.get_data();
    std::string data((char *)data_pointer, deal.size);

    result.push_back((DealInfo){
        deal.timestamp, query::code_to_origin(deal.origin), query::code_to_origin(deal.destination),
        query::int_to_date(deal.departure_date), query::int_to_date(deal.return_date),
        deal.stay_days, deal.flags, deal.price, data});
  }

  return result;
}

//      ***************************************************
//                   CHEAPEST BY PERIOD
//      ***************************************************

/*---------------------------------------------------------
* DealsDatabase  searchForCheapestEver
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapestEver(
    std::string origin, std::string destinations, std::string departure_date_from,
    std::string departure_date_to, std::string departure_days_of_week, std::string return_date_from,
    std::string return_date_to, std::string return_days_of_week, uint16_t stay_from,
    uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from, uint32_t price_to,
    uint16_t limit, uint32_t max_lifetime_sec, ::utils::Threelean roundtrip_flights)

{
  DealsCheapestByPeriod query(*db_index);

  query.apply_filters(origin, destinations, departure_date_from, departure_date_to,
                      departure_days_of_week, return_date_from, return_date_to, return_days_of_week,
                      stay_from, stay_to, direct_flights, price_from, price_to, limit,
                      max_lifetime_sec, roundtrip_flights);

  query.execute();

  if (limit > 0 && limit < query.exec_result.size()) {
    std::sort(query.exec_result.begin(), query.exec_result.end(),
              [](const i::DealInfo &a, const i::DealInfo &b) { return a.price < b.price; });
    query.exec_result.resize(limit);
  }

  std::vector<DealInfo> result = fill_deals_with_data(query.exec_result);

  return result;
}

//----------------------------------------------------------------
// DealsCheapestByPeriod PRESEARCH
//----------------------------------------------------------------
void DealsCheapestByPeriod::pre_search() {
  // init values
  if (result_deals != nullptr) {
    delete result_deals;
  }
  result_deals = new i::DealInfo[destination_values_size];
  max_price_deal = 0;
  deals_slots_used = 0;
  result_deals[0].price = 0;
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
bool DealsCheapestByPeriod::process_deal(const i::DealInfo &deal) {
  // ----------------------------------
  // try to find deal by destination in result array
  // ----------------------------------
  for (uint16_t fidx = 0; fidx < deals_slots_used; ++fidx) {
    i::DealInfo &result_deal = result_deals[fidx];

    if (deal.destination != result_deal.destination) {
      continue;
    }

    // we already have this destination, let's check for price
    if (deal.price < result_deal.price) {
      bool overriden = result_deal.flags.overriden;
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = overriden;
      // evaluate it here but not every compare itearation
      max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    }
    // if  not cheaper but same dates, replace with
    // newer results
    else if (deal.departure_date == result_deal.departure_date &&
             deal.return_date == result_deal.return_date) {
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = true;
      // evaluate it here but not every compare itearation
      max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    }

    // we was found destination, so goto the next deal element
    return true;
  }

  // ----------------------------------
  // no destinations are found in result
  // if there are unUsed slots -> fill them
  if (deals_slots_used < destination_values_size) {
    // deals::utils::print(deal);
    deals::utils::copy(result_deals[deals_slots_used], deal);
    deals_slots_used++;
    // evaluate it here but not every compare itearation
    max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    return true;
  }

  if (filter_destination) {
    // not possible situation
    std::cout << "filter destination exist but not found and all slots are used. but how?"
              << std::endl;
    throw "DealsCheapestByPeriod::process_deal.deals_slots_used.full";
  }

  // ----------------------------------
  // if all slots are used, but current deals
  // is cheaper than deals in result -> let replace most expensive with new
  // one (new destiantion is not in result_deals)
  if (deal.price < result_deals[max_price_deal].price) {
    deals::utils::copy(result_deals[max_price_deal], deal);
    // evaluate it here but not every compare itearation
    max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
    return true;
  }

  // result_deals are full by "limit"
  // and current deal price is more than any price in result vector
  // (result_deals)
  // so just skip it
  return true;
}

//----------------------------------------------------------------
// DealsCheapestByPeriod POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestByPeriod::post_search() {
  // process results
  for (int i = 0; i < deals_slots_used; ++i) {
    exec_result.push_back(result_deals[i]);
  }
}

//----------------------------------------------------------------
// DealsCheapestByPeriod destructor
//----------------------------------------------------------------
DealsCheapestByPeriod::~DealsCheapestByPeriod() {
  if (result_deals != nullptr) {
    delete result_deals;
  }
}

//      ***************************************************
//                   CHEAPEST DAY BY DAY
//      ***************************************************

/*---------------------------------------------------------
* DealsDatabase  searchForCheapestDayByDay
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapestDayByDay(
    std::string origin, std::string destinations, std::string departure_date_from,
    std::string departure_date_to, std::string departure_days_of_week, std::string return_date_from,
    std::string return_date_to, std::string return_days_of_week, uint16_t stay_from,
    uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from, uint32_t price_to,
    uint16_t limit, uint32_t max_lifetime_sec, ::utils::Threelean roundtrip_flights) {
  //
  DealsCheapestDayByDay query(*db_index);

  query.apply_filters(origin, destinations, departure_date_from, departure_date_to,
                      departure_days_of_week, return_date_from, return_date_to, return_days_of_week,
                      stay_from, stay_to, direct_flights, price_from, price_to, limit,
                      max_lifetime_sec, roundtrip_flights);

  query.execute();

  std::vector<DealInfo> result = fill_deals_with_data(query.exec_result);

  std::sort(result.begin(), result.end(), [](const DealInfo &a, const DealInfo &b) {
    return a.departure_date < b.departure_date;
  });

  // for (auto& deal : result) {
  //   deals::utils::print(deal);
  // }

  return result;
}

//----------------------------------------------------------------
// DealsCheapestDayByDay PRESEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::pre_search() {
  // init values
  if (!filter_departure_date || !departure_date_values.duration) {
    std::cout << "ERROR no departure_date range" << std::endl;
    throw "zero interval. departure date interval must be specified";
  }

  deals_slots_available = destination_values_size * departure_date_values.duration;

  if (deals_slots_available > 1000) {
    std::cout << "ERROR deals_slots_available > 1000" << std::endl;
    throw "too much deals count requested, reduce";
  }
  // std::cout << "destination_values_size:" << destination_values_size << std::endl;
  // std::cout << "departure_date_values.duration:" << departure_date_values.duration << std::endl;
  // std::cout << "deals_slots_available:" << deals_slots_available << std::endl;

  if (result_deals != nullptr) {
    delete result_deals;
  }
  result_deals = new i::DealInfo[deals_slots_available];
  deals_slots_used = 0;
}

//---------------------------------------------------------
// Process selected deal and decide go next or stop here
//---------------------------------------------------------
bool DealsCheapestDayByDay::process_deal(const i::DealInfo &deal) {
  // ----------------------------------
  // try to find deal by date and destination in result array
  // ----------------------------------
  uint16_t deals_with_current_date_count = 0;
  i::DealInfo *deals_with_current_date[destination_values_size];

  for (uint16_t fidx = 0; fidx < deals_slots_used; ++fidx) {
    i::DealInfo &result_deal = result_deals[fidx];

    if (deal.departure_date != result_deal.departure_date) {
      continue;
    }

    if (deals_with_current_date_count < destination_values_size) {
      // save pointer to deals with departure_date equal to processed one
      deals_with_current_date[deals_with_current_date_count++] = &result_deal;
    }

    if (deal.destination != result_deal.destination) {
      // below we check only date+destination already have been in top
      // so if it's new -> skip it
      continue;
    }

    // we already have this destination, let's check for price
    if (deal.price < result_deal.price) {
      bool overriden = result_deal.flags.overriden;
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = overriden;
    }
    // if price not cheaper but same dates, replace with
    // newer results
    else if (deal.departure_date == result_deal.departure_date &&
             deal.return_date == result_deal.return_date) {
      deals::utils::copy(result_deal, deal);
      result_deal.flags.overriden = true;
    }

    // we was found destination, so goto the next deal element
    return true;
  }

  if (deals_with_current_date_count >= destination_values_size) {
    // limit reached for this date. need to replace something
    uint16_t idx_max = deals::utils::get_max_price_in_pointers_array(deals_with_current_date,
                                                                     deals_with_current_date_count);
    if (deal.price < deals_with_current_date[idx_max]->price) {
      deals::utils::copy(*deals_with_current_date[idx_max], deal);
    }
    return true;
  }

  // ----------------------------------
  // no destinations are found in result
  // if there are unUsed slots -> fill them
  if (deals_slots_used < deals_slots_available) {
    // deals::utils::print(deal);
    deals::utils::copy(result_deals[deals_slots_used], deal);
    deals_slots_used++;
    return true;
  }

  std::cout << "\ndeals_slots_used:" << deals_slots_used
            << " deals_slots_available:" << deals_slots_available
            << " deals_with_current_date_count:" << deals_with_current_date_count
            << " destination_values_size:" << destination_values_size
            << " departure_date_values.duration:" << departure_date_values.duration
            << " ERROR very strange place. there will be exacly days*destinations deals,"
            << " no more. but it seems we found something extra?" << std::endl;

  return true;
}

//----------------------------------------------------------------
// DealsCheapestDayByDay POSTSEARCH
//----------------------------------------------------------------
void DealsCheapestDayByDay::post_search() {
  // process results
  for (int i = 0; i < deals_slots_used; ++i) {
    if (result_deals[i].price > 0) {
      exec_result.push_back(result_deals[i]);
    }
  }
}

//----------------------------------------------------------------
// DealsCheapestDayByDay destructor
//----------------------------------------------------------------
DealsCheapestDayByDay::~DealsCheapestDayByDay() {
  if (result_deals != nullptr) {
    delete result_deals;
  }
}

//      ***************************************************
//                   UTILS
//      ***************************************************

namespace utils {

void copy(i::DealInfo &dst, const i::DealInfo &src) {
  memcpy(&dst, &src, sizeof(i::DealInfo));
}

uint16_t get_max_price_in_array(i::DealInfo *&dst, uint16_t size) {
  // assert(size > 0);
  uint16_t max = 0;
  uint16_t pos = 0;

  for (uint16_t i = 0; i < size; ++i) {
    if (max < dst[i].price) {
      max = dst[i].price;
      pos = i;
    }
  }

  if (max == 0) {
    throw "get_max_price_in_array.max_not_found";
  }

  return pos;
}

uint16_t get_max_price_in_pointers_array(i::DealInfo *dst[], uint16_t size) {
  uint16_t max = 0;
  uint16_t pos = 0;

  for (uint16_t i = 0; i < size; ++i) {
    if (max < dst[i]->price) {
      max = dst[i]->price;
      pos = i;
    }
  }

  if (max == 0) {
    throw "get_max_price_in_pointers_array.max_not_found";
  }

  return pos;
}

void print(const i::DealInfo &deal) {
  std::cout << "i::DEAL: (" << query::int_to_date(deal.departure_date) << ")"
            << query::code_to_origin(deal.origin) << "-" << query::code_to_origin(deal.destination)
            << "(" << query::int_to_date(deal.return_date) << ") : " << deal.price << std::endl;
}
void print(const DealInfo &deal) {
  std::cout << "DEAL: (" << deal.departure_date << ")" << deal.origin << "-" << deal.destination
            << "(" << deal.return_date << ") : " << deal.price << std::endl;
}

std::string sprint(const DealInfo &deal) {
  return "(" + deal.departure_date + ")" + deal.origin + "-" + deal.destination + "(" +
         deal.return_date + ") : " + std::to_string(deal.price) + "|" + deal.data + "\n";
}

}  // utils namespace

std::string getRandomOrigin() {
  static const std::string origins[] = {"MOW", "MAD", "BER", "LON", "PAR",
                                        "LAX", "LED", "FRA", "BAR"};
  uint16_t place = rand() % (sizeof(origins) / sizeof(origins[0]));
  return origins[place];
}

uint32_t getRandomPrice(uint32_t minPrice) {
  uint32_t price = rand() & 0x0000FFFF;
  price += minPrice;

  if (price < minPrice) {
    std::cout << "ALARM!! " << minPrice << " " << price << std::endl;
  }
  return price;
}

std::string getRandomDate(uint32_t year = 2016) {
  uint32_t month = (rand() & 0x00000003) + (rand() & 0x00000003) + (rand() & 0x00000003) + 1;
  uint32_t day = (rand() & 0x00000007) + (rand() & 0x00000007) + (rand() & 0x00000007) + 1;

  return query::int_to_date(year * 10000 + month * 100 + day);
}

void convertertionsTest() {
  std::cout << "Origin encoder/decoder" << std::endl;
  std::string origins[10] = {"MOW", "MAD", "BER", "PAR", "LON", "FRA", "VKO", "JFK", "LAX", "MEX"};

  for (int i = 0; i < 10; ++i) {
    uint32_t code = query::origin_to_code(origins[i]);
    std::string decode = query::code_to_origin(code);
    assert(origins[i] == decode);
  }

  std::cout << "Locale encoder/decoder" << std::endl;
  std::string locales[] = {"ru", "de", "uk", "ua", "us"};

  for (int i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i) {
    uint32_t code = query::locale_to_code(locales[i]);
    std::string decode = query::code_to_locale(code);
    assert(locales[i] == decode);
  }

  std::cout << "Date encoder/decoder\n";
  uint32_t code = query::date_to_int("2017-01-01");
  std::string date = query::int_to_date(code);

  assert(code == 20170101);
  assert(date == "2017-01-01");
}

#define TEST_ELEMENTS_COUNT 50000
void unit_test() {
  assert(::utils::days_between_dates("2015-01-01", "2015-01-01") == 0);
  assert(::utils::days_between_dates("2015-01-01", "2016-01-01") == 365);
  assert(::utils::days_between_dates("2015-02-28", "2015-03-01") == 1);

  assert(::utils::day_of_week_str_from_date("2016-06-25") == "sat");
  assert(::utils::day_of_week_str_from_date("2016-04-13") == "wed");
  assert(::utils::day_of_week_from_str("sat") == 5);
  assert(::utils::day_of_week_from_str("mon") == 0);
  assert(::utils::day_of_week_from_str("sun") == 6);
  assert(::utils::day_of_week_from_str("eff") == 7);
  std::cout << "Date functions... OK" << std::endl;

  convertertionsTest();
  std::cout << "City conv functions... OK" << std::endl;

  DealsDatabase db;
  db.truncate();

  std::string dumb = "1, 2, 3, 4, 5, 6, 7, 8";
  std::string check = "7, 7, 7";

  timing::Timer timer("SimpleSearch");

  srand(timing::getTimestampSec());

  // add some data, that will be outdated
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(1000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(2000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(3000), dumb);
  }

  // go to the feature (+1000 seconds)
  timing::TimeLord time;
  time += 1000;

  // add data we will expect
  db.addDeal("MOW", "MAD", "2016-05-01", "2016-05-21", true, 5000, check);
  db.addDeal("MOW", "BER", "2016-06-01", "2016-06-11", true, 6000, check);
  db.addDeal("MOW", "PAR", "2016-07-01", "2016-07-15", true, 7000, check);

  time += 5;

  // add some good
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), "MAD", getRandomDate(2015), getRandomDate(2015), true,
               getRandomPrice(5100), dumb);
    db.addDeal(getRandomOrigin(), "BER", getRandomDate(), getRandomDate(), true,
               getRandomPrice(6200), dumb);
    db.addDeal(getRandomOrigin(), "PAR", getRandomDate(), getRandomDate(), true,
               getRandomPrice(7200), dumb);

    // MAD will be 2016 here: and > 8000 price
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), true,
               getRandomPrice(8000), dumb);
  }

  timer.tick("before test1");
  // 1st test ----------------------------
  // *********************************************************
  std::vector<DealInfo> result = db.searchForCheapestEver(
      "MOW", "AAA,PAR,BER,MAD", "", "", "", "", "", "", 0, 0, ::utils::Threelean::Undefined, 0, 0,
      0, 10, ::utils::Threelean::Undefined);
  timer.tick("test1");

  for (auto &deal : result) {
    deals::utils::print(deal);
  }

  assert(result.size() == 3);
  int city_count[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); ++i) {
    if (result[i].destination == "MAD") {
      city_count[0]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 5000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 5000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-05-01");
      assert(result[i].return_date == "2016-05-21");
    } else if (result[i].destination == "BER") {
      city_count[1]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 6000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 6000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-06-01");
      assert(result[i].return_date == "2016-06-11");
    } else if (result[i].destination == "PAR") {
      city_count[2]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 7000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 7000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-07-01");
      assert(result[i].return_date == "2016-07-15");
    }
  }

  timer.tick("before test2");
  // 2nd test -------------------------------
  // *********************************************************
  result = db.searchForCheapestEver(
      "MOW", "AAA,PAR,BER,MAD", "2016-06-01", "2016-06-23", "", "2016-06-10", "2016-06-22", "", 0,
      0, ::utils::Threelean::Undefined, 0, 0, 0, 10, ::utils::Threelean::Undefined);

  timer.tick("test2");

  for (auto &deal : result) {
    deals::utils::print(deal);
  }

  assert(result.size() <= 3);
  int city_count2[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); ++i) {
    assert(query::date_to_int(result[i].departure_date) >= query::date_to_int("2016-06-01"));
    assert(query::date_to_int(result[i].departure_date) <= query::date_to_int("2016-06-23"));
    assert(query::date_to_int(result[i].return_date) >= query::date_to_int("2016-06-10"));
    assert(query::date_to_int(result[i].return_date) <= query::date_to_int("2016-06-22"));

    if (result[i].destination == "MAD") {
      city_count2[0]++;
      // madrid in this dates only over 8000
      assert(result[i].price >= 8000);
      assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");

    } else if (result[i].destination == "BER") {
      city_count2[1]++;
      if (result[i].flags.overriden) {
        assert(result[i].price > 6000);
        assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
      } else {
        assert(result[i].price == 6000);
        assert(result[i].data == "7, 7, 7");
      }

      assert(result[i].departure_date == "2016-06-01");
      assert(result[i].return_date == "2016-06-11");

    } else if (result[i].destination == "PAR") {
      city_count2[2]++;
      // Paris in this dates only over 7200
      assert(result[i].price >= 7200);
      assert(result[i].data == "1, 2, 3, 4, 5, 6, 7, 8");
    }
  }

  assert(city_count2[0] <= 1);
  assert(city_count2[1] == 1);
  assert(city_count2[2] <= 1);

  // std::string origin, std::string destinations, std::string departure_date_from,
  //       std::string departure_date_to, std::string departure_days_of_week,
  //       std::string return_date_from, std::string return_date_to, std::string
  //       return_days_of_week,
  //       uint16_t stay_from, uint16_t stay_to, bool direct_flights, bool stops_flights, uint16_t
  //       limit,
  //       uint32_t max_lifetime_sec);

  //--------------
  // 3rd test -------------------------------
  // *********************************************************
  timer.tick("before test3");
  result = db.searchForCheapestEver("MOW", "", "", "", "fri,sat,sun", "", "", "sat,sun,mon", 4, 18,
                                    ::utils::Threelean::False, 9100, 19200, 0, 2000,
                                    ::utils::Threelean::Undefined);
  timer.tick("test3");
  std::cout << "search 3 result size:" << result.size() << std::endl;

  for (auto &deal : result) {
    deals::utils::print(deal);
  }

  for (int i = 0; i < result.size(); ++i) {
    assert(result[i].price >= 9100);
    assert(result[i].price <= 19200);
    assert(result[i].stay_days >= 4 && result[i].stay_days <= 18);
    assert(result[i].flags.direct == false);
    std::string dw = ::utils::day_of_week_str_from_code(result[i].flags.departure_day_of_week);
    std::string rw = ::utils::day_of_week_str_from_code(result[i].flags.return_day_of_week);
    assert(dw == "fri" || dw == "sat" || dw == "sun");
    assert(rw == "sat" || rw == "sun" || rw == "mon");
  }

  std::cout << "OK" << std::endl;
}

}  // deals namespace

int mainasd() {
  deals::unit_test();

  return 0;
}
