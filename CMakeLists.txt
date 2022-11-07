cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)

project(md80)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Release)
add_compile_options(-Wall -Wextra -Wpedantic)

# Eigen
find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIR})
message("EIGEN3_INCLUDE_DIR: " ${EIGEN3_INCLUDE_DIR})

# ros2
find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
include_directories(/opt/ros/foxy/include /opt/ros/foxy/lib)

if(ARCH STREQUAL "armhf")
    message("[CANDLE] Compiling for armhf")
    set(CMAKE_CXX_COMPILER /usr/bin/arm-linux-gnueabihf-g++)
    set(CMAKE_C_COMPILER /usr/bin/arm-linux-gnueabihf-gcc)
elseif(ARCH STREQUAL "aarm64")
    message("[CANDLE] Compiling for aarm64")
    set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)
    set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
endif()

file(GLOB CANDLE_SOURCE "src/*cpp")

add_definitions( -DCANDLE_BUILD_PYTHON=${CANDLE_BUILD_PYTHON} )

if(DEFINED CANDLE_BUILD_PYTHON)
    set(CANDLE_BUILD_STATIC "TRUE")
    set(CANDLE_BUILD_SHARED "FALSE")
    add_library(candle STATIC ${CANDLE_SOURCE})
    add_subdirectory(pyCandle)
else()
    set(CANDLE_BUILD_STATIC "FALSE")
    set(CANDLE_BUILD_SHARED "TRUE")
    add_library(candle SHARED ${CANDLE_SOURCE})
endif()

message(STATUS "\nCANDLE_BUILD_SHARED: ${CANDLE_BUILD_SHARED}\nCANDLE_BUILD_STATIC: ${CANDLE_BUILD_STATIC}\nCANDLE_BUILD_PYTHON: ${CANDLE_BUILD_PYTHON}")

target_include_directories (candle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(candle pthread)
ament_target_dependencies(candle rclcpp sensor_msgs)
set_target_properties(candle PROPERTIES PUBLIC_HEADER candle.hpp)