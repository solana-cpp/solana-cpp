set(SOURCES
    SlotSubscriberImpl.cpp)
add_library(SYNTHFI_solana_slot_subscriber OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_solana_slot_subscriber PUBLIC $<TARGET_OBJECTS:SYNTHFI_solana_http_client>)