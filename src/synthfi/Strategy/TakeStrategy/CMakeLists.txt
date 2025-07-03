set(SOURCES
    TakeStrategyImpl.cpp
    TakeStrategyService.cpp
    TakeStrategyTypes.cpp)

add_library(SYNTHFI_take_strategy OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_take_strategy)
