#ifndef LCPPROTOCOL_H
#define LCPPROTOCOL_H

#include <cstdint>

enum class LCPCommand : std::int32_t {
  READ_REGS     = 1,
  WRITE_REGS    = 2,
  READ_WAVEFORM = 3,
  ERASE_BLOCK   = 4,
  READ_BLOCK    = 5,
  WRITE_BLOCK   = 6
};

enum class LCPStatus : std::int16_t {
  SUCCESS       =  0,
  ACCESS_DENIED = -1,
  INVALID_ID    = -2,
  INVALID_PARAM = -3,
  MAX_CLIENTS   = -4,
  MAX_CMDS      = -5,
  INVALID_CMD   = -6,
  ERROR         = -999  // unclassified error
};

#endif // LCPPROTOCOL_H