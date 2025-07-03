set(SOURCES
    FtxMarketDataClientImpl.cpp)
add_library(SYNTHFI_ftx_market_data_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_ftx_market_data_client PUBLIC $<TARGET_OBJECTS:SYNTHFI_ftx> $<TARGET_OBJECTS:SYNTHFI_ftx_ws_client> simdjson)