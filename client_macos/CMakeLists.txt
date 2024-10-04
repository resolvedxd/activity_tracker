cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME activity_monitor_macos)
project(${PROJECT_NAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# look for files in ./src, ./include
file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE INCLUDES ${PROJECT_SOURCE_DIR}/include/*.hpp)
list(REMOVE_DUPLICATES INCLUDES)

find_package(CURL REQUIRED)

add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME} "-framework coreGraphics")
target_link_libraries(${PROJECT_NAME} "-framework ApplicationServices")
target_link_libraries(${PROJECT_NAME} CURL::libcurl)

target_include_directories(${PROJECT_NAME} PUBLIC include)
