cmake_minimum_required(VERSION 3.1)

project(svgconvert)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(./lib)

add_executable(svgconvert svgconvert.cpp)
