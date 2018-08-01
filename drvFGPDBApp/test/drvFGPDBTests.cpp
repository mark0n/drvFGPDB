/**
 * @file  drvFGPDBTests.cpp
 * @brief Unit tests
 */

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
  MOCK_METHOD4(write, asynStatus(asynUser *pasynUser, writeData outData,
                                 size_t *nbytesOut, double timeout));
  MOCK_METHOD5(read, asynStatus(asynUser *pasynUser, readData inData,
                                size_t *nbytesIn, double timeout,
                                int *eomReason));
  MOCK_METHOD7(writeRead, asynStatus(asynUser *pasynUser, writeData outData,
                                     size_t *nbytesOut, readData inData,
                                     size_t *nbytesIn, double timeout,
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
  MOCK_METHOD6(writeOnce, asynStatus(const char *port, int addr,
                                     writeData outData, size_t *nbytesOut,
                                     double timeout, const char *drvInfo));
  MOCK_METHOD7(readOnce, asynStatus(const char *port, int addr, readData inData,
                                    size_t *nbytesIn, double timeout,
                                    int *eomReason, const char *drvInfo));
  MOCK_METHOD9(writeReadOnce, asynStatus(const char *port, int addr,
                                         writeData outData, size_t *nbytesOut,
                                         readData inData, size_t *nbytesIn,
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
    testDrv = make_unique<drvFGPDB>(drvName, syncIO, UDPPortName,
                                    startupDiagFlags,
                                    ResendMode::AfterCtlrRestart);

    if (udpPortStat)
      cout << drvName << " unable to create asyn UDP port: " << UDPPortName
           << endl << endl;

    numDrvParams = testDrv->numParams();
  };

  static const vector<uint32_t> rd_returnData;

  static asynStatus read_mock_helper(Unused, readData inData, size_t *nbytesIn,
                                     Unused, Unused);
};

//-----------------------------------------------------------------------------
/**
 * @brief UDP/IP connection is configured successfully
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, canBeConstructedWithoutAnyErrors) {
  ASSERT_THAT(udpPortStat, Eq(0));
}

//-----------------------------------------------------------------------------
/**
 * @brief Driver instance has params registered
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, newDriverInstanceContainsDriverParams) {
  ASSERT_THAT(numDrvParams, Gt(0));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not add empty parameters definition
 * @note  Formats allowed are:
 *        - name addr asynType ctlrFmt.
 *        - name addr chipID blockSize eraseReq offset length statusName.
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, rejectsEmptyParamDef) {
  id = addParam("  ");
  ASSERT_THAT(id, Eq(-1));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not add multiple param definitions
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, rejectsMultipleParamDefinitions) {
  id = addParam("testParam1 0x10001 Float64 F32 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(-1));
}

//-----------------------------------------------------------------------------
/**
 * @brief Add incomplete param definitions
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, canCreateIncompleteParam) {
  id = addParam("testParam1 0x0 Float64");
  ASSERT_THAT(id, Eq(numDrvParams));
}

//-----------------------------------------------------------------------------
/**
 * @brief Add another param definition
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, canAddAnotherParam) {
  id = addParam("testParam1 0x10000 Int32");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam2 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(numDrvParams+1));
}

//-----------------------------------------------------------------------------
/*
 * @brief Add properties to existing param
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, canAddPropertiesToExistingParam) {
  id = addParam("testParam1 0x10001 Int32");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  ParamInfo param = testDrv->getParamInfo(numDrvParams);

  ASSERT_THAT(param.getRegAddr(),  Eq(0x10001u));
  ASSERT_THAT(param.getAsynType(), Eq(asynParamInt32));
  ASSERT_THAT(param.getCtlrFmt(),  Eq(CtlrDataFmt::U32));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not modify existing param definitions calling drvUserCreate
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnParamDefConflict) {
  id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  const char *param1Def = { "testParam1 0x10001 Float64 F32" };
  stat = testDrv->drvUserCreate(pasynUser, param1Def, nullptr, nullptr);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not add different params with same address
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsIfMultParamsWithSameRegAddr) {
  id = addParam("lcpRegWA_1 0x20001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("lcpRegWA_01 0x20001 Int32 U32");
  ASSERT_THAT(id, Eq(-1));
}

//-----------------------------------------------------------------------------
/**
 * @brief Create new asyn params
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, createsAsynParams) {
  addParams();

  ParamInfo param = testDrv->getParamInfo(testParamID_WA);

  stat = testDrv->findParam(param.name.c_str(), &id);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(id, Eq(testParamID_WA));
}

//-----------------------------------------------------------------------------
/**
 * @brief Find a registered param by its name
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, canFindParamByName) {
  addParams();

  id = testDrv->findParamByName("sessionID");
  ASSERT_THAT(id , Ge(0));

  ParamInfo param = testDrv->getParamInfo(id);
  ASSERT_THAT(param.name,  Eq("sessionID"));
}

//-----------------------------------------------------------------------------
/**
 * @brief Param's, asynID and driverID, are equal
 */
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
/**
 * @brief All registered param's addresses are correctly mapped
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, createsRegAddrToParamMaps) {
  addParams();

  ASSERT_THAT(testDrv->procGroupSize(ProcGroup_LCP_RO), Eq(RO_groupSize));
  ASSERT_THAT(testDrv->procGroupSize(ProcGroup_LCP_WA), Eq(WA_groupSize));
  ASSERT_THAT(testDrv->procGroupSize(ProcGroup_LCP_WO), Eq(WO_groupSize));
}

//-----------------------------------------------------------------------------
/**
 * @brief Check if given params range is a valid LCP params range
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, rangeCheckReturnsValidResults) {
  addParams();

  bool validRange = testDrv->inDefinedRegRange(0, 1000);
  ASSERT_THAT(validRange, Eq(false));

  ParamInfo param = testDrv->getParamInfo(maxParamID_RO);

  uint numRegsRO = param.getRegAddr() - 0x10000;
  validRange = testDrv->inDefinedRegRange(0x10000, numRegsRO);
  ASSERT_THAT(validRange, Eq(true));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not write in a non registered address
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteForUnmappedRegValue) {
  addParams();

  stat = testDrv->writeRegs(0x20001, 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not read outside of a registered params range
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnReadOutsideDefinedRegRange) {
  addParams();

  stat = testDrv->readRegs(0x10005, 10);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not write multiple params if they are not all registered
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWriteWithUnsetRegs) {
  addParams();

  stat = testDrv->writeRegs(0x20002, 4);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not write outside of a registered params range
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnWritesOutsideDefinedRegRange) {
  addParams();

  ParamInfo param = testDrv->getParamInfo(testParamID_WA);

  stat = testDrv->writeRegs(param.getRegAddr(), lastRegID_WA - 0x20000 + 2);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not write a Read-Only param
 */
TEST_F(AnFGPDBDriverUsingIOSyncMock, failsOnAttemptToSendReadOnlyRegs) {
  addParams();

  ParamInfo param = testDrv->getParamInfo(testParamID_RO);

  stat = testDrv->writeRegs(param.getRegAddr(), 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
/**
 * @brief Do not write through asyn layer a Read-Only param
 */
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
TEST_F(AnFGPDBDriverUsingIOSyncMock, passesMsgToAsyn) {
  vector<uint32_t> arbitraryData { 42, 43, 44 };
  size_t bytesToBeSent = arbitraryData.size() * sizeof(arbitraryData[0]);
  writeData outData {
    .write_buffer = reinterpret_cast<char *>(arbitraryData.data()),
    .write_buffer_len = bytesToBeSent,
  };
  EXPECT_CALL(
    *static_pointer_cast<asynOctetSyncIOWrapperMock>(syncIO),
    write(pasynUser, outData, _, testDrv->writeTimeout)
  ).WillOnce(DoAll(SetArgPointee<2>(bytesToBeSent), Return(asynSuccess)));

  stat = testDrv->sendMsg(pasynUser, arbitraryData);

  ASSERT_THAT(stat, Eq(asynSuccess));
}

const vector<uint32_t> AnFGPDBDriverUsingIOSyncMock::rd_returnData =
    { 42, 43, 45, 46 };

asynStatus AnFGPDBDriverUsingIOSyncMock::read_mock_helper(Unused,
                                                          readData inData,
                                                          size_t *nbytesIn,
                                                          Unused, Unused)
{
  *nbytesIn = min(rd_returnData.size() * sizeof(rd_returnData[0]),
                  inData.read_buffer_len); // don't exceed read buffer size!
  memcpy(inData.read_buffer, rd_returnData.data(), *nbytesIn);
  return asynSuccess;
}

TEST_F(AnFGPDBDriverUsingIOSyncMock, receivesMsgFromAsyn) {
  vector<uint32_t> respBuf(10); // plenty of space
  EXPECT_CALL(
    *static_pointer_cast<asynOctetSyncIOWrapperMock>(syncIO),
    read(pasynUser, _, _, testDrv->readTimeout, _)
  ).WillOnce(Invoke(read_mock_helper));

  int bytesRead = testDrv->readResp(pasynUser, respBuf);

  ASSERT_THAT(bytesRead, Eq(rd_returnData.size() * sizeof(rd_returnData[0])));
}

TEST_F(AnFGPDBDriverUsingIOSyncMock, detectsTrunkatedMsgFromAsyn) {
  vector<uint32_t> respBuf(3); // too small
  EXPECT_CALL(
    *static_pointer_cast<asynOctetSyncIOWrapperMock>(syncIO),
    read(pasynUser, _, _, testDrv->readTimeout, _)
  ).WillOnce(Invoke(read_mock_helper));

  int bytesRead = testDrv->readResp(pasynUser, respBuf);

  ASSERT_THAT(bytesRead, Eq(respBuf.size() * sizeof(respBuf[0])));
}
