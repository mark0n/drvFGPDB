
#include "gmock/gmock.h"

#include "drvFGPDB.h"
#include "drvAsynIPPort.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------

static int testNum = 0;

#define UDPPortName  "FGPDB_com"

//-----------------------------------------------------------------------------
int createPortUDP(void)
{
  static int  stat = 0;

  if (testNum == 1)
    stat = drvAsynIPPortConfigure(UDPPortName, "127.0.0.1:2005 udp", 0, 0, 1);

  return stat;
}

//-----------------------------------------------------------------------------
class AnFGPDBDriver: public ::testing::Test
{
public:
  AnFGPDBDriver() :
    pasynUser(pasynManager->createAsynUser(nullptr, nullptr)),
    drvName("testDriver" + std::to_string(++testNum)),
    // NOTE: asyn UDP port must be created before drvFGPDB object
    udpPortStat(createPortUDP()),
    testDrv(drvFGPDB(drvName, UDPPortName, maxParams)),
    testParamID(0)
  {
    if (udpPortStat)
      cout << drvName << " unable to create asyn UDP port: " << UDPPortName
           << endl << endl;
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
    auto stat = addParam("lcpRegRO_1 0x10002 Int32 U32");
    ASSERT_THAT(stat, Eq(0));
    stat = addParam("lcpRegRO_5 0x10005 Float64 F32");
    ASSERT_THAT(stat, Eq(1));

    stat = addParam("lcpRegWA_1 0x20001 Int32 U32");
    ASSERT_THAT(stat, Eq(2));
    stat = addParam("lcpRegWA_2 0x20002 Float64 F32");
    ASSERT_THAT(stat, Eq(3));  testParamID = stat;
    stat = addParam("lcpRegWA_4 0x20004 Float64 F32");
    ASSERT_THAT(stat, Eq(4));

    stat = addParam("lcpRegWO_2 0x30002 Int32 U32");
    ASSERT_THAT(stat, Eq(5));
  }

  //---------------------------------------------
  void determineGroupRanges()  {
    addParams();

    auto stat = testDrv.createAsynParams();
    ASSERT_THAT(stat, Eq(asynSuccess));

    stat = testDrv.determineGroupRanges();
    ASSERT_THAT(stat, Eq(asynSuccess));

    ASSERT_THAT(testDrv.max_LCP_RO, Eq(0x10005));
    ASSERT_THAT(testDrv.max_LCP_WA, Eq(0x20004));
    ASSERT_THAT(testDrv.max_LCP_WO, Eq(0x30002));
  }

  //---------------------------------------------
  void createProcessingGroups()  {
    determineGroupRanges();

    testDrv.createProcessingGroups();

    ASSERT_THAT(testDrv.LCP_RO_group.size(), Eq(5));
    ASSERT_THAT(testDrv.LCP_WA_group.size(), Eq(4));
    ASSERT_THAT(testDrv.LCP_WO_group.size(), Eq(2));
  }

  //---------------------------------------------
  void sortParams()  {
    createProcessingGroups();

    auto stat = testDrv.sortParams();
    ASSERT_THAT(stat, Eq(asynSuccess));
  }

  //---------------------------------------------
  const int maxParams = 200;
  asynUser  *pasynUser;
  std::string  drvName;
  int  udpPortStat;
  drvFGPDB  testDrv;
  int  testParamID;
};


//-----------------------------------------------------------------------------
TEST(joinMapKeys, returnsEmptyStringForEmptyMap) {
  const map<string, int> aMap;
  const string separator("--");

  string result = joinMapKeys<int>(aMap, separator);

  ASSERT_THAT(result, Eq(""));
}

//-----------------------------------------------------------------------------
TEST(joinMapKeys, concatenatesMapKeysAndSeparators) {
  const int arbitraryInt = 0;
  const string key1("KEY1");
  const string key2("KEY2");
  const string key3("KEY3");
  const map<string, int> aMap = {
    { key1, arbitraryInt },
    { key2, arbitraryInt },
    { key3, arbitraryInt }
  };
  const string separator("--");

  string result = joinMapKeys<int>(aMap, separator);

  ASSERT_THAT(result, Eq(key1 + separator + key2 + separator + key3));
}


//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canBeConstructedWithoutAnyErrors) {
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, rejectsEmptyParamDef) {
  const char *paramDesc = { " " };

  ASSERT_ANY_THROW(testDrv.drvUserCreate(pasynUser, paramDesc, nullptr,
                                         nullptr));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canCreateIncompleteParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(0));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddAnotherParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(0));

  id = addParam("testParam2 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(1));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddPropertiesToExistingParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(0));

  id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(0));

  ParamInfo param1;
  asynStatus stat = testDrv.getParamInfo(0, param1);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param1.regAddr,  Eq(0x10001));
  ASSERT_THAT(param1.asynType, Eq(asynParamInt32));
  ASSERT_THAT(param1.ctlrFmt,  Eq(CtlrDataFmt::U32));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnParamDefConflict) {
  int id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(0));

  const char *param1Def = { "testParam1 0x10001 Float64 F32" };
  auto stat = testDrv.drvUserCreate(pasynUser, param1Def, nullptr, nullptr);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, createsAsynParams) {
  addParams();

  auto stat = testDrv.createAsynParams();
  ASSERT_THAT(stat, Eq(asynSuccess));

  int paramID;
  stat = testDrv.findParam("lcpRegWA_2", &paramID);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(paramID, Eq(testParamID));
}

//-----------------------------------------------------------------------------
// Test reading LCP register values
// NOTE: This requires the LCP simulator appl to be running on the same mach
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, readRegValues) {
  addParams();

  auto stat = testDrv.readRegs(0x10001, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
// Test writing LCP register values
// NOTE: This requires the LCP simulator appl to be running on the same mach
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, writeRegValues) {
  addParams();

  auto stat = testDrv.writeRegs(0x20001, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, determineGroupRanges) { determineGroupRanges(); }

TEST_F(AnFGPDBDriver, createProcessingGroups) { createProcessingGroups(); }

TEST_F(AnFGPDBDriver, sortParameters) { sortParams(); }

//-----------------------------------------------------------------------------

