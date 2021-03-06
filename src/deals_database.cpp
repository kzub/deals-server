#include <cassert>
#include <cinttypes>
#include <climits>
#include <iostream>

#include "deals_database.hpp"
#include "timing.hpp"

namespace deals {
//---------------------------------------------------------
//  DealsDatabase  constructor
//---------------------------------------------------------
DealsDatabase::DealsDatabase()
    : db_context{DEALS_DB_NAME},
      db_index{DEALINFO_TABLENAME, DEALINFO_PAGES, DEALINFO_ELEMENTS, DEALS_EXPIRES, db_context},
      db_data{DEALDATA_TABLENAME, DEALDATA_PAGES, DEALDATA_ELEMENTS, DEALS_EXPIRES, db_context} {
  if (TEST_BUILD) {
    std::cout << "!!! TEST BUILD !!!!" << std::endl;
  }
}

//---------------------------------------------------------
//  DealsDatabase  truncate
//---------------------------------------------------------
void DealsDatabase::truncate() {
  db_data.cleanup();
  db_index.cleanup();
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
  auto result = db_data.addRecord(data_pointer, data_size);

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
  auto di_result = db_index.addRecord(&info);
}

/*---------------------------------------------------------
* DealsDatabase  fill_deals_with_data
*---------------------------------------------------------*/
std::vector<DealInfo> DealsDatabase::fill_deals_with_data(std::vector<i::DealInfo> i_deals) {
  std::vector<DealInfo> result;

  for (const auto &deal : i_deals) {
    auto deal_data = i::sharedDealData{db_data, deal.page_name, deal.index, deal.size};
    auto data_pointer = (char *)deal_data.get_element_data();
    std::string data = {data_pointer, deal.size};

    if (TEST_BUILD) {
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

//---------------------------------------------------------
// DealsDatabase  getUniqueRoutesDeals
//--------------------------------------------------------
const std::string DealsDatabase::getUniqueRoutesDeals() {
  return getUniqueRoutesRoutine(this->db_index);
}

//---------------------------------------------------------
// DealsDatabase  stat
//--------------------------------------------------------
const std::string DealsDatabase::getStats() {
  return getStatsRoutine(this->db_index);
}

}  // deals namespace
