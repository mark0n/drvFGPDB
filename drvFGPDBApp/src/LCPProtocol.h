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

enum class ProcessGroup : std::int16_t {
  Driver  =  0,
  LCP_RO  =  1,
  LCP_WA  =  2,
  LCP_WO  =  3
};

// To reduce clutter in code to make it readable
#define  ProcGroup_Driver  ( static_cast<std::int16_t> (ProcessGroup::Driver) )
#define  ProcGroup_LCP_RO  ( static_cast<std::int16_t> (ProcessGroup::LCP_RO) )
#define  ProcGroup_LCP_WA  ( static_cast<std::int16_t> (ProcessGroup::LCP_WA) )
#define  ProcGroup_LCP_WO  ( static_cast<std::int16_t> (ProcessGroup::LCP_WO) )


class LCPUtil {
public:
  static int addrGroupID(uint addr)  { return ((addr >> 16) & 0x7FFF); }

  static uint addrOffset(uint addr)   { return addr & 0xFFFF; }

  static bool validRegAddr(uint addr)  {
    uint groupID = addrGroupID(addr);
    return (groupID > 0) and (groupID < 4);
  }

  static bool readOnlyAddr(uint addr)  {
    uint groupID = addrGroupID(addr);
    uint offset = addrOffset(addr);
    return (groupID == ProcGroup_LCP_RO) or
           ((groupID == ProcGroup_Driver) and (offset == 1));
  }
};


#endif // LCPPROTOCOL_H