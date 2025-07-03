set(SOURCES
    SerumMarketDataClientImpl.cpp)

add_library(SYNTHFI_serum_market_data_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_serum_market_data_client PUBLIC
    $<TARGET_OBJECTS:SYNTHFI_solana_http_client>
    $<TARGET_OBJECTS:SYNTHFI_solana_ws_client>
    $<TARGET_OBJECTS:SYNTHFI_serum>)