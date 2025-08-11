cmake_minimum_required(VERSION 3.10)

project(client-linux VERSION 1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE INCLUDES ${PROJECT_SOURCE_DIR}/include/*.h)
list(REMOVE_DUPLICATES INCLUDES)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(JANSSON REQUIRED jansson)

add_executable(client-linux ${SRCS})

# libevdev
target_link_libraries(client-linux PRIVATE ${LIBEVDEV_LINK_LIBRARIES})
target_include_directories(client-linux PRIVATE ${LIBEVDEV_INCLUDE_DIRS})

# curl
target_link_libraries(client-linux PRIVATE ${CURL_LINK_LIBRARIES})
target_include_directories(client-linux PRIVATE ${CURL_INCLUDE_DIRS})

# jansson
target_link_libraries(client-linux PRIVATE ${JANSSON_LINK_LIBRARIES})
target_include_directories(client-linux PRIVATE ${JANSSON_INCLUDE_DIRS})

target_include_directories(client-linux PUBLIC include)
