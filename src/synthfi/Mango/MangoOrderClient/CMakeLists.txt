set(SOURCES
    MangoOrderClientImpl.cpp)
add_library(SYNTHFI_mango_order_client OBJECT ${SOURCES})
target_link_libraries(
    SYNTHFI_mango_order_client PUBLIC
    $<TARGET_OBJECTS:SYNTHFI_mango>
)
