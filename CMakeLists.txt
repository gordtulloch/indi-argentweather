cmake_minimum_required(VERSION 3.16)
project(indi-argentweather VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Prefer the INDI CMake module when available; Debian and Ubuntu packages
# without it provide libindi through pkg-config.
list(APPEND CMAKE_MODULE_PATH "/usr/share/indi/cmake_modules")
list(APPEND CMAKE_MODULE_PATH "/usr/local/share/indi/cmake_modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

find_package(INDI 2.0 QUIET)

add_executable(indi_argentweather indi_argentweather.cpp)

if(INDI_FOUND AND (INDI_DRIVER_LIBRARIES OR INDI_LIBRARIES))
    target_include_directories(indi_argentweather PRIVATE
        ${INDI_INCLUDE_DIR} ${INDI_INCLUDE_DIRS})
    if(INDI_DRIVER_LIBRARIES)
        target_link_libraries(indi_argentweather PRIVATE ${INDI_DRIVER_LIBRARIES})
    else()
        target_link_libraries(indi_argentweather PRIVATE ${INDI_LIBRARIES})
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(INDI REQUIRED IMPORTED_TARGET libindi>=2.0)
    find_library(INDI_DRIVER_LIBRARY NAMES indidriver)
    if(NOT INDI_DRIVER_LIBRARY)
        message(FATAL_ERROR "Could not find the INDI driver library (indidriver)")
    endif()
    target_link_libraries(indi_argentweather
        PRIVATE PkgConfig::INDI ${INDI_DRIVER_LIBRARY})
endif()

if(DEFINED INDI_DATA_DIR)
    set(_indi_data_dir "${INDI_DATA_DIR}")
else()
    find_path(_indi_data_dir drivers.xml PATH_SUFFIXES share/indi)
    if(NOT _indi_data_dir)
        set(_indi_data_dir "${CMAKE_INSTALL_DATAROOTDIR}/indi")
    endif()
endif()
set(INDI_INSTALL_DATA_DIR "${_indi_data_dir}" CACHE STRING
    "Installation directory for INDI device definitions")

install(TARGETS indi_argentweather
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES indi_argentweather.xml
    DESTINATION ${INDI_INSTALL_DATA_DIR})
