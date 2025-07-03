set(SOURCES
    FtxReferenceDataClientImpl.cpp)
add_library(SYNTHFI_ftx_reference_data_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_ftx_reference_data_client PUBLIC $<TARGET_OBJECTS:SYNTHFI_ftx> $<TARGET_OBJECTS:SYNTHFI_ftx_rest_client>)