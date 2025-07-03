set(SOURCES JsonHttpSocket.cpp)

add_library(SYNTHFI_json_http_socket OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_json_http_socket PUBLIC )