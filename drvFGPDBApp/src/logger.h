#ifndef DRVFGPDB_LOGGER_H
#define DRVFGPDB_LOGGER_H

#include <string>
#include <memory>

#include <errlog.h>

/**
 * @file  logger.h
 * @brief Helper class that takes care of writing log messages.
 */

class logger {
public:
  //-----------------------------------------------------------------------------
  //  print the specified date/time in YYYY-MM-DD HH:MM:SS format
  //-----------------------------------------------------------------------------
  static std::string dateTimeToStr(const time_t dateTime);

  virtual int write(const errlogSevEnum sev, const std::string& msg) const = 0;

  int fatal(const std::string& msg) const { return write(errlogFatal, msg); };
  int major(const std::string& msg) const { return write(errlogMajor, msg); };
  int minor(const std::string& msg) const { return write(errlogMinor, msg); };
  int info (const std::string& msg) const { return write(errlogInfo,  msg); };
};

class epicsLogger : public logger {
public:
  epicsLogger() {};
  ~epicsLogger();
  int write(const errlogSevEnum sev, const std::string& msg) const override;
};

class loggerDecorator : public logger {
private:
  std::shared_ptr<logger> wrapped;
public:
  loggerDecorator(std::shared_ptr<logger> log) : wrapped(log) {}
  virtual int write(const errlogSevEnum sev, const std::string& msg) const override { return wrapped->write(sev, msg); }
};

class timeDateDecorator : public loggerDecorator {
public:
  timeDateDecorator(std::shared_ptr<logger> log) : loggerDecorator(log) {}
  virtual int write(const errlogSevEnum sev, const std::string& msg) const override;
};

class threadIDDecorator : public loggerDecorator {
public:
  threadIDDecorator(std::shared_ptr<logger> log) : loggerDecorator(log) {}
  virtual int write(const errlogSevEnum sev, const std::string& msg) const override;
};


#endif // DRVFGPDB_LOGGER_H