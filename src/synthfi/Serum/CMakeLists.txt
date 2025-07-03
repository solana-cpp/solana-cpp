set(SOURCES
   SerumTypes.cpp)

add_library(SYNTHFI_serum OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_serum PUBLIC)

add_subdirectory(SerumMarketDataClient)
add_subdirectory(SerumReferenceDataClient)