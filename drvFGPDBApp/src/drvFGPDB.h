#ifndef DRVFGPDB_H
#define DRVFGPDB_H

/**
 * @file      drvFGPDB.h
 * @brief     asynPortDriver-based driver to communicate with controllers
 *            that support the LLRF Communication Protocol
 * @author    Mark Davis (davism50@msu.edu)
 * @author    Martin Konrad (konrad@frib.msu.edu)
 * @copyright Copyright (c) 2017, FRIB/NSCL, Michigan State University
 * @note      Release log (most recent 1st) (see git repository for more details)
 *            - 2017-01-24/2017-xx-xx: initial development of completely new version
 */

#include <string>
#include <vector>
#include <list>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

#include <asynPortDriver.h>

#include "asynOctetSyncIOInterface.h"
#include "ParamInfo.h"
#include "LCPProtocol.h"
#include "eventTimer.h"


// Bit usage for diagFlags parameter

const uint32_t  ShowPackets_    = 0x00000001; //!< Show pkts sent to ctlr                @warning TODO: Not currently used
const uint32_t  ShowContents_   = 0x00000002; //!< Show content sent to ctlr             @warning TODO: Not currently used
const uint32_t  ShowRegWrites_  = 0x00000004; //!< Show register writes info
const uint32_t  ShowRegReads_   = 0x00000008; //!< Show register reads info

const uint32_t  ShowWaveReads_  = 0x00000010; //!< Show waveform reads info              @warning TODO: Not currently used
const uint32_t  ShowBlkWrites_  = 0x00000020; //!< Show memory block writes info
const uint32_t  ShowBlkReads_   = 0x00000040; //!< Show memory block reads info
const uint32_t  ShowBlkErase_   = 0x00000080; //!< Show memory block erase info

const uint32_t  ShowErrors_     = 0x00000100; //!< Show runtime errors                   @warning TODO: Not currently used
const uint32_t  ShowParamState_ = 0x00000200; //!< Show current param state info         @warning TODO: Not currently used
const uint32_t  ForSyncThread_  = 0x00000400; //!< Show SyncThread info                  @warning TODO: Not currently used
const uint32_t  ForAsyncThread_ = 0x00000800; //!< Show AsyncThread info                 @warning TODO: Not currently used

const uint32_t  ShowInit_       = 0x00001000; //!< Show driver initialization info
const uint32_t  DebugTrace_     = 0x00004000; //!< Show debugging trace                  @warning TODO: Not currently used
const uint32_t  DisableStreams_ = 0x00008000; //!< Disable streams                       @warning TODO: Not currently used

const uint32_t  ShowCallbacks_  = 0x00010000; //!< Show eventTimer callbacks


/**
 * @brief Stores the IDs of all registered params from one processing group.
 */
class ProcGroup {
  public:
    std::vector<int>  paramIDs;  //!< ID of each param in this processing group
};

/**
 * @brief How controller and IOC restarts should be handled
 */
enum class ResendMode {
  AfterCtlrRestart,  //!< All settings resent whenever the ctlr restarts
  AfterIOCRestart,   //!< All settings resent whenever IOC restarts
  Never              //!< Old settings NEVER resent after IOC or ctlr restart
};

/**
 * @brief Enum class to describe currently used stateFlags.
 */
enum class eStateFlags{
  SyncConActive,
  AsyncConActive,
  AllRegsConnected,
  WriteAccess
};

/**
 * The main class of the driver.
 * Implements every method required to communicate with the ctlr and with
 * the asynDriver layer.
 */
class drvFGPDB : public asynPortDriver {

  public:

    /**
     * @brief Constructor of the driver instance
     *
     * @param[in] drvPortName       the name of the asyn port driver to be created
     * @param[in] syncIOWrapper     interface to perform "synchronous" I/O operations
     * @param[in] udpPortName       name of the UPD port to communicate with
     * @param[in] startupDiagFlags  diagnostics flag
     * @param[in] resendMode        mode to handle ctlr and IOC restarts
     */
    drvFGPDB(const std::string &drvPortName,
             std::shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
             const std::string &udpPortName, uint32_t startupDiagFlags,
             uint32_t resendMode);

    /**
     * @brief Destructor of the driver instance
     */
    ~drvFGPDB();

    /**
     * @brief Method to create and register new parameters in the driver
     *        and in the asynDriver layer (assigns pasynUser->reason)
     *
     * This func is called during IOC startup to get the ID for a parameter for a
     * given "port" (device) and "addr" (sub-device).  The first call for any
     * parameter must include the name and asynParamType, and at least one of the
     * calls for each parameter must include all the properties that define a
     * parameter. For EPICS records, the strings come from the record's INP or OUT
     * field (everything after the "@asyn(port, addr, timeout)" prefix).
     *
     * @param[in] pasynUser  structure that encodes the reason and the address
     * @param[in] drvInfo    string containing info about what drv function is being referenced
     * @param[in] pptypeName location in which driver can write information.
     * @param[in] psize      location where drv can write info about size of pptypeName
     */
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize)
                                     override;

    /**
     * @brief Called from the initHook registered function, sets the initComplete flag to true.
     *        Allows the syncComLCP thread to start the communication with the ctlr.
     */
    void startCommunication();

    /**
     * @brief Called from the initHook registered function, completes the initialization of
     *        all parameters related with read/write arrays
     */
    void completeArrayParamInit ();


    /**
     * @brief Returns the value for an integer from the parameter library.
     *
     * @param[in]  list   The parameter list number. Must be < maxAddr
     * @param[in]  index  The parameter number
     * @param[out] value  Value of the param
     *
     * @return asynStatus
     */
    virtual asynStatus getIntegerParam(int list, int index, int *value)
                                       override;
    /**
     * @brief Returns the value for a double from the parameter library.
     *
     * @param[in]  list  The parameter list number. Must be < maxAddr
     * @param[in]  index The parameter number
     * @param[out] value Value of the param
     *
     * @return asynStatus
     */
    virtual asynStatus getDoubleParam(int list, int index, double * value)
                                      override;
    /**
     * @brief Returns the value for a UInt32Digital from the parameter library.
     *
     * @param[in]  list  The parameter list number. Must be < maxAddr
     * @param[in]  index The parameter number
     * @param[out] value Value of the param
     * @param[in]  mask  The mask to apply when getting the value
     *
     * @return asynStatus
     */
    virtual asynStatus getUIntDigitalParam(int list, int index,
                                           epicsUInt32 *value, epicsUInt32 mask);
    /**
     * @brief Method called by EPICS client to set int32 values
     *
     * @param[in] pasynUser structure that encodes the reason and address
     * @param[in] newVal    new int32 value to set
     *
     * @return asynStatus
     */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 newVal)
                                  override;
    /**
     * @brief Method called by EPICS client to set UInt32Digital values
     *
     * @param[in] pasynUser structure that encodes the reason and address
     * @param[in] newVal    new UInt32Digital value to set
     * @param[in] mask      The mask to apply when setting the value
     *
     * @return asynStatus
     */
    virtual asynStatus writeUInt32Digital(asynUser *pasynUser,
                                          epicsUInt32 newVal, epicsUInt32 mask)
                                         override;
    /**
     * @brief Method called by EPICS client to set float values
     *
     * @param[in] pasynUser structure that encodes the reason and address
     * @param[in] newVal    new float value to set
     *
     * @return asynStatus
     */
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 newVal)
                                    override;

    /**
     * @brief Method called by EPICS client to read arrays of int32 values
     *
     * @param[in]  pasynUser structure that encodes the reason and address
     * @param[out] value     array read
     * @param[in]  nElements number of elements to read
     * @param[out] nIn       number of elements read
     *
     * @return asynStatus
     */
    virtual asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                     size_t nElements, size_t *nIn) override;
    /**
     * @brief Method called by EPICS client to write arrays of int8 values
     *
     * @param[in]  pasynUser structure that encodes the reason and address
     * @param[in]  values    array to write
     * @param[in]  nElements number of elements to write
     *
     * @return asynStatus
     */
    virtual asynStatus writeInt8Array(asynUser *pasynUser, epicsInt8 *values,
                                      size_t nElements) override;

    /**
     * @brief Returns the number of registered params in the driver
     *
     * @return number of params registered
     */
    uint numParams(void) const { return params.size(); }

    /**
     * @brief Method to update status value if no previous error
     *
     * @param[out] curStat state to be updated.
     * @param[in]  newStat new state if curStat not already an error value
     */
    static void setIfNewError(asynStatus &curStat, asynStatus newStat)
                  { if (curStat == asynSuccess)  curStat = newStat; }

    /**
     * @brief Method to check ID of thread that did a callback to make sure
     *        they always come from the same thread.
     */
    void checkCallbackThread(const std::string &funcName);

    /**
     * @brief Method to update the diagnostic flag at runtime
     *
     * @param[in] val new diagnostic flag
     */
    void setDiagFlags(uint32_t val) { diagFlags = val; };


#ifndef TEST_DRVFGPDB
  private:
#endif
    /**
     * @brief Method to reset read state of all param values
     *        - @b ReadStates set to @b Undefined
     *        - @b paramStatus set to @b asynDisconnected
     */
    void resetReadStates(void);
    /**
     * @brief Method to reset set state of all restored/written settings
     *        - @b setStates for settings changed to @b Pending
     */
    void resetSetStates(void);
    /**
     * @brief Method to change set state of restored param values
     *        - @b setStates changed from @b Restored to @b Sent
     */
    void clearSetStates(void);
    /**
     * @brief Method to abort any Pending/incomplete array write operations
     */
    void cancelArrayWrites(void);

    /**
     * @brief Method to determine if ctlr restarted since last connected
     *
     * @param[in] newUpSecs latest upSecs value
     */
    void checkForRestart(uint32_t newUpSecs);



    /**
     * @brief Attempt to gain write access to the ctlr (by setting the sessionID reg)
     *
     * @return asynStatus
     */
    asynStatus getWriteAccess(void);

    /**
     * @brief Event-timer callback func to maintain write access
     *
     * @return > 0: Use returned time until next callback
     *           0: Use Default time until next callback
     *         < 0: Sleep unless/until woken up
     */
    double keepWriteAccess(void);

    /**
     * @brief Event-timer callback func to update scalar readings
     *
     * @return > 0: Use returned time until next callback
     *           0: Use Default time until next callback
     *         < 0: Sleep unless/until woken up
     */
    double processScalarReads(void);

    /**
     * @brief Event-timer callback func to process pending writes to scalar
     *        values
     *
     * @return > 0: Use returned time until next callback
     *           0: Use Default time until next callback
     *         < 0: Sleep unless/until woken up
     */
    double processScalarWrites(void);

    /**
     * @brief Event-timer callback func to process pending/ongoing reads of
     *        array values
     *
     * @return > 0: Use returned time until next callback
     *           0: Use Default time until next callback
     *         < 0: Sleep unless/until woken up
     */
    double processArrayReads(void);

    /**
     * @brief Event-timer callback func to process pending/ongoing writes to
     *        array values
     *
     * @return > 0: Use returned time until next callback
     *           0: Use Default time until next callback
     *         < 0: Sleep unless/until woken up
     */
    double processArrayWrites(void);

    /*
     * @brief Event-timer callback func to post any changes to the readings
     *
     * @return asynStatus
     */
    double postNewReadings(void);

    /**
     * @brief Event-timer callback func to check if the ctlr is connected,
     *        disconnected or was rebooted
     *
     * @return > 0: Use returned time until next callback
     *           0: Use Default time until next callback
     *         < 0: Sleep unless/until woken up
     */
    double checkComStatus(void);

    /**
     * @brief Check if a given param is a valid param
     *
     * @param[in] paramID param to validate
     *
     * @return true/false
     */
    bool validParamID(int paramID) const {
      return ((uint)paramID < params.size()); }

    /**
     * @brief Check if all params inside a given range belong to the same
     *        processing group
     *
     * @param[in] firstReg address of the first param
     * @param[in] numRegs  number of params to evaluate
     *
     * @return true/false
     */
    bool inDefinedRegRange(uint firstReg, uint numRegs);


    /**
     * @brief Method to determine when to accept/reject new settings
     *
     * @return true if write operation should be accepted
     *         parameter
     */
    bool acceptWrites(void) {
      return ((connected and writeAccess) or !initComplete); }

    /**
     * @brief Method to register a new param in the driver
     *
     * @param newParam new parameter to be registered
     *
     * @return ID of the new parameter
     */
    int addNewParam(const ParamInfo &newParam);

    /**
     * @brief Process a param def and add it to the driver and asyn layer lists or update
     *        its properties.
     *
     * @param[in] paramDef string with the definition of the param.
     *
     * @return ID of the new parameter
     */
    int processParamDef(const std::string &paramDef);

    /**
     * @brief Method to register in the driver all required Parameters
     *
     * @return asynStatus
     */
    asynStatus addRequiredParams(void);
    asynStatus verifyReqParams(void) const;

    /**
     * @brief Method that calls syncIO interface to perform the action
     *        described in cmdBuf
     *
     * @param[in] pComPort UDP port to communicate with
     * @param[in] cmdBuf   vector that describes the action to perform in the ctlr
     *
     * @return asynStatus
     */
    asynStatus sendMsg(asynUser *pComPort, std::vector<uint32_t> &cmdBuf);

    /**
     * @brief Method that calls syncIO interface to perform the action
     *        described in cmdBuf
     *
     * @param[in]  pComPort         UDP port to communicate with
     * @param[out] respBuf          vector that stores the response to the action performed in the ctlr

     * @return # bytes read or -1 if an error
     */
    int  readResp(asynUser *pComPort, std::vector<uint32_t> &respBuf);

    /**
     * @brief Method that takes care of all the actions performed to the ctlr.
     *        Initializes all buffers needed and checks correct content between the
     *        msg sent to the ctlr and the received response.
     *
     * @param[in]  pComPort   UDP port to communicate with
     * @param[in]  cmdBuf     vector that describes the action to perform in the ctlr
     * @param[out] respBuf    vector that stores the response to the action performed in the ctlr
     * @param[out] respStatus LCP status returned by last command send to the ctlr
     *
     */
    asynStatus sendCmdGetResp(asynUser *pComPort,
                              std::vector<uint32_t> &cmdBuf,
                              std::vector<uint32_t> &respBuf,
                              LCPStatus  &respStatus);

    /**
     * @brief Method that reads the ctlr's current values for one or more LCP registers
     *
     * @param[in] firstReg address of the first register to read
     * @param[in] numRegs  number of registers to read
     *
     * @return asynStatus
     */
    asynStatus readRegs(epicsUInt32 firstReg, uint numRegs);

    /**
     * @brief Method that sends the driver's current value for one or more
     *        writeable LCP registers to the LCP controller
     *
     * @param[in] firstReg address of the first register to write
     * @param[in] numRegs number of registers to write
     *
     * @return asynStatus
     */
    asynStatus writeRegs(epicsUInt32 firstReg, uint numRegs);

    /**
     * @brief Method that updates the state of the specified asyn param
     *
     * @param[in] paramID ID of the param to update
     *
     * @return asynStatus
     */
    asynStatus setAsynParamVal(int paramID);

    /*
     * @brief Method that reads the lastest scalar values (from the controller
     *        and local driver variables) and updates the read state of the
     *        corresponding ParamInfo objects
     *
     * @return asynStatus
     */
    asynStatus updateScalarReadValues();

    /**
     * @brief Method to search a given param
     *
     * Search the driver's list of parameters for an entry with the given name.
     * Unlike asynPortDriver::findParam(), this func works during IOC startup.
     * Returns the paramID (index in to list) or < 0 if param not in the list.
     *
     * @param[in] name param name to search
     *
     * @warning clients should use asynPortDriver::findParam() instead
     *
     * @return paramID
     */
    int findParamByName(const std::string &name) const;

    /**
     * @brief Method that checks if a param is valid and returns a copy of
     *        the ParamInfo struct
     *
     * @param[in] paramID ID of the param to validate and get a copy
     *
     * @return copy of the param
     */
    ParamInfo& getParamInfo(const int paramID);

    /**
     * @brief Method that updates the regAddr to paramID maps
     *        based on the specified param def.
     *
     * @param[in] paramID ID of the param
     *
     * @return asynStatus
     */
    asynStatus updateRegMap(int paramID);

    /**
     * @brief Method to check if a valid param is writable or read-only
     *        The paramID is the pasynUser->reason
     *
     * @param[in] funcName  name of the function calling this method
     * @param[in] pasynUser structure that encodes the reason and address
     *
     * @return true/false
     */
    bool isValidWritableParam(const char *funcName, asynUser *pasynUser);
    /**
     * @brief Method that takes care of setting:
     *        - param's ctlrValSet to setVal
     *        - param's setState to Pending
     *        to update a ctlr register
     *
     * @param[in] param  param whose ctlrValSet is updated
     * @param[in] setVal new value to write to the ctlr
     */
    void applyNewParamSetting(ParamInfo &param, uint32_t setVal);

    /**
     * @brief Method that reads a block of data from Flash or one of the EEPROMs on the ctlr
     *
     * @note - blockSize must be a power of 2
     *       - blockNum is relative to blockSize (i.e. the first byte read is at
     *         offset + blockSize * blockNum)
     *
     * @param[in]  chipNum   memory chip to access
     * @param[in]  blockSize size (in bytes) of the block to be read
     * @param[in]  blockNum  block number to be read
     * @param[out] rwBuf     data read
     *
     * @return asynStatus
     */
    asynStatus readBlock(uint chipNum, uint32_t blockSize, uint32_t blockNum,
                          std::vector<uint8_t> &rwBuf);
    /**
     * @brief Method that erases a block of data in Flash or one of the EEPROMs on the ctlr
     *
     * @note - blockSize must be a power of 2
     *       - blockNum is relative to blockSize (i.e. the first byte erased is at
     *         offset + blockSize * blockNum)
     *
     * @param[in] chipNum   memory chip accessed
     * @param[in] blockSize size (in bytes) of the block to be erased
     * @param[in] blockNum  block number to erase
     *
     * @return asynStatus
     */
    asynStatus eraseBlock(uint chipNum, uint32_t blockSize, uint32_t blockNum);

    /**
     * @brief Method that writes a block of data to Flash or one of the EEPROMs on the ctlr
     *
     * @note - blockSize must be a power of 2
     *       - blockNum is relative to blockSize (i.e. the first byte written is at
     *         offset + blockSize * blockNum)
     *
     * @param[in] chipNum   memory chip to access
     * @param[in] blockSize size (in bytes) of the block to be written
     * @param[in] blockNum  block number to be written
     * @param[in] rwBuf     data to write
     *
     * @return asynStatus
     */
    asynStatus writeBlock(uint chipNum, uint32_t blockSize, uint32_t blockNum,
                          std::vector<uint8_t> &rwBuf);

    /**
     * @brief Method that reads next block of a PMEM array value from the ctlr
     *
     * @param[in] param parameter in charge of read PMEM
     *
     * @return asynStatus
     */
    asynStatus readNextBlock(ParamInfo &param);

    /**
     * @brief Method that sends next block of a new array value to the ctlr
     *
     * @param[in] param parameter in charge of write PMEM
     *
     * @return asynStatus
     */
    asynStatus writeNextBlock(ParamInfo &param);

    /**
     * @brief Method that initializes array parameter for readback operation
     *        and insures the readback event timer is active.
     *
     * @param[in] param parameter for the array value to be read
     */
    void initArrayReadback(ParamInfo &param);

    /**
     * @brief Method that sets the status param value to the percentage done
     *        for the current PMEM read or write operation.
     *
     * @param[in] param    the PMEM/array param
     *
     * @return asynStatus
     */
    asynStatus setArrayOperStatus(ParamInfo &param);


    static const int MaxAddr = 1;    //!< MAX number of asyn addresses supported by this driver

    static const int InterfaceMask = asynInt8ArrayMask | asynInt32Mask |
                                     asynInt32ArrayMask |
                                     asynUInt32DigitalMask | asynFloat64Mask |
                                     asynFloat64ArrayMask | asynOctetMask |
                                     asynDrvUserMask;
                                     //!< Asyn Interfaces supported by the driver

    static const int InterruptMask = asynInt8ArrayMask | asynInt32Mask |
                                     asynInt32ArrayMask |
                                     asynUInt32DigitalMask | asynFloat64Mask |
                                     asynFloat64ArrayMask | asynOctetMask;
                                     //!< Asyn Interfaces that can generate interrupts

    static const int AsynFlags = ASYN_CANBLOCK;
                                      /*!< Flags when creating the asyn port driver;
                                       *   includes ASYN_CANBLOCK and ASYN_MULTIDEVICE.\n
                                       *   This driver does not block and it is not multi-device
                                       */
    static const int AutoConnect = 1; /*!< Flag for the asyn port driver (1->autoconnect)*/
    static const int Priority = 0;    /*!< The thread priority for the asyn port driver if ASYN_CANBLOCK.\n
                                       *   0 -> epicsThreadPriorityMedium (default value)
                                       */
    static const int StackSize = 0;   /*!< The stack size for the asyn port driver thread if ASYN_CANBLOCK.\n
                                       *   0 -> epicsThreadStackMedium (default value)
                                       */
    static const uint  TimerThreadPriority = epicsThreadPriorityMedium;

    epicsTimerQueueActive  &timerQueue;  //<! queue used by timer thread to manage our timer events

    eventTimer  writeAccessTimer;     //<! To manage writeAccess keep-alives
    eventTimer  scalarReadsTimer;     //<! To periodically update scalar readings
    eventTimer  scalarWritesTimer;    //<! To process pending writes to scalar values
    eventTimer  arrayReadsTimer;      //<! To process pending reads of array values
    eventTimer  arrayWritesTimer;     //<! To process pending writes to array values
    eventTimer  postNewReadingsTimer; //<! To post the latest readings
    eventTimer  comStatusTimer;       //<! To periodically update status of connection

    std::thread::id callback_thread_id;


    std::shared_ptr<asynOctetSyncIOInterface> syncIO; //!< interface to perform "synchronous" I/O operations

    std::array<ProcGroup, ProcessGroupSize> procGroup; //!< 2D-array w/ all supported proc groups and their size

    /**
     * @brief Method that returns a reference to a ProcGroup object for the specified groupID.
     *
     * @param[in] groupID procGroup's ID
     *
     * @return reference to the procGroup
     */
    ProcGroup & getProcGroup(uint groupID);

    /**
     * @brief Method to get the procGroup's size
     *
     * @param[in] groupID procGroup's ID
     *
     * @return size
     */
    int procGroupSize(uint groupID)  {
           return getProcGroup(groupID).paramIDs.size(); }

    /**
     * @brief update stateFlag with newValue
     *
     * @param[in] bitPos bit to be updated
     * @param[in] value  true/false
     */
    void setStateFlags(eStateFlags bitPos, bool value);

    std::vector<ParamInfo> params;  //!< Vector with all the parameters registered in the driver

    uint ParamID(ParamInfo &param)  { return (&param - params.data()); }

    asynUser *pAsynUserUDP;          //!< asynUser for UDP asyn port

    std::atomic<bool> initComplete;  //!< initialization has finished
    std::atomic<bool> exitDriver;    //!< exit the driver. Set to true by epicsAtExit registered function

    std::atomic<bool> writeAccess;   //!< the driver has write access to the ctlr

    bool  arrayWritesInProgress;     //!< actively writing array values
    bool  arrayReadsInProgress;      //!< actively reading array values
    bool  updateRegs;                //!< registers must be updated
    bool  firstRestartCheck;         //!< 1st time testing for ctlr restart

    bool  connected;
    std::chrono::system_clock::time_point  lastRespTime,   //!< time of the last response received from the ctlr
                                           lastWriteTime;  //!< time of last write to the ctlr

    //=== paramIDs for required parameters ===
    // reg values the ctlr must support
    int idUpSecs;         uint32_t upSecs;          //!< ctlr's number of seconds awake
    int idSessionID;      LCP::sessionId sessionID; //!< ctlr's sessionID

    //driver-only values
    int idSyncPktID ;     uint32_t syncPktID;       //!< ID of last packet sent/received
    int idSyncPktsSent;   uint32_t syncPktsSent;    //!< Updated in sendMsg()
    int idSyncPktsRcvd;   uint32_t syncPktsRcvd;    //!< Updated in readResp()

    int idAsyncPktID;     uint32_t asyncPktID;      //!< TODO: Not being used
    int idAsyncPktsSent;  uint32_t asyncPktsSent;   //!< TODO: Not being used
    int idAsyncPktsRcvd;  uint32_t asyncPktsRcvd;   //!< TODO: Not being used

    int idStateFlags;     uint32_t stateFlags;      /*< Bits currently used:
                                                        - SyncConActive:  0x00000001
                                                        - AsyncConActive: 0x00000002
                                                        - UndefRegs:      0x00000004
                                                        - DisconRegs:     0x00000008
                                                        - WriteAccess:    0x00000010
                                                     */

    int idCtlrUpSince;    uint32_t ctlrUpSince;     //!< last time ctlr restarted

    ResendMode  resendMode;  //!< mode for determining if/when to resend settings to the ctlr

    int  idDiagFlags;     uint32_t diagFlags;

    bool ShowPackets() const     { return diagFlags & ShowPackets_;    } //!< true if ShowPackets_ enabled
    bool ShowContents() const    { return diagFlags & ShowContents_;   } //!< true if ShowContents_ enabled
    bool ShowRegWrites() const   { return diagFlags & ShowRegWrites_;  } //!< true if ShowRegWrites_ enabled
    bool ShowRegReads() const    { return diagFlags & ShowRegReads_;   } //!< true if ShowRegReads_ is enabled

    bool ShowWaveReads() const   { return diagFlags & ShowWaveReads_;  } //!< true if ShowWaveReads_ enabled
    bool ShowBlkWrites() const   { return diagFlags & ShowBlkWrites_;  } //!< true if ShowBlkWrites_ enabled
    bool ShowBlkReads() const    { return diagFlags & ShowBlkReads_;   } //!< true if ShowBlkReads_ enabled
    bool ShowBlkErase() const    { return diagFlags & ShowBlkErase_;   } //!< true if ShowBlkErase_ enabled

    bool ShowErrors() const      { return diagFlags & ShowErrors_;     } //!< true if ShowErrors_ enabled
    bool ShowParamState() const  { return diagFlags & ShowParamState_; } //!< true if ShowParamState_ enabled
    bool ForSyncThread() const   { return diagFlags & ForSyncThread_;  } //!< true if ForSyncThread_ enabled
    bool ForAsyncThread() const  { return diagFlags & ForAsyncThread_; } //!< true if ForAsyncThread_ enabled

    bool ShowInit() const        { return diagFlags & ShowInit_;       } //!< true if ShowInit_ enabled
    bool DebugTrace() const      { return diagFlags & DebugTrace_;     } //!< true if DebugTrace_ enabled
//  bool DisableStreams()   { return (diagFlags & _DisableStreams_)
//                                                 or readOnlyMode; }

    bool ShowCallbacks() const   { return diagFlags & ShowCallbacks_;  } //!< true if ShowCallbacks_ enabled
};

//-----------------------------------------------------------------------------
#endif // DRVFGPDB_H
