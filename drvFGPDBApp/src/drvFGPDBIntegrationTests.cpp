// The tests in this file require the LCP simulator application to be running
// on the same machine.

#include "gmock/gmock.h"

#define TEST_DRVFGPDB
#include "drvFGPDB.h"
#include "drvAsynIPPort.h"
#include "asynOctetSyncIOWrapper.h"

using namespace testing;
using namespace std;

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
    syncIO(new asynOctetSyncIOWrapper),
    drvName("testDriver" + std::to_string(++testNum)),
    udpPortStat(createPortUDP()),  // Must be created before drvFGPDB object
    testDrv(drvName, syncIO, UDPPortName, maxParams),
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
  shared_ptr<asynOctetSyncIOWrapper>  syncIO;
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
