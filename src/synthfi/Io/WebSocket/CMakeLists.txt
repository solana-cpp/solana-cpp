set(SOURCES
    SSLWebSocket.cpp
    UnsecureWebSocket.cpp)

add_library(SYNTHFI_web_socket OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_web_socket )