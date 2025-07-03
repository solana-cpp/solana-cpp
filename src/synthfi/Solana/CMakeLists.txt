set(SOURCES
    SolanaHttpMessage.cpp
    SolanaTypes.cpp
    SolanaWsMessage.cpp
    Token.cpp)
add_library(SYNTHFI_solana OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_solana )

add_subdirectory(AccountBatcher)
add_subdirectory(AccountSubscriber)
add_subdirectory(SignatureSubscriber)
add_subdirectory(SlotSubscriber)
add_subdirectory(SolanaHttpClient)
add_subdirectory(SolanaWsClient)
