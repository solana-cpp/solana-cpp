set(SOURCES
    ClientRequest.cpp
    RpcClientHandler.cpp
    RpcServerService.cpp)

add_library(SYNTHFI_rpc_server OBJECT ${SOURCES})
target_link_libraries(SYNTHFI_rpc_server PUBLIC )