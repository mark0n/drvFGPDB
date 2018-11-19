#ifndef LCPPROTOCOL_H
#define LCPPROTOCOL_H

/**
 * @file  LCPProtocol.h
 * @brief Defines extra utilities of the LLRF Control Protocol.
 */

#include <cstdint>
#include <unordered_map>
#include <random>
#include <arpa/inet.h>

/**
 * @brief LCP commands to communicate with the ctlr.
 */
enum class LCPCommand : std::int32_t {
  READ_REGS        = 1,    //!< Read registers
  WRITE_REGS       = 2,    //!< Write Registers
  READ_WAVEFORM    = 3,    //!< Read Waveform
  ERASE_BLOCK      = 4,    //!< Ease memory block
  READ_BLOCK       = 5,    //!< Read memory block
  WRITE_BLOCK      = 6,    //!< Write memory block
  REQ_WRITE_ACCESS = 7     //!< Request write access
};

/**
 * @brief LCP status returned by each command send to the ctlr
 */
enum class LCPStatus : std::int16_t {
  SUCCESS       =  0,   //!< No errors
  ACCESS_DENIED = -1,   //!< Write attempt by a non write-enabled client
  INVALID_ID    = -2,   //!< Packet ID out of sequence or reused w/ different cmd
  INVALID_PARAM = -3,   //!< At least one param value was invalid
  MAX_CLIENTS   = -4,   //!< Max # of unique clients exceeded
  MAX_CMDS      = -5,   //!< Max # of unprocessed cmds in the ctlr side
  INVALID_CMD   = -6,   //!< Unrecognized cmd
  ERROR         = -999  //!< Unclassified error
};

/**
 * @brief Param processing groups supported
 */
enum class ProcessGroup : std::int16_t {
  Driver  =  0, //!< Driver-Only
  LCP_RO  =  1, //!< Read-Only
  LCP_WA  =  2, //!< Write-Anytime
  LCP_WO  =  3  //!< Write-Once
};
const uint ProcessGroupSize = 4;  //!< Number of processing groups supported.

// To reduce clutter in code to make it readable
const auto ProcGroup_Driver = static_cast<std::int16_t>(ProcessGroup::Driver);
const auto ProcGroup_LCP_RO = static_cast<std::int16_t>(ProcessGroup::LCP_RO);
const auto ProcGroup_LCP_WA = static_cast<std::int16_t>(ProcessGroup::LCP_WA);
const auto ProcGroup_LCP_WO = static_cast<std::int16_t>(ProcessGroup::LCP_WO);

/**
 * Defines extra utilities of the LLRF Control Protocol.
 * Implements necessary methods to check param properties:
 * - Processing Group, Offset, is LCP param?, is read only param?
 */
class LCPUtil {
public:
  /**
   * @brief Method to know the processing group of a given param addr
   *
   * @param[in] addr address of the param
   *
   * @return Processing group ID of the param
   */
  static int addrGroupID(uint addr)  { return ((addr >> 16) & 0x7FFF); }

  /**
   * @brief Offset of a given param addr inside a processing group.
   *        (i.e: addr=0x20004, GroupID=2 and Offset=4)
   *
   * @param[in] addr address of the param
   *
   * @return Offset of the param
   */
  static uint addrOffset(uint addr)   { return addr & 0xFFFF; }

  /**
   * @brief Method to know if a given param is a valid LCP param.
   *        (LCP_RO, LCP_WA and LCP_WO)
   *
   * @param[in] addr address of the param
   *
   * @return true/false
   *
   */
  static bool isLCPRegParam(uint addr)  {
    uint groupID = addrGroupID(addr);
    return (groupID > 0) and (groupID < 4);
  }

  /**
   * @brief Method to know if a param is read only
   *
   * @param[in] addr address of the param
   *
   * @return true/false
   */
  static bool readOnlyAddr(uint addr)  {
    uint groupID = addrGroupID(addr);
    uint offset = addrOffset(addr);
    return (groupID == ProcGroup_LCP_RO) or
           ((groupID == ProcGroup_Driver) and (offset == 1));
  }

  /**
   * @brief Method to generate a random Session ID
   *
   * @return SessionID
   */
  static int generateSessionId();

#ifndef TEST_DRVFGPDB
  private:
#endif

  static const std::unordered_map<int16_t, int16_t> StatusOffset; //!< Map w/ LCP_Status cmd offset in ctlr response
};

namespace LCP {
  /**
   * Class that takes care of the sessionID. It handles its generation and its interface with the driver
   */
  class sessionId {
    std::default_random_engine randGen; //!< Pseudo-random numbers generator

  public:
    /**
     * @brief Constructs the sessionID in the range of [1,65534],
     *        using a random number distribution with a pseudo-random generated seed
     *
     * @param seedVal Number used by the random number engine to generate the seed
     */
    sessionId(const unsigned seedVal);

    /**
     * @brief Constructs the sessionID in the range of [1,65534],
     *        calling previous C-tor using the number of periods since epoch as seedVal.
     */
    sessionId();

    /**
     * @brief Re-initializes the internal state value of the Pseudo-random numbers generator
     *
     * @param[in] val seed
     */
    void seed(const unsigned val);

    /**
     * @brief Method that generates a random sessionID in the range of [1,65534]
     *
     * @return sessionID value
     */
    uint16_t generate();

    /**
     * @brief Method to get the current sessionID
     *
     * @return sessionID value
     */
    uint16_t get() const;

  private:
    uint16_t sId;  //!< current value
  };
}

/**
 * Class that handles common LCP-Cmd's members
 */
class LCPCmdBase{
public:
  /**
   * @brief C-tor of base LCP-Cmd object. It includes creation and pre-initialization (0) of
   *        request and response buffers
   *
   * @param[in] CmdHdrWords     Number of uint32_t words in the request buffer
   *
   * @param[in] RespHdrWords    Number of uint32_t words in the response buffer
   *
   * @param[in] cmdBufSize      Size of the request buffer (in uint32_ words)
   *
   * @param[in] respBufSize     Size of the request buffer (in uint32_ words)
   */

  LCPCmdBase(const int CmdHdrWords, const int RespHdrWords, const int cmdBufSize, const int respBufSize);

  /**
   * @brief Method that returns the number of words in the request buffer
   *
   * @return value
   */
  int getCmdHdrWords(){ return CmdHdrWords;}

  /**
   * @brief Method that returns the number of words in the response buffer
   *
   * @return value
   */
  int getRespHdrWords(){ return RespHdrWords;}

  /**
   * @brief Method that returns the request buffer
   *
   * @return buffer
   */
  std::vector<uint32_t>& getCmdBuf(){ return cmdBuf; }

  /**
   * @brief Method that returns the response buffer
   *
   * @return buffer
   */
  std::vector<uint32_t>& getRespBuf(){ return respBuf; }

  /**
   * @brief Method to set request buffer values
   *
   * @param[in] idx    index of the value to be set
   * @param[in] value  value set in network format (NOT in host format)
   */
  void setCmdBufData(const int idx,const uint32_t value){ cmdBuf.at(idx) = htonl(value); }

  /**
   * @brief Method to set response buffer values
   *
   * @param[in] idx    index of the value to be set
   * @param[in] value  value set in network format (NOT in host format)
   */
  void setRespBufData(const int idx,const uint32_t value){ respBuf.at(idx) = htonl(value); }

  /**
   * @brief Method that returns any value of the request buffer
   *
   * @param[in] idx  index of the value desired
   *
   * @return value in host format (NOT in network format)
   */
  uint32_t getCmdBufData(const int idx){ return ntohl(cmdBuf.at(idx)); }

  /**
   * @brief Method that returns any value of the response buffer
   *
   * @param[in] idx  index of the value desired
   *
   * @return value in host format (NOT in network format)
   */
  uint32_t getRespBufData(const int idx){ return ntohl(respBuf.at(idx)); }

  /**
   * @brief Method to set the PktID in the request buffer
   *
   * @param[in] value  PktID value
   */
  void setCmdPktID(const uint32_t value){ setCmdBufData(0,value); }

  /**
   * @brief Method to set the PktID in the response buffer
   *
   * @param[in] value  PktID value
   */
  void setRespPktID(const uint32_t value){ setRespBufData(0,value); }

  /**
   * @brief Method that returns the PktID of the request buffer
   *
   * @return PktID value
   */
  uint32_t getCmdPktID(){ return getCmdBufData(0); }

  /**
   * @brief Method that returns the PktID of the response buffer
   *
   * @return PktID value
   */
  uint32_t getRespPktID(){ return getRespBufData(0); }

  /**
   * @brief Method that returns the LCP Command of the request buffer
   *
   * @return LCP Command value
   */
  uint32_t getCmdLCPCommand(){ return getCmdBufData(1); }

  /**
   * @brief Method that returns the LCP Command of the response buffer
   *
   * @return LCP Command value
   */
  uint32_t getRespLCPCommand(){ return getRespBufData(1); }

  /**
   * @brief Method that returns the SessionID of the response buffer
   *
   * @return sessionID value
   */
  uint32_t getRespSessionID(){ return ((getRespBufData(2) >> 16) & 0xFFFF);}

  /**
   * @brief Method that returns the Status of the response buffer
   *
   * @return LCPStatus value
   */
  LCPStatus getRespStatus(){ return static_cast<LCPStatus>(getRespBufData(2) & 0xFFFF);}

private:
  const int CmdHdrWords;    //!< Number of uint32_t words in the request buffer
  const int RespHdrWords;   //!< Number of uint32_t words in the response buffer

  std::vector<uint32_t> cmdBuf;   //!< Request Buffer
  std::vector<uint32_t> respBuf;  //!< Response Buffer
};


/**
 * Class that handles common PMEM-related LCP-Cmd's members
 */
class LCPCmdPmemBase : public LCPCmdBase {
public:

  /**
   * C-tor of PMEM related LCP-Cmd.
   *
   * @param[in] cmdHdrWords     Number of uint32_t words in the request buffer
   *
   * @param[in] respHdrWords    Number of uint32_t words in the response buffer
   *
   * @param[in] cmdBufSize      Size of the request buffer (in uint32_ words)
   *
   * @param[in] respBufSize     Size of the request buffer (in uint32_ words)
   */
  LCPCmdPmemBase(const int cmdHdrWords, const int respHdrWords,const int cmdBufSize, const int respBufSize);

  /**
   * @brief Method to set the PMEM-relate LCP-Cmd in the request buffer
   *
   * @param[in] command  ERASE_BLOCK, READ_BLOCK or WRITE_BLOCK
   */
  void setPmemCmd(const LCPCommand command);

  /**
   * @brief Method to set the Chip Number in the request buffer
   *
   * @param[in] chipNum  value indicating which memory chip to access
   */
  void setChipNum(const uint chipNum);

  /**
   * @brief Method to set the Block Size in the request buffer
   *
   * @param[in] blockSize  size of the block
   */
  void setBlockSize(const uint32_t blockSize);

  /**
   * @brief Method to set the Block Number in the request buffer
   *
   * @param[in] blockNum  block number
   */
  void setBlockNum(const uint32_t blockNum);

};

/**
 * @brief Read Registers LCP-Cmd class
 */
class LCPReadRegs : public LCPCmdBase {
public:
  /**
   * C-tor of the Read Registers LCP-Cmd
   *
   * @param[in] Offset     Address of the first register to read
   *
   * @param[in] Count      The number of register values to return
   *
   * @param[in] Interval   How often to repeat the read. A value larger than zero causes the controller
   *                       to send new values without the client having to request them. The value specifies
   *                       the minimum interval, in milliseconds, between new response packets
   *
   */
  LCPReadRegs(const uint Offset, const uint Count, const uint32_t Interval);
};

/**
 * @brief Write Registers LCP-Cmd class
 */
class LCPWriteRegs : public LCPCmdBase {
public:
  /**
   * C-tor of the Read Registers LCP-Cmd
   *
   * @param[in] Offset     Address of the first register to write
   *
   * @param[in] Count      The number of register values to write
   *
   */
  LCPWriteRegs(const uint Offset, const uint Count);
};

/**
 * @brief Read Waveforms LCP-Cmd class
 */
class LCPReadWF : public LCPCmdBase {
public:
  /**
   * C-tor of the Read Waveforms LCP-Cmd
   *
   * @param[in] waveformID  ID of waveform to be read
   *
   * @param[in] Offset      The offset of the first value to return from the waveform
   *
   * @param[in] Count       The number of values to return from the waveform
   *
   * @param[in] Interval    How often to repeat the read. (See LCPReadRegs for details)
   *
   */
  LCPReadWF(const uint32_t waveformID, const uint32_t Offset,const uint32_t Count, const uint32_t Interval);
};

/**
 * @brief Erase PMEM Block LCP-Cmd class
 */
class LCPEraseBlock : public LCPCmdPmemBase {
public:
  /**
   * C-tor of Erase PMEM Block LCP-Cmd.
   *
   * @param[in] chipNum    value indicating which memory chip to access
   *
   * @param[in] blockSize  size of the block to erase
   *
   * @param[in] blockNum   block number to erase
   *
   */
  LCPEraseBlock(const uint chipNum, const uint32_t blockSize, const uint32_t blockNum);
};

/**
 * @brief Read PMEM Block LCP-Cmd class
 */
class LCPReadBlock : public LCPCmdPmemBase {
public:
  /**
   * C-tor of Read PMEM Block LCP-Cmd.
   *
   * @param[in] chipNum    value indicating which memory chip to access
   *
   * @param[in] blockSize  size of the block to read
   *
   * @param[in] blockNum   block number to read
   *
   */
  LCPReadBlock(const uint chipNum, const uint32_t blockSize, const uint32_t blockNum);
};

/**
 * @brief Write PMEM Block LCP-Cmd class
 */
class LCPWriteBlock : public LCPCmdPmemBase {
public:
  /**
   * C-tor of Write PMEM Block LCP-Cmd.
   *
   * @param[in] chipNum    value indicating which memory chip to access
   *
   * @param[in] blockSize  size of the block to write
   *
   * @param[in] blockNum   block number to write
   *
   */
  LCPWriteBlock(const uint chipNum, const uint32_t blockSize, const uint32_t blockNum);
};

/**
 * @brief Require Write Access LCP-Cmd class
 */
class LCPReqWriteAccess : public LCPCmdBase {
public:
  /**
   * C-tor of Require Write Access LCP-Cmd.
   *
   * @param[in] drvsessionID    sessionID value assigned to the driver instance.
   *
   */
  LCPReqWriteAccess(const uint16_t drvsessionID);

  std::string getWriterIP();

  uint16_t getWriterPort(){return static_cast<uint16_t>((getRespBufData(4) >> 16) & 0xFFFF);}

};

#endif // LCPPROTOCOL_H
