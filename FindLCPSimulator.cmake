include(FindPackageHandleStandardArgs)

find_path(LCPSimulator_INCLUDE_DIR lcpsimulatorClass.hpp PATHS ${LCPSimulator_PATH}/include)
find_library(LCPSimulator_LIBRARY NAMES llrfsim PATHS ${LCPSimulator_PATH}/lib)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LCPSimulator
 DEFAULT_MSG
  LCPSimulator_INCLUDE_DIR
  LCPSimulator_LIBRARY
)

mark_as_advanced(LCPSimulator_LIBRARY LCPSimulator_INCLUDE_DIR)

if(LCPSimulator_FOUND)
  set(LCPSimulator_LIBRARIES    ${LCPSimulator_LIBRARY})
  set(LCPSimulator_INCLUDE_DIRS ${LCPSimulator_INCLUDE_DIR})
endif()