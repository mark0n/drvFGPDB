#include <chrono>
#include <arpa/inet.h>
#include <iostream>

#include "LCPProtocol.h"

using namespace std;
using namespace std::chrono;

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

//-----------------------------------------------------------------------------
LCPCmdBase::LCPCmdBase(const int cmdHdrWords, const int respHdrWords,const int cmdBufSize, const int respBufSize):
    CmdHdrWords(cmdHdrWords),
    RespHdrWords(respHdrWords),
    cmdBuf(cmdBufSize,0),
    respBuf(respBufSize,0)
{
}

//-----------------------------------------------------------------------------
LCPCmdPmemBase::LCPCmdPmemBase(const int cmdHdrWords, const int respHdrWords,const int cmdBufSize, const int respBufSize):
    LCPCmdBase(cmdHdrWords,respHdrWords,cmdBufSize,respBufSize)
{
}

void LCPCmdPmemBase::setPmemCmd(const LCPCommand command){
  setCmdBufData(1, htonl(static_cast<int32_t>(command)));};

void LCPCmdPmemBase::setChipNum(const uint chipNum){
  setCmdBufData(2, htonl(chipNum));};

void LCPCmdPmemBase::setBlockSize(const uint32_t blockSize){
  setCmdBufData(3, htonl(blockSize));};

void LCPCmdPmemBase::setBlockNum(const uint32_t blockNum){
  setCmdBufData(4, htonl(blockNum));};


//-----------------------------------------------------------------------------
LCPReadRegs::LCPReadRegs(const uint Offset, const uint Count, const uint32_t Interval):
    LCPCmdBase(5,5,5,5+Count)
{
  setCmdBufData(1, htonl(static_cast<int32_t>(LCPCommand::READ_REGS)));
  setCmdBufData(2, htonl(Offset));
  setCmdBufData(3, htonl(Count));
  setCmdBufData(4, htonl(Interval));
}

LCPWriteRegs::LCPWriteRegs(const uint Offset, const uint Count):
    LCPCmdBase(4,5,4+Count,5)
{
  setCmdBufData(1, htonl(static_cast<int32_t>(LCPCommand::WRITE_REGS)));
  setCmdBufData(2, htonl(Offset));
  setCmdBufData(3, htonl(Count));
}

LCPReadWF::LCPReadWF(const uint32_t waveformID, const uint32_t Offset,const uint32_t Count, const uint32_t Interval):
    LCPCmdBase(6,9,6,9+Count)
{
  setCmdBufData(1, htonl(static_cast<int32_t>(LCPCommand::READ_WAVEFORM)));
  setCmdBufData(2, htonl(waveformID));
  setCmdBufData(3, htonl(Offset));
  setCmdBufData(4, htonl(Count));
  setCmdBufData(5, htonl(Interval));
}

LCPEraseBlock::LCPEraseBlock(const uint chipNum,const uint32_t blockSize, const uint32_t blockNum):
    LCPCmdPmemBase(5,6,5,6)
{
  setPmemCmd(LCPCommand::ERASE_BLOCK);
  setChipNum(chipNum);
  setBlockSize(blockSize);
  setBlockNum(blockNum);
}

LCPReadBlock::LCPReadBlock(const uint chipNum, const uint32_t blockSize, const uint32_t blockNum):
    LCPCmdPmemBase(5,6,5,6 + blockSize/4)
{
  setPmemCmd(LCPCommand::READ_BLOCK);
  setChipNum(chipNum);
  setBlockSize(blockSize);
  setBlockNum(blockNum);
}
LCPWriteBlock::LCPWriteBlock(const uint chipNum, const uint32_t blockSize, const uint32_t blockNum):
    LCPCmdPmemBase(5,6,5 + blockSize/4, 6)
{
  setPmemCmd(LCPCommand::WRITE_BLOCK);
  setChipNum(chipNum);
  setBlockSize(blockSize);
  setBlockNum(blockNum);
}

LCPReqWriteAccess::LCPReqWriteAccess(const uint16_t drvsessionID,  bool keepAlive):
    LCPCmdBase(2,2,4,5)
{
  setCmdBufData(1, htonl(static_cast<int32_t>(LCPCommand::REQ_WRITE_ACCESS)));
  setCmdBufData(2, htonl((static_cast<int32_t>(drvsessionID))<<16));
  setCmdBufData(3, htonl(static_cast<int32_t>(keepAlive)));

}
