#include <chrono>

#include "LCPProtocol.h"

using namespace std;
using namespace std::chrono;

#define CMD(id)  static_cast<std::int16_t>(LCPCommand::id)
const std::map<int16_t, int16_t> LCPUtil::StatusOffset = {
  { CMD(READ_REGS),     4 },
  { CMD(WRITE_REGS),    4 },
  { CMD(READ_WAVEFORM), 5 },
  { CMD(ERASE_BLOCK),   5 },
  { CMD(READ_BLOCK),    5 },
  { CMD(WRITE_BLOCK),   5 }
};

namespace LCP {
  sessionId::sessionId(unsigned seedVal) :
    randGen(seedVal), sId(generate()) {}

  sessionId::sessionId() :
    sessionId(system_clock::now().time_since_epoch().count()) {}

  void sessionId::seed(unsigned val) {
    randGen.seed(val);
  };

  uint16_t sessionId::generate() {
    uniform_int_distribution<int> distribution(1, 0xFFFE);
    sId = distribution(randGen);
    return sId;
  }

  uint16_t sessionId::get() const {
    return sId;
  }
}
