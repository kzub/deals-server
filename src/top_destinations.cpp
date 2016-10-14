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
//
// -----------------------------------------------------------------
bool TopDstDatabase::addDestination(std::string locale, std::string destination,
                                    std::string departure_date) {
  uint32_t departure_date_int = query::date_to_int(departure_date);
  if (departure_date_int == 0) {
    std::cout << "addDestination() wrong departure date:" << departure_date << std::endl;
    return false;
  }

  i::DstInfo info;
  info.locale = query::locale_to_code(locale);
  info.destination = query::origin_to_code(destination);
  info.departure_date = departure_date_int;

  // Secondly add deal to index, include data position information
  auto di_result = db_index->addRecord(&info);
  if (di_result.error != shared_mem::ErrorCode::NO_ERROR) {
    std::cout << "ERROR addDestination():" << (int)di_result.error << std::endl;
    return false;
  }

  return true;
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
std::vector<DstInfo> TopDstDatabase::getCachedResult(std::string locale,
                                                     std::string departure_date_from,
                                                     std::string departure_date_to,
                                                     uint16_t limit) {
  if (departure_date_from.size() > 0 || departure_date_to.size() > 0) {
    // std::cout << "[CACHE] date range exists:" << locale << std::endl;
    return {};
  }
  auto cache = result_cache_by_locale.find(locale);

  if (cache == result_cache_by_locale.end()) {
    // std::cout << "[CACHE] not found:" << locale << std::endl;
    return {};
  }
  // std::cout << "[CACHE] found:" << locale << std::endl;

  if (cache->second.is_expired()) {
    result_cache_by_locale.erase(cache);
    // std::cout << "[CACHE] erase:" << locale << std::endl;
    return {};
  }

  std::vector<DstInfo>&& result = cache->second.get_value();
  if (result.size() < limit) {
    result_cache_by_locale.erase(cache);
    // std::cout << "[CACHE] size:" << result.size() << " < limit:" << limit << std::endl;
    return {};
  }

  // std::cout << "[CACHE] use result:"  << locale << std::endl;
  return result;
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
void TopDstDatabase::saveResultToCache(std::string locale, std::vector<DstInfo>& result) {
  // one minute top destinations cache:
  if (result.size() > 0) {
    auto cached_result = TopDstDatabase::CachedResult{result, 60};
    result_cache_by_locale.emplace(locale, cached_result);
    // result_cache_by_locale.insert(std::pair<std::string, TopDstDatabase::CachedResult>(locale,
    // cached_result));
    // result_cache_by_locale.insert(
    // std::pair<decltype(locale), decltype(cached_result)>({locale, cached_result}));
    // std::cout << "[CACHE] write result" << locale << std::endl;
  }
}

// -----------------------------------------------------------------
//
// -----------------------------------------------------------------
std::vector<DstInfo> TopDstDatabase::getLocaleTop(std::string locale,
                                                  std::string departure_date_from,
                                                  std::string departure_date_to, uint16_t limit) {
  auto result = getCachedResult(locale, departure_date_from, departure_date_to, limit);
  if (result.size() > 0) {
    return result;
  }

  TopDstSearchQuery query(*db_index);

  query.locale(locale);
  query.departure_dates(departure_date_from, departure_date_to);
  query.result_limit(limit);

  result = query.exec();

  saveResultToCache(locale, result);
  return result;
}

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

  if (top_destinations.size() > filter_limit) {
    top_destinations.resize(filter_limit);
  }

  // for (auto dst : top_destinations) { utils::print(dst); }
  return top_destinations;
}

/* function that will be called by TableProcessor
      *  for iterating over all not expired pages in table */
void TopDstSearchQuery::process_element(const i::DstInfo& current_element) {
  // ******************************************************************
  // FILTERING OUT AREA
  // ******************************************************************

  // if departure date interval provided let's look it matches
  // --------------------------------
  if (filter_locale) {
    if (locale_value != current_element.locale) {
      // std::cout << "filter_locale" << std::endl;
      return;
    }
  }

  // if departure date interval provided let's look it matches
  // --------------------------------
  if (filter_departure_date) {
    if (current_element.departure_date < departure_date_values.from ||
        current_element.departure_date > departure_date_values.to) {
      // std::cout << "filter_departure_date" << std::endl;
      return;
    }
  }

  // **********************************************************************
  // BUILD DESTINATIONS HASH AREA
  // **********************************************************************
  grouped_destinations[current_element.destination]++;
}

namespace utils {
void print(const i::DstInfo& deal) {
  std::cout << "i::DEAL: " << query::code_to_locale(deal.locale) << " "
            << query::code_to_origin(deal.destination) << " "
            << query::int_to_date(deal.departure_date) << std::endl;
}

void print(const DstInfo& deal) {
  std::cout << "DEAL: " << query::code_to_origin(deal.destination) << " " << deal.counter
            << std::endl;
}
}  // i namespace

void unit_test() {
  std::cout << "NO TEST YET" << std::endl;
}
}  // top namespace
