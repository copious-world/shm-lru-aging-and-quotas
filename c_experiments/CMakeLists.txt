cmake_minimum_required(VERSION 3.5.1)

set (CMAKE_CXX_STANDARD 20)

project (cpp-testing)



file(GLOB SOURCE_FILES "src/*.cc" "src/*.h")
find_package( Threads )

add_executable(atests ${SOURCE_FILES})


target_link_libraries( atests ${CMAKE_THREAD_LIBS_INIT} )


if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
