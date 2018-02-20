#include "gmock/gmock.h"

#include "LCPProtocol.h"

using namespace testing;
using namespace std;

TEST(AnLCPProtocol, sessionIDIsBetween1and0xFFFE)  {
  auto sessionId = LCPUtil::generateSessionId();

  ASSERT_THAT(sessionId, Gt(0));
  ASSERT_THAT(sessionId, Lt(0xFFFF));
}
