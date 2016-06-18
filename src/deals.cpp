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
std::vector<i::DealInfo> DealsSearchQuery::exec() {
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
  i::DealInfo deals_storage[filter_limit];
  // initialize pointer
  result_deals = deals_storage;
  deals_storage[0].price = 0;

  // -------------------------------------------
  max_price_deal = 0;
  deals_slots_used = 0;

  // call this class pre/procesee/post functions with shared mem page pointers
  table.process(this);

  // reformat result to vector
  std::vector<i::DealInfo> exec_result;

  for (int i = 0; i < deals_slots_used; ++i) {
    exec_result.push_back(deals_storage[i]);
    // utils::print(deals_storage[i]);
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

    // --------------------------------
    // skip old data
    if (filter_timestamp && filter_timestamp_value > deal.timestamp) {
      // std::cout << "filter_timestamp" << std::endl;
      continue;
    }

    // --------------------------------
    // if origin is provided let's look only for this origin
    if (filter_origin && filter_origin_value != deal.origin) {
      // std::cout << "filter_origin" << std::endl;
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
        // std::cout << "condition_matched" << std::endl;
        continue;
      }
    }

    // --------------------------------

    // std::cout << "GOT" << idx << " " << deal.timestamp << " | ";
    // utils::print(deal);

    // ----------------------------------
    // try to find deal by destination in result array
    // ----------------------------------
    bool found_deal_by_destination = false;
    for (uint16_t fidx = 0; fidx < deals_slots_used; fidx++) {
      i::DealInfo &result_deal = result_deals[fidx];

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
      // deals::utils::print(deal);
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

    std::cout << std::endl << "SOME EXPSNSIVE DEAL:" << deal.price << std::endl;
  }
  // std::cout << std::endl << "found:" << found << std::endl;
  return true;
}

// after iteration
void DealsSearchQuery::post_process_function() {
  // std::cout << "(POSTPROCESS) max price:" << result_deals[max_price_deal].price
  //          << std::endl;
}

void DealsSearchQuery::origin(std::string origin) {
  filter_origin = true;
  filter_origin_value = deals::utils::origin_to_code(origin);
}

void DealsSearchQuery::destinations(std::string destinations) {
  if (!destinations.length()) {
    return;
  }
  std::vector<std::string> split_result = ::utils::split_string(destinations);

  for (std::vector<std::string>::iterator dst = split_result.begin();
       dst != split_result.end(); ++dst) {
    filter_destination_values_vector.push_back(
        deals::utils::origin_to_code(*dst));
  }

  filter_destination = true;
}

void DealsSearchQuery::max_lifetime_sec(uint32_t max_lifetime) {
  filter_timestamp = true;
  filter_timestamp_value = timing::getTimestampSec() - max_lifetime;
}

DealsDatabase::DealsDatabase() {
  // 1k pages x 10k elements per page, 10m records total, expire 60 seconds
  db_index = new shared_mem::Table<i::DealInfo>("DealsInfo", 1000 /* pages */,
                                                10000 /* elements in page */,
                                                10 /* page expire */);

  // 10k pages x 3.2m per page = 32g bytes, expire 60 s§econds
  db_data = new shared_mem::Table<i::DealData>("DealsData", 10000 /* pages */,
                                               3200000 /* elements in page */,
                                               10 /* page expire */);
}

DealsDatabase::~DealsDatabase() {
  delete db_data;
  delete db_index;
}

void DealsDatabase::truncate(){
  db_data->cleanup();
  db_index->cleanup();
}


void DealsDatabase::addDeal(std::string origin, std::string destination,
                            uint32_t departure_date, uint32_t return_date,
                            bool direct_flight, uint32_t price,
                            std::string data) {
  if (origin.length() != 3) {
    std::cout << "wrong origin length:" << origin << std::endl;
    return;
  }

  if (destination.length() != 3) {
    std::cout << "wrong destination length:" << destination << std::endl;
    return;
  }

  // Firstly add data and get data position at db
  deals::i::DealData *data_pointer = (deals::i::DealData *)data.c_str();
  uint32_t data_size = data.length();

  shared_mem::ElementPointer<i::DealData> result =
      db_data->addRecord(data_pointer, data_size);
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
  info.departure_date = departure_date;
  info.return_date = return_date;
  info.flags = direct_flight;
  info.price = price;
  strncpy(info.page_name, result.page_name.c_str(), MEMPAGE_NAME_MAX_LEN);
  info.index = result.index;
  info.size = result.size;

  // Secondly add deal to index, include data position information
  shared_mem::ElementPointer<i::DealInfo> di_result =
      db_index->addRecord(&info);
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
    std::string origin, std::string destinations, uint32_t max_lifetime_sec) {
  DealsSearchQuery query(*db_index);

  query.max_lifetime_sec(max_lifetime_sec);
  query.origin(origin);
  query.destinations(destinations);

  std::vector<i::DealInfo> deals = query.exec();
  std::vector<DealInfo> result;

  for (std::vector<i::DealInfo>::iterator deal = deals.begin();
       deal != deals.end(); ++deal) {
    // std::cout << "PPP(" << deal->page_name << " " << deal->index << " " <<
    // deal->size << ")" << std::endl;
    shared_mem::ElementPointer<i::DealData> deal_data(*db_data, deal->page_name,
                                                      deal->index, deal->size);
    i::DealData *data_pointer = deal_data.get_data();
    std::string data((char *)data_pointer, deal->size);

    // std::cout << text << std::endl;
    result.push_back((DealInfo){
        deal->timestamp, utils::code_to_origin(deal->origin),
        utils::code_to_origin(deal->destination),
        utils::int_to_date(deal->departure_date),
        utils::int_to_date(deal->return_date), deal->flags, deal->price, data});
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
  std::cout << "i::DEAL: (" << deals::utils::int_to_date(deal.departure_date)
            << ")" << deals::utils::code_to_origin(deal.origin) << "-"
            << deals::utils::code_to_origin(deal.destination) << "("
            << deals::utils::int_to_date(deal.return_date)
            << ") : " << deal.price << std::endl;
}
void print(const DealInfo &deal) {
  std::cout << "DEAL: (" << deal.departure_date << ")" << deal.origin << "-"
            << deal.destination << "(" << deal.return_date
            << ") : " << deal.price << std::endl;
}

std::string sprint(const DealInfo &deal) {
  return "(" + deal.departure_date + ")" + deal.origin + "-" +
         deal.destination + "(" + deal.return_date + ") : " +
         std::to_string(deal.price) + "|" + deal.data + "\n";
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

  result = std::to_string(year) + "-" + (month < 10 ? "0" : "") +
           std::to_string(month) + "-" + (day < 10 ? "0" : "") +
           std::to_string(day);
  return result;
};

std::string DealInfo_to_json(const DealInfo di) {
  // return ("{" +
  // "\"ts\":"  + std::to_string(di.timestamp) +
  // "\"origin\":\"" + origin + "\"" +
  // "\"destination\":" + destination + "\"" +
  // "\"departure_date\":" + departure_date + "\"" +
  // "\"return_date\":" + return_date + "\"" +
  // "\"data\":" + data;
  // flags;  // direct?
  // price;
  // "}");
  return "nojson";
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

uint32_t getRandomDate() {
  uint32_t year = 2016;
  uint32_t month = (rand() & 0x00000007) + 1;
  uint32_t day = (rand() & 0x00000007) + 1;

  return year * 10000 + month * 100 + day;
}

void convertertionTest() {
  std::cout << "Origin encoder/decoder test:" << std::endl;
  std::string origins[10] = {"MOW", "MAD", "BER", "PAR", "LON",
                             "FRA", "VKO", "JFK", "LAX", "MEX"};

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

#define TEST_ELEMENTS_COUNT 10000
void unit_test() {
  DealsDatabase db;
  std::string dumb = "1, 2, 3, 4, 5, 6, 7, 8";
  std::string check = "7, 7, 7";

  timing::Timer timer("SimpleSearch");

  srand(timing::getTimestampSec());

  // add some data, that will be outdated
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(),
               getRandomDate(), true, getRandomPrice(1000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(),
               getRandomDate(), true, getRandomPrice(2000), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(),
               getRandomDate(), true, getRandomPrice(3000), dumb);
  }

  // go to the feature (+1000 seconds)
  timing::TimeLord time;
  time += 1000;

  // add data we will expect
  db.addDeal("MOW", "MAD", getRandomDate(), getRandomDate(), true, 5000, check);
  db.addDeal("MOW", "BER", getRandomDate(), getRandomDate(), true, 6000, check);
  db.addDeal("MOW", "PAR", getRandomDate(), getRandomDate(), true, 7000, check);

  time += 5;

  // add some good
  for (int i = 0; i < TEST_ELEMENTS_COUNT; ++i) {
    db.addDeal(getRandomOrigin(), "MAD", getRandomDate(), getRandomDate(), true,
               getRandomPrice(5100), dumb);
    db.addDeal(getRandomOrigin(), "BER", getRandomDate(), getRandomDate(), true,
               getRandomPrice(6200), dumb);
    db.addDeal(getRandomOrigin(), "PAR", getRandomDate(), getRandomDate(), true,
               getRandomPrice(7200), dumb);
    db.addDeal(getRandomOrigin(), getRandomOrigin(), getRandomDate(),
               getRandomDate(), true, getRandomPrice(8000), dumb);
  }

  timer.tick("data generated");

  std::vector<DealInfo> result =
      db.searchForCheapestEver("MOW", "AAA,PAR,BER,MAD", 10);
  timer.tick("ok");
  timer.report();

  for (std::vector<DealInfo>::iterator deal = result.begin();
       deal != result.end(); ++deal) {
    deals::utils::print(*deal);
  }

  assert(result.size() == 3);
  int city_count[3] = {0, 0, 0};

  for (int i = 0; i < result.size(); i++) {
    if (result[i].destination == "MAD") {
      city_count[0]++;
      assert(result[i].price == 5000);
    } else if (result[i].destination == "BER") {
      city_count[1]++;
      assert(result[i].price == 6000);
    } else if (result[i].destination == "PAR") {
      city_count[2]++;
      assert(result[i].price == 7000);
    }

    assert(result[i].data == "7, 7, 7");
  }

  assert(city_count[0] == 1);
  assert(city_count[1] == 1);
  assert(city_count[2] == 1);

  convertertionTest();

  std::cout << "OK" << std::endl;
}


} // deals namespace

int main222(){
  // deals::unit_test();
  deals::DealsDatabase db;
  std::string dumb = "1, 2, 3, 4, 5, 6, 7, 8";
  srand(timing::getTimestampSec());

  // add some data, that will be outdated
  for (int i = 0; i < 1000000; ++i) {
    db.addDeal(deals::getRandomOrigin(), deals::getRandomOrigin(), deals::getRandomDate(),
               deals::getRandomDate(), true, deals::getRandomPrice(1000), dumb);
  }

  sleep(10);

  for (int i = 0; i < 1000000; ++i) {
    db.addDeal(deals::getRandomOrigin(), deals::getRandomOrigin(), deals::getRandomDate(),
               deals::getRandomDate(), true, deals::getRandomPrice(1000), dumb);
  }

  sleep(10);

  return 0;
}




