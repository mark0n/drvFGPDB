
#include "LCPProtocol.h"

using namespace std;


#define CMD(id)  static_cast<std::int16_t>(LCPCommand::id)
const std::map<int16_t, int16_t> LCPUtil::StatusOffset = {
  { CMD(READ_REGS),     4 },
  { CMD(WRITE_REGS),    4 },
  { CMD(READ_WAVEFORM), 5 },
  { CMD(ERASE_BLOCK),   5 },
  { CMD(READ_BLOCK),    5 },
  { CMD(WRITE_BLOCK),   5 }
};

