/**
 * @file  drvFGPDBIntegrationTests.cpp
 * @brief Integration tests
 * @note  The tests in this file require the LCP simulator application to be running
          on the same machine.
 */
#include <memory>

#include "gmock/gmock.h"

#define TEST_DRVFGPDB
#include "drvFGPDB.h"
#include "LCPProtocol.h"
#include "drvAsynIPPort.h"
#include "asynOctetSyncIOWrapper.h"
#include "drvFGPDBTestCommon.h"

#include <lcpsimulatorClass.hpp>
#include <diagFlagsClass.hpp>

const std::string localIPAddr="0.0.0.0";
const unsigned short localPort=2005;
