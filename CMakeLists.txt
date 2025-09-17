cmake_minimum_required(VERSION 3.10)

# Extract version.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/vkbind.h" VKBIND_VERSION_LINE REGEX "vkbind - v[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+")

string(REGEX MATCH "v([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" _ ${VKBIND_VERSION_LINE})
set(VKBIND_VERSION ${CMAKE_MATCH_1})

message(STATUS "vkbind Version: ${VKBIND_VERSION}")

project(vkbind VERSION ${VKBIND_VERSION})


# Options
option(VKBIND_BUILD_EXAMPLES "Build vkbind examples"            OFF)
option(VKBIND_BUILD_TESTS    "Build vkbind tests"               OFF)
option(VKBIND_BUILD_TOOLS    "Build vkbind tools"               OFF)
option(VKBIND_FORCE_CXX      "Force compilation as C++"         OFF)
option(VKBIND_FORCE_C89      "Force compilation as C89"         OFF)


# Construct compiler options.
set(COMPILE_OPTIONS)

if(VKBIND_FORCE_CXX AND VKBIND_FORCE_C89)
    message(FATAL_ERROR "VKBIND_FORCE_CXX and VKBIND_FORCE_C89 cannot be enabled at the same time.")
endif()

if(VKBIND_FORCE_CXX)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C++ (GNU/Clang)")
        list(APPEND COMPILE_OPTIONS -x c++)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Compiling as C++ (MSVC)")
        list(APPEND COMPILE_OPTIONS /TP)
    else()
        message(WARNING "VKBIND_FORCE_CXX is enabled but the compiler does not support it. Ignoring.")
    endif()
endif()

if(VKBIND_FORCE_C89)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C89")
        list(APPEND COMPILE_OPTIONS -std=c89)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(WARNING "MSVC does not support forcing C89. VKBIND_FORCE_C89 ignored.")
    else()
        message(WARNING "VKBIND_FORCE_C89 is enabled but the compiler does not support it. Ingoring.")
    endif()
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMPILE_OPTIONS -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    #list(APPEND COMPILE_OPTIONS /W4)
endif()


# Link libraries
set(COMMON_LINK_LIBRARIES)

if (UNIX)
    list(APPEND COMMON_LINK_LIBRARIES dl)      # For dlopen(), etc. Most compilers will link to this by default, but some may not.
    list(APPEND COMMON_LINK_LIBRARIES pthread) # Some compilers will not link to pthread by default so list it here just in case.
    list(APPEND COMMON_LINK_LIBRARIES m)

    # If we're compiling for 32-bit ARM we need to link to -latomic.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        list(APPEND COMMON_LINK_LIBRARIES atomic)
    endif()
endif()


# Common interface
add_library(vkbind_common INTERFACE)
target_compile_options(vkbind_common INTERFACE ${COMPILE_OPTIONS})
target_link_libraries (vkbind_common INTERFACE ${COMMON_LINK_LIBRARIES})


# Examples
if (VKBIND_BUILD_EXAMPLES)
    add_executable(01_Fundamentals examples/01_Fundamentals/01_Fundamentals.c)
    target_link_libraries(01_Fundamentals vkbind_common)
endif()

# Tools
if (VKBIND_BUILD_TOOLS)
    add_executable(vkbind_build source/vkbind_build.cpp)
    target_link_libraries(vkbind_build vkbind_common)

    #add_executable(vkbind_example_codegen source/vkbind_example_codegen.cpp)
    #target_link_libraries(vkbind_example_codegen vkbind_common)
endif()
