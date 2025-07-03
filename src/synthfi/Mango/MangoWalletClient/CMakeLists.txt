set(SOURCES
    MangoWalletClientImpl.cpp)
add_library(SYNTHFI_mango_wallet_client OBJECT ${SOURCES})
target_link_libraries(
    SYNTHFI_mango_wallet_client PUBLIC
    $<TARGET_OBJECTS:SYNTHFI_mango>
)
