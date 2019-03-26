#ifndef DRVFGPDB_LOGGER_H
#define DRVFGPDB_LOGGER_H

#include <string>

#include <errlog.h>

/**
 * @file  logger.h
 * @brief Helper class that takes care of writing log messages.
 */

class logger {
  //-----------------------------------------------------------------------------
  //  print the current date/time incl milliseconds
  //-----------------------------------------------------------------------------
  std::string logMsgHdr() const;

public:
  //-----------------------------------------------------------------------------
  //  print the specified date/time in YYY-MM-DD HH:MM:SS format
  //-----------------------------------------------------------------------------
  static std::string dateTimeToStr(const time_t dateTime);

  template<typename... Args> void fatal(const char * format, Args... args) const {
    std::string fullFormat = logMsgHdr() + format;
    errlogSevPrintf(errlogFatal, fullFormat.c_str(), args...);
  };
  template<typename... Args> void major(const char * format, Args... args) const {
    std::string fullFormat = logMsgHdr() + format;
    errlogSevPrintf(errlogMajor, fullFormat.c_str(), args...);
  };
  template<typename... Args> void minor(const char * format, Args... args) const {
    std::string fullFormat = logMsgHdr() + format;
    errlogSevPrintf(errlogMinor, fullFormat.c_str(), args...);
  };
  template<typename... Args> void info(const char * format, Args... args) const {
    std::string fullFormat = logMsgHdr() + format;
    errlogSevPrintf(errlogInfo, fullFormat.c_str(), args...);
  };
};

#endif // DRVFGPDB_LOGGER_H