
#include "gmock/gmock.h"

#include "drvFGPDB.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------

static int testNum = 0;

class AnFGPDBDriver: public ::testing::Test
{
public:
  AnFGPDBDriver()
    : pasynUser(pasynManager->createAsynUser(nullptr, nullptr))  { }

  ~AnFGPDBDriver() { pasynManager->freeAsynUser(pasynUser); };

  int addParam(string paramStr) {
    asynStatus stat;
    pasynUser->reason = -1;
    stat = testDrv.drvUserCreate(pasynUser, paramStr.c_str(), NULL, NULL);
    return (stat == asynSuccess) ? pasynUser->reason : -1;
  }

  drvFGPDB testDrv = drvFGPDB("testDriver" + std::to_string(++testNum));
  asynUser *pasynUser;
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

  ASSERT_ANY_THROW(testDrv.drvUserCreate(pasynUser, paramDesc, NULL, NULL));
}

//-----------------------------------------------------------------------------
// Add "testParam" to the list of parameters but don't provide any property
// info for it.
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canCreateIncompleteParam) {
  const char *param1Name = { "testParam1" };

  asynStatus stat = asynError;
  stat = testDrv.drvUserCreate(pasynUser, param1Name, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(pasynUser->reason, Ge(0));
}

//-----------------------------------------------------------------------------
// Add a 2nd parameter
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddAnotherParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(0));

  id = addParam("testParam2 0x10000 Float64 F32");
  ASSERT_THAT(id, Eq(1));
}

//-----------------------------------------------------------------------------
// Add properties to an existing testParam
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddPropertiesToExistingParam) {
  int id = addParam("testParam1");
  ASSERT_THAT(id, Eq(0));

  id = addParam("testParam1 0x10000 Int32 U32");
  ASSERT_THAT(id, Eq(0));
  ParamInfo param1;
  asynStatus stat = testDrv.getParamInfo(pasynUser->reason, param1);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param1.regAddr,  Eq(0x10000));
  ASSERT_THAT(param1.asynType, Eq(asynParamInt32));
  ASSERT_THAT(param1.ctlrFmt,  Eq(CtlrDataFmt::U32));
}

//-----------------------------------------------------------------------------
// Test for rejection of conflicting properties for an existing param
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnParamDefConflict) {
  addParam("testParam1 0x10000 Int32 U32");
  const char *param1Def = { "testParam1 0x10000 Float64 F32" };

  auto stat = testDrv.drvUserCreate(pasynUser, param1Def, NULL, NULL);

  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
// Test creation of asyn params
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, createsAsynParams) {
  addParam("testParam1 0x10001 Int32 U32");
  addParam("testParam2 0x10002 Float64 F32");

  auto stat = testDrv.createAsynParams();
  ASSERT_THAT(stat, Eq(asynSuccess));

  int paramID;
  stat = testDrv.findParam("testParam2", &paramID);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(paramID, Eq(1));
}

//-----------------------------------------------------------------------------


