cmake_minimum_required(VERSION 3.21)
project(n2ajl)

set(CMAKE_CXX_STANDARD 14)

add_library(n2ajl src/Node.cpp src/Parser.cpp src/Serializer.cpp)
target_include_directories(n2ajl PUBLIC include)
