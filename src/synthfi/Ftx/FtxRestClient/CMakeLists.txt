set(SOURCES
    FtxRestClientImpl.cpp)
add_library(SYNTHFI_ftx_rest_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_ftx_rest_client PUBLIC $<TARGET_OBJECTS:SYNTHFI_ftx> simdjson)