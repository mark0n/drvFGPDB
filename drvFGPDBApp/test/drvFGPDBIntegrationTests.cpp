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


//=============================================================================
class AnFGPDBDriverUsingIOSyncWrapper : public AnFGPDBDriver
{
public:
  AnFGPDBDriverUsingIOSyncWrapper() :
    AnFGPDBDriver(make_shared<asynOctetSyncIOWrapper>())
  {
    testDrv = make_unique<drvFGPDB>(drvName, syncIO, UDPPortName, startupDiagFlags);

    if (udpPortStat)
      cout << drvName << " unable to create asyn UDP port: " << UDPPortName
           << endl << endl;

    numDrvParams = testDrv->numParams();
  };

  //----------------------------------------
  void initConnection()
  {
    testDrv->readRegs(0x10000, testDrv->procGroupSize(ProcGroup_LCP_RO));
    testDrv->readRegs(0x20000, testDrv->procGroupSize(ProcGroup_LCP_WA));

    testDrv->postNewReadValues();
    testDrv->checkComStatus();

    ASSERT_THAT(testDrv->connected, Eq(true));

    ASSERT_THAT(testDrv->getWriteAccess(), Eq(0));

    testDrv->startCommunication();
  }

};

/**
 * @brief Read registers in a registered range
 */
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, readsWithinDefinedRegRange) {
  addParams();

  tie(stat, param) = testDrv->getParamInfo(testParamID_RO);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->readRegs(param.regAddr, 4);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

/**
 * @brief Write a group of registers previously set by asyn interface
 */
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, writesGroupOfSetRegs) {
  addParams();

  initConnection();

  pasynUser->reason = testParamID_WA;
  stat = testDrv->writeInt32(pasynUser, 222);
  ASSERT_THAT(stat, Eq(asynSuccess));

  pasynUser->reason = testParamID_WA + 1;
  stat = testDrv->writeInt32(pasynUser, 333);
  ASSERT_THAT(stat, Eq(asynSuccess));

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->writeRegs(param.regAddr, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

/**
 * @brief  Write a group of registers
 */
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, writesRegValues) {
  addParams();

  initConnection();

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->writeRegs(param.regAddr, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

/*
 * @brief param's setState is set to pending due to an asyn write call
 */
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, setsPendingWriteStateForAParam) {
  addParams();

  initConnection();

  epicsInt32 arbitraryInt = 42;
  auto arbitraryIntUnsigned = static_cast<epicsUInt32>(arbitraryInt);

  pasynUser->reason = testParamID_WA;
  stat = testDrv->writeInt32(pasynUser, arbitraryInt);
  ASSERT_THAT(stat, Eq(asynSuccess));

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(param.ctlrValSet, Eq(arbitraryIntUnsigned));
  ASSERT_THAT(param.setState, Eq(SetState::Pending));
}

/**
 * @brief Proccess pending writes of params previously set
 */
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, processesPendingWrites) {
  addParams();

  initConnection();

  pasynUser->reason = testParamID_WA;
  stat = testDrv->writeInt32(pasynUser, 222);
  ASSERT_THAT(stat, Eq(asynSuccess));

  auto ackdWrites = testDrv->processPendingWrites();
  ASSERT_THAT(ackdWrites, Eq(1));

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.setState, Eq(SetState::Sent));
}


/**
 * @brief Process an array write param and check its final status
 */
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, writeInt8Array) {
  addParams();

  initConnection();

  epicsInt8 testData[1024];
  pasynUser->reason = testArrayID;
  stat = testDrv->writeInt8Array(pasynUser, testData, sizeof(testData));
  ASSERT_THAT(stat, Eq(asynSuccess));

  testDrv->processPendingWrites();

  pasynUser->reason = arrayWriteStatusID;
  int32_t  percDone;

  for (int i=0; i<20; ++i) {
    testDrv->updateReadValues();
    stat = testDrv->postNewReadValues();
    ASSERT_THAT(stat, Eq(asynSuccess));

    stat = testDrv->readInt32(pasynUser, &percDone);
    ASSERT_THAT(stat, Eq(asynSuccess));
    if (percDone == 100)  break;

    testDrv->processPendingWrites();
  }

  ASSERT_THAT(percDone, Eq(100));
}

//-----------------------------------------------------------------------------

