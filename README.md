# Prediction Market Arbitrage Engine

A C++20 student project that monitors binary prediction markets on [Polymarket](https://polymarket.com), maintains live YES/NO order books, and reports simulated buy-side arbitrage opportunities.

> **Important:** This project is an educational market-monitoring tool. It does not connect to a wallet, authenticate a trader, submit orders, or guarantee that a displayed quote is still available. Live prices can change before an opportunity can be acted on.

## What it does

For each binary market, the engine looks for a pair of asks satisfying:

```text
YES ask + NO ask < $1.00
```

Buying one YES share and one NO share guarantees one winning share at settlement, with a maximum gross payout of `$1.00`. For example:

```text
Buy YES @ $0.42
Buy NO  @ $0.55
Total cost: $0.97
Gross profit: $0.03 per pair
```

The engine also:

- Loads active markets from the Polymarket Gamma API.
- Fetches initial order-book snapshots from the CLOB REST API.
- Uses multiple workers to initialize books in parallel.
- Subscribes to live market updates over a secure WebSocket connection.
- Walks multiple ask levels to estimate executable quantity and slippage.
- Limits execution to the quantity available on both YES and NO books.
- Ranks opportunities by net profit, return on capital, and quantity.
- Prints the market question, token IDs, and a canonical Polymarket URL.
- Suppresses identical opportunity messages so the terminal does not become a bottleneck.
- Resynchronizes books from REST after a WebSocket disconnect and reconnects with exponential backoff.

## Project status

This is a simulation and monitoring project. The current default fee model is a configurable flat rate and is set to `0.0` in `main.cpp`; real trading costs, slippage, order matching, and quote staleness should be considered before treating an opportunity as executable.

## Architecture

```text
Gamma API
   |
   v
MarketLoader -----> Market objects
                         |
CLOB REST API ----------> Initial OrderBook snapshots
                         |
CLOB WebSocket ---------> Incremental book updates
                         |
                         v
                  ArbitrageEngine
                   /      |      \
                  /       |       \
       OrderBook   Detector   Simulator
                         |
                         v
              OpportunityRanker + CLI reporter
```

### Main components

| Component | Responsibility |
|---|---|
| `MarketLoader` | Fetches and parses active binary markets, including token IDs and URL slugs. |
| `ClobRestClient` | Performs HTTPS requests to Gamma/CLOB REST endpoints and parses JSON responses. |
| `OrderBook` | Stores bids and asks, applies snapshots and price changes, and calculates simulated fills. |
| `ArbitrageDetector` | Checks whether the two sides of a binary market are profitable at the requested quantity. |
| `ExecutionSimulator` | Estimates executable quantity, cost, slippage, fees, payout, and return on capital. |
| `ArbitrageEngine` | Connects books, market identity, detection, simulation, and ranking. |
| `OpportunityRanker` | Sorts opportunities by net profit, return on capital, and quantity. |
| `SocketClient` | Maintains the TLS WebSocket connection, processes messages, and handles reconnects. |
| `main.cpp` | Controls startup, parallel initialization, reporting, deduplication, and runtime arguments. |

## Requirements

- C++20 compiler
- CMake 3.20 or newer
- Ninja or another supported CMake generator
- Boost.System
- OpenSSL
- nlohmann/json
- GoogleTest
- Internet access to Polymarket's public APIs

The repository includes a CMake preset for a Windows/MSYS2 UCRT64 environment. The preset expects the compiler and dependencies under `C:/msys64/ucrt64`.

### MSYS2 UCRT64 example

From an MSYS2 UCRT64 terminal, install the toolchain and libraries if they are not already available:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-boost \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-nlohmann-json \
  mingw-w64-ucrt-x86_64-gtest
```

Package names can vary between distributions. CMake must be able to find the packages listed in `CMakeLists.txt`.

## Build

### Using the included preset

```bash
cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
```

This creates the executable under:

```text
build/ucrt64-debug/prediction_market.exe
```

### Using a normal build directory

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

The executable will be located at:

```text
build/prediction_market.exe
```

## Run

The command-line format is:

```text
prediction_market [marketCount] [quantity] [minNetProfit]
```

Defaults:

| Argument | Default | Meaning |
|---|---:|---|
| `marketCount` | `500` | Number of active markets to load. Gamma pages are fetched in batches of 100. |
| `quantity` | `1` | Requested number of YES/NO pairs per market. Actual quantity may be lower because of liquidity. |
| `minNetProfit` | `0.0` | Minimum simulated net profit in dollars required before printing an opportunity. |

Examples on Windows PowerShell:

```powershell
.\build\prediction_market.exe
.\build\prediction_market.exe 100 1 0.01
.\build\prediction_market.exe 500 10 0
```

The second example scans 100 markets, requests one pair per market, and only prints opportunities with at least one cent of simulated net profit.

## Example output

```text
Discovered 500 active markets
Requested quantity per market: 1, fee rate: 0
Fetching initial books (1000 assets, 16 workers)...
Initialized 996 books
Connecting...
Connection successful
Subscribed to 1000 assets
Arbitrage found: Example binary market
  qty 1: buy YES @ 0.420 + NO @ 0.550 = $0.970 -> payout $1 | net $0.030 | roi 3.092784%
  tokens 1234567890123456... (YES), 9876543210987654... (NO) | https://polymarket.com/market/example
```

Prices in the output are observations from the local order-book state. The linked page shows the current market state, which may differ by the time it is opened.

Periodic diagnostics include message throughput, average processing latency, maximum latency, and markets closest to a combined ask of `$1.00`.

## Data sources

The program uses Polymarket's public endpoints:

- **Gamma API:** market discovery and metadata
  - `https://gamma-api.polymarket.com`
- **CLOB REST API:** order-book snapshots
  - `https://clob.polymarket.com`
- **CLOB WebSocket:** live market updates
  - `wss://ws-subscriptions-clob.polymarket.com/ws/market`

No API keys or wallet credentials are required for the current read-only monitoring mode.

## How the engine works

### 1. Market discovery

`MarketLoader` requests active markets from Gamma in pages of 100. It skips markets that are closed, inactive, not accepting orders, have no reported liquidity, or do not contain two CLOB token IDs.

### 2. Initial book synchronization

Both outcome tokens for every selected market are fetched from the CLOB REST API. The program uses 16 worker threads and has request/deadline protections so a slow or rate-limited endpoint does not block startup forever. Failed snapshots are skipped rather than treated as valid books.

### 3. WebSocket updates

After REST initialization, the client subscribes to the selected assets and processes `book` and `price_change` events. The subscription uses `initial_dump=false` because REST already supplied the initial snapshots; requesting another large snapshot burst can overwhelm a synchronous consumer.

### 4. Arbitrage calculation

For a requested quantity, each order book is walked from the cheapest ask upward. The detector:

1. Calculates the YES fill.
2. Calculates the NO fill.
3. Uses the smaller filled quantity as the number of complete pairs.
4. Recalculates the deeper side if necessary.
5. Computes total cost and guaranteed payout.
6. Rejects the result unless gross profit and net profit are positive.

The current default fee rate is zero, but the fee model and opportunity fields support fees for future experiments.

### 5. Reconnection

If the WebSocket closes or encounters an error, the client:

1. Marks the connection as disconnected.
2. Clears and re-fetches the local books through the resync callback.
3. Waits using exponential backoff, up to 30 seconds.
4. Reconnects and resubscribes to the asset list.

This is intentionally conservative because incremental updates may have been missed while disconnected.

## Testing

Build and run the GoogleTest suite with:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests cover:

- Binary arbitrage detection
- Missing books and missing liquidity
- Multiple ask levels and partial executable quantity
- Price-change messages
- Arrays of WebSocket messages
- Engine pipeline behavior
- Opportunity ranking
- Order-book fixtures

## Limitations and future work

- **No order execution:** the project does not place trades or manage a wallet.
- **Quotes are ephemeral:** an opportunity can disappear before a user opens the link or submits an order.
- **Fee model is simplified:** the default fee rate is zero and does not automatically retrieve current market-specific fees.
- **Synchronous message processing:** heavy console output or unusually large update bursts can create backpressure.
- **Binary markets only:** multi-outcome and combination-market strategies are outside the current detector.
- **No persistence:** order books, opportunities, and statistics are kept in memory only.
- **No authentication:** private account data, balances, positions, and orders are not queried.
- **No production safeguards:** there is no portfolio management, risk limit, order confirmation, or execution reconciliation.

Possible next steps include adding market-specific fee schedules, a persistent event log, asynchronous reporting, stronger price validation, and a paper-trading mode with simulated fills.

## License

This project is released under the [MIT License](LICENSE).
