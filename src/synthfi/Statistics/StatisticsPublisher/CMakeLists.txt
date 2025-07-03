set(SOURCES
    StatisticsPublisherImpl.cpp)
add_library(SYNTHFI_statistics_publisher OBJECT ${SOURCES})
target_link_libraries(
    SYNTHFI_statistics_publisher PUBLIC
    SYNTHFI_statistics
)
