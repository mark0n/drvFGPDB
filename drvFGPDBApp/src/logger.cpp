#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <epicsThread.h>
#include <thread>

using namespace std;

typedef chrono::milliseconds ms;

//-----------------------------------------------------------------------------
//  print the specified date/time in YYYY-MM-DD HH:MM:SS format
//-----------------------------------------------------------------------------
string logger::dateTimeToStr(const time_t dateTime) {
  ostringstream oss;
  tm *lt = localtime(&dateTime);
  // Work around missing std::put_time() function (not supported by g++ <5).
  // The following statement can be replaced by 'oss << put_time(lt, "%F %T.")'
  // when we don't need to support Debian Jessie anymore.
  oss << dec << 1900 + lt->tm_year << "-"
      << setw(2) << setfill('0') << 1 + lt->tm_mon << "-"
      << setw(2) << setfill('0') << lt->tm_mday << " "
      << setw(2) << setfill('0') << lt->tm_hour << ":"
      << setw(2) << setfill('0') << lt->tm_min << ":"
      << setw(2) << setfill('0') << lt->tm_sec;
  return oss.str();
}

int timeDateDecorator::write(const errlogSevEnum sev, const std::string& msg) const {
  auto now = chrono::system_clock::now();
  auto now_c = chrono::system_clock::to_time_t(now);
  auto msec = chrono::duration_cast<ms>(now.time_since_epoch()).count() % 1000;

  ostringstream oss(dateTimeToStr(now_c), ios_base::ate);
  oss << "." << setw(3) << setfill('0') << msec << " " << msg;

  return loggerDecorator::write(sev, oss.str());
}

int threadIDDecorator::write(const errlogSevEnum sev, const std::string& msg) const {
  ostringstream oss;
  oss << epicsThreadGetNameSelf() << "[" << epicsThreadGetIdSelf() << "]: "
      << msg;

  return loggerDecorator::write(sev, oss.str());
}
