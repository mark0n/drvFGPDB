// The tests in this file require the LCP simulator application to be running
// on the same machine.

#include <memory>

#include "gmock/gmock.h"

#define TEST_DRVFGPDB
#include "drvFGPDB.h"
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
};

//=============================================================================
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, readsWithinDefinedRegRange) {
  addParams();

  tie(stat, param) = testDrv->getParamInfo(testParamID_RO);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->readRegs(param.regAddr, 4);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, writesGroupOfSetRegs) {
  addParams();

  ASSERT_THAT(testDrv->getWriteAccess(), Eq(0));

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

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, writesRegValues) {
  addParams();

  ASSERT_THAT(testDrv->getWriteAccess(), Eq(0));

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->writeRegs(param.regAddr, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, processesPendingWrites) {
  addParams();

  ASSERT_THAT(testDrv->getWriteAccess(), Eq(0));

  pasynUser->reason = testParamID_WA;
  stat = testDrv->writeInt32(pasynUser, 222);
  ASSERT_THAT(stat, Eq(asynSuccess));

  auto ackdWrites = testDrv->processPendingWrites();
  ASSERT_THAT(ackdWrites, Eq(1));

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.setState, Eq(SetState::Sent));
}

//-----------------------------------------------------------------------------

