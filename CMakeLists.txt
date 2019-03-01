cmake_minimum_required(VERSION 3.0)
project(drvFGPDB)

set(drvFGPDB_VERSION_MAJOR 1)
set(drvFGPDB_VERSION_MINOR 3)
set(drvFGPDB_VERSION_PATCH 6)

set(VERSION_STRING "${drvFGPDB_VERSION_MAJOR}.${drvFGPDB_VERSION_MINOR}.${drvFGPDB_VERSION_PATCH}")

if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
  set(VERSION_STRING "${VERSION_STRING}-git")
endif(EXISTS "${PROJECT_SOURCE_DIR}/.git")

list(APPEND CMAKE_MODULE_PATH "/usr/lib/epics/cmake/Modules")
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
set(INSTALL_GTEST OFF)
add_subdirectory(${GMOCK_SRC_DIR} ${CMAKE_BINARY_DIR}/gmock-build)

# For some reason the above is not sufficient on Ubuntu 18.10 (gtest 1.8.0).
# We work around this by manually including the Google Test include directory
# to avoid compiler errors.
find_path(GTEST_SRC_DIR NAMES CMakeLists.txt PATHS /usr/src/googletest/googletest /usr/src/gtest NO_DEFAULT_PATH)
include_directories("${GTEST_SRC_DIR}/include")

enable_testing()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -fprofile-arcs -ftest-coverage")
  include(CodeCoverage)
  # Set target to merge lcov reports
  ADD_CUSTOM_TARGET(drvCoverageMerge
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ${LCOV_PATH} --rc lcov_branch_coverage=1 -a drvFGPDBCoverage.info.cleaned;
                                                     -a drvFGPDBIntegrationCoverage.info.cleaned;
                                                     -a ParamInfoCoverage.info.cleaned;
                                                     -a LCPProtocolCoverage.info.cleaned;
                                                     -o drvCoverageMerge.info
    COMMAND ${GENHTML_PATH} --rc genhtml_branch_coverage=1 -o ./drvCoverageMerge/ ./drvCoverageMerge.info
    COMMAND ${CMAKE_COMMAND} -E remove *.info *.info.cleaned
  )
  # Show info where to find the report
  ADD_CUSTOM_COMMAND(TARGET drvCoverageMerge POST_BUILD
    COMMAND ;
    COMMENT "Open ./drvCoverageMerge/index.html in your browser to view the coverage report summary of all test units."
  )
  add_dependencies(drvCoverageMerge drvFGPDBTests_coverage drvFGPDBIntegrationTests_coverage ParamInfoTests_coverage LCPProtocolTests_coverage)
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_subdirectory(drvFGPDBApp)
add_subdirectory(doc)
