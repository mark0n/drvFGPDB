cmake_minimum_required(VERSION 3.0)
project(drvFGPDB)

set(drvFGPDB_VERSION_MAJOR 0)
set(drvFGPDB_VERSION_MINOR 0)
set(drvFGPDB_VERSION_PATCH 1)

set(VERSION_STRING "${drvFGPDB_VERSION_MAJOR}.${drvFGPDB_VERSION_MINOR}.${drvFGPDB_VERSION_PATCH}")

if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
  set(VERSION_STRING "${VERSION_STRING}-git")
endif(EXISTS "${PROJECT_SOURCE_DIR}/.git")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")
find_package(EPICS COMPONENTS Com dbCore asyn)
find_package(EPICSTools)
include_directories(${EPICS_INCLUDE_DIRS})

# Build Google Mock as part of this project as recommended by the project (see
# Debian's README as well as
# https://groups.google.com/forum/#!topic/googletestframework/Zo7_HOv1MJ0).
set(GMOCK_SRC_DIR "/usr/src/gmock")
add_subdirectory(${GMOCK_SRC_DIR} ${CMAKE_BINARY_DIR}/gmock-build)

enable_testing()

add_subdirectory(drvFGPDBApp)