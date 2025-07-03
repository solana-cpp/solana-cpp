set(SOURCES
    SolanaHttpClientImpl.cpp)
add_library(SYNTHFI_solana_http_client OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_solana_http_client PUBLIC SYNTHFI_solana simdjson)