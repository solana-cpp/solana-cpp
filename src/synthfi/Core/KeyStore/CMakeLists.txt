set(SOURCES KeyStoreImpl.cpp)

add_library(SYNTHFI_key_store OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_key_store PUBLIC )