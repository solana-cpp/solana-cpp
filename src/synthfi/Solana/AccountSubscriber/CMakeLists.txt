set(SOURCES
    AccountSubscriberImpl.cpp)
add_library(SYNTHFI_solana_account_subscriber OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_solana_account_subscriber PUBLIC $<TARGET_OBJECTS:SYNTHFI_solana_http_client>)