cmake_minimum_required(VERSION 3.16)
project(indi-argentweather VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Locate INDI cmake modules – check common installation paths
list(APPEND CMAKE_MODULE_PATH "/usr/share/indi/cmake_modules")
list(APPEND CMAKE_MODULE_PATH "/usr/local/share/indi/cmake_modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

find_package(INDI 2.0 REQUIRED)

include_directories(${INDI_INCLUDE_DIR})

# ---- indi_argentweather ----
add_executable(indi_argentweather indi_argentweather.cpp)
target_link_libraries(indi_argentweather indidriver)

install(TARGETS indi_argentweather
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES indi_argentweather.xml
    DESTINATION ${INDI_DATA_DIR})
