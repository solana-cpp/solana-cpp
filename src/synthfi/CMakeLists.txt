add_subdirectory(Core)
add_subdirectory(Ftx)
add_subdirectory(Io)
add_subdirectory(Mango)
add_subdirectory(Serum)
add_subdirectory(Solana)
add_subdirectory(Statistics)
add_subdirectory(Strategy)
add_subdirectory(Trading)
add_subdirectory(Util)

add_library(synthfi STATIC
    $<TARGET_OBJECTS:SYNTHFI_backtest_strategy>
    $<TARGET_OBJECTS:SYNTHFI_core>
    $<TARGET_OBJECTS:SYNTHFI_ftx>
    $<TARGET_OBJECTS:SYNTHFI_ftx_market_data_client>
    $<TARGET_OBJECTS:SYNTHFI_ftx_order_client>
    $<TARGET_OBJECTS:SYNTHFI_ftx_reference_data_client>
    $<TARGET_OBJECTS:SYNTHFI_ftx_rest_client>
    $<TARGET_OBJECTS:SYNTHFI_ftx_wallet_client>
    $<TARGET_OBJECTS:SYNTHFI_ftx_ws_client>
    $<TARGET_OBJECTS:SYNTHFI_json_http_socket>
    $<TARGET_OBJECTS:SYNTHFI_key_store>
    $<TARGET_OBJECTS:SYNTHFI_mango>
    $<TARGET_OBJECTS:SYNTHFI_mango_reference_data_client>
    $<TARGET_OBJECTS:SYNTHFI_mango_order_client>
    $<TARGET_OBJECTS:SYNTHFI_mango_wallet_client>
    $<TARGET_OBJECTS:SYNTHFI_serum>
    $<TARGET_OBJECTS:SYNTHFI_serum_market_data_client>
    $<TARGET_OBJECTS:SYNTHFI_serum_reference_data_client>
    $<TARGET_OBJECTS:SYNTHFI_solana>
    $<TARGET_OBJECTS:SYNTHFI_solana_account_batcher>
    $<TARGET_OBJECTS:SYNTHFI_solana_account_subscriber>
    $<TARGET_OBJECTS:SYNTHFI_solana_http_client>
    $<TARGET_OBJECTS:SYNTHFI_solana_signature_subscriber>
    $<TARGET_OBJECTS:SYNTHFI_solana_slot_subscriber>
    $<TARGET_OBJECTS:SYNTHFI_solana_ws_client>
    $<TARGET_OBJECTS:SYNTHFI_statistics>
    $<TARGET_OBJECTS:SYNTHFI_statistics_publisher>
    $<TARGET_OBJECTS:SYNTHFI_take_strategy>
    $<TARGET_OBJECTS:SYNTHFI_trading>
    $<TARGET_OBJECTS:SYNTHFI_web_socket>)


set( SYNTHFI_DEPENDENCIES
    Boost::coroutine
    Boost::json
    Boost::log
    crypto
    fmt::fmt
    magic_enum
    simdjson
    ssl
    uring
    z)

target_link_libraries(synthfi PUBLIC ${SYNTHFI_DEPENDENCIES})
