set(SOURCES
    Book.cpp
    Currency.cpp
    Order.cpp
    Price.cpp
    Quantity.cpp
    TradingPair.cpp)
add_library(SYNTHFI_trading OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_trading PUBLIC)