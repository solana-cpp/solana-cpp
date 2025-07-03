set(SOURCES
    BacktestStrategyImpl.cpp
    BacktestStrategyService.cpp
    BacktestStrategyTypes.cpp)

add_library(SYNTHFI_backtest_strategy OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_backtest_strategy simdjson)
