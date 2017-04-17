cmake_minimum_required(VERSION 3.0)
project(drvFGPDB)

set(drvFGPDB_VERSION_MAJOR 0)
set(drvFGPDB_VERSION_MINOR 0)
set(drvFGPDB_VERSION_PATCH 1)

set(VERSION_STRING "${drvFGPDB_VERSION_MAJOR}.${drvFGPDB_VERSION_MINOR}.${drvFGPDB_VERSION_PATCH}")

if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
  set(VERSION_STRING "${VERSION_STRING}-git")
endif(EXISTS "${PROJECT_SOURCE_DIR}/.git")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake4epics/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")
find_package(EPICS COMPONENTS Com dbCore)
find_package(EPICSTools)
find_package(EPICSModule)
find_epics_module(NAME asyn IDFILES include/asynDriver.h DBDS asyn.dbd LIBS asyn)
include_directories(${EPICS_INCLUDE_DIRS} ${asyn_INCLUDE_DIRS})

# Build Google Mock as part of this project as recommended by the project (see
# Debian's README as well as
# https://groups.google.com/forum/#!topic/googletestframework/Zo7_HOv1MJ0).
find_path(GMOCK_SRC_DIR NAMES CMakeLists.txt PATHS /usr/src/googletest/googlemock /usr/src/gmock NO_DEFAULT_PATH)
add_subdirectory(${GMOCK_SRC_DIR} ${CMAKE_BINARY_DIR}/gmock-build)

enable_testing()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -fprofile-arcs -ftest-coverage")
  include(CodeCoverage)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_subdirectory(drvFGPDBApp)