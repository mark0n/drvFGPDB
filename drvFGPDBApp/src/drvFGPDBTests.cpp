
#include "gmock/gmock.h"

#include "drvFGPDB.h"


using namespace testing;
using namespace std;


//-----------------------------------------------------------------------------
class drvFGPDB_test: public Test {

  public:
    drvFGPDB *testDrv;
};

//-----------------------------------------------------------------------------
TEST_F(drvFGPDB_test, canBeConstructedWithoutAnyErrors) {
  const string drvPortName = "testDriver";
  testDrv = new drvFGPDB(drvPortName);
}

//-----------------------------------------------------------------------------
TEST_F(drvFGPDB_test, canCreateIncompleteParameter) {
  asynUser  *pasynUser= pasynManager->createAsynUser(NULL, NULL);

  const char *paramInfo = { "paramName" };
  auto stat = testDrv->drvUserCreate(pasynUser, paramInfo, NULL, NULL);

  ASSERT_THAT(stat, Eq(asynSuccess));
}

//-----------------------------------------------------------------------------
