set(SOURCES KeyPair.cpp)

add_library(SYNTHFI_core OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_core PUBLIC)

add_subdirectory(KeyStore)
