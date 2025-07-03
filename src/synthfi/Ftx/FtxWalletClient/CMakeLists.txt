set(SOURCES
    FtxWalletClientImpl.cpp)
add_library(SYNTHFI_ftx_wallet_client OBJECT ${SOURCES})
target_link_libraries(
    SYNTHFI_ftx_wallet_client PUBLIC
    $<TARGET_OBJECTS:SYNTHFI_ftx>
)
