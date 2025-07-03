set(SOURCES
    SolanaWsClientImpl.cpp)
add_library(SYNTHFI_solana_ws_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_solana_ws_client PUBLIC SYNTHFI_solana simdjson)
