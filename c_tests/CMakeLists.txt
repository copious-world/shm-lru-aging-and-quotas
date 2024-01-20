cmake_minimum_required(VERSION 3.5.1)

set (CMAKE_CXX_STANDARD 20)

project (shm-lru-c-testing)

include_directories(${CMAKE_JS_INC} /usr/local/lib/node_modules/nan)

file(GLOB SOURCE_FILES "src/*.cc" "src/*.h")

add_executable(atests ${SOURCE_FILES})


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
