#include <algorithm>
#include <cinttypes>
#include <iostream>

#include "deals.hpp"
#include "shared_memory.hpp"
#include "timing.hpp"
#include "top_destinations.hpp"

namespace top {
// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
TopDstDatabase::TopDstDatabase() {
  // 1k pages x 10k elements per page, 10m records total, expire 60 seconds
  db_index = new shared_mem::Table<i::DstInfo>(TOPDST_TABLENAME, TOPDST_PAGES /* pages */,
                                               TOPDST_ELEMENTS /* elements in page */,
                                               TOPDST_EXPIRES /* page expire */);
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
TopDstDatabase::~TopDstDatabase() {
  delete db_index;
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
void TopDstDatabase::truncate() {
  db_index->cleanup();
}

// -----------------------------------------------------------------
// addDestination
// -----------------------------------------------------------------
void TopDstDatabase::addDestination(const types::CountryCode& locale,
                                    const types::IATACode& destination,
                                    const types::Date& departure_date) {
  i::DstInfo info = {locale.get_code(), destination.get_code(), departure_date.get_code()};

  // Secondly add deal to index, include data position information
  auto di_result = db_index->addRecord(&info);
  if (di_result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cout << "ERROR addDestination():" << (int)di_result.error << std::endl;
    throw types::Error("Internal Error: addRecord->DstInfo", types::ErrorCode::InternalError);
  }
}

// -----------------------------------------------------------------
// getCachedResult
// -----------------------------------------------------------------
std::vector<DstInfo> TopDstDatabase::getCachedResult(const types::CountryCode& locale,
                                                     const types::Date& departure_date_from,
                                                     const types::Date& departure_date_to,
                                                     const types::Number& limit) {
  //
  if (departure_date_from.isDefined() || departure_date_to.isDefined()) {
    return {};
  }

  auto cache = result_cache_by_locale.find(locale.get_code());
  if (cache == result_cache_by_locale.end()) {
    return {};
  }

  if (cache->second.is_expired()) {
    result_cache_by_locale.erase(cache);
    return {};
  }

  auto result = cache->second.get_value();

  if (limit.isDefined()) {
    auto limit_value = limit.get_value();
    if (result.size() < limit_value) {
      return {};
    }

    if (result.size() > limit_value) {
      result.resize(limit.get_value());
    }
  }

  return result;
}

// -----------------------------------------------------------------
// saveResultToCache
// -----------------------------------------------------------------
void TopDstDatabase::saveResultToCache(const types::CountryCode& locale,
                                       const types::Date& departure_date_from,
                                       const types::Date& departure_date_to,
                                       const std::vector<DstInfo>& result) {
  // one minute top destinations cache:
  if (result.size() == 0 || departure_date_from.isDefined() || departure_date_to.isDefined()) {
    return;
  }

  auto locale_code = locale.get_code();
  auto cache = result_cache_by_locale.find(locale_code);

  if (cache != result_cache_by_locale.end()) {
    result_cache_by_locale.erase(cache);
  }

  auto cached_result = TopDstDatabase::CachedResult{result, 60};
  result_cache_by_locale.emplace(locale_code, cached_result);
  // result_cache_by_locale.insert(
  // std::pair<decltype(locale_code), decltype(cached_result)>({locale_code, cached_result}));
}

// -----------------------------------------------------------------
// getLocaleTop
// -----------------------------------------------------------------
std::vector<DstInfo> TopDstDatabase::getLocaleTop(
    const types::Required<types::CountryCode>& locale,
    const types::Optional<types::Date>& departure_date_from,
    const types::Optional<types::Date>& departure_date_to,
    const types::Optional<types::Number>& limit) {
  //
  auto cache_result = getCachedResult(locale, departure_date_from, departure_date_to, limit);
  if (cache_result.size() > 0) {
    return std::move(cache_result);
  }

  TopDstSearchQuery query(*db_index);

  query.locale(locale);
  query.departure_dates(departure_date_from, departure_date_to);
  query.result_limit(limit);

  auto result = query.exec();

  saveResultToCache(locale, departure_date_from, departure_date_to, result);
  return std::move(result);
}

// -----------------------------------------------------------------
// exec
// -----------------------------------------------------------------
std::vector<DstInfo> TopDstSearchQuery::exec() {
  grouped_destinations.clear();

  table.processRecords(*this);

  // convert result
  std::vector<DstInfo> top_destinations;

  for (const auto& v : grouped_destinations) {
    top_destinations.push_back({v.first, v.second});
  }

  std::sort(top_destinations.begin(), top_destinations.end(),
            [](const DstInfo& a, const DstInfo& b) { return a.counter > b.counter; });

  if (top_destinations.size() > filter_result_limit) {
    top_destinations.resize(filter_result_limit);
  }

  return top_destinations;
}

// -----------------------------------------------------------------
// function that will be called by TableProcessor
// for iterating over all not expired pages in table
// -----------------------------------------------------------------
void TopDstSearchQuery::process_element(const i::DstInfo& current_element) {
  if (filter_locale && locale_value != current_element.locale) {
    return;
  }

  if (filter_departure_date && (current_element.departure_date < departure_date_values.from ||
                                current_element.departure_date > departure_date_values.to)) {
    return;
  }

  // BUILD DESTINATIONS HASH
  grouped_destinations[current_element.destination]++;
}

// -----------------------------------------------------------------
namespace utils {
void print(const i::DstInfo& deal) {
  std::cout << "i::DEAL: " << types::code_to_country(deal.locale) << " "
            << types::code_to_origin(deal.destination) << " "
            << types::int_to_date(deal.departure_date) << std::endl;
}

void print(const DstInfo& deal) {
  std::cout << "DEAL: " << types::code_to_origin(deal.destination) << " " << deal.counter
            << std::endl;
}
}  // i namespace

void unit_test() {
  std::cout << "NO TEST YET" << std::endl;
}
}  // top namespace
