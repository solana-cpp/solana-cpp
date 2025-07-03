set(SOURCES
    FtxOrderClientImpl.cpp)
add_library(SYNTHFI_ftx_order_client OBJECT ${SOURCES})
target_link_libraries(
    SYNTHFI_ftx_order_client PUBLIC
    $<TARGET_OBJECTS:SYNTHFI_ftx>
    $<TARGET_OBJECTS:SYNTHFI_ftx_rest_client>
    $<TARGET_OBJECTS:SYNTHFI_ftx_ws_client>
    simdjson
)