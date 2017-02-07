
#include "gmock/gmock.h"

#include "drvFGPDB.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------

static drvFGPDB *testDrv;
static int  testParamIdx;
static asynUser *pasynUser;


//=============================================================================
// *** WARNING ***
//
// The order of these tests is important.  Many of them depend on the state the
// previous tests left the test object in.
//=============================================================================

//-----------------------------------------------------------------------------
// Start by constructing
//-----------------------------------------------------------------------------
TEST(drvFGPDB, canBeConstructedWithoutAnyErrors) {
  const string drvPortName = "testDriver";
  testDrv = new drvFGPDB(drvPortName);

  pasynUser = pasynManager->createAsynUser(NULL, NULL);
}

//-----------------------------------------------------------------------------
// Add "testParam" to the list of parameters but don't provide any property
// info for it.
//-----------------------------------------------------------------------------
TEST(drvFGPDB, canCreateIncompleteParam) {
  const char *paramDesc = { "testParam" };

  pasynUser = pasynManager->createAsynUser(NULL, NULL);  //tdebug
  pasynUser->reason = -1;

  asynStatus stat = asynError;
  stat = testDrv->drvUserCreate(pasynUser, paramDesc, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynSuccess));

  testParamIdx = pasynUser->reason;
}

//-----------------------------------------------------------------------------
// Add properties to the existing testParam
//-----------------------------------------------------------------------------
TEST(drvFGPDB, canAddPropertiesToExistingParam) {
  const char *paramDesc = { "testParam LCP_RO 0x10000 Int32 U32" };

  pasynUser->reason = -1;
  auto stat = testDrv->drvUserCreate(pasynUser, paramDesc, NULL, NULL);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(pasynUser->reason, Eq(testParamIdx));

  ParamInfo param;
  stat = testDrv->getParamInfo(pasynUser->reason, param);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.asynType, Eq(asynParamInt32));
}

//-----------------------------------------------------------------------------
