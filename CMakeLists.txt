cmake_minimum_required(VERSION 2.6)
project(opsyshwiv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall  -lpthread -std=c++11")

add_executable(opsyshwiv main.cpp)

install(TARGETS opsyshwiv RUNTIME DESTINATION bin)
