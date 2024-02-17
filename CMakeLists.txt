cmake_minimum_required(VERSION 3.10)
project(rdbms)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMKAE_CXX_FLAGS} -g -Wall")
set(RDBMS_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/src/include)

include_directories(${RDBMS_INCLUDE_DIR})
include_directories(${GTEST_INCLUDE_DIR})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(ClangTidy)
include(FindGTest)

add_subdirectory(src)
add_subdirectory(test)