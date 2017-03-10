// The tests in this file require the LCP simulator application to be running
// on the same machine.

#include <memory>

#include "gmock/gmock.h"

#define TEST_DRVFGPDB
#include "drvFGPDB.h"
#include "drvAsynIPPort.h"
#include "asynOctetSyncIOWrapper.h"
#include "drvFGPDBTestCommon.h"

class AnFGPDBDriverUsingIOSyncWrapper : public AnFGPDBDriver
{
public:
  AnFGPDBDriverUsingIOSyncWrapper() : AnFGPDBDriver(make_shared<asynOctetSyncIOWrapper>()) {};
};

TEST_F(AnFGPDBDriverUsingIOSyncWrapper, readsWithinDefinedRegRange) {
  createAddrToParamMaps();

  auto stat = testDrv.getParamInfo(testParamID_RO, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.readRegs(param.regAddr, 4);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, writesGroupOfSetRegs) {
  createAddrToParamMaps();

  pasynUser->reason = testParamID_WA;
  auto stat = testDrv.writeInt32(pasynUser, 222);
  ASSERT_THAT(stat, Eq(asynSuccess));

  pasynUser->reason = testParamID_WA + 1;
  stat = testDrv.writeInt32(pasynUser, 333);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.getParamInfo(testParamID_WA, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.writeRegs(param.regAddr, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, writeRegValues) {
  createAddrToParamMaps();

  auto stat = testDrv.getParamInfo(testParamID_WA, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.writeRegs(param.regAddr, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncWrapper, processesPendingWrites) {
  setsPendingWriteStateForAParam();

  auto ackdWrites = testDrv.processPendingWrites();
  ASSERT_THAT(ackdWrites, Eq(1));

  auto stat = testDrv.getParamInfo(testParamID_WA, param);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.setState, Eq(SetState::Sent));
}
