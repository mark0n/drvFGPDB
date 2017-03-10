#ifndef DRVFGPDBTESTCOMMON_H
#define DRVFGPDBTESTCOMMON_H

using namespace testing;
using namespace std;

static int testNum = 0;

const string UDPPortName("FGPDB_com");

//=============================================================================
int createPortUDP(void)
{
  // Asyn does not provide a way to destroy a drvAsynIPPort, yet. For now we
  // only create it once and reuse it for all our tests. As soon as Asyn
  // supports destroying it we should make our tests 100% independent again.
  static int stat = drvAsynIPPortConfigure(UDPPortName.c_str(), "127.0.0.1:2005 udp", 0, 0, 1);

  return stat;
}

//=============================================================================

class AnFGPDBDriver: public ::testing::Test
{
public:
  AnFGPDBDriver(shared_ptr<asynOctetSyncIOInterface> syncIOIn) :
    pasynUser(pasynManager->createAsynUser(nullptr, nullptr)),
    syncIO(syncIOIn),
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
  shared_ptr<asynOctetSyncIOInterface>  syncIO;
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

#endif // DRVFGPDBTESTCOMMON_H