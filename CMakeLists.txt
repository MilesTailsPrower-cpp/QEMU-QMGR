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

# Find Qt5
find_package(Qt5 COMPONENTS Core Widgets REQUIRED)

# Find SDL2
if(WIN32)
    # You may need to set SDL2_DIR manually if SDL2 isn't in a standard location
    # set(SDL2_DIR "C:/SDL2")
endif()
find_package(SDL2 REQUIRED)

# Add executable
add_executable(qmgr qmgr.cpp)

# Link libraries
target_link_libraries(qmgr Qt5::Core Qt5::Widgets SDL2::SDL2)

