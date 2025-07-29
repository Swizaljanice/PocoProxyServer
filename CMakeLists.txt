cmake_minimum_required(VERSION 3.20)
project(PocoProxyServer)

set(CMAKE_CXX_STANDARD 17)

# Use vcpkg toolchain
# (This line is only needed if you're configuring manually without Presets)
# set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

find_package(Poco REQUIRED COMPONENTS Net Util)

add_executable(PocoProxyServer src/main.cpp)

target_link_libraries(PocoProxyServer PRIVATE Poco::Net Poco::Util)
