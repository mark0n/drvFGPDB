
#include "gmock/gmock.h"

#include "drvFGPDB.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------


static drvFGPDB  *testDrv = NULL;
static asynUser  *pasynUser = NULL;
static int  testParamIdx = -1;

class AnFGPDBDriver: public ::testing::Test
{
  public:
//    static drvFGPDB  *testDrv;
//    static asynUser  *pasynUser;
//    static int  testParamIdx;

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

  asynStatus stat = testDrv->drvUserCreate(pasynUser, paramDesc, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynError));
}

//-----------------------------------------------------------------------------
// Add "testParam" to the list of parameters but don't provide any property
// info for it.
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canCreateIncompleteParam) {
  const char *paramDesc = { "testParam" };

  asynStatus stat = asynError;
  stat = testDrv->drvUserCreate(pasynUser, paramDesc, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynSuccess));

  testParamIdx = pasynUser->reason;
}

//-----------------------------------------------------------------------------
// Add properties to the existing testParam
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, canAddPropertiesToExistingParam) {
  const char *paramDesc = { "testParam 0x10000 Int32 U32" };

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
