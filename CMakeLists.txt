cmake_minimum_required(VERSION 3.16)
project(QMGR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Detect platform
if(WIN32)
    message(STATUS "Building on Windows")
elseif(UNIX)
    message(STATUS "Building on Linux")
endif()

# Find Qt5 (Widgets required)
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# Add executable
add_executable(qmgr qmgr.cpp)

# Link Qt5 libraries
target_link_libraries(qmgr Qt5::Core Qt5::Widgets)
