set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
# set(CMAKE_CXX_COMPILER "clang++")

#find_package(Boost COMPONENTS unit_test_framework REQUIRED)
#include_directories(${Boost_INCLUDE_DIRS})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR})

# configure a header file to pass some of the CMake settings to the source code
#configure_file (
#  "${PROJECT_SOURCE_DIR}/config.h.in"
#  "${PROJECT_BINARY_DIR}/config.h"
#)

# add the binary tree to the search path for include files so that we will find
# config.h
include_directories("${PROJECT_BINARY_DIR}")
# include_directories("${EPICS_asyn_INCLUDE_DIRS}")

set(LIB_COMPONENTS
  drvFGPDB.cpp
  ParamInfo.cpp
)
add_library(drvFGPDBShared SHARED ${LIB_COMPONENTS})
set_target_properties(drvFGPDBShared PROPERTIES
  OUTPUT_NAME drvFGPDB
  CLEAN_DIRECT_OUTPUT 1
  VERSION ${drvFGPDB_VERSION_MAJOR}.${drvFGPDB_VERSION_MINOR}.${drvFGPDB_VERSION_PATCH}
  SOVERSION ${drvFGPDB_VERSION_MAJOR}.${drvFGPDB_VERSION_MINOR}
)
target_link_libraries(drvFGPDBShared ${EPICS_LIBRARIES})
add_library(drvFGPDBStatic STATIC ${LIB_COMPONENTS})
set_target_properties(drvFGPDBStatic
  PROPERTIES OUTPUT_NAME drvFGPDB
  CLEAN_DIRECT_OUTPUT 1
)
target_link_libraries(drvFGPDBStatic ${EPICS_LIBRARIES})
epics_install(LIBS drvFGPDBShared drvFGPDBStatic DBDS drvFGPDB.dbd)

include_directories(${GTEST_INCLUDE_DIRS})
set(DRVFGPDBTEST_COMPONENTS
  drvFGPDBTests.cpp
)
add_executable(drvFGPDBTests ${DRVFGPDBTEST_COMPONENTS})
target_link_libraries(drvFGPDBTests drvFGPDBShared ${EPICS_LIBRARIES} gmock_main)

set(PARAMINFOTEST_COMPONENTS
  ParamInfoTests.cpp
)
add_executable(ParamInfoTests ${PARAMINFOTEST_COMPONENTS})
target_link_libraries(ParamInfoTests drvFGPDBShared ${EPICS_LIBRARIES} gmock_main)

function(add_unit_tests target)
  get_target_property(sourceFiles ${target} SOURCES)
  set(tests "")
  set(test_suite "")
  foreach(sourceFile ${sourceFiles})
    file(READ "${sourceFile}" content)
    string(REGEX MATCHALL "[ \t]*TEST_?F?\\(([A-Za-z0-9_ ,]+)\\)" test_lines ${content})
    foreach(test_line ${test_lines})
      string(REGEX REPLACE ".*\\(([A-Za-z_]+)[, \t]*([A-Za-z0-9_]+)\\).*" "\\1.\\2" test_name ${test_line})
      add_test(NAME ${test_name} COMMAND ${target} --gtest_filter=${test_name})
    endforeach()
  endforeach()
endfunction()

add_unit_tests(drvFGPDBTests)
add_unit_tests(ParamInfoTests)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  setup_target_for_coverage(drvFGPDBTests_coverage drvFGPDBTests coverage '*Tests.cpp')
  setup_target_for_coverage(ParamInfoTests_coverage ParamInfoTests coverage '*Tests.cpp')
endif(CMAKE_BUILD_TYPE MATCHES Debug)
