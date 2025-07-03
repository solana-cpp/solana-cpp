set(SOURCES
    FtxWsClientServiceImpl.cpp)
add_library(SYNTHFI_ftx_ws_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_ftx_ws_client PUBLIC $<TARGET_OBJECTS:SYNTHFI_ftx> $<TARGET_OBJECTS:SYNTHFI_web_socket> simdjson)