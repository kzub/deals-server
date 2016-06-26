#include <sys/mman.h>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <iostream>

#include "deals.hpp"
#include "timing.hpp"

// Как найти популярное,
// hash таблица, ключ город, на поиске +1
// каждую секунду/минуту -1
// дальше сортировка по числу
namespace deals {
/* ----------------------------------------------------------
**  Check results
** ----------------------------------------------------------*/
std::vector<i::DealInfo> DealsSearchQuery::exec() {
  std::vector<i::DealInfo> exec_result;

  if (query_is_broken) {
    std::cout << "ERROR: query has inconsistent parameters" << std::endl;
    return exec_result;
  }

  // redefine filter limit if destinations are specified
  // -------------------------------------------
  if (filter_destination) {
    filter_limit = destination_values_vector.size();
  }

  // INIT LOCAL VARS instead "new" operator for search speed
  // we need this definition scope to let variable live till the end of
  // function (for search speed optimization)
  // -------------------------------------------
  uint32_t destination_storage[filter_limit];
  destination_values = destination_storage;

  i::DealInfo deals_storage[filter_limit];
  result_deals = deals_storage;

  // -------------------------------------------
  // INIT VALUES
  // -------------------------------------------
  max_price_deal = 0;
  deals_slots_used = 0;
  result_deals[0].price = 0;

  // fill destination array
  // -------------------------------------------
  if (filter_destination) {
    std::vector<uint32_t>::iterator dst;
    uint32_t counter = 0;
    for (dst = destination_values_vector.begin(); dst != destination_values_vector.end();
         ++dst, ++counter) {
      destination_values[counter] = *dst;
    }
  }

  // call this class pre/procesee/post functions with shared mem page pointers
  // -----------------------------------------
  table.process(this);

  //-------------------------------------------
  // process results
  for (int i = 0; i < deals_slots_used; ++i) {
    exec_result.push_back(deals_storage[i]);
  }

  return exec_result;
};

// before iteration
void DealsSearchQuery::pre_process_function() {
  // std::cout << "(PREPROCESS)" << std::endl;
}

/* function that will be called by TableProcessor
      *  for iterating over all not expired pages in table */
bool DealsSearchQuery::process_function(i::DealInfo *elements, uint32_t size) {
  for (uint32_t idx = 0; idx < size; idx++) {
    const i::DealInfo &deal = elements[idx];

    // ******************************************************************
    // FILTERING OUT AREA
    // ******************************************************************

    // skip old data
    // --------------------------------
    if (filter_timestamp && timestamp_value > deal.timestamp) {
      // std::cout << "filter_timestamp" << std::endl;
      continue;
    }

    // if origin is provided let's look only for this origin
    // --------------------------------
    if (filter_origin && origin_value != deal.origin) {
      // std::cout << "filter_origin" << std::endl;
      continue;
    }

    // if destanations are provided let's look only for this destinations
    // --------------------------------
    if (filter_destination) {
      bool condition_matched = false;
      for (uint16_t dst_idx = 0; dst_idx < filter_limit; dst_idx++) {
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
      // hide direct flights
      if (direct_flights_flag == false && deal.flags.direct == true) {
        // std::cout << "filter_flight_by_stops1" << std::endl;
        continue;
      }

      // hide stops flights
      if (stops_flights_flag == false && deal.flags.direct == false) {
        // std::cout << "filter_flight_by_stops2" << std::endl;
        continue;
      }
    }

    // **********************************************************************
    // SEARCHING FOR CHEAPEST DEAL AREA
    // **********************************************************************

    // ----------------------------------
    // try to find deal by destination in result array
    // ----------------------------------
    bool found_deal_by_destination = false;
    for (uint16_t fidx = 0; fidx < deals_slots_used; fidx++) {
      i::DealInfo &result_deal = result_deals[fidx];

      if (deal.destination == result_deal.destination) {
        // we already have this destination, let's check for price
        if (deal.price < result_deal.price) {
          bool overriden = false;
          if (result_deal.flags.overriden) {
            overriden = true;
          }
          deals::utils::copy(result_deal, deal);
          result_deal.flags.overriden = overriden;
          max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
        }
        // if price not cheaper but same dates, replace with
        // newer results
        else if (deal.departure_date == result_deal.departure_date &&
                 deal.return_date == result_deal.return_date) {
          deals::utils::copy(result_deal, deal);
          result_deal.flags.overriden = true;
          max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
        }

        found_deal_by_destination = true;
        break;
      }
    }

    // ----------------------------------
    // there was found destination, so goto the next deal element
    if (found_deal_by_destination) {
      continue;
    }

    // ----------------------------------
    // no destinations are found in result
    // if there are unUsed slots -> fill them
    if (deals_slots_used < filter_limit) {
      // deals::utils::print(deal);
      deals::utils::copy(result_deals[deals_slots_used], deal);
      deals_slots_used++;
      max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
      continue;
    }

    // ----------------------------------
    // if all slots are used, but current deals
    // is cheaper than deals in result -> let replace most expensive with new
    // one (new destiantion is not in result_deals)
    if (deal.price < result_deals[max_price_deal].price) {
      deals::utils::copy(result_deals[max_price_deal], deal);
      max_price_deal = deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
      continue;
    }

    // result_deals are full by "limit"
    // and current deal price is more than any price in result vector
    // (result_deals)
    // so just skip it
  }

  // true - means continue to iterate
  return true;
}

// after iteration
void DealsSearchQuery::post_process_function() {
  // std::cout << "(POSTPROCESS) max price:" <<
  // result_deals[max_price_deal].price
  //          << std::endl;
}

void DealsSearchQuery::origin(std::string origin) {
  if (origin.length() == 0) {
    return;
  }
  filter_origin = true;
  origin_value = deals::utils::origin_to_code(origin);
}

void DealsSearchQuery::destinations(std::string destinations) {
  if (destinations.length() == 0) {
    return;
  }
  std::vector<std::string> split_result = ::utils::split_string(destinations);

  for (std::vector<std::string>::iterator dst = split_result.begin(); dst != split_result.end();
       ++dst) {
    if (dst->length() == 3) {
      destination_values_vector.push_back(deals::utils::origin_to_code(*dst));
    }
  }

  if (destination_values_vector.size()) {
    filter_destination = true;
  }
}

void DealsSearchQuery::max_lifetime_sec(uint32_t max_lifetime) {
  if (max_lifetime == 0) {
    return;
  }

  filter_timestamp = true;
  timestamp_value = timing::getTimestampSec() - max_lifetime;
}

void DealsSearchQuery::departure_dates(std::string departure_date_from,
                                       std::string departure_date_to) {
  if (departure_date_from.length() == 0 && departure_date_to.length() == 0) {
    return;
  }

  filter_departure_date = true;

  if (departure_date_from.length() == 0) {
    departure_date_values.from = 0;
  } else {
    departure_date_values.from = deals::utils::date_to_int(departure_date_from);
  }

  if (departure_date_to.length() == 0) {
    departure_date_values.to = UINT32_MAX;
  } else {
    departure_date_values.to = deals::utils::date_to_int(departure_date_to);
  }

  if (departure_date_values.from > departure_date_values.to) {
    query_is_broken = true;
  }
}

void DealsSearchQuery::return_dates(std::string return_date_from, std::string return_date_to) {
  if (return_date_from.length() == 0 && return_date_to.length() == 0) {
    return;
  }

  filter_return_date = true;
  std::cout << "DealsSearchQuery::return_dates:(" << return_date_from << "-" << return_date_to
            << ")" << std::endl;

  if (return_date_from.length() == 0) {
    return_date_values.from = 0;
  } else {
    return_date_values.from = deals::utils::date_to_int(return_date_from);
  }

  if (return_date_to.length() == 0) {
    return_date_values.to = UINT32_MAX;
  } else {
    return_date_values.to = deals::utils::date_to_int(return_date_to);
  }

  if (return_date_values.from > return_date_values.to) {
    query_is_broken = true;
  }
  if (return_date_values.from == 0 && return_date_values.to == 0) {
    query_is_broken = true;
  }
}

void DealsSearchQuery::stay_days(uint16_t stay_from, uint16_t stay_to) {
  if (stay_from == 0 && (stay_to == 0 || stay_to >= UINT8_MAX)) {
    return;
  }

  filter_stay_days = true;

  if (stay_from == 0) {
    stay_days_values.from = 0;
  } else {
    stay_days_values.from = stay_from;
  }

  if (stay_to == 0 || stay_to >= UINT8_MAX) {
    stay_days_values.to = UINT8_MAX;
  } else {
    stay_days_values.to = stay_to;
  }

  if (stay_days_values.from > stay_days_values.to) {
    query_is_broken = true;
  }
  if (stay_days_values.from == 0 && stay_days_values.to == 0) {
    query_is_broken = true;
  }
}

void DealsSearchQuery::departure_weekdays(std::string days_of_week) {
  if (days_of_week.length() == 0) {
    return;
  }
  departure_weekdays_bitmask = weekdays_bitmask(days_of_week);
  filter_departure_weekdays = true;
}

void DealsSearchQuery::return_weekdays(std::string days_of_week) {
  if (days_of_week.length() == 0) {
    return;
  }
  return_weekdays_bitmask = weekdays_bitmask(days_of_week);
  filter_return_weekdays = true;
}

uint8_t DealsSearchQuery::weekdays_bitmask(std::string days_of_week) {
  uint8_t result = 0;
  std::vector<std::string> split_result = ::utils::split_string(days_of_week);

  std::vector<std::string>::iterator day = split_result.begin();
  for (; day != split_result.end(); ++day) {
    uint8_t daycode = ::utils::day_of_week_from_str(*day);
    if (daycode < 0 || daycode > 6) {
      std::cout << "bad daycode for " << *day << "-" << days_of_week << std::endl;
      continue;
    }
    result |= (1 << daycode);
  }

  if (result == 0) {
    std::cout << "bad bitmask for " << days_of_week << std::endl;
    query_is_broken = true;
  }

  return result;
}

void DealsSearchQuery::direct_flights(bool direct_flights, bool stops_flights) {
  if (direct_flights == true && stops_flights == true) {
    return;
  }

  filter_flight_by_stops = true;
  direct_flights_flag = direct_flights;
  stops_flights_flag = stops_flights;

  if (direct_flights_flag == false && stops_flights_flag == false) {
    query_is_broken = true;
  }
}

void DealsSearchQuery::deals_limit(uint16_t _filter_limit) {
  if (_filter_limit) {
    // ignore zero value
    filter_limit = _filter_limit;
  }
}

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

DealsDatabase::~DealsDatabase() {
  delete db_data;
  delete db_index;
}

void DealsDatabase::truncate() {
  db_data->cleanup();
  db_index->cleanup();
}

void DealsDatabase::addDeal(std::string origin, std::string destination, std::string departure_date,
                            std::string return_date, bool direct_flight, uint32_t price,
                            std::string data) {
  if (origin.length() != 3) {
    std::cout << "wrong origin length:" << origin << std::endl;
    return;
  }

  if (destination.length() != 3) {
    std::cout << "wrong destination length:" << destination << std::endl;
    return;
  }

  uint32_t departure_date_int = deals::utils::date_to_int(departure_date);
  if (departure_date_int == 0) {
    std::cout << "wrong departure date:" << departure_date << std::endl;
    return;
  }

  uint32_t return_date_int = deals::utils::date_to_int(return_date);

  // Firstly add data and get data position at db
  deals::i::DealData *data_pointer = (deals::i::DealData *)data.c_str();
  uint32_t data_size = data.length();

  shared_mem::ElementPointer<i::DealData> result = db_data->addRecord(data_pointer, data_size);
  if (result.error) {
    std::cout << "ERROR:" << result.error << std::endl;
    return;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;

  i::DealInfo info;
  info.timestamp = timing::getTimestampSec();
  info.origin = deals::utils::origin_to_code(origin);
  info.destination = deals::utils::origin_to_code(destination);
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
  if (result.error) {
    std::cout << "ERROR:" << di_result.error << std::endl;
    return;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;
  // std::cout << "addDeal OK" << std::endl;
}

/*---------------------------------------------------------
* DealsDatabase   searchForCheapestEver
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::searchForCheapestEver(
    std::string origin, std::string destinations, std::string departure_date_from,
    std::string departure_date_to, std::string departure_days_of_week, std::string return_date_from,
    std::string return_date_to, std::string return_days_of_week, uint16_t stay_from,
    uint16_t stay_to, bool direct_flights, bool stops_flights, uint16_t limit,
    uint32_t max_lifetime_sec)

{
  DealsSearchQuery query(*db_index);

  query.origin(origin);
  query.destinations(destinations);
  query.departure_dates(departure_date_from, departure_date_to);
  query.return_dates(return_date_from, return_date_to);
  query.stay_days(stay_from, stay_to);
  query.direct_flights(direct_flights, stops_flights);
  query.deals_limit(limit);
  query.max_lifetime_sec(max_lifetime_sec);
  query.departure_weekdays(departure_days_of_week);
  query.return_weekdays(return_days_of_week);

  /*std::cout << "origin:" << origin
            << " destinations:" << destinations
            << " departure_date_from:" << departure_date_from
            << " departure_date_to:" << departure_date_to
            << " return_date_from:" << return_date_from
            << " return_date_to:" << return_date_to
            << " direct_flights:" << direct_flights
            << " stops_flights:" << stops_flights
            << " max_lifetime_sec:" << max_lifetime_sec
            << " limit:" << limit << std::endl;*/

  std::vector<i::DealInfo> deals = query.exec();
  // internal <i::DealInfo> contain shared memory page name and
  // information offsets. It's not useful anywhere outside
  // so lets transofrm internal format to external <DealInfo>
  std::vector<DealInfo> result;

  for (std::vector<i::DealInfo>::iterator deal = deals.begin(); deal != deals.end(); ++deal) {
    // std::cout << "DEAL> page:(" << deal->page_name << " " << deal->index << "
    // " << deal->size << ")" << std::endl;
    shared_mem::ElementPointer<i::DealData> deal_data(*db_data, deal->page_name, deal->index,
                                                      deal->size);
    i::DealData *data_pointer = deal_data.get_data();
    std::string data((char *)data_pointer, deal->size);

    result.push_back((DealInfo){
        deal->timestamp, utils::code_to_origin(deal->origin),
        utils::code_to_origin(deal->destination), utils::int_to_date(deal->departure_date),
        utils::int_to_date(deal->return_date), deal->stay_days, deal->flags, deal->price, data});
  }

  return result;
}

namespace utils {
// deals Utils
uint32_t origin_to_code(std::string code) {
  PlaceCodec a2i;
  a2i.iata_code[0] = 0;
  a2i.iata_code[1] = code[0];
  a2i.iata_code[2] = code[1];
  a2i.iata_code[3] = code[2];
  return a2i.int_code;
}

std::string code_to_origin(uint32_t code) {
  PlaceCodec a2i;
  a2i.int_code = code;
  std::string result;
  result += a2i.iata_code[1];
  result += a2i.iata_code[2];
  result += a2i.iata_code[3];
  return result;
}

void copy(i::DealInfo &dst, const i::DealInfo &src) {
  memcpy(&dst, &src, sizeof(i::DealInfo));
}

uint16_t get_max_price_in_array(i::DealInfo *&dst, uint16_t size) {
  // assert(size > 0);
  uint16_t max = 0;
  uint16_t pos = 0;

  for (uint16_t i = 0; i < size; i++) {
    if (max < dst[i].price) {
      max = dst[i].price;
      pos = i;
    }
  }
  return pos;
}

void print(const i::DealInfo &deal) {
  std::cout << "i::DEAL: (" << deals::utils::int_to_date(deal.departure_date) << ")"
            << deals::utils::code_to_origin(deal.origin) << "-"
            << deals::utils::code_to_origin(deal.destination) << "("
            << deals::utils::int_to_date(deal.return_date) << ") : " << deal.price << std::endl;
}
void print(const DealInfo &deal) {
  std::cout << "DEAL: (" << deal.departure_date << ")" << deal.origin << "-" << deal.destination
            << "(" << deal.return_date << ") : " << deal.price << std::endl;
}

std::string sprint(const DealInfo &deal) {
  return "(" + deal.departure_date + ")" + deal.origin + "-" + deal.destination + "(" +
         deal.return_date + ") : " + std::to_string(deal.price) + "|" + deal.data + "\n";
}

// ISO date standare 2016-06-16
uint32_t date_to_int(std::string date) {
  if (date.length() != 10) {
    return 0;
  }
  if (date[4] != '-' || date[7] != '-') {
    return 0;
  }

  date.erase(4, 1);
  date.erase(6, 1);

  try {
    return std::stol(date);
  } catch (std::exception e) {
    return 0;
  }
};

std::string int_to_date(uint32_t date) {
  if (!date) {
    return "";
  }

  std::string result;
  uint16_t year;
  uint16_t month;
  uint16_t day;

  // 20160601
  year = date / 10000;
  month = (date - year * 10000) / 100;
  day = date - year * 10000 - month * 100;

  result = std::to_string(year) + "-" + (month < 10 ? "0" : "") + std::to_string(month) + "-" +
           (day < 10 ? "0" : "") + std::to_string(day);
  return result;
};

bool check_destinations_format(std::string destinations) {
  if (destinations.length() == 0) {
    return true;
  }

  std::vector<std::string> split_result = ::utils::split_string(destinations);

  for (std::vector<std::string>::iterator dst = split_result.begin(); dst != split_result.end();
       ++dst) {
    if (dst->length() != 3) {
      return false;
    }
  }

  return true;
}

bool check_weekdays_format(std::string weekdays) {
  if (weekdays.length() == 0) {
    return true;
  }

  std::vector<std::string> split_result = ::utils::split_string(weekdays);

  for (std::vector<std::string>::iterator day = split_result.begin(); day != split_result.end();
       ++day) {
    if (day->length() != 3) {
      return false;
    }

    if (::utils::day_of_week_from_str(*day) > 6) {
      return false;
    }
  }

  return true;
}

//--------------------------------------------------
//
//--------------------------------------------------
bool check_date_format(std::string date) {
  return date_to_int(date) != 0;
}

//--------------------------------------------------
//
//--------------------------------------------------
bool check_date_to_date(std::string _date_from, std::string _date_to) {
  uint32_t date_from = date_to_int(_date_from);
  uint32_t date_to = date_to_int(_date_to);
  if (date_from && date_to) {
    return date_from <= date_to;
  }
  return true;
}

}  // utils napespace

std::string getRandomOrigin() {
  static const std::string origins[] = {"MOW", "MAD", "BER", "LON", "PAR",
                                        "LAX", "LED", "FRA", "BAR"};
  uint16_t place = rand() % (sizeof(origins) / sizeof(origins[0]));
  return origins[place];
}

uint32_t getRandomPrice(uint32_t minPrice) {
  return minPrice;

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

  return deals::utils::int_to_date(year * 10000 + month * 100 + day);
}

void convertertionTest() {
  std::cout << "Origin encoder/decoder test:" << std::endl;
  std::string origins[10] = {"MOW", "MAD", "BER", "PAR", "LON", "FRA", "VKO", "JFK", "LAX", "MEX"};

  for (int i = 0; i < 10; i++) {
    uint32_t code = deals::utils::origin_to_code(origins[i]);
    std::string decode = deals::utils::code_to_origin(code);
    std::cout << origins[i] << " -> " << code << " -> " << decode << std::endl;

    assert(origins[i] == decode);
  }

  std::cout << "Date encoder/decoder test:\n";
  uint32_t code = deals::utils::date_to_int("2017-01-01");
  std::string date = deals::utils::int_to_date(code);

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
  std::cout << "date functions... OK" << std::endl;

  convertertionTest();
  std::cout << "city conv functions... OK" << std::endl;

  DealsDatabase db;
  // db.truncate();

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
  return;
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
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(), getRandomDate(), false,
               getRandomPrice(8000), dumb);
  }

  timer.tick("before test1");
  // 1st test ----------------------------
  // *********************************************************
  std::vector<DealInfo> result = db.searchForCheapestEver("MOW", "AAA,PAR,BER,MAD", "", "", "", "",
                                                          "", "", 0, 0, true, true, 0, 10);
  timer.tick("test1");

  for (std::vector<DealInfo>::iterator deal = result.begin(); deal != result.end(); ++deal) {
    deals::utils::print(*deal);
  }

  assert(result.size() == 3);
  int city_count[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); i++) {
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
  result = db.searchForCheapestEver("MOW", "AAA,PAR,BER,MAD", "2016-06-01", "2016-06-23", "",
                                    "2016-06-10", "2016-06-22", "", 0, 0, true, true, 0, 10);

  timer.tick("test2");

  for (std::vector<DealInfo>::iterator deal = result.begin(); deal != result.end(); ++deal) {
    deals::utils::print(*deal);
  }

  assert(result.size() <= 3);
  int city_count2[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); i++) {
    assert(deals::utils::date_to_int(result[i].departure_date) >=
           deals::utils::date_to_int("2016-06-01"));
    assert(deals::utils::date_to_int(result[i].departure_date) <=
           deals::utils::date_to_int("2016-06-23"));
    assert(deals::utils::date_to_int(result[i].return_date) >=
           deals::utils::date_to_int("2016-06-10"));
    assert(deals::utils::date_to_int(result[i].return_date) <=
           deals::utils::date_to_int("2016-06-22"));

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
                                    false, true, 0, 2000);
  timer.tick("test3");

  for (std::vector<DealInfo>::iterator deal = result.begin(); deal != result.end(); ++deal) {
    deals::utils::print(*deal);
  }

  for (int i = 0; i < result.size(); i++) {
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

  deals::DealsDatabase db;
  std::string dumb = "1, 2, 3, 4, 5, 6, 7, 8";
  srand(timing::getTimestampSec());

  // add some data, that will be outdated
  for (int i = 0; i < 1000000; ++i) {
    db.addDeal(deals::getRandomOrigin(), deals::getRandomOrigin(), deals::getRandomDate(),
               deals::getRandomDate(), true, deals::getRandomPrice(1000), dumb);
  }

  for (int i = 0; i < 1000000; ++i) {
    db.addDeal(deals::getRandomOrigin(), deals::getRandomOrigin(), deals::getRandomDate(),
               deals::getRandomDate(), true, deals::getRandomPrice(1000), dumb);
  }

  return 0;
}
