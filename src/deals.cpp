#include <sys/mman.h>
#include <cassert>
#include <cinttypes>
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
class DealsSearchQuery : public shared_mem::TableProcessor<DealInfo> {
 public:
  DealsSearchQuery(shared_mem::Table<DealInfo> *table)
      : table(table),
        filter_origin(false),
        filter_destination(false),
        filter_departure_date(false),
        filter_return_date(false),
        filter_timestamp(false),
        filter_limit(20) {}

  std::vector<DealInfo> exec() {
    if (!filter_timestamp) {
      std::cout << "WARNING no timestamp specified" << std::endl;
    }

    // -------------------------------------------
    // build destination's fixed array (for search)
    if (filter_destination) {
      filter_limit = filter_destination_values_vector.size();
    }

    // we need this definition scope to let variable live till the end of
    // function
    uint32_t destination_storage[filter_limit];

    if (filter_destination) {
      filter_destination_values = destination_storage;
      std::vector<uint32_t>::iterator dst;
      uint32_t counter = 0;
      for (dst = filter_destination_values_vector.begin();
           dst != filter_destination_values_vector.end(); ++dst, ++counter) {
        filter_destination_values[counter] = *dst;
      }
    }

    // -------------------------------------------
    // define storage to write founded deals
    DealInfo deals_storage[filter_limit];
    // initialize pointer
    result_deals = deals_storage;
    deals_storage[0].price = 0;

    // -------------------------------------------
    max_price_deal = 0;
    deals_slots_used = 0;

    table->process(this);

    // reformat result to vector
    std::vector<DealInfo> exec_result;

    for (int i = 0; i < deals_slots_used; ++i) {
      exec_result.push_back(deals_storage[i]);
      // utils::print(deals_storage[i]);
    }

    return exec_result;
  };

  // before iteration
  void pre_process_function() { std::cout << "(PREPROCESS)" << std::endl; }

  /* function that will be called by TableProcessor
        *  for iterating over all not expired pages in table */
  bool process_function(DealInfo *elements, uint32_t size) {
    for (uint32_t idx = 0; idx < size; idx++) {
      const DealInfo &deal = elements[idx];

      // --------------------------------
      // skip old data
      if (filter_timestamp && filter_timestamp_value > deal.timestamp) {
        continue;
      }

      // --------------------------------
      // if origin is provided let's look only for this origin
      if (filter_origin && filter_origin_value != deal.origin) {
        continue;
      }

      // --------------------------------
      // if destanations are provided let's look only for this destinations
      if (filter_destination) {
        bool condition_matched = false;
        for (uint16_t dst_idx = 0; dst_idx < filter_limit; dst_idx++) {
          if (filter_destination_values[dst_idx] == deal.destination) {
            condition_matched = true;
            break;
          }
        }

        if (!condition_matched) {
          continue;
        }
      }

      // --------------------------------

      // std::cout << idx << " " << deal.timestamp << " | ";
      // utils::print(deal);

      // ----------------------------------
      // try to find deal by destination in result array
      // ----------------------------------
      bool found_deal_by_destination = false;
      for (uint16_t fidx = 0; fidx < deals_slots_used; fidx++) {
        DealInfo &result_deal = result_deals[fidx];

        if (deal.destination == result_deal.destination) {
          // we already have this destination, let's check for price
          if (deal.price < result_deal.price) {
            deals::utils::copy(result_deal, deal);
            max_price_deal = deals::utils::get_max_price_in_array(
                result_deals, deals_slots_used);
          }

          found_deal_by_destination = true;
          break;
        }
      }

      // there was found destination, so goto the next deal element
      if (found_deal_by_destination) {
        continue;
      }
      // ----------------------------------

      // no destinations are found in result
      // if there are unUsed slots -> fill them
      if (deals_slots_used < filter_limit) {
        deals::utils::print(deal);
        deals::utils::copy(result_deals[deals_slots_used], deal);
        deals_slots_used++;
        max_price_deal =
            deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
        continue;
      }
      // if all slots are used, but current deals
      // is cheaper than deals in result -> let replace most expensive with new
      // one
      else if (result_deals[max_price_deal].price > deal.price) {
        deals::utils::copy(result_deals[max_price_deal], deal);
        max_price_deal =
            deals::utils::get_max_price_in_array(result_deals, deals_slots_used);
        continue;
      }

      std::cout << std::endl
                << "SOME EXPSNSIVE DEAL:" << deal.price << std::endl;
    }
    // std::cout << std::endl << "found:" << found << std::endl;
    return true;
  }

  // after iteration
  void post_process_function() {
    std::cout << "(POSTPROCESS)" << std::endl;
    std::cout << "max price:" << result_deals[max_price_deal].price
              << std::endl;
  }

  shared_mem::Table<DealInfo> *table;
  std::vector<DealInfo> matched_deals;

  bool filter_origin;
  uint32_t filter_origin_value;
  void origin(std::string origin) {
    filter_origin = true;
    filter_origin_value = deals::utils::origin_to_code(origin);
  }

  bool filter_destination;
  uint32_t *filter_destination_values;
  std::vector<uint32_t> filter_destination_values_vector;
  void destinations(std::string destinations) {
    std::vector<std::string> split_result =
        ::utils::split_string(destinations);

    for (std::vector<std::string>::iterator dst = split_result.begin();
         dst != split_result.end(); ++dst) {
      filter_destination_values_vector.push_back(
          deals::utils::origin_to_code(*dst));
    }

    filter_destination = true;
  }

  bool filter_departure_date;
  Int32Interval filter_departure_date_values;

  bool filter_return_date;
  Int32Interval filter_return_date_values;

  bool filter_timestamp;
  uint32_t filter_timestamp_value;
  void max_lifetime_sec(uint32_t max_lifetime) {
    filter_timestamp = true;
    filter_timestamp_value = timing::getTimestampSec() - max_lifetime;
  }

  int16_t filter_limit;
  int16_t deals_slots_used;
  DealInfo *result_deals;
  uint16_t max_price_deal;
};

DealsDatabase::DealsDatabase() {
  // 1k pages x 10k elements per page, 10m records total, expire 60 seconds
  db_index = new shared_mem::Table<DealInfo>("DealsInfo", 1000 /* pages */,
                                             10000 /* elements in page */,
                                             60 /* page expire */);

  // 10k pages x 3.2m per page = 32g bytes, expire 60 s§econds
  db_data = new shared_mem::Table<DealData>("DealsData", 10000 /* pages */,
                                            3200000 /* elements in page */,
                                            60 /* page expire */);
}

DealsDatabase::~DealsDatabase() {
  delete db_data;
  delete db_index;
}

void DealsDatabase::addDeal(std::string origin, std::string destination,
                            uint32_t departure_date, uint32_t return_date,
                            bool direct_flight, uint32_t price, DealData *data,
                            uint32_t size) {
  if (origin.length() != 3) {
    std::cout << "wrong origin length:" << origin << std::endl;
    return;
  }

  if (destination.length() != 3) {
    std::cout << "wrong destination length:" << destination << std::endl;
    return;
  }

  // Firstly add data and get data position at db
  shared_mem::OperationResult result = db_data->addRecord(data, size);
  if (result.error) {
    std::cout << "ERROR:" << result.error << std::endl;
    return;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;

  DealInfo info;
  info.timestamp = timing::getTimestampSec();
  info.origin = deals::utils::origin_to_code(origin);
  info.destination = deals::utils::origin_to_code(destination);
  info.departure_date = departure_date;
  info.return_date = return_date;
  info.flags = direct_flight;
  info.price = price;
  strncpy(info.page_name, result.page_name.c_str(), MEMPAGE_NAME_MAX_LEN);
  info.index = result.index;
  info.size = result.size;

  // Secondly add deal to index include data position information
  result = db_index->addRecord(&info);
  if (result.error) {
    std::cout << "ERROR:" << result.error << std::endl;
    return;
  }

  // std::cout << "{" << result.page_name << "}" << std::endl;
  // std::cout << "{" << result.index << "}" << std::endl;
  // std::cout << "{" << result.size << "}" << std::endl;
  // std::cout << "{" << result.error << "}" << std::endl;
  // std::cout << "addDeal OK" << std::endl;
}

std::vector<DealInfo> DealsDatabase::searchForCheapestEver(
    std::string origin, std::string destinations) {
  DealsSearchQuery query(db_index);

  query.max_lifetime_sec(10);
  query.origin(origin);
  query.destinations(destinations);

  std::vector<DealInfo> res = query.exec();

  return res;
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

void copy(DealInfo& dst, const DealInfo& src) {
  memcpy(&dst, &src, sizeof(DealInfo));
}

uint16_t get_max_price_in_array(DealInfo*& dst, uint16_t size) {
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

void print(const DealInfo& deal) {
  std::cout << "DEAL: " << deals::utils::code_to_origin(deal.origin) << "-"
            << deals::utils::code_to_origin(deal.destination) << " "
            << deal.price << std::endl;
}
}



std::string getRandomOrigin() {
  static const std::string origins[] = {"MOW", "MAD", "BER", "LON", "PAR",
                                        "LAX", "LED", "FRA", "BAR"};
  uint16_t place = rand() % (sizeof(origins) / sizeof(origins[0]));
  return origins[place];
}

uint32_t getRandomPrice(uint32_t minPrice){
  return minPrice;

  uint32_t price = rand() & 0x0000FFFF;
  price += minPrice;

  if(price < minPrice){
    std::cout << "ALARM!! " << minPrice << " " << price << std::endl;
  }
  return price;
}

void convertertionTest() {
  std::string origins[10] = {"MOW", "MAD", "BER", "PAR", "LON",
                             "FRA", "VKO", "JFK", "LAX", "MEX"};

  for (int i = 0; i < 10; i++) {
    uint32_t code = deals::utils::origin_to_code(origins[i]);
    std::string decode = deals::utils::code_to_origin(code);
    std::cout << origins[i] << " -> " << code << " -> " << decode << std::endl;

    assert(origins[i] == decode);
  }
}


#define TEST_ELEMENTS_COUNT 10000
int test() {
  DealsDatabase db;
  DealData dumb[] = {1, 2, 3, 4, 5, 6, 7, 8};
  DealData check[] = {7, 7, 7};

  timing::Timer timer("SimpleSearch");

  srand(timing::getTimestampSec());

  // add some data, that will be outdated
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), getRandomOrigin(), timing::getTimestampSec(),
               timing::getTimestampSec(), true, getRandomPrice(1000),
               dumb, sizeof(dumb));
    db.addDeal(getRandomOrigin(), getRandomOrigin(), timing::getTimestampSec(),
               timing::getTimestampSec(), true, getRandomPrice(2000),
               dumb, sizeof(dumb));
    db.addDeal(getRandomOrigin(), getRandomOrigin(), timing::getTimestampSec(),
               timing::getTimestampSec(), true, getRandomPrice(3000),
               dumb, sizeof(dumb));
  }

  // go to the feature (+1000 seconds)
  timing::TimeLord time;
  time += 1000;

  // add data we will expect
  db.addDeal("MOW", "MAD", timing::getTimestampSec(), timing::getTimestampSec(),
             true, 5000, check, sizeof(check));
  db.addDeal("MOW", "BER", timing::getTimestampSec(), timing::getTimestampSec(),
             true, 6000, check, sizeof(check));
  db.addDeal("MOW", "PAR", timing::getTimestampSec(), timing::getTimestampSec(),
             true, 7000, check, sizeof(check));

  time += 5;

  // add some good
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), "MAD", timing::getTimestampSec(),
               timing::getTimestampSec(), true, getRandomPrice(5100),
               dumb, sizeof(dumb));
    db.addDeal(getRandomOrigin(), "BER", timing::getTimestampSec(),
               timing::getTimestampSec(), true, getRandomPrice(6200),
               dumb, sizeof(dumb));
    db.addDeal(getRandomOrigin(), "PAR", timing::getTimestampSec(),
               timing::getTimestampSec(), true, getRandomPrice(7200),
               dumb, sizeof(dumb));
    db.addDeal(getRandomOrigin(), getRandomOrigin(), timing::getTimestampSec(),
               timing::getTimestampSec(), true, getRandomPrice(8000),
               dumb, sizeof(dumb));
  }

  timer.tick("data generated");

  std::vector<DealInfo> result =
      db.searchForCheapestEver("MOW", "AAA,PAR,BER,MAD");
  timer.tick("ok");
  timer.report();

  for (std::vector<DealInfo>::iterator deal = result.begin();
       deal != result.end(); ++deal) {
    deals::utils::print(*deal);
  }

  assert(result.size() == 3);
  int city_count[3] = { 0,0,0};

  for(int i = 0; i < result.size(); i++){
    // std::cout << deals::utils::code_to_origin(result[i].destination) << std::endl;
    if(deals::utils::code_to_origin(result[i].destination) == "MAD"){
      city_count[0]++;
      assert(result[i].price == 5000);
    }
    if(deals::utils::code_to_origin(result[i].destination) == "BER"){
      city_count[1]++;
      assert(result[i].price == 6000);
    }
    if(deals::utils::code_to_origin(result[i].destination) == "PAR"){
      city_count[2]++;
      assert(result[i].price == 7000);
    }
  }

  assert(city_count[0] == 1);
  assert(city_count[1] == 1);
  assert(city_count[2] == 1);

  convertertionTest();

  std::cout << "OK" << std::endl;
  return 0;
}


}

int main2(){
  deals::test();
  return 0;
}




