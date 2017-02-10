
#include "gmock/gmock.h"

#include "drvFGPDB.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------

static int  testParamIdx;
static asynUser *pasynUser;

class AnFGPDBDriver: public testing::Test {
public:
  const string drvPortName = "testDriver";
  drvFGPDB testDrv = drvFGPDB(drvPortName);
};

TEST(AParamInfoObject, defaultsToTheCorrectValues) {
  ParamInfo pi;

  ASSERT_THAT(pi.regAddr, Eq(0));
  ASSERT_THAT(pi.asynType, Eq(asynParamNotDefined));
  ASSERT_THAT(pi.ctlrFmt, Eq(CtlrDataFmt::NotDefined));
}

TEST(AParamInfoObject, canBeConstructedFromAValidString) {
  const string paramName("paramName");
  const string regAddr("0x01234");
  const string asynTypeName("UInt32Digital");
  const string ctlrFmtName("U32");
  const string validParamStr(paramName + " " + regAddr + " " + asynTypeName +
                             " " + ctlrFmtName);

  ParamInfo pi(validParamStr);

  ASSERT_THAT(pi.name, Eq(paramName));
  ASSERT_THAT(pi.regAddr, Eq(0x1234));
  ASSERT_THAT(pi.asynType, Eq(asynParamUInt32Digital));
  ASSERT_THAT(pi.ctlrFmt, Eq(CtlrDataFmt::U32));
}

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
TEST_F(AnFGPDBDriver, DISABLED_canCreateIncompleteParam) {
  const char *paramDesc = { "testParam" };

  pasynUser = pasynManager->createAsynUser(NULL, NULL);  //tdebug
  pasynUser->reason = -1;

  asynStatus stat = asynError;
  stat = testDrv.drvUserCreate(pasynUser, paramDesc, NULL, NULL);
  ASSERT_THAT(stat, Eq(asynSuccess));

  testParamIdx = pasynUser->reason;
}

//-----------------------------------------------------------------------------
// Add properties to the existing testParam
//-----------------------------------------------------------------------------
TEST_F(AnFGPDBDriver, DISABLED_canAddPropertiesToExistingParam) {
  const char *paramDesc = { "testParam 0x10000 Int32 U32" };

  pasynUser->reason = -1;
  auto stat = testDrv.drvUserCreate(pasynUser, paramDesc, NULL, NULL);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(pasynUser->reason, Eq(testParamIdx));

  ParamInfo param;
  stat = testDrv.getParamInfo(pasynUser->reason, param);

  ASSERT_THAT(stat, Eq(asynSuccess));
  ASSERT_THAT(param.asynType, Eq(asynParamInt32));
}

//-----------------------------------------------------------------------------
