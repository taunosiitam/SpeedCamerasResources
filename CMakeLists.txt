cmake_minimum_required(VERSION 3.23)
project(speedcameras)

set(CMAKE_CXX_STANDARD 17)

include_directories("C:/boost_1_80_0")

add_executable(speedcameras main.cpp shp.cpp)
