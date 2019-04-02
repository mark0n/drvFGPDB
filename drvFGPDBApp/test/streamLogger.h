#ifndef STREAM_LOGGER_H
#define STREAM_LOGGER_H

#include "logger.h"

#include <mutex>

/**
 * @file  streamLogger.h
 * @brief Helper class that takes care of writing log messages. This version
 *        buffers log messages and returns them for inspection. This is useful
 *        for unit testing. This class is not intended for production!
 */
class streamLogger : public logger {
  mutable std::stringstream msgs; // mutable since we don't want streamLogger to break constness of all methods that use a logger
  mutable std::mutex msgsMu;
public:
  int write(const severity sev, const std::string& msg) const override
  {
    std::string prefix = std::string("sevr=") + sevStr.at(sev) + " ";
    std::lock_guard<std::mutex> lock(msgsMu);
    msgs << prefix << msg << "\n";
    return prefix.size() + msg.size() + sizeof("\n");
  };

  std::string getMsgs() const {
    std::lock_guard<std::mutex> lock(msgsMu);
    return msgs.str();
  }
};

#endif // STREAM_LOGGER_H