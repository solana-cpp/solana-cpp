cmake_minimum_required( VERSION 3.22 )

# Add project_options v0.22.4
# https://github.com/aminya/project_options
# Change the version in the following URL to update the package (watch the releases of the repository for future updates)
include(FetchContent)
FetchContent_Declare(_project_options URL https://github.com/aminya/project_options/archive/refs/tags/v0.22.4.zip)
FetchContent_MakeAvailable(_project_options)
include(${_project_options_SOURCE_DIR}/Index.cmake)

# install vcpkg dependencies: - should be called before defining project()
run_vcpkg()

# project synthfi
project( Synthfi VERSION 0.0.1 LANGUAGES CXX C )

# Enable sanitizers and static analyzers when running the tests
set(ENABLE_CLANG_TIDY ON)
set(ENABLE_CPPCHECK ON )
set(ENABLE_SANITIZER_ADDRESS ON )
set(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR ON )
set(ENABLE_COVERAGE OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(ENABLE_DEVELOPER_MODE TRUE CACHE BOOL "Enable developer mode")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(ENABLE_SANITIZER_ADDRESS "ENABLE_SANITIZER_ADDRESS")
set(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR "ENABLE_SANITIZER_UNDEFINED_BEHAVIOR")

# default build convention
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RELEASE)
endif()


# set( Boost_USE_STATIC_LIBS ON )
# set( Boost_USE_STATIC_RUNTIME ON )
find_package(Boost 1.80 REQUIRED COMPONENTS coroutine json log_setup log program_options system)
find_package(fmt CONFIG REQUIRED)
find_package(simdjson 2.2.2 CONFIG REQUIRED)

# Initialize project_options variable related to this project
# This overwrites `project_options` and sets `project_warnings`
# uncomment to enable the options. Some of them accept one or more inputs:
project_options(
      ENABLE_CACHE
      ${ENABLE_CPPCHECK}
      ${ENABLE_CLANG_TIDY}
      ENABLE_VS_ANALYSIS
      # ENABLE_CONAN
      # ENABLE_INTERPROCEDURAL_OPTIMIZATION
      #ENABLE_NATIVE_OPTIMIZATION
      #ENABLE_SANITIZER_ADDRESS
      #ENABLE_SANITIZER_UNDEFINED_BEHAVIOR
      # ENABLE_SANITIZER_THREAD
      # ENABLE_SANITIZER_MEMORY
      # ENABLE_PCH
      # PCH_HEADERS
      # WARNINGS_AS_ERRORS
      # ENABLE_INCLUDE_WHAT_YOU_USE
      # ENABLE_USER_LINKER
      # ENABLE_BUILD_WITH_TIME_TRACE
      # ENABLE_UNITY
      # CONAN_PROFILE "${PROJECT_SOURCE_DIR}/conanfile.txt"  # passes a profile to conan: see https://docs.conan.io/en/latest/reference/profiles.html
)

add_compile_definitions( BOOST_ASIO_HAS_IO_URING=1 )

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)

# find synthesizer header files
include_directories(${PROJECT_SOURCE_DIR}/program/src/ )

# gcc compiler/linker flags

set( SANITIZER_OPTIONS -fsanitize=address -fsanitize=leak -fsanitize=undefined )


# lots of warnings and all warnings as errors
add_compile_options( -ggdb -march=native -Wno-maybe-uninitialized -Wall -Wextra -Werror -Wno-unused -Wcast-align -Winit-self -Wignored-qualifiers -fdiagnostics-color=always -I$(HOME)/local/include -m64 -Wno-deprecated-declarations -DBOOST_LOG_DYN_LINK -fsanitize=address,leak,undefined )

set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -fsanitize=address,leak,undefined")

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

add_subdirectory( src )

#
# install
#
install( FILES ${PROJECT_SOURCE_DIR}/program/src/synthesizer/synthesizer.h DESTINATION include/synthesizer )
