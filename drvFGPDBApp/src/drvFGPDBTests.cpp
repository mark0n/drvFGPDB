
#include "gmock/gmock.h"

#include "drvFGPDB.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------
TEST(drvFGPDB, canBeConstructedWithoutAnyErrors) {
  const string drvPortName = "testDriver";
  drvFGPDB testDrv(drvPortName);
}

//-----------------------------------------------------------------------------
TEST(drvFGPDB, canCreateIncompleteParameter) {
  const string drvPortName = "testDriver2";
  drvFGPDB testDrv(drvPortName);

  asynUser  *pasynUser= pasynManager->createAsynUser(NULL, NULL);

  const char *paramInfo = { "paramName" };
  auto stat = testDrv.drvUserCreate(pasynUser, paramInfo, NULL, NULL);

  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
