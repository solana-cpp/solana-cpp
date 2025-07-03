set(SOURCES
    MangoReferenceDataClientImpl.cpp)

add_library(SYNTHFI_mango_reference_data_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_mango_reference_data_client PUBLIC
    $<TARGET_OBJECTS:SYNTHFI_solana_account_subscriber>
    $<TARGET_OBJECTS:SYNTHFI_mango>)
