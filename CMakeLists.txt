cmake_minimum_required(VERSION 3.22)

project(Harmonia VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(HARMONIA_BUILD_CLI    "Build the command line renderer" ON)
option(HARMONIA_BUILD_TESTS  "Build the core unit tests"       ON)
option(HARMONIA_BUILD_PLUGIN "Build the VST3 / standalone app" OFF)
option(HARMONIA_INSTALL_PLUGIN "Copy the built VST3 into the system plugin folder" OFF)

add_subdirectory(core)

if(HARMONIA_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(HARMONIA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HARMONIA_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
