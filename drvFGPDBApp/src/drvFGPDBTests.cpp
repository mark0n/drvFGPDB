
#include "gmock/gmock.h"

#include "drvFGPDB.h"
#include "drvAsynIPPort.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------

static int testNum = 0;

#define UDPPortName  "FGPDB_com"
#define MaxParams    200


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
    udpPortStat(createPortUDP())
  {
    if (udpPortStat)
      cout << drvName << " unable to create asyn UDP port: " << UDPPortName
           << endl << endl;
  };

  ~AnFGPDBDriver() { pasynManager->freeAsynUser(pasynUser); };

  int addParam(string paramStr) {
    asynStatus stat;
    pasynUser->reason = -1;
    stat = testDrv.drvUserCreate(pasynUser, paramStr.c_str(), nullptr, nullptr);
    return (stat == asynSuccess) ? pasynUser->reason : -1;
  }

  asynUser  *pasynUser;
  std::string  drvName;
  int  udpPortStat;
  drvFGPDB  testDrv = drvFGPDB(drvName, UDPPortName, MaxParams);
};


//=============================================================================
// *** WARNING ***
//
// The order of these tests is important.  Many of them depend on the state the
// previous tests left the test object in.
//=============================================================================


//-----------------------------------------------------------------------------
// Start by constructing
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canBeConstructedWithoutAnyErrors) {
}

//-----------------------------------------------------------------------------
// Add "testParam" to the list of parameters but don't provide any property
// info for it.
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, rejectsEmptyParamDef) {
  const char *paramDesc = { " " };

  ASSERT_ANY_THROW(testDrv.drvUserCreate(pasynUser, paramDesc, nullptr,
                                         nullptr));
}

//-----------------------------------------------------------------------------
// Add "testParam" to the list of parameters but don't provide any property
// info for it.
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canCreateIncompleteParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(0));
}

//-----------------------------------------------------------------------------
// Add a 2nd parameter
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddAnotherParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(0));

  id = addParam("testParam2 0x10001 Float64 F32");
  ASSERT_THAT(id, Eq(1));
}

//-----------------------------------------------------------------------------
// Add properties to an existing testParam
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
// Test for rejection of conflicting properties for an existing param
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnParamDefConflict) {
  int id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(0));

  const char *param1Def = { "testParam1 0x10001 Float64 F32" };
  auto stat = testDrv.drvUserCreate(pasynUser, param1Def, nullptr, nullptr);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
// Test creation of asyn params
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, createsAsynParams) {
  int id = addParam("testParam1 0x10001 Int32 U32");
  ASSERT_THAT(id, Eq(0));

  id = addParam("testParam2 0x10002 Float64 F32");
  ASSERT_THAT(id, Eq(1));

  auto stat = testDrv.createAsynParams();
  ASSERT_THAT(stat, Eq(asynSuccess));

  int paramID;
  stat = testDrv.findParam("testParam2", &paramID);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(paramID, Eq(1));
}

//-----------------------------------------------------------------------------
// Test reading LCP register values
// NOTE:  This requires the LCP simulator appl to be running on the same mach
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, readRegValues) {
  addParam("testParam1 0x10001 Int32 U32");
  addParam("testParam2 0x10002 Float64 F32");

  auto stat = testDrv.readRegs(0x10001, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
// Test writing LCP register values
// NOTE:  This requires the LCP simulator appl to be running on the same mach
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, writeRegValues) {
  addParam("testParam1 0x20001 Int32 U32");
  addParam("testParam2 0x20002 Float64 F32");

  auto stat = testDrv.writeRegs(0x20001, 2);
  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------

