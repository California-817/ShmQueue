# CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_language(ASM)

project(ShmQueue VERSION 1.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

include_directories(${PROJECT_SOURCE_DIR})

file(GLOB SOURCES ./*.cpp)

add_executable(test.out ${SOURCES})

target_link_libraries(test.out pthread)
