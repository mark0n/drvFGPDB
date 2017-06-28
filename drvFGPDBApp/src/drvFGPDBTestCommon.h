#ifndef DRVFGPDBTESTCOMMON_H
#define DRVFGPDBTESTCOMMON_H

#include <memory>

using namespace testing;
using namespace std;


static int testNum = 0;

const string UDPPortName("FGPDB_com");

const uint32_t startupDiagFlags = TestMode_;

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
    testParamID_RO(-1),
    maxParamID_RO(-1),
    testParamID_WA(-1),
    arrayWriteStatusID(-1),
    testArrayID(-1),
    RO_groupSize(0),
    WA_groupSize(0),
    WO_groupSize(0)
  {};

  //---------------------------------------------
  ~AnFGPDBDriver() { pasynManager->freeAsynUser(pasynUser); };

  //---------------------------------------------
  int addParam(string paramStr) {
    pasynUser->reason = -1;
    stat = testDrv->drvUserCreate(pasynUser, paramStr.c_str(), nullptr, nullptr);
    return (stat == asynSuccess) ? pasynUser->reason : -1;
  }

  //---------------------------------------------
  void addParams()  {
    // No params for addrs 0x10000 - 0x10001
    id = addParam("lcpRegRO_1 0x10002 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams));  testParamID_RO = id;
    // No params for addr 0x10003
    id = addParam("lcpRegRO_4 0x10004 Float64 F32");
    ASSERT_THAT(id, Eq(numDrvParams+1));  maxParamID_RO = id;

    // Set addr for required RO regs already added by driver
    addParam("upSecs 0x10005 Int32 U32");
    addParam("upMs 0x10006 Int32 U32");
    addParam("writerIP 0x10007 Int32 U32");
    id = addParam("writerPort 0x10008 Int32 U32");
    RO_groupSize = 0x0009u;

    id = addParam("lcpRegWA_1 0x20000 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+2));
    //No param defined for addr 0x20001
    id = addParam("lcpRegWA_2 0x20002 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+3));  testParamID_WA = id;
    id = addParam("lcpRegWA_3 0x20003 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+4));
    id = addParam("lcpRegWA_4 0x20004 Float64 F32");
    ASSERT_THAT(id, Eq(numDrvParams+5));  lastRegID_WA = id;
    WA_groupSize = 0x0005u;

    id = addParam("sessionID  0x3000E Int32 U32");
    id = addParam("lcpRegWO_2 0x300FF Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+6));
    WO_groupSize = 0x0100u;

    id = addParam("pmemWriteStatus 0x1 Int32");
    ASSERT_THAT(id, Eq(numDrvParams+7));  arrayWriteStatusID = id;
    id = addParam("pmemWriteTest 0x2 1 256 Y 0x00800000 0x410000 pmemWriteStatus");
    ASSERT_THAT(id, Eq(numDrvParams+8));  testArrayID = id;
  }

  //---------------------------------------------
  asynUser  *pasynUser;
  asynStatus  stat;
  shared_ptr<asynOctetSyncIOInterface>  syncIO;
  std::string  drvName;
  int  udpPortStat;
  unique_ptr<drvFGPDB> testDrv;
  int  id, testParamID_RO, maxParamID_RO, testParamID_WA, lastRegID_WA,
       arrayWriteStatusID, testArrayID;
  int  numDrvParams;
  uint RO_groupSize, WA_groupSize, WO_groupSize;
  ParamInfo  param;
};


#endif // DRVFGPDBTESTCOMMON_H