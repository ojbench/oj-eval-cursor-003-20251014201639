cmake_minimum_required(VERSION 3.10)
project(icpc_mgmt)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
add_compile_options(-O2 -pipe -static-libstdc++ -static-libgcc)

add_executable(code code.cpp)
