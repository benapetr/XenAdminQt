cmake_minimum_required(VERSION 3.22)
project(XenAdminQt VERSION 0.0.6 LANGUAGES CXX) # keep in sync with docs/version.md

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Xml Widgets Charts)
qt_standard_project_setup()

add_subdirectory(src/xenlib)
add_subdirectory(src/xenadmin-ui)
