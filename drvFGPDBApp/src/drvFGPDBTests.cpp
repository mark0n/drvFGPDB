#include <memory>

#include "gmock/gmock.h"

#define TEST_DRVFGPDB
#include "drvFGPDB.h"
#include "drvAsynIPPort.h"
#include "asynOctetSyncIOInterface.h"
#include "drvFGPDBTestCommon.h"

//-----------------------------------------------------------------------------
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
  id = addParam("  ");
  ASSERT_THAT(id, Eq(-1));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, rejectsMultipleParamDefinitions) {
  id = addParam("testParam1 0x10001 Float64 F32 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(-1));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canCreateIncompleteParam) {
  id = addParam("testParam1 0x0 Float64");
  ASSERT_THAT(id, Eq(numDrvParams));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canAddAnotherParam) {
  id = addParam("testParam1 0x10000 Int32");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam2 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(numDrvParams+1));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canAddPropertiesToExistingParam) {
  id = addParam("testParam1 0x10001 Int32");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  tie(stat, param) = testDrv->getParamInfo(numDrvParams);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.regAddr,  Eq(0x10001u));
  ASSERT_THAT(param.asynType, Eq(asynParamInt32));
  ASSERT_THAT(param.ctlrFmt,  Eq(CtlrDataFmt::U32));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnParamDefConflict) {
  id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  const char *param1Def = { "testParam1 0x10001 Float64 F32" };
  stat = testDrv->drvUserCreate(pasynUser, param1Def, nullptr, nullptr);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsIfMultParamsWithSameRegAddr) {
  id = addParam("lcpRegWA_1 0x20001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("lcpRegWA_01 0x20001 Int32 U32");
  ASSERT_THAT(id, Eq(-1));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, createsAsynParams) {
  addParams();

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->findParam(param.name.c_str(), &id);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(id, Eq(testParamID_WA));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, canFindParamByName) {
  addParams();

  id = testDrv->findParamByName("sessionID");
  ASSERT_THAT(id , Ge(0));

  tie(stat, param) = testDrv->getParamInfo(id);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.name,  Eq("sessionID"));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, asynAndDriverIDsMatch) {
  addParams();
  int asynID = -1;

  id = testDrv->findParamByName("sessionID");
  stat = testDrv->findParam("sessionID", &asynID);

  ASSERT_THAT(id , Ge(0));
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(id, Eq(asynID));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, createsRegAddrToParamMaps) {
  addParams();

  ASSERT_THAT(testDrv->regGroup[0].paramIDs.size(), Eq(0x0006u));
  ASSERT_THAT(testDrv->regGroup[1].paramIDs.size(), Eq(0x0005u));
  ASSERT_THAT(testDrv->regGroup[2].paramIDs.size(), Eq(0x0100u));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, rangeCheckReturnsValidResults) {
  addParams();

  bool validRange = testDrv->inDefinedRegRange(0, 1000);
  ASSERT_THAT(validRange, Eq(false));

  tie(stat, param) = testDrv->getParamInfo(maxParamID_RO);
  ASSERT_THAT(stat, Eq(asynSuccess));

  uint numRegsRO = param.regAddr - 0x10000;
  validRange = testDrv->inDefinedRegRange(0x10000, numRegsRO);
  ASSERT_THAT(validRange, Eq(true));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteForUnmappedRegValue) {
  addParams();

  stat = testDrv->writeRegs(0x20001, 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnReadOutsideDefinedRegRange) {
  addParams();

  stat = testDrv->readRegs(0x10005, 10);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteWithUnsetRegs) {
  addParams();

  stat = testDrv->writeRegs(0x20002, 4);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWritesOutsideDefinedRegRange) {
  addParams();

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->writeRegs(param.regAddr, 10);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnAttemptToSendReadOnlyRegs) {
  addParams();

  tie(stat, param) = testDrv->getParamInfo(testParamID_RO);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv->writeRegs(param.regAddr, 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteToReadOnlyParam) {
  addParams();

  pasynUser->reason = testParamID_RO;
  stat = testDrv->writeInt32(pasynUser, 42);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/* The writeXxx() functions to NOT do I/O because they would have to block...
TEST_F(AnFGPDBDriver, writesDataToAsyn) {
  EXPECT_CALL(*syncIO, write(pasynUser, _, _)).WillOnce(Return(asynSuccess));
  testDrv->writeInt32(pasynUser, 42);
}
*/

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverUsingIOSyncMock, setsPendingWriteStateForAParam) {
  addParams();
  epicsInt32 arbitraryInt = 42;
  auto arbitraryIntUnsigned = static_cast<epicsUInt32>(arbitraryInt);

  pasynUser->reason = testParamID_WA;
  stat = testDrv->writeInt32(pasynUser, arbitraryInt);
  ASSERT_THAT(stat, Eq(asynSuccess));

  tie(stat, param) = testDrv->getParamInfo(testParamID_WA);
  ASSERT_THAT(param.ctlrValSet, Eq(arbitraryIntUnsigned));
  ASSERT_THAT(param.setState, Eq(SetState::Pending));
}

//-----------------------------------------------------------------------------

