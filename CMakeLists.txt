cmake_minimum_required(VERSION 3.6)
project(door-agent CXX)

find_package(PkgConfig)
pkg_check_modules(JSON REQUIRED IMPORTED_TARGET jsoncpp)
pkg_check_modules(MOSQ REQUIRED IMPORTED_TARGET libmosquittopp)
pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiodcxx)
find_package(Boost REQUIRED COMPONENTS program_options)

set(CORE_SRC "door-agent.cc" "Log.cc" "Door.cc" "MqttClient.cc")

set(CMAKE_CXX_FLAGS "-std=gnu++17")

include_directories(${JSON_INCLUDES})
include_directories(${Boost_INCLUDE_DIRS})

set(BUILD_UVW_SHARED_LIB ON)
set(FIND_LIBUV ON)
set(BUILD_TESTING OFF)
add_subdirectory("uvw")

add_executable("door-agent" ${CORE_SRC})
target_link_libraries("door-agent" PkgConfig::JSON PkgConfig::MOSQ ${Boost_LIBRARIES} PkgConfig::GPIOD "uvw")
