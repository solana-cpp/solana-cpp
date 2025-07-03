#
# build libsynthfi
#

add_library(magic_enum INTERFACE)
target_include_directories(magic_enum INTERFACE magic_enum)

add_subdirectory(synthfi)

# Build applications
#
add_executable(synthfi-client synthfi-client/SynthfiClient.cpp)
target_link_libraries(synthfi-client PUBLIC synthfi Boost::program_options simdjson)

#
# Install
#
install( TARGETS synthfi DESTINATION lib )
install( TARGETS synthfi-client DESTINATION bin )
install( FILES ${INCLUDE_DIRECTORIES} DESTINATION include/synthfi )
