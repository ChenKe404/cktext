cmake_minimum_required(VERSION 3.5)

project(CKText LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(ENABLE_TEST_CKTEXT "Enable cktext test target." OFF)

if(MSVC)
    add_compile_options(/utf-8 /MP /bigobj /D1033)
    add_definitions(-DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
endif()

find_package(Lz4++ REQUIRED)

add_library(cktext STATIC
    text.h
    text.cpp
    var.hpp
)
target_include_directories(cktext PUBLIC .)
target_link_libraries(cktext PRIVATE Lz4++::static)

if(ENABLE_TEST_CKTEXT)
    add_executable(test_cktext main.cpp)
    target_link_libraries(test_cktext PRIVATE cktext)
endif()

install(
    TARGETS
        cktext
    RUNTIME
        COMPONENT Runtime
        DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    ARCHIVE
        COMPONENT Development
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)
install(
    FILES
        text.h
        var.hpp
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/cktext
)
