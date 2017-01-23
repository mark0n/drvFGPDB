#include "gmock/gmock.h"
#include "drvFGPDB.h"

using namespace testing;
using namespace std;

TEST(drvFGPDB, canBeConstructedWithoutAnyErrors) {
  const string drvPortName = "testDriver";
  drvFGPDB drvFGPDB(drvPortName);
}