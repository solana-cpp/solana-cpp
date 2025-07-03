set(SOURCES 
    StatisticsTypes.cpp
    )
add_library(SYNTHFI_statistics OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_statistics)

add_subdirectory(StatisticsPublisher)
