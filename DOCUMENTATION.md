# deals-server Technical Documentation

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Architecture Overview](#2-architecture-overview)
3. [Data Model](#3-data-model)
4. [Shared Memory IPC Architecture](#4-shared-memory-ipc-architecture)
5. [API Reference](#5-api-reference)
6. [Query Parameters Reference](#6-query-parameters-reference)
7. [Query Engine](#7-query-engine)
8. [Custom Response Format](#8-custom-response-format)
9. [Monitoring and Metrics](#9-monitoring-and-metrics)
10. [Build and Deployment](#10-build-and-deployment)
11. [Testing](#11-testing)
12. [Project Structure](#12-project-structure)

---

## 1. Project Overview

**deals-server** is a high-performance, in-memory flight deals server written in C++. It stores flight fare data indexed by origin, destination, date, and price, and serves sub-millisecond search queries over that dataset.

### Purpose

The server is the data store and query engine for flight deal recommendations. It ingests deal records via HTTP POST (each record carries a JSON payload representing a fare/itinerary), indexes the metadata in shared memory, and answers structured search queries — returning the cheapest deal per destination, per date, or per country.

### Tech Stack

| Component | Technology |
|---|---|
| Language | C++11 (compiled with `clang++`) |
| Build flags | `-O3 -Wall -Werror -std=c++0x` |
| IPC / Storage | POSIX shared memory (`shm_open`, `mmap`) |
| I/O model | Single-threaded, `poll()`-based non-blocking TCP |
| HTTP parsing | Custom hand-written HTTP/1.0 parser |
| Synchronization | POSIX named semaphores |
| Metrics | StatsD UDP client |

### Codebase Size

- ~4,436 lines of C++ across 21 `.cpp` and 20 `.hpp` source files
- 224 commits spanning 2016–2023
- No external C++ library dependencies (stdlib only)

### Operational Context

Multiple instances of the binary run concurrently (typically 8), each listening on a distinct port (5000–5007), behind an nginx upstream that load-balances across them. All instances share the same data through POSIX shared memory segments, so any instance can serve any request.

---

## 2. Architecture Overview

### I/O Model

The server is strictly single-threaded. There are no worker threads, no thread pool, and no async framework. All concurrency is handled by a single `poll()` call per event loop iteration, multiplexing the listen socket and all active client connections.

Key TCP server constants (defined in `tcp_server.hpp`):

```
ACCEPT_QUEUE_LENGTH         100      // listen() backlog
MAX_CONNECTION_LIFETIME_SEC 10       // hard connection TTL
MAX_CONNECTION_IDLE_TIME_SEC 2       // idle connection TTL
POLL_TIMEOUT_MS             3000     // poll() timeout
```

### Template Architecture

The TCP server is implemented as a C++ class template `TCPServer<Context>` (in `tcp_server.hpp`). The template parameter `Context` is the per-connection state object. In production use, this is instantiated as:

```
TCPServer<DealsServer::Context>
```

where `DealsServer::Context` holds an `http::HttpParser` instance that accumulates incoming bytes until a complete HTTP request is available.

`DealsServer` derives from `TCPServer<Context>` and overrides two pure virtual methods:

- `on_connect(Connection&)` — called on each new accepted connection
- `on_data(Connection&)` — called each time readable data arrives on a connection socket

### Connection Lifecycle

```
listen socket readable
  -> accept_new_connection()
     -> on_connect()            // initialize per-connection context

client socket readable
  -> network_read()             // recv() into string buffer
  -> on_data()
     -> http.write(data)        // feed bytes into HTTP parser
     -> if bad_request -> 400
     -> if incomplete  -> return (wait for more data)
     -> route by method + path
     -> execute handler
     -> conn.close(response)    // queue response, mark connection for teardown

client socket writable
  -> network_write()            // send() buffered response bytes

poll() timeout / idle / lifetime exceeded
  -> conn.close()               // drop stale connections
```

Connection objects are heap-allocated and tracked in a `std::vector<Connection*>`. Dead connections are reaped on each `process()` iteration.

### HTTP Parsing

The `http::HttpParser` class (in `http.hpp` / `http.cpp`) is a simple stateful parser. It accepts raw bytes through `write()` calls and accumulates them. It detects the end of headers by scanning for `\r\n\r\n`, then reads `Content-Length` bytes for the body.

Parsed data is accessible as:
- `request.method` — `"GET"` or `"POST"`
- `request.uri` — raw URI string
- `request.query.path` — URL path component
- `request.query.params` — key-value map of query string parameters
- `headers` — key-value map of HTTP headers
- `get_body()` — request body as a string

### Request Routing

Routing is performed in `DealsServer::on_data()` by string-matching the parsed path:

```
GET  /deals/top         -> getTop()
GET  /deals/uniqueRoutes -> getUniqueRoutes()
GET  /deals/stats       -> getStats()
GET  /destinations/top  -> getDestiantionsTop()
GET  /deals/clear       -> db.truncate()
GET  /destinations/clear -> db_dst.truncate()
GET  /clear             -> db.truncate() + db_dst.truncate()
GET  /ping              -> "pong"
GET  /quit              -> graceful shutdown
POST /deals/add         -> addDeal()
```

Unmatched paths return HTTP 404.

### Signal Handling

The server installs handlers for `SIGINT`, `SIGTERM`, and `SIGBUS`. On receipt, `gotQuitSignal` is set. The next `process()` call translates this into a graceful shutdown: new requests receive HTTP 503, and the process exits once all active connections are drained. A second signal forces immediate `std::exit(-1)`.

---

## 3. Data Model

### Internal Deal Record: `i::DealInfo`

Defined in `deals_types.hpp` (namespace `deals::i`). This is the struct stored in shared memory pages.

```cpp
struct DealInfo {
  uint32_t timestamp;               // Unix timestamp of when the deal was stored
  uint32_t origin;                  // IATACode encoded as uint32 (4 ASCII bytes packed)
  uint32_t destination;             // IATACode encoded as uint32
  uint32_t departure_date;          // Date encoded as uint32 (YYYYMMDD)
  uint32_t return_date;             // Date encoded as uint32; 0 = one-way
  uint32_t price;                   // Fare price as integer
  uint8_t  stay_days;               // Number of nights (derived from dates)
  uint8_t  destination_country;     // CountryCode encoded as uint8 (index into COUNTRIES array)
  uint8_t  departure_day_of_week;   // Bitmask: bits 0-6 = mon-sun
  uint8_t  return_day_of_week;      // Bitmask: bits 0-6 = mon-sun
  bool     direct;                  // True if no connecting flights
  bool     overriden;               // True if this is the most recent, not cheapest, in its period
  char     page_name[MEMPAGE_NAME_MAX_LEN]; // Shared memory page name where DealData blob lives
  uint32_t index;                   // Element offset within the DealData page
  uint32_t size;                    // Byte length of the DealData blob
};
```

`MEMPAGE_NAME_MAX_LEN` is 20. `page_name`, `index`, and `size` are the three fields needed to retrieve the corresponding opaque JSON blob from the `db_data` table.

### Encoding Conventions

**IATACode** (`uint32_t`): A 3-letter IATA airport code is packed into 4 bytes using `union PlaceCodec`. The string `"MOW"` becomes the integer whose little-endian bytes are `'M', 'O', 'W', 0`. Conversion functions are `origin_to_code(string)` and `code_to_origin(uint32_t)` in `types.hpp`.

**Date** (`uint32_t`): Stored as `YYYYMMDD` — year * 10000 + month * 100 + day. For example, `2023-12-25` = `20231225`. Conversion functions: `date_to_int(string)` and `int_to_date(uint32_t)`.

**CountryCode** (`uint8_t`): Index into the `COUNTRIES` array of 252 ISO 3166-1 alpha-2 codes defined in `types.hpp`. For example, `"RU"` maps to the integer offset of `"RU"` in that array.

**Weekday bitmask** (`uint8_t`): Bit 0 = Monday, bit 1 = Tuesday, ..., bit 6 = Sunday. Multiple days are OR-combined.

### External Deal Record: `DealInfo`

The external-facing deal structure (also in `deals_types.hpp`, namespace `deals`):

```cpp
class DealInfo {
  std::string data;                // Raw JSON blob (the stored POST body)
  std::shared_ptr<DealInfoTest> test; // Non-null only in unit tests
};
```

`DealInfoTest` carries decoded string fields for assertion purposes in tests (origin, destination, dates, price, etc.).

### Tie-Breaking: `findCheapestAndLast`

When the same origin-destination-date combination has multiple stored deals, `deals::utils::findCheapestAndLast(const std::vector<i::DealInfo>& history)` selects the winner:
1. The deal with the lowest `price` wins.
2. If multiple deals share the same lowest price, the one with the highest `timestamp` (most recently stored) wins.

This logic is applied in query post-processing to collapse per-destination or per-date groups.

### Dual-Table Storage

`DealsDatabase` (in `deals_database.hpp`) owns two separate shared memory tables:

| Table | Type | Shared Memory Name | Max Pages | Elements/Page |
|---|---|---|---|---|
| `db_index` | `Table<i::DealInfo>` | `"DealsInfo"` | 5,000 | 10,000 |
| `db_data` | `Table<i::DealData>` (= `Table<uint8_t>`) | `"DealsData"` | 10,000 | 50,000,000 |

`db_index` holds the searchable metadata. `db_data` holds the raw JSON blobs (POST bodies). The two are linked by `(page_name, index, size)` stored in each `i::DealInfo`.

A `SharedContext` named `"Deals"` provides the global `DBContext` (expiration tracking) shared across all process instances.

---

## 4. Shared Memory IPC Architecture

### POSIX Shared Memory

All persistent data lives in POSIX shared memory segments, allocated via `shm_open()` + `mmap()`. This means:
- Data survives server restarts (as long as no `shm_unlink()` is called and the OS has not reclaimed the segments).
- All 8 server instances read and write the same pages concurrently.
- No data is duplicated between instances; each instance maps the same physical memory.

### `SharedMemoryPage<T>`

```cpp
template <typename ELEMENT_T>
class SharedMemoryPage {
  // Opens or creates a named shared memory segment large enough for
  // (sizeof(Page_information) + elements * sizeof(ELEMENT_T)) bytes.
  // mmap()s it into the process address space.
  // Provides getElements() -> ELEMENT_T* pointer into the mapped region.
};
```

Each page carries a small `Page_information` header:
```cpp
struct Page_information {
  bool     unlinked;           // True if this page has been shm_unlink()ed
  uint32_t expiration_check;   // Timestamp; used to detect expired pages
};
```

Page names are ASCII strings up to `MEMPAGE_NAME_MAX_LEN` (20) characters, used as the `shm_open()` path argument.

### `Table<T>`

`Table<T>` is the core data structure. It manages a collection of `SharedMemoryPage<T>` instances and a separate index page (`SharedMemoryPage<TablePageIndexElement>`) that tracks all live pages.

```cpp
struct TablePageIndexElement {
  uint32_t expire_at;                       // Unix timestamp after which this page is expired
  uint32_t page_elements_available;         // How many element slots remain on this page
  char     page_name[MEMPAGE_NAME_MAX_LEN]; // Name of the shared memory page
};
```

Key operations:

- **`addRecord(ELEMENT_T*, size, lifetime_seconds)`**: Finds (or creates) a page with enough free slots, writes the element(s) there, updates the index, returns an `ElementExtractor` reference.
- **`processRecords(TableProcessor<T>&)`**: Iterates over all live (non-expired) pages, calling `process_element()` on each element. This is the hot path for all searches.
- **`cleanup()`**: Reclaims expired pages. Up to `MEMPAGE_REMOVE_EXPIRED_PAGES_AT_ONCE` (5) pages are unlinked per call, with a minimum delay of `MEMPAGE_REMOVE_EXPIRED_PAGES_DELAY_SEC` (60 seconds) between cleanup sweeps.

### Page Lifecycle

```
1. ALLOCATE  -- Table locates a page with free capacity (or creates a new one)
2. FILL      -- addRecord() writes elements into the page
3. USE       -- processRecords() iterates elements for query processing
4. EXPIRE    -- expire_at timestamp passes; page is no longer iterated
5. RECLAIM   -- cleanup() calls shm_unlink(); memory is released by OS
               when all processes unmap it
```

Low-memory behavior:
- If free system memory falls below `LOWMEM_PERCENT_FOR_PAGE_REUSING` (15%), the oldest expired page may be reused instead of allocating a new segment.
- If free memory falls below `LOWMEM_ERROR_PERCENT` (10%), an error is logged.

The function `isMemAvailable()` and `isMemLow()` check `/proc/meminfo` (Linux) for these thresholds.

### Data Expiry

Both deal tables use `DEALS_EXPIRES = 60 * 60 * 24` (86,400 seconds = 24 hours) as the default record lifetime. Pages are expired as a unit: once the last element written to a page would expire, the whole page is eligible for cleanup.

The `global_expire_at` field in `SharedContext::shm` (a `DBContext` in its own named shared memory segment) tracks a server-wide minimum expiry timestamp used to gate page cleanup operations.

### Cross-Process Locking

`locks::CriticalSection` (in `locks.hpp` / `locks.cpp`) wraps a POSIX named semaphore:

```cpp
class CriticalSection {
  CriticalSection(std::string name);  // sem_open("/name", ...)
  void enter();                       // sem_timedwait() with 30-second timeout
  void exit();                        // sem_post()
};
```

Each `Table<T>` instance holds one `CriticalSection` initialized with the table name. The lock is acquired around index reads and writes to ensure that concurrent processes see a consistent view of the page index. Element writes within an already-allocated page slot are not additionally locked (slots are claimed atomically via the index lock).

`AutoCloser` is an RAII wrapper that calls `exit()` on scope exit.

### `ElementExtractor<T>`

Returned by `addRecord()`, this object provides deferred access to a stored element's data:

```cpp
template <typename ELEMENT_T>
class ElementExtractor {
  ELEMENT_T* get_element_data(); // Opens the page by name and returns a pointer
  const std::string page_name;
  const uint32_t    index;
  const uint32_t    size;
};
```

`DealsDatabase::fill_deals_with_data()` uses `ElementExtractor` to read back JSON blobs from `db_data` pages and attach them to `DealInfo` objects returned to callers.

---

## 5. API Reference

All endpoints listen on the TCP port passed as a command-line argument. The protocol is HTTP/1.0.

---

### `POST /deals/add`

Stores a new deal record.

**Required query parameters:**

| Parameter | Type | Description |
|---|---|---|
| `origin` | IATACode | Origin airport (e.g., `MOW`) |
| `destination` | IATACode | Destination airport (e.g., `BER`) |
| `departure_date` | Date | Departure date (`YYYY-MM-DD`) |
| `price` | Number | Integer fare price |
| `locale` | CountryCode | Locale/market code (e.g., `ru`) |
| `destination_country` | CountryCode | Destination country ISO code |
| `direct_flight` | Boolean | `true` or `false` |

**Optional query parameters:**

| Parameter | Type | Description |
|---|---|---|
| `return_date` | Date | Return date for roundtrip deals |

**Request body:** Arbitrary byte payload (the JSON itinerary blob). Stored verbatim and returned in search results.

**Response:**
- `200 OK` — body: `"Well done\n"`
- `400 Bad Request` — on missing required parameters, origin == destination, or departure > return date

**Side effects:**
- Writes one `i::DealInfo` record to `db_index` (`DealsInfo` shared memory).
- Writes the request body as raw bytes to `db_data` (`DealsData` shared memory).
- Registers the destination in `TopDstDatabase` for the given locale.

---

### `GET /deals/top`

Main search endpoint. Returns the cheapest deals matching the filter criteria.

**Required query parameters:**

| Parameter | Type | Description |
|---|---|---|
| `origin` | IATACode | Origin airport |

**Optional query parameters:** See [Section 6](#6-query-parameters-reference) for the full list.

**Behavior:**
- Default (no grouping flags): executes `SimplyCheapest` query — one cheapest deal per destination, sorted by price ascending.
- `group_by_date=true`: executes `CheapestByDay` query — one cheapest deal per departure date (or departure+return date combination).
- `group_by_country=true`: executes `CheapestByCountry` query — one cheapest deal per destination country, sorted by price ascending.
- `add_locale_top=true` + `locale=XX`: pre-populates the `destinations` filter from `TopDstDatabase` before executing the main search.

**Response:**
- `200 OK` — Content-Type: `application/octet-stream`. Custom binary format; see [Section 8](#8-custom-response-format).
- `204 No Content` — no deals matched the query.
- `400 Bad Request` — invalid date parameter combinations.

---

### `GET /deals/uniqueRoutes`

Returns all unique origin-destination route pairs currently in the database.

**No query parameters.**

**Response:**
- `200 OK` — Content-Type: `text/plain`. Each line: `ORIGIN-DESTINATION` (CSV-style, one route per line).
- `204 No Content` — database is empty.

---

### `GET /deals/stats`

Returns database statistics as a JSON-like text payload.

**No query parameters.**

**Response:**
- `200 OK` — Content-Type: `text/plain`. Contains: element count, total size in bytes, minimum timestamp, maximum timestamp, and per-route counts.
- `204 No Content` — database is empty.

---

### `GET /destinations/top`

Returns the most popular destinations for a given locale, ordered by deal count.

**Required query parameters:**

| Parameter | Type | Description |
|---|---|---|
| `locale` | CountryCode | Market locale code (e.g., `ru`) |

**Optional query parameters:**

| Parameter | Type | Description |
|---|---|---|
| `departure_date_from` | Date | Filter by departure date range start |
| `departure_date_to` | Date | Filter by departure date range end |
| `destinations_limit` | Number | Maximum number of destinations to return |

**Response:**
- `200 OK` — Content-Type: `text/plain`. Each line: `IATA_CODE;COUNT\n`.
- `204 No Content` — no destinations found for this locale.

Results are served from an in-process `Cache<std::vector<DstInfo>>` keyed by locale. Cache TTL matches `DEALS_EXPIRES` (24 hours).

---

### `GET /deals/clear`

Truncates the deals database (both `db_index` and `db_data` shared memory tables).

**Response:** `200 OK` — body: `"deals cleared\n"`

---

### `GET /destinations/clear`

Truncates the top-destinations database.

**Response:** `200 OK` — body: `"destinations cleared\n"`

---

### `GET /clear`

Truncates all databases (deals + destinations).

**Response:** `200 OK` — body: `"ALL cleared\n"`

---

### `GET /ping`

Health check endpoint.

**Response:** `200 OK` — body: `"pong\n"`

---

### `GET /quit`

Initiates graceful shutdown. Sets `quit_request = true`. The server stops accepting new requests (returns 503 to subsequent requests) and exits once all active connections have been closed.

**Response:** `200 OK` — body: `"quiting...\n"`

---

## 6. Query Parameters Reference

All parameters are passed as URL query string key-value pairs. Parsing is handled by `URIQueryParams` in `http.hpp`. Type validation and decoding are handled by the `types::` wrapper classes.

| Parameter | Type | Applies To | Description |
|---|---|---|---|
| `origin` | IATACode | `/deals/top` (required) | Origin airport code, e.g. `MOW` |
| `destinations` | IATACodes | `/deals/top` | Comma-separated destination airport codes to restrict results to, e.g. `MAD,BER,LAX` |
| `destination_countries` | CountryCodes | `/deals/top` | Comma-separated ISO country codes to restrict results to, e.g. `ES,DE` |
| `departure_date_from` | Date | `/deals/top`, `/destinations/top` | Earliest acceptable departure date, `YYYY-MM-DD` |
| `departure_date_to` | Date | `/deals/top`, `/destinations/top` | Latest acceptable departure date, `YYYY-MM-DD` |
| `return_date_from` | Date | `/deals/top` | Earliest acceptable return date |
| `return_date_to` | Date | `/deals/top` | Latest acceptable return date |
| `departure_days_of_week` | Weekdays | `/deals/top` | Comma-separated weekday names: `mon,tue,wed,thu,fri,sat,sun`. Filters by departure day. |
| `return_days_of_week` | Weekdays | `/deals/top` | Comma-separated weekday names. Filters by return day. |
| `stay_from` | Number | `/deals/top` | Minimum stay duration in days |
| `stay_to` | Number | `/deals/top` | Maximum stay duration in days |
| `timelimit` | Number | `/deals/top` | Discard deals older than this many seconds (relative to current time) |
| `deals_limit` | Number | `/deals/top` | Maximum number of results to return (default: 20) |
| `direct_flights` | Boolean | `/deals/top` | `true` = return only direct flights; `false` = return only connecting |
| `roundtrip_flights` | Boolean | `/deals/top` | `true` = return only roundtrip deals; `false` = return only one-way |
| `locale` | CountryCode | `/deals/top`, `/destinations/top` | Market locale code, e.g. `ru` |
| `add_locale_top` | Boolean | `/deals/top` | `true` = pre-fill `destinations` from `TopDstDatabase` for the given locale |
| `departure_or_return_date` | Date | `/deals/top` | Exact date that must match either the departure or return date |
| `group_by_date` | Boolean | `/deals/top` | `true` = group by departure date (CheapestByDay mode) |
| `group_by_country` | Boolean | `/deals/top` | `true` = group by destination country (CheapestByCountry mode) |
| `all_combinations` | Boolean | `/deals/top` | Used with `group_by_date=true`: `true` = group by full departure+return date combination key |
| `destinations_limit` | Number | `/destinations/top` | Maximum destinations to return from top destinations query |

### Type Details

**IATACode**: Three uppercase ASCII letters packed into a `uint32_t`. Parsing is case-insensitive; the value is normalized internally.

**Date**: Accepts `YYYY-MM-DD` format. Internally stored as `uint32_t` (`YYYYMMDD`). Invalid dates result in a `400 Bad Request`.

**Weekdays**: Accepts a comma-separated list of three-letter day abbreviations (`mon`, `tue`, `wed`, `thu`, `fri`, `sat`, `sun`). Converted to a `uint8_t` bitmask.

**Boolean**: Accepts `"true"` or `"false"` (case-insensitive). The `isDefined()` / `isUndefined()` methods on `Optional<Boolean>` distinguish between "not provided" and "provided as false".

**Number**: Parsed as `uint32_t`. Invalid or missing values produce 0.

---

## 7. Query Engine

### Class Hierarchy

```
query::SearchQuery          (search_query.hpp)
  |-- deals::DealsSearchQuery  (deals_query.hpp)
        |-- deals::SimplyCheapest        (deals_cheapest.hpp)
        |-- deals::CheapestByDay         (deals_cheapest_by_date.hpp)
        |-- deals::CheapestByCountry     (deals_cheapest_by_country.hpp)

shared_mem::TableProcessor<i::DealInfo>
  |-- deals::DealsSearchQuery
  |-- deals::UniqueProcessor   (deals_unique_routes.hpp)
  |-- deals::StatsProcessor    (deals_stats.hpp)
  |-- top::TopDstSearchQuery   (top_destinations.hpp)
```

### `SearchQuery` Base Class

Stores all filter parameters as protected member variables with corresponding `bool filter_*` flags. Setter methods are called by `DealsDatabase::searchFor<QueryClass>()` before `execute()` is invoked. Notable members:

```cpp
uint32_t  origin_value;
unordered_set<uint32_t> destination_values_set;
unordered_set<uint8_t>  destination_country_set;
DateInterval  departure_date_values;      // {from, to, duration}
DateInterval  return_date_values;
DateValue     exact_date_value;
bool          direct_flights_flag;
bool          roundtrip_flight_flag;
uint8_t       departure_weekdays_bitmask;
uint8_t       return_weekdays_bitmask;
StayInterval  stay_days_values;           // {from, to}
uint16_t      filter_result_limit;        // default 20
uint8_t       locale_value;
bool          filter_all_combinations;
```

### `DealsSearchQuery` and `process_element()`

`DealsSearchQuery` extends both `SearchQuery` (filter parameters) and `TableProcessor<i::DealInfo>` (iteration callback). When `execute()` is called, it invokes `table.processRecords(*this)`, which calls `process_element()` for every live element in the shared memory index.

`process_element()` applies all enabled filters in sequence and short-circuits on the first mismatch:

1. **Expiration check**: `timestamp >= min_timestamp` (enforced via `timelimit` parameter)
2. **Origin check**: `element.origin == origin_value`
3. **Destination set**: element's destination is in `destination_values_set` (if filter is active)
4. **Destination country set**: element's `destination_country` is in `destination_country_set`
5. **Departure date range**: `departure_date_from <= element.departure_date <= departure_date_to`
6. **Return date range**: `return_date_from <= element.return_date <= return_date_to`
7. **Exact date**: `element.departure_date == exact_date_value || element.return_date == exact_date_value`
8. **Departure weekday bitmask**: `element.departure_day_of_week & departure_weekdays_bitmask != 0`
9. **Return weekday bitmask**: `element.return_day_of_week & return_weekdays_bitmask != 0`
10. **Stay days range**: `stay_from <= element.stay_days <= stay_to`
11. **Direct flight flag**: `element.direct == direct_flights_flag`
12. **Roundtrip flag**: `element.return_date != 0` matches `roundtrip_flight_flag`

Elements that pass all active filters are forwarded to `process_deal()` in the derived class.

### Query Types

#### `SimplyCheapest`

Default query mode. Groups deals by destination (`uint32_t` code). For each destination, maintains:
- A map `grouped_destinations: destination -> i::DealInfo` storing the current cheapest deal.
- A map `grouped_destinations_hist: destination -> vector<i::DealInfo>` as a history for tie-breaking.

In `post_search()`, calls `findCheapestAndLast()` on each destination's history vector to resolve ties, then sorts the final result vector by `price` ascending.

Result: one deal per unique destination, sorted cheapest first.

#### `CheapestByDay`

Activated by `group_by_date=true`. Groups by a date key:
- Default: group key = `departure_date`
- If both departure and return date ranges are specified (i.e., roundtrip context without `all_combinations`): group key = `departure_date`
- With `all_combinations=true`: group key = a composite of `departure_date` and `return_date`

Internal state: `grouped_by_date: date_key -> i::DealInfo` and a history map for tie-breaking. `post_search()` resolves each group via `findCheapestAndLast()` and sorts by price.

Result: one deal per date key, sorted cheapest first.

#### `CheapestByCountry`

Activated by `group_by_country=true`. Groups by `destination_country` (`uint8_t`). Internal state: `grouped_by_country: country_code -> i::DealInfo` and history map.

`post_search()` resolves each country group via `findCheapestAndLast()` and sorts by price.

Result: one deal per destination country, sorted cheapest first.

#### `UniqueRoutes`

Implemented as `UniqueProcessor` (a direct `TableProcessor<i::DealInfo>` subclass, not a `DealsSearchQuery`). No filter parameters. Iterates all live elements and inserts `origin + destination` pairs into `grouped_by_routes: uint64_t -> i::DealInfo` where the key is `(origin << 32) | destination`.

`getStringResults()` formats the output as CSV text: one `ORIGIN,DESTINATION` pair per line.

#### `StatsProcessor`

Direct `TableProcessor<i::DealInfo>` subclass. Counts `elements`, accumulates `size`, and tracks `min`/`max` timestamp values across all live records. Also maintains a `group_by_route: string -> uint32_t` count map.

`getStringResults()` returns a JSON-like text document with the aggregate statistics.

#### `TopDestinations` (`TopDstSearchQuery`)

Operates on `Table<i::DstInfo>` (the top-destinations shared memory table, separate from deals). Each `i::DstInfo` record stores `{locale, destination, departure_date}`. The query filters by locale and optionally departure date range, then counts occurrences per destination IATA code.

Results are cached in-process in `TopDstDatabase::result_cache_by_locale`: an `unordered_map<uint8_t, Cache<vector<DstInfo>>>` keyed by locale code. On cache hit (non-expired), the cached vector is returned directly without scanning shared memory. Cache TTL is `DEALS_EXPIRES` (24 hours).

### Search Dispatch

`DealsDatabase::searchFor<QueryClass>(...)` is a function template that:
1. Constructs a `QueryClass` instance bound to `db_index`.
2. Calls all `query.*()` setter methods with the provided parameter objects.
3. Calls `query.execute()`, which triggers `table.processRecords()`.
4. Passes the resulting `vector<i::DealInfo>` through `fill_deals_with_data()` to load JSON blobs from `db_data` pages.
5. Returns `vector<DealInfo>` to the calling handler.

---

## 8. Custom Response Format

`/deals/top` does not return plain JSON. Each deal's JSON blob was independently zlib-compressed when stored, and the response frames multiple compressed blocks in a custom binary envelope.

### Wire Format

```
<size_info><block_0><block_1>...<block_N>
```

**size_info** is an ASCII string terminated by the last semicolon before the first data block:

```
{total_size_info_len};{len_0};{len_1};...;{len_N};
```

- `total_size_info_len` — decimal integer: the total byte length of the entire `size_info` string (including the leading length field and its own semicolon separator).
- `len_i` — decimal integer: the byte length of `block_i`.
- Each field is separated by `;`. The string ends with a trailing `;`.

**Blocks** immediately follow the size_info string. Each block is a zlib-compressed (`DEFLATE`) byte sequence that decompresses to a JSON object.

### Example

A response with two deals of sizes 121 and 45 bytes:

```
17;121;45;{zlib_block_0}{zlib_block_1}
```

The `size_info` string is `"17;121;45;"` (10 characters), so `total_size_info_len = 17` (to account for the `"17;"` prefix itself being 3 characters: 3 + 14 = 17 — the server does a correction pass to handle this self-referential length).

### Decoding Algorithm (from `test/decode.js`)

```
1. Scan the response bytes for the first ';' byte (0x3B).
2. Parse the integer preceding it as info_length.
3. Extract the first info_length bytes as the size_info string.
4. Split size_info on ';'; first token is info_length (already consumed),
   remaining tokens are the individual block sizes.
5. Set pointer = info_length.
6. For each block size s[i]:
     block[i] = response_bytes[pointer .. pointer + s[i]]
     pointer += s[i]
7. zlib.inflate(block[i]) -> JSON text
8. JSON.parse(JSON text) -> deal object
```

### Empty Result

If the query returns no results, the server responds with `204 No Content` and `Content-Length: 0`. Clients must handle this case before attempting to decode.

### HTTP Headers for `/deals/top` Response

```
HTTP/1.0 200 OK
Content-Type: application/octet-stream
Content-Length: <total byte length of size_info + all blocks>
```

---

## 9. Monitoring and Metrics

### StatsD Client

`statsd::Client` (in `statsd_client.hpp` / `statsd_client.cpp`) is a UDP-based StatsD client. A global singleton `statsd::metric` is available throughout the codebase.

Default configuration: host `127.0.0.1`, port `8125`.

```cpp
class Client {
  void config(const std::string& host, int port, const std::string& ns = "");
  int inc(const std::string& key, const Tags& tags = {}, float sample_rate = 1.0);
  int dec(const std::string& key, const Tags& tags = {}, float sample_rate = 1.0);
  int count(const std::string& key, int32_t value, const Tags& tags = {});
  int gauge(const std::string& key, int32_t value, const Tags& tags = {});
  int timing(const std::string& key, int32_t ms, const Tags& tags = {});
};
```

`Tags` is a `map<string, string>` passed as Datadog-style tag annotations.

### Emitted Metrics

| Metric | Type | Description |
|---|---|---|
| `dealsrv.page_use` | gauge | Percentage of shared memory page capacity currently utilized |
| `dealsrv.shmem_free` | gauge | Percentage of free system memory available |

These are reported from `shared_memory.cpp` during `reportMemUsage()`, called each time a new page is allocated.

### Timing Utilities

`timing.hpp` / `timing.cpp` provide:

**`timing::getTimestampMs()`** — Returns current Unix time in milliseconds (`long long`).

**`timing::getTimestampSec()`** — Returns current Unix time in seconds (`uint32_t`). Used for record expiration and connection lifetime checks.

**`timing::Timer`** — Multi-point latency measurement tool:
```cpp
Timer t("request_handler");
t.tick("after_parse");
t.tick("after_search");
t.finish("done");
t.report(); // prints all intervals to stdout
```

**`timing::TimeLord`** — Test utility for simulating the passage of time without sleeping. Wraps a counter that advances time by `1/ticks_in_second` seconds per increment. Used in unit tests to expire pages and records deterministically.

```cpp
TimeLord tl(10); // 10 ticks per second
++tl;            // advance time by 0.1 seconds
tl += 600;       // advance time by 60 seconds
```

---

## 10. Build and Deployment

### Build System

The project uses a hand-written `Makefile`. All `.cpp` files under `src/` are compiled individually and linked into a single binary.

```
make          # builds bin/deals-server
make clean    # removes build/ and bin/deals-server
make tester   # builds test/tester.cpp -> bin/tester
make install  # installs binary to $(PREFIX)/bin/ (default: /usr/local/bin/)
```

**Compiler:** `clang++`

**Compiler flags:** `-O3 -Wall -Werror -std=c++0x`

**Platform-specific linking:**
- Linux: `-lrt -pthread` (for `shm_open` and POSIX semaphores)
- macOS: no extra libs (semaphores and shared memory available in standard libc)

**Output binary:** `bin/deals-server`

**Object files:** `build/*.o`

### System Dependencies

From `script/host_setup.txt`:

```bash
sudo apt-get install build-essential
sudo apt-get install make
sudo apt-get install clang-3.8
sudo apt-get install nginx
sudo apt-get install runit       # process supervision
```

### Binary Invocation

```
bin/deals-server <host> <port>

# Example:
bin/deals-server 0.0.0.0 5000
```

Special modes:
```
bin/deals-server test   # runs built-in unit tests
bin/deals-server stat   # sends a test StatsD metric and exits
```

### Deployment Model

The standard production deployment runs 8 instances on a single host, listening on ports 5000–5007. nginx sits in front and load-balances with the `least_conn` algorithm.

**Start/stop script:** `script/deals.sh`

```bash
# Start instances 0 through 7 (ports 5000-5007):
./script/deals.sh start 0 7

# Stop all instances:
./script/deals.sh stop
```

The script uses `chpst` (from `runit`) to set per-process file descriptor limits (`-o 8000`) and manage process identity (`-u zubkov`). Stdout and stderr are appended to `/tmp/deals-server.log`.

**nginx upstream configuration** (`/etc/nginx/sites-enabled/default`):

```nginx
upstream deals_server {
    least_conn;
    server 127.0.0.1:5000;
    server 127.0.0.1:5001;
    server 127.0.0.1:5002;
    server 127.0.0.1:5003;
    server 127.0.0.1:5004;
    server 127.0.0.1:5005;
    server 127.0.0.1:5006;
    server 127.0.0.1:5007;
}

location / {
    proxy_set_header X-Real-IP $remote_addr;
    proxy_pass http://deals_server;
    proxy_next_upstream error timeout http_502 http_503;
}
```

The `proxy_next_upstream` directive retries failed requests on the next backend, providing resilience against individual instance crashes.

### Debian Packaging

The `debian/` directory contains a standard `debhelper` package specification:
- `debian/control` — source/binary package metadata; build dependency on `clang`
- `debian/rules` — build rules
- `debian/changelog` — version history
- `debian/compat` — debhelper compatibility level

Package name: `deals-server`. Build with `dpkg-buildpackage`.

---

## 11. Testing

### Built-in Unit Tests

Unit tests are compiled into the main binary and run via:

```
bin/deals-server test
```

This executes four test suites:

| Function | Location | Tests |
|---|---|---|
| `http::unit_test()` | `http.cpp` | HTTP parser correctness |
| `deals::unit_test()` | `deals_database.cpp` | Deal insertion, search queries, expiration |
| `timing::unit_test()` | `timing.cpp` | Timestamp functions, `TimeLord` behavior |
| `locks::unit_test()` | `locks.cpp` | Semaphore acquire/release, `AutoCloser` |

Before running tests, the test harness resets the named semaphores (`"DealsInfo"`, `"DealsData"`, `"TopDst"`) via `CriticalSection::reset_not_for_production()` to ensure a clean state.

Tests use `DealInfoTest` structs and `TimeLord` to simulate time advancement and verify that records expire correctly and that queries return expected results.

### Load Test: `test/bench.js`

A Node.js script that sends concurrent HTTP requests to measure server throughput. Useful for baseline performance benchmarking before and after changes.

### Response Decoder: `test/decode.js`

A Node.js script that reads the raw binary response from stdin (e.g., piped from `curl`) and decodes it:

```bash
curl deals:8090/deals/top?origin=MOW | node test/decode.js
```

It implements the full decoding pipeline:
1. Parse `size_info` header
2. Split into per-block byte ranges
3. `zlib.inflate()` each block
4. `JSON.parse()` each decompressed block
5. Prints a human-readable summary: `ORIGIN-DESTINATION:COUNTRY PRICE SEGMENTS TIMESTAMP`

### Log Analyzer: `test/dealstat.js`

A Node.js script that reads server log output and computes statistics: request rates, error frequencies, and latency distributions.

---

## 12. Project Structure

```
deals-server/
├── Makefile                     # Build rules; targets: all, clean, tester, install
├── README                       # Brief project description
├── monitoring.txt               # Notes on monitoring setup
├── bin/                         # Compiled binary output directory
│   └── deals-server             # Production binary
├── build/                       # Intermediate object files (*.o)
├── debian/                      # Debian package metadata
│   ├── control                  # Package name, dependencies, maintainer
│   ├── rules                    # dpkg build rules
│   ├── changelog                # Package version history
│   └── compat                   # debhelper compatibility level
├── script/
│   ├── deals.sh                 # Start/stop script for 8-instance deployment
│   └── host_setup.txt           # System dependency installation commands and nginx config
├── spike/                       # Experimental/scratch code (not production)
├── test/
│   ├── bench.js                 # Node.js HTTP load test
│   ├── decode.js                # Node.js binary response decoder (stdin -> human-readable)
│   └── dealstat.js              # Node.js log analyzer
└── src/
    ├── deals_server.cpp         # Main entry point; signal handling; HTTP routing; request handlers
    ├── deals_server.hpp         # DealsServer class declaration; Context type
    ├── deals_database.cpp       # DealsDatabase: addDeal(), fill_deals_with_data(), getStats(), truncate()
    ├── deals_database.hpp       # DealsDatabase class; searchFor<QueryClass>() template
    ├── deals_types.hpp          # i::DealInfo struct; DealInfo class; DealInfoTest; findCheapestAndLast()
    ├── deals_types.cpp          # utils::print(), sprint(), equal(), findCheapestAndLast() implementation
    ├── deals_query.hpp          # DealsSearchQuery base class; process_element() filter chain
    ├── deals_query.cpp          # DealsSearchQuery::execute() implementation
    ├── deals_cheapest.hpp       # SimplyCheapest: group by destination, sort by price
    ├── deals_cheapest.cpp       # SimplyCheapest implementation
    ├── deals_cheapest_by_date.hpp   # CheapestByDay: group by date key
    ├── deals_cheapest_by_date.cpp   # CheapestByDay implementation
    ├── deals_cheapest_by_country.hpp # CheapestByCountry: group by destination country
    ├── deals_cheapest_by_country.cpp # CheapestByCountry implementation
    ├── deals_unique_routes.hpp  # UniqueProcessor: collect unique origin-destination pairs
    ├── deals_unique_routes.cpp  # UniqueProcessor implementation; CSV output
    ├── deals_stats.hpp          # StatsProcessor: element count, size, timestamp min/max
    ├── deals_stats.cpp          # StatsProcessor implementation; JSON text output
    ├── deals_test.cpp           # C++ unit tests (http, deals, timing, locks)
    ├── top_destinations.hpp     # TopDstDatabase; TopDstSearchQuery; DstInfo structs; Cache integration
    ├── top_destinations.cpp     # TopDstDatabase implementation; getCachedResult(); addDestination()
    ├── shared_memory.hpp        # SharedMemoryPage<T>; Table<T>; TableProcessor<T>; ElementExtractor<T>
    ├── shared_memory.cpp        # isMemAvailable(); isMemLow(); reportMemUsage(); SharedContext
    ├── shared_memory.tpp        # Table<T> template method implementations (included by shared_memory.hpp)
    ├── cache.hpp                # Cache<T>: TTL-based in-process value cache (header-only template)
    ├── tcp_server.hpp           # TCPServer<Context> template; TCPConnection; poll() event loop
    ├── tcp_server.cpp           # TCPConnection methods; inet_addr_to_string()
    ├── http.hpp                 # HttpParser; HttpRequest; HttpHeaders; URIQueryParams; HttpResponse
    ├── http.cpp                 # HTTP parser implementation; unit_test()
    ├── locks.hpp                # CriticalSection (named semaphore); AutoCloser RAII wrapper
    ├── locks.cpp                # CriticalSection implementation; sem_timedwait() (macOS compat)
    ├── search_query.hpp         # SearchQuery base: all filter parameters and setter methods
    ├── search_query.cpp         # SearchQuery setter implementations
    ├── statsd_client.hpp        # statsd::Client UDP StatsD emitter; statsd::metric singleton
    ├── statsd_client.cpp        # Client::send_to_daemon(); metric format builders
    ├── timing.hpp               # getTimestampMs(); getTimestampSec(); Timer; TimeLord
    ├── timing.cpp               # Timing implementation; debug_time_shift; unit_test()
    ├── types.hpp                # IATACode; CountryCode; Date; Weekdays; Number; Boolean;
    │                            # Required<T>; Optional<T>; ObjectMap; Error; COUNTRIES array
    ├── types.cpp                # Type parsing and codec implementations (origin_to_code, etc.)
    ├── utils.hpp                # split_string(); day_of_week(); date arithmetic utilities
    └── utils.cpp                # Utility function implementations
```
