set(SOURCES
    SignatureSubscriberImpl.cpp)
add_library(SYNTHFI_solana_signature_subscriber OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_solana_signature_subscriber PUBLIC SYNTHFI_solana $<TARGET_OBJECTS:SYNTHFI_solana_ws_client>)