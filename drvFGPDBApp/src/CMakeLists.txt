set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Wunused -Wshadow")
# set(CMAKE_CXX_COMPILER "clang++")

find_package(Threads)
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

set(LIB_COMPONENTS
  drvFGPDB.cpp
  drvFGPDB_IOC.cpp
  LCPProtocol.cpp
  ParamInfo.cpp
  asynOctetSyncIOWrapper.cpp
)
add_library(drvFGPDBShared SHARED ${LIB_COMPONENTS})
set_target_properties(drvFGPDBShared PROPERTIES
  OUTPUT_NAME drvFGPDB
  CLEAN_DIRECT_OUTPUT 1
  VERSION ${drvFGPDB_VERSION_MAJOR}.${drvFGPDB_VERSION_MINOR}.${drvFGPDB_VERSION_PATCH}
  SOVERSION ${drvFGPDB_VERSION_MAJOR}.${drvFGPDB_VERSION_MINOR}
)
target_link_libraries(drvFGPDBShared ${asyn_LIBRARIES} ${EPICS_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
add_library(drvFGPDBStatic STATIC ${LIB_COMPONENTS})
set_target_properties(drvFGPDBStatic
  PROPERTIES OUTPUT_NAME drvFGPDB
  CLEAN_DIRECT_OUTPUT 1
)
target_link_libraries(drvFGPDBStatic ${asyn_LIBRARIES} ${EPICS_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
epics_install(LIBS drvFGPDBShared drvFGPDBStatic DBDS drvFGPDB.dbd)
