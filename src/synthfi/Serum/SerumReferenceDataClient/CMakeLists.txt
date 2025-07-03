set(SOURCES
    SerumReferenceDataClientImpl.cpp)

add_library(SYNTHFI_serum_reference_data_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_serum_reference_data_client PUBLIC
    $<TARGET_OBJECTS:SYNTHFI_solana_account_subscriber>
    $<TARGET_OBJECTS:SYNTHFI_serum>)