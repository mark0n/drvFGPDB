
#include "gmock/gmock.h"

#include "drvFGPDB.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------


static drvFGPDB  *testDrv = NULL;
static asynUser  *pasynUser = NULL;
static int  testParam1ID = -1;
static int  testParam2ID = -1;

class AnFGPDBDriver: public ::testing::Test
{
  protected:
    static void SetUpTestCase()  { testDrv = new drvFGPDB("testDriver"); }

    static void TearDownTestCase()  { delete testDrv; }

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
  pasynUser = pasynManager->createAsynUser(NULL, NULL);
}

//-----------------------------------------------------------------------------
// Add "testParam" to the list of parameters but don't provide any property
// info for it.
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, rejectsInvalidParamDef) {
  const char *paramDesc = { "  " };

  ASSERT_ANY_THROW(testDrv->drvUserCreate(pasynUser, paramDesc, NULL, NULL));
}

//-----------------------------------------------------------------------------
// Add "testParam" to the list of parameters but don't provide any property
// info for it.
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canCreateIncompleteParam) {
  const char *param1Name = { "testParam1" };

  asynStatus stat = asynError;
  stat = testDrv->drvUserCreate(pasynUser, param1Name, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(pasynUser->reason, Ge(0));

  testParam1ID = pasynUser->reason;
}

//-----------------------------------------------------------------------------
// Add a 2nd parameter
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddAnotherParam) {
  const char *param2Def = { "testParam2 0x10000 Float64 F32" };

  pasynUser->reason = -1;
  auto stat = testDrv->drvUserCreate(pasynUser, param2Def, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(pasynUser->reason, Gt(0));
  testParam2ID = pasynUser->reason;
}

//-----------------------------------------------------------------------------
// Add properties to an existing testParam
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddPropertiesToExistingParam) {
  const char *param1Def = { "testParam1 0x10000 Int32 U32" };

  pasynUser->reason = -1;
  auto stat = testDrv->drvUserCreate(pasynUser, param1Def, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(pasynUser->reason, Eq(testParam1ID));

  ParamInfo param1;
  stat = testDrv->getParamInfo(pasynUser->reason, param1);
  ASSERT_THAT(stat, Eq(asynSuccess));

  ASSERT_THAT(param1.regAddr,  Eq(0x10000));
  ASSERT_THAT(param1.asynType, Eq(asynParamInt32));
  ASSERT_THAT(param1.ctlrFmt,  Eq(CtlrDataFmt::U32));
}

//-----------------------------------------------------------------------------
// Test for rejection of conflicting properties for an existing param
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, failsOnParamDefConflict) {
  const char *param1Def = { "testParam1 0x10000 Float64 F32" };

  pasynUser->reason = -1;
  auto stat = testDrv->drvUserCreate(pasynUser, param1Def, NULL, NULL);

  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
// Test creation of asyn params
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, createsAsynParams) {

  drvFGPDB_initHookFunc(initHookAfterInitDatabase);

  int paramID;
  auto stat = testDrv->findParam("testParam2", &paramID);
  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(paramID, Eq(testParam2ID));
}

//-----------------------------------------------------------------------------
// Test ability to write to the asyn parameters
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, writeToParams) {

  pasynUser->reason = testParam1ID;
//  asynSetTraceMask(testDrv->portName, 0, ASYN_TRACEIO_DRIVER);
  auto stat = testDrv->writeInt32(pasynUser, 123);
//  asynSetTraceMask(testDrv->portName, 0, 0);
  ASSERT_THAT(stat, Eq(asynSuccess));

}

//-----------------------------------------------------------------------------


