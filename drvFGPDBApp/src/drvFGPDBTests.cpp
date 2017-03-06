
#include "gmock/gmock.h"

#define TEST_DRVFGPDB
#include "drvFGPDB.h"
#include "drvAsynIPPort.h"
#include "asynOctetSyncIOInterface.h"

using namespace testing;
using namespace std;


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

static int testNum = 0;

#define UDPPortName  "FGPDB_com"

//=============================================================================
int createPortUDP(void)
{
  static int  stat = 0;

  if (testNum == 1)
    stat = drvAsynIPPortConfigure(UDPPortName, "127.0.0.1:2005 udp", 0, 0, 1);

  return stat;
}

//=============================================================================
class AnFGPDBDriver: public ::testing::Test
{
public:
  AnFGPDBDriver() :
    pasynUser(pasynManager->createAsynUser(nullptr, nullptr)),
    syncIO(new asynOctetSyncIOWrapperMock),
    drvName("testDriver" + std::to_string(++testNum)),
    udpPortStat(createPortUDP()),  // Must be created before drvFGPDB object
    testDrv(drvFGPDB(drvName, syncIO, UDPPortName, maxParams)),
    testParamID_RO(-1),
    maxParamID_RO(-1),
    testParamID_WA(-1)
  {
    if (udpPortStat)
      cout << drvName << " unable to create asyn UDP port: " << UDPPortName
           << endl << endl;

    numDrvParams = testDrv.paramList.size();
  };

  //---------------------------------------------
  ~AnFGPDBDriver() { pasynManager->freeAsynUser(pasynUser); };

  //---------------------------------------------
  int addParam(string paramStr) {
    asynStatus stat;
    pasynUser->reason = -1;
    stat = testDrv.drvUserCreate(pasynUser, paramStr.c_str(), nullptr, nullptr);
    return (stat == asynSuccess) ? pasynUser->reason : -1;
  }

  //---------------------------------------------
  void addParams()  {
    // Np params for addrs 0x10000 - 0x10001
    auto stat = addParam("lcpRegRO_1 0x10002 Int32 U32");
    ASSERT_THAT(stat, Eq(numDrvParams));  testParamID_RO = stat;
    // No params for addrs 0x10003 - 0x10004
    stat = addParam("lcpRegRO_5 0x10005 Float64 F32");
    ASSERT_THAT(stat, Eq(numDrvParams+1));  maxParamID_RO = stat;

    stat = addParam("lcpRegWA_1 0x20000 Int32 U32");
    ASSERT_THAT(stat, Eq(numDrvParams+2));
    //No param defined for addr 0x20001
    stat = addParam("lcpRegWA_2 0x20002 Int32 U32");
    ASSERT_THAT(stat, Eq(numDrvParams+3));  testParamID_WA = stat;
    stat = addParam("lcpRegWA_3 0x20003 Int32 U32");
    ASSERT_THAT(stat, Eq(numDrvParams+4));
    stat = addParam("lcpRegWA_4 0x20004 Float64 F32");
    ASSERT_THAT(stat, Eq(numDrvParams+5));

    stat = addParam("lcpRegWO_2 0x30002 Int32 U32");
    ASSERT_THAT(stat, Eq(numDrvParams+6));
  }

  //---------------------------------------------
  void determineAddrRanges()  {
    addParams();

    auto stat = testDrv.createAsynParams();
    ASSERT_THAT(stat, Eq(asynSuccess));

    stat = testDrv.determineAddrRanges();
    ASSERT_THAT(stat, Eq(asynSuccess));

    ASSERT_THAT(testDrv.regGroup[0].maxOffset, Eq(5));
    ASSERT_THAT(testDrv.regGroup[1].maxOffset, Eq(4));
    ASSERT_THAT(testDrv.regGroup[2].maxOffset, Eq(2));
  }

  //---------------------------------------------
  void createAddrToParamMaps()  {
    determineAddrRanges();

    auto stat = testDrv.createAddrToParamMaps();
    ASSERT_THAT(stat, Eq(asynSuccess));

    ASSERT_THAT(testDrv.regGroup[0].paramIDs.size(), Eq(6));
    ASSERT_THAT(testDrv.regGroup[1].paramIDs.size(), Eq(5));
    ASSERT_THAT(testDrv.regGroup[2].paramIDs.size(), Eq(3));
  }

//---------------------------------------------
  void setsPendingWriteStateForAParam()  {
    createAddrToParamMaps();

    pasynUser->reason = testParamID_WA;
    auto stat = testDrv.writeInt32(pasynUser, 42);
    ASSERT_THAT(stat, Eq(asynSuccess));

    stat = testDrv.getParamInfo(testParamID_WA, param);
    ASSERT_THAT(param.ctlrValSet, Eq(42));
    ASSERT_THAT(param.setState, Eq(SetState::Pending));
  }

  //---------------------------------------------
  const int  maxParams = 200;
  asynUser  *pasynUser;
  shared_ptr<asynOctetSyncIOWrapperMock>  syncIO;
  std::string  drvName;
  int  udpPortStat;
  drvFGPDB  testDrv;
  int  testParamID_RO, maxParamID_RO, testParamID_WA;
  int  numDrvParams;
  ParamInfo  param;
};


//=============================================================================
class AnFGPDBDriverWithAParameter: public AnFGPDBDriver {
public:
  virtual void SetUp() {
    auto stat = addParam("lcpRegWA_1 0x20001 Int32 U32");
    ASSERT_THAT(stat, Eq(numDrvParams));
  }
};



//=============================================================================
TEST_F(AnFGPDBDriver, canBeConstructedWithoutAnyErrors) {
  ASSERT_THAT(udpPortStat, Eq(0));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, newDriverInstanceContainsDriverParams) {
  ASSERT_THAT(numDrvParams, Gt(0));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, rejectsEmptyParamDef) {
  const char *paramDesc = { " " };

  ASSERT_ANY_THROW(testDrv.drvUserCreate(pasynUser, paramDesc, nullptr,
                                         nullptr));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, rejectsMultipleParamDefinitions) {
  const char *paramDesc = "testParam1 0x10001 Float64 F32 0x10001 Float64 F32";

  ASSERT_ANY_THROW(testDrv.drvUserCreate(pasynUser, paramDesc, nullptr,
                                         nullptr));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canCreateIncompleteParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(numDrvParams));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddAnotherParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam2 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(numDrvParams+1));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddPropertiesToExistingParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(numDrvParams));

  id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  asynStatus stat = testDrv.getParamInfo(numDrvParams, param);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.regAddr,  Eq(0x10001));
  ASSERT_THAT(param.asynType, Eq(asynParamInt32));
  ASSERT_THAT(param.ctlrFmt,  Eq(CtlrDataFmt::U32));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnParamDefConflict) {
  int id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(numDrvParams));

  const char *param1Def = { "testParam1 0x10001 Float64 F32" };
  auto stat = testDrv.drvUserCreate(pasynUser, param1Def, nullptr, nullptr);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, createsAsynParams) {
  addParams();

  auto stat = testDrv.createAsynParams();
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.getParamInfo(testParamID_WA, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  int paramID;
  stat = testDrv.findParam(param.name.c_str(), &paramID);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(paramID, Eq(testParamID_WA));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, rangeCheckReturnsValidResults) {
  createAddrToParamMaps();

  bool validRange = testDrv.inDefinedRegRange(0, 10);
  ASSERT_THAT(validRange, Eq(false));

  auto stat = testDrv.getParamInfo(maxParamID_RO, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  uint numRegsRO = param.regAddr - 0x10000;
  validRange = testDrv.inDefinedRegRange(0x10000, numRegsRO);
  ASSERT_THAT(validRange, Eq(true));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnWriteForUnmappedRegValue) {
  createAddrToParamMaps();

  auto stat = testDrv.writeRegs(0x20001, 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnReadOutsideDefinedRegRange) {
  createAddrToParamMaps();

  auto stat = testDrv.readRegs(0x10005, 10);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnWriteWithUnsetRegs) {
  createAddrToParamMaps();

  auto stat = testDrv.writeRegs(0x20002, 4);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnWritesOutsideDefinedRegRange) {
  createAddrToParamMaps();

  auto stat = testDrv.getParamInfo(testParamID_WA, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.writeRegs(param.regAddr, 10);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnAttemptToSendReadOnlyRegs) {
  createAddrToParamMaps();

  auto stat = testDrv.getParamInfo(testParamID_RO, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.writeRegs(param.regAddr, 1);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnWriteToReadOnlyParam) {
  createAddrToParamMaps();

  pasynUser->reason = testParamID_RO;
  auto stat = testDrv.writeInt32(pasynUser, 42);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriverWithAParameter, failsIfMultParamsWithSameRegAddr) {
  auto stat = addParam("lcpRegWA_01 0x20001 Int32 U32");
  ASSERT_THAT(stat, Eq(numDrvParams+1));

  stat = testDrv.determineAddrRanges();
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.createAddrToParamMaps();
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
TEST_F(AnFGPDBDriver, determineAddrRanges) { determineAddrRanges(); }

TEST_F(AnFGPDBDriver, createAddrToParamMaps) { createAddrToParamMaps(); }



//=============================================================================
// NOTE: The follow tests all require the LCP simulator appl to be running on
// the same machine
//=============================================================================
TEST_F(AnFGPDBDriver, setsPendingWriteStateForAParam) {
  setsPendingWriteStateForAParam();
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, readsWithinDefinedRegRange) {
  createAddrToParamMaps();

  auto stat = testDrv.getParamInfo(testParamID_RO, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.readRegs(param.regAddr, 4);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, writesGroupOfSetRegs) {
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
// Test writing LCP register values
// NOTE: This requires the LCP simulator appl to be running on the same mach
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, writeRegValues) {
  createAddrToParamMaps();

  auto stat = testDrv.getParamInfo(testParamID_WA, param);
  ASSERT_THAT(stat, Eq(asynSuccess));

  stat = testDrv.writeRegs(param.regAddr, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, processesPendingWrites) {
  setsPendingWriteStateForAParam();

  auto ackdWrites = testDrv.processPendingWrites();
  ASSERT_THAT(ackdWrites, Eq(1));

  auto stat = testDrv.getParamInfo(testParamID_WA, param);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.setState, Eq(SetState::Sent));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, launchesSyncComThread) {
  for (int i=0; i<200; ++i)  {
    if (testDrv.syncThreadInitialized)  break;
    epicsThreadSleep(0.010);
  }
  ASSERT_THAT(testDrv.syncThreadInitialized, Eq(true));
}

//-----------------------------------------------------------------------------

