#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <climits>
#include <iostream>

#include "deals_database.hpp"
#include "timing.hpp"

namespace deals {
//---------------------------------------------------------
// DealsDatabase constructor
//---------------------------------------------------------
DealsDatabase::DealsDatabase() {
  // 1k pages x 10k elements per page, '0m records total, expire 60 seconds
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
      std::cout << "TEST MODE" << std::endl;

      result.push_back({data, testdata});
    } else {
      result.push_back({data, nullptr});
    }
  }

  return result;
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
