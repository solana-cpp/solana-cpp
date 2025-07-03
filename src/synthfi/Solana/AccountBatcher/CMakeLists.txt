set(SOURCES
    AccountBatcherImpl.cpp)
add_library(SYNTHFI_solana_account_batcher OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_solana_account_batcher PUBLIC $<TARGET_OBJECTS:SYNTHFI_solana_http_client>)