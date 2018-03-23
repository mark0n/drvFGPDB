#ifndef LCPPROTOCOL_H
#define LCPPROTOCOL_H

/**
 * @file  LCPProtocol.h
 * @brief Defines extra utilities of the LLRF Control Protocol.
 */

#include <cstdint>
#include <map>
#include <random>

/**
 * @brief LCP commands to communicate with the ctlr.
 */
enum class LCPCommand : std::int32_t {
  READ_REGS     = 1,    //!< Read registers
  WRITE_REGS    = 2,    //!< Write Registers
  READ_WAVEFORM = 3,    //!< Read Waveform
  ERASE_BLOCK   = 4,    //!< Ease memory block
  READ_BLOCK    = 5,    //!< Read memory block
  WRITE_BLOCK   = 6     //!< Write memory block
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
   * @brief Method to know the offset of the LCP_Status in the ctlr response
   *        depending of the cmd sent
   *
   * @param[in] cmdID LCP_Command sent
   *
   * @return Offset of the LCP_Status
   */
  static int16_t statusOffset(const int16_t cmdID)
  {
    auto it = StatusOffset.find(cmdID);
    return it == StatusOffset.end() ? 0 : it->second;
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

  static const std::map<int16_t, int16_t> StatusOffset; //!< Map w/ LCP_Status cmd offset in ctlr response
};

namespace LCP {
  /**
   * Class that takes care of the sessionID. It handles its generation and its interface with the driver
   */
  class sessionId {
    std::default_random_engine randGen; //!< Pseudo-random numbers generator
  public:
    uint16_t sId;    //!< SessionID. @warning Its use from outside of this class is deprecated

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
  };
}


#endif // LCPPROTOCOL_H
