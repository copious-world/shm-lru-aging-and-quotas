cmake_minimum_required(VERSION 3.5.1)

set (CMAKE_CXX_STANDARD 17)

project (shm-lru-c-testing)

include_directories(${CMAKE_JS_INC} /usr/local/lib/node_modules/nan)

file(GLOB SOURCE_FILES "src/*.cc" "src/*.h")

add_executable(atests ${SOURCE_FILES})


