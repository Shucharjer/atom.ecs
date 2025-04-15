cmake_minimum_required(VERSION 3.05)

if (NOT CMAKE_CXX_STANDARD)
set(CMAKE_CXX_STANDARD 20)
endif()

if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "..." FORCE)
endif()

set(CMAKE_NAMESPACE "Atom::")

project(Ecs
    DESCRIPTION "A ecs framework based on atom.utils"
    LANGUAGES C CXX
)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_library(Ecs INTERFACE)
add_library(Atom::Ecs ALIAS Ecs)

target_include_directories(Ecs INTERFACE
    $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

option(ATOM_USE_PMR_CONTAINER OFF)
if (ATOM_USE_PMR_CONTAINER)
    add_compile_definitions(ATOM_USE_PMR_CONTAINER)
endif()

option(ATOM_SINGLE_THREAD OFF)
if (ATOM_SINGLE_THREAD)
    add_compile_definitions(ATOM_SINGLE_THREAD)
endif()

include(CMakePackageConfigHelpers)

find_package(Utils CONFIG REQUIRED)
if(NOT Utils_FOUND)
    message(FATAL_ERROR "atom.utils not found!")
endif()
target_link_libraries(Ecs INTERFACE Atom::Utils)

option(Ecs_INCLUDE_HEADERS OFF)
if(Ecs_INCLUDE_HEADERS)
    target_sources(Ecs INTERFACE
        # $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/asset.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/command.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/components.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/custom_reflection.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/ecs.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/queryer.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/resources.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/scheduler.hpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/include/world.hpp>

        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/src/schedule.cpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/src/world.cpp>
    )
else()
    target_sources(Ecs INTERFACE
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/src/schedule.cpp>
        $<BUILD_INTERFACE:${Ecs_SOURCE_DIR}/src/world.cpp>
    )
endif()

install(
    TARGETS Ecs
    EXPORT EcsTargets
    LIBRARY DESTINATION ${INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${INSTALL_LIBDIR}
    RUNTIME DESTINATION ${INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

configure_package_config_file(
    ${Ecs_SOURCE_DIR}/cmake/in/EcsConfig.cmake.in
    EcsConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/Ecs/cmake
)

export(
    EXPORT EcsTargets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/EcsTargets.cmake
    NAMESPACE Atom::
)

install(
    EXPORT EcsTargets
    FILE EcsTargets.cmake
    NAMESPACE Atom::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/Ecs/cmake
)

install(
    FILES ${PROJECT_BINARY_DIR}/EcsConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/Ecs/cmake
)

install(
    DIRECTORY
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
)

export(PACKAGE Ecs)

option(Ecs_USE_CLANG_FORMAT OFF)
if (Ecs_USE_CLANG_FORMAT)
    find_program(Ecs_CLANG_FORMAT_EXECUTABLE "clang-format")
    if (NOT Ecs_CLANG_FORMAT_EXECUTABLE)
        message(FATAL_ERROR "Ecs_USE_CLANG_FORMAT has been set true, but couldn't find clang-format")
    endif()
endif()

option(Ecs_USE_CLANG_TIDY OFF)
if (Ecs_USE_CLANG_TIDY)
    find_program(Ecs_CLANG_TIDY_EXECUTABLE "clang-tidy")
    if (NOT Ecs_CLANG_TIDY_EXECUTABLE)
        message(FATAL_ERROR "Ecs_USE_CLANG_TIDY has been set true, but couldn't find clang-tidy")
    endif()
endif()

option(BUILD_Ecs_DOCS ON)
if(BUILD_Ecs_DOCS)
    find_package(Doxygen REQUIRED dot)
    if (DOXYGEN_FOUND)
        include(FetchContent)

        FetchContent_Declare(
            doxygen-awesome-css
            GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css
            GIT_TAG main
            GIT_SHALLOW 1
        )

        FetchContent_GetProperties(doxygen-awesome-css)

        if(NOT doxygen-awesome-css_POPULATED)
            FetchContent_Populate(doxygen-awesome-css)
            set(doxygen-awesome-css_INCLUDE_DIR ${doxygen-awesome-css_SOURCE_DIR})
        endif()

        set(DOXY_SOURCE_DIRECTORY ${Ecs_SOURCE_DIR})
        set(DOXY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
        set(DOXY_CSS_DIRECTORY ${doxygen-awesome-css_INCLUDE_DIR})

        configure_file(doxy.in doxy.cfg @ONLY)

        add_custom_target(
            documents
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxy.cfg
            WORKING_DIRECTORY ${Utils_SOURCE_DIR}
            VERBATIM
        )
    else()
        message(FATAL_ERROR "Required to build documents but doxygen is not found!")
    endif()
endif()

option(BUILD_Ecs_TEST OFF)
if(BUILD_Ecs_TEST)
    add_executable(main ${Ecs_SOURCE_DIR}/test/main.cpp)
    target_link_libraries(main PRIVATE Ecs)
endif()