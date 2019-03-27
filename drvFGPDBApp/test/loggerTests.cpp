#include "gmock/gmock.h"

#include "streamLogger.h"

#include <sstream>
#include <memory>
#include <atomic>

#include <epicsThread.h>

using namespace testing;
using namespace std;

class ALogger : public Test {
public:
  streamLogger log;
};

TEST_F(ALogger, terminatesEachMessageWithALinebreak) {
  string msg("an arbitrary log message");

  log.major(msg);

  ASSERT_EQ(log.getMsgs().back(), '\n');
}

TEST_F(ALogger, includesTheSeverityInEachMessage) {
  log.minor("an arbitrary log message");

  ASSERT_THAT(log.getMsgs(), HasSubstr("sevr=minor"));
}

TEST(ADateTimeLogger, includesDateAndTime) {
  shared_ptr<streamLogger> baseLog = make_shared<streamLogger>();
  shared_ptr<logger> log = make_shared<timeDateDecorator>(baseLog);

  log->info("an arbitrary log message");

  const string reSev = "sevr=info";
  const string reDate = "[0-9]{4}-[0-9]{1,2}-[0-9]{1,2}";
  const string reTime = "[0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2}.[0-9]{3}";
  const string reFull = reSev + "\\s+" + reDate + "\\s+" + reTime + ".*";
  EXPECT_THAT(baseLog->getMsgs(), MatchesRegex(reFull));
}

class testThread: public epicsThreadRunable {
public:
  testThread(const string name, shared_ptr<logger> pLog, const string msg) :
    thread(*this, name.c_str(), epicsThreadGetStackSize(epicsThreadStackSmall)),
    log(pLog), message(msg) {
      thread.start();
      while(!running.load(memory_order_acquire)) {};
    };
  virtual ~testThread() {
    thread.exitWait();
  };
  virtual void run() {
    running.store(true, memory_order_release);
    log->minor(message);
  };
  epicsThread thread;
  atomic<bool> running { false };
  shared_ptr<logger> log;
  const string message;
};

TEST(AThreadIDLogger, includesTheThreadIDInEachMessage) {
  shared_ptr<streamLogger> baseLog = make_shared<streamLogger>();
  shared_ptr<logger> log = make_shared<threadIDDecorator>(baseLog);
  const string threadName = "arbitraryThreadName";
  const string msg = "message from thread";

  {
    testThread t(threadName, log, msg);
  } // wait for thread to shut down

  const string reID = "0x[0-9a-fA-F]+";
  const string reNameID = threadName + "\\[" + reID + "\\]";
  const string reFull = string(".*") + reNameID + ": " + msg + "\n";
  EXPECT_THAT(baseLog->getMsgs(), MatchesRegex(reFull));
}

TEST(ADateTimeThreadIDLogger, includesDateAndTimeAsWellAsTheThreadIDInEachMessage) {
  std::shared_ptr<streamLogger> baseLog = std::make_shared<streamLogger>();
  std::shared_ptr<logger> dateTimeLog = std::make_shared<timeDateDecorator>(baseLog);
  std::shared_ptr<logger> log = std::make_shared<threadIDDecorator>(dateTimeLog);
  const string threadName = "arbitraryThreadName";
  const string msg = "message from thread";

  {
    testThread t(threadName, log, msg);
  } // wait for thread to shut down

  const string reSev = "sevr=minor";
  const string reDate = "[0-9]{4}-[0-9]{1,2}-[0-9]{1,2}";
  const string reTime = "[0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2}.[0-9]{3}";
  const string reID = "0x[0-9a-fA-F]+";
  const string reNameID = threadName + "\\[" + reID + "\\]";
  const string reFull = reSev + "\\s+" + reDate + "\\s+" + reTime + "\\s+" + reNameID + ": " + msg + "\n";
  EXPECT_THAT(baseLog->getMsgs(), MatchesRegex(reFull));
}
