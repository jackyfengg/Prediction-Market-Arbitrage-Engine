cmake_minimum_required(VERSION 3.20)

project(prediction_market
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE PREDICTION_MARKET_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(prediction_market ${PREDICTION_MARKET_SOURCES})

target_include_directories(prediction_market
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(prediction_market
    PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        ws2_32
)

enable_testing()

add_executable(arbitrage_tests
    tests/ArbitrageDetectorTest.cpp
    tests/ArbitrageDetectorEconomicsTest.cpp
    tests/ArbitrageEngineTest.cpp
    tests/ArbitrageEnginePipelineTest.cpp
    tests/OrderBookTest.cpp
    tests/FeeModelTest.cpp
    tests/ExecutionSimulatorTest.cpp
    tests/OpportunityRankerTest.cpp
    tests/MarketLoaderTest.cpp
    src/ArbitrageDetector.cpp
    src/ArbitrageEngine.cpp
    src/OrderBook.cpp
    src/FeeModel.cpp
    src/ExecutionSimulator.cpp
    src/OpportunityRanker.cpp
    src/MarketLoader.cpp
    src/ClobRestClient.cpp
)

target_include_directories(arbitrage_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(arbitrage_tests
    PRIVATE
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        ws2_32
)

include(GoogleTest)

gtest_discover_tests(arbitrage_tests)

file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/tests
)