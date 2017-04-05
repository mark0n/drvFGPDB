#include <memory>

#include "gmock/gmock.h"

#define TEST_DRVFGPDB
#include "drvFGPDB.h"
#include "drvAsynIPPort.h"
#include "asynOctetSyncIOInterface.h"
#include "drvFGPDBTestCommon.h"

class asynOctetSyncIOWrapperMock: public asynOctetSyncIOInterface {
public:
  MOCK_METHOD4(connect, asynStatus(const char *port, int addr,
                                   asynUser **ppasynUser, const char *drvInfo));
  MOCK_METHOD1(disconnect, asynStatus(asynUser *pasynUser));
  MOCK_METHOD3(write, asynStatus(asynUser *pasynUser, writeData outData,
                                 double timeout));
  MOCK_METHOD4(read, asynStatus(asynUser *pasynUser, readData inData,
                                double timeout, int *eomReason));
  MOCK_METHOD5(writeRead, asynStatus(asynUser *pasynUser, writeData outData,
                                     readData inData, double timeout,
                                     int *eomReason));
  MOCK_METHOD1(flush, asynStatus(asynUser *pasynUser));
  MOCK_METHOD3(setInputEos, asynStatus(asynUser *pasynUser, const char *eos,
                                       int eoslen));
  MOCK_METHOD4(getInputEos, asynStatus(asynUser *pasynUser, char *eos,
                                       int eossize, int *eoslen));
  MOCK_METHOD3(setOutputEos, asynStatus(asynUser *pasynUser, const char *eos,
                                        int eoslen));
  MOCK_METHOD4(getOutputEos, asynStatus(asynUser *pasynUser, char *eos,
                                        int eossize, int *eoslen));
  MOCK_METHOD5(writeOnce, asynStatus(const char *port, int addr,
                                     writeData outData, double timeout,
                                     const char *drvInfo));
  MOCK_METHOD6(readOnce, asynStatus(const char *port, int addr, readData inData,
                                    double timeout, int *eomReason,
                                    const char *drvInfo));
  MOCK_METHOD7(writeReadOnce, asynStatus(const char *port, int addr,
                                         writeData outData, readData inData,
                                         double timeout, int *eomReason,
                                         const char *drvInfo));
  MOCK_METHOD3(flushOnce, asynStatus(const char *port, int addr,
                                     const char *drvInfo));

  MOCK_METHOD5(setInputEosOnce, asynStatus(const char *port, int addr,
                                           const char *eos, int eoslen,
                                           const char *drvInfo));
  MOCK_METHOD6(getInputEosOnce, asynStatus(const char *port, int addr,
                                           char *eos, int eossize, int *eoslen,
                                           const char *drvInfo));
  MOCK_METHOD5(setOutputEosOnce, asynStatus(const char *port, int addr,
                                            const char *eos, int eoslen,
                                            const char *drvInfo));
  MOCK_METHOD6(getOutputEosOnce, asynStatus(const char *port, int addr,
                                            char *eos, int eossize, int *eoslen,
                                            const char *drvInfo));
};

//-----------------------------------------------------------------------------
class AnFGPDBDriverUsingIOSyncMock : public AnFGPDBDriver
{
public:
  AnFGPDBDriverUsingIOSyncMock() :
    AnFGPDBDriver(make_shared<asynOctetSyncIOWrapperMock>())
  {
    EXPECT_CALL(*static_pointer_cast<asynOctetSyncIOWrapperMock>(syncIO),
                connect(_, _, _, _)).WillOnce(Return(asynSuccess));
    EXPECT_CALL(*static_pointer_cast<asynOctetSyncIOWrapperMock>(syncIO),
                disconnect(_)).WillOnce(Return(asynSuccess));
    testDrv = make_unique<drvFGPDB>(drvName, syncIO, UDPPortName, startupDiagFlags);

    if (udpPortStat)
      cout << drvName << " unable to create asyn UDP port: " << UDPPortName
           << endl << endl;

    numDrvParams = testDrv->numParams();
  };
};

//-----------------------------------------------------------------------------
class AnFGPDBDriverUsingIOSyncMockWithAParameter: public AnFGPDBDriverUsingIOSyncMock {
public:
  virtual void SetUp() {
    auto stat = addParam("lcpRegWA_1 0x20001 Int32 U32");
    ASSERT_THAT(stat, Eq(numDrvParams));
  }
};

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canBeConstructedWithoutAnyErrors) {
  ASSERT_THAT(udpPortStat, Eq(0));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, launchesSyncComThread) {
  ASSERT_THAT(testDrv->syncThread.joinable(), Eq(true));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, newDriverInstanceContainsDriverParams) {
  ASSERT_THAT(numDrvParams, Gt(0));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, rejectsEmptyParamDef) {
  const char *paramDesc = { " " };

  ASSERT_ANY_THROW(testDrv->drvUserCreate(pasynUser, paramDesc, nullptr,
                                         nullptr));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, rejectsMultipleParamDefinitions) {
  const char *paramDesc = "testParam1 0x10001 Float64 F32 0x10001 Float64 F32";

  ASSERT_ANY_THROW(testDrv->drvUserCreate(pasynUser, paramDesc, nullptr,
                                         nullptr));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canCreateIncompleteParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(numDrvParams));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canAddAnotherParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam2 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(numDrvParams+1));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canAddPropertiesToExistingParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  asynStatus stat;

  tie(stat, param) = testDrv->getParamInfo(numDrvParams);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.regAddr,  Eq(0x10001));
  ASSERT_THAT(param.asynType, Eq(asynParamInt32));
  ASSERT_THAT(param.ctlrFmt,  Eq(CtlrDataFmt::U32));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnParamDefConflict) {
  int id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  const char *param1Def = { "testParam1 0x10001 Float64 F32" };
  auto stat = testDrv->drvUserCreate(pasynUser, param1Def, nullptr, nullptr);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, createsAsynParams) {
  addParams();

  auto stat = testDrv->createAsynParams();
  ASSERT_THAT(stat, Eq(asynSuccess));

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));

  int paramID;
  stat = testDrv->findParam(param.name.c_str(), &paramID);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(paramID, Eq(testParamID_WA));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, rangeCheckReturnsValidResults) {
  createAddrToParamMaps();

  bool validRange = testDrv->inDefinedRegRange(0, 1000);
  ASSERT_THAT(validRange, Eq(false));

  asynStatus stat;

  tie(stat, param) = testDrv->getParamInfo(maxParamID_RO);
  ASSERT_THAT(stat, Eq(asynSuccess));

  uint numRegsRO = param.regAddr - 0x10000;
  validRange = testDrv->inDefinedRegRange(0x10000, numRegsRO);
  ASSERT_THAT(validRange, Eq(true));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteForUnmappedRegValue) {
  createAddrToParamMaps();

  auto stat = testDrv->writeRegs(0x20001, 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnReadOutsideDefinedRegRange) {
  createAddrToParamMaps();

  auto stat = testDrv->readRegs(0x10005, 10);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteWithUnsetRegs) {
  createAddrToParamMaps();

  auto stat = testDrv->writeRegs(0x20002, 4);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWritesOutsideDefinedRegRange) {
  createAddrToParamMaps();
  asynStatus stat;

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->writeRegs(param.regAddr, 10);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnAttemptToSendReadOnlyRegs) {
  createAddrToParamMaps();
  asynStatus stat;

  tie(stat, param) = testDrv->getParamInfo(testParamID_RO);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->writeRegs(param.regAddr, 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteToReadOnlyParam) {
  createAddrToParamMaps();

  pasynUser->reason = testParamID_RO;
  auto stat = testDrv->writeInt32(pasynUser, 42);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMockWithAParameter, failsIfMultParamsWithSameRegAddr) {
  auto stat = addParam("lcpRegWA_01 0x20001 Int32 U32");
  ASSERT_THAT(stat, Eq(numDrvParams+1));

  stat = testDrv->determineAddrRanges();
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->createAddrToParamMaps();
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/* The writeXxx() functions to NOT do I/O because they would have to block...
TEST_F(AnFGPDBDriver, writesDataToAsyn) {
  EXPECT_CALL(*syncIO, write(pasynUser, _, _)).WillOnce(Return(asynSuccess));
  testDrv.writeInt32(pasynUser, 42);
}
*/

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, determineAddrRanges) { determineAddrRanges(); }

TEST_F(AnFGPDBDriverUsingIOSyncMock, createAddrToParamMaps) { createAddrToParamMaps(); }

TEST_F(AnFGPDBDriverUsingIOSyncMock, setsPendingWriteStateForAParam) {
  setsPendingWriteStateForAParam();
}
