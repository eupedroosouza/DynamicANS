cmake_minimum_required(VERSION 3.13)
project(DynamicANS VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
        source/Utils/Util.h
        source/Utils/Debug.h
        source/Utils/Bitstream.h
        source/Common/TabledANS.h
        source/Common/Context.h
        source/Dec/ANSDecoder.cpp
        source/Enc/ANSEncoder.cpp
        source/DynamicANS.cpp)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_executable(DynamicANS ${SOURCES})
target_include_directories(DynamicANS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_options(DynamicANS PRIVATE -O2 -Wall -Wextra)
target_link_options(DynamicANS PRIVATE -static -static-libgcc -static-libstdc++)