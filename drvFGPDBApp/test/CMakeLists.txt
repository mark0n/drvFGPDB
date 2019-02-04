find_package(Boost COMPONENTS filesystem system REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Wunused -Wshadow")
include_directories(${GTEST_INCLUDE_DIRS} "${PROJECT_SOURCE_DIR}/drvFGPDBApp/src")
set(DRVFGPDBTEST_COMPONENTS
  drvFGPDBTests.cpp
)
add_executable(drvFGPDBTests ${DRVFGPDBTEST_COMPONENTS})
target_link_libraries(drvFGPDBTests drvFGPDBShared ${asyn_LIBRARIES} ${EPICS_LIBRARIES} gmock_main)

set(DRVFGPDBINTEGRATIONTEST_COMPONENTS
  drvFGPDBIntegrationTests.cpp
)
add_executable(drvFGPDBIntegrationTests ${DRVFGPDBINTEGRATIONTEST_COMPONENTS})
target_link_libraries(drvFGPDBIntegrationTests drvFGPDBShared llrfsim ${Boost_LIBRARIES} ${asyn_LIBRARIES} ${EPICS_LIBRARIES} gmock_main)

set(PARAMINFOTEST_COMPONENTS
  ParamInfoTests.cpp
)
add_executable(ParamInfoTests ${PARAMINFOTEST_COMPONENTS})
target_link_libraries(ParamInfoTests drvFGPDBShared ${asyn_LIBRARIES} ${EPICS_LIBRARIES} gmock_main)

set(LCPPROTOCOLTEST_COMPONENTS
  LCPProtocolTests.cpp
)
add_executable(LCPProtocolTests ${LCPPROTOCOLTEST_COMPONENTS})
target_link_libraries(LCPProtocolTests drvFGPDBShared gmock_main)

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
add_unit_tests(LCPProtocolTests)

if(CMAKE_BUILD_TYPE MATCHES Debug)
  setup_target_for_coverage(drvFGPDBTests_coverage drvFGPDBTests drvFGPDBCoverage '*Tests.cpp')
  setup_target_for_coverage(drvFGPDBIntegrationTests_coverage drvFGPDBIntegrationTests drvFGPDBIntegrationCoverage '*Tests.cpp')
  setup_target_for_coverage(ParamInfoTests_coverage ParamInfoTests ParamInfoCoverage '*Tests.cpp')
  setup_target_for_coverage(LCPProtocolTests_coverage LCPProtocolTests LCPProtocolCoverage '*Tests.cpp')
endif(CMAKE_BUILD_TYPE MATCHES Debug)
