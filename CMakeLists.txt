cmake_minimum_required(VERSION 3.10)
project(telnet_server)

set(CMAKE_CXX_STANDARD 11)

add_executable(telnet_server main.cpp err.h err.cpp)