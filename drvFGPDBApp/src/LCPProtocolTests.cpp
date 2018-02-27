#include "gmock/gmock.h"

#include "LCPProtocol.h"

using namespace testing;

class sessionID : public Test {
public:
  LCP::sessionId sessionId;
  sessionID() : sessionId(1) {} // seed to fixed value to ensure tests are
                                // reproducible
};

TEST_F(sessionID, isAlwaysBetween1and0xFFFE) {
  const int sessionIdsToTest = 100;

  for(int i = 0; i < sessionIdsToTest; i++) {
    auto aSessionId = sessionId.get();

    ASSERT_THAT(aSessionId, Gt(0));
    ASSERT_THAT(aSessionId, Lt(0xFFFF));
  }
}

TEST_F(sessionID, isDifferentAfterRegenerating) {
  auto sessionId1 = sessionId.get();
  auto sessionId2 = sessionId.generate();

  ASSERT_THAT(sessionId1, Ne(sessionId2));
}

TEST_F(sessionID, canBeReadOutAgain) {
  auto aSessionId = sessionId.generate();
  auto rereadSessionId = sessionId.get();

  ASSERT_THAT(aSessionId, Eq(rereadSessionId));
}
