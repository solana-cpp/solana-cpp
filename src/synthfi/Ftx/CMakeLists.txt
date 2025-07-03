set(SOURCES
    FtxRestMessage.cpp
    FtxTypes.cpp
    FtxUtils.cpp
    FtxWsMessage.cpp)

add_library(SYNTHFI_ftx OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_ftx PUBLIC)

add_subdirectory(FtxMarketDataClient)
add_subdirectory(FtxOrderClient)
add_subdirectory(FtxReferenceDataClient)
add_subdirectory(FtxRestClient)
add_subdirectory(FtxWalletClient)
add_subdirectory(FtxWsClient)

