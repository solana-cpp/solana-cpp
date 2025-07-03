set(SOURCES
   MangoTypes.cpp
   MangoInstruction.cpp)

add_library(SYNTHFI_mango OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_mango PUBLIC)

add_subdirectory(MangoOrderClient)
add_subdirectory(MangoReferenceDataClient)
add_subdirectory(MangoWalletClient)
