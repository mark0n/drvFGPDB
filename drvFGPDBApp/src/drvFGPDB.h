//-----------------------------------------------------------------------------
//  asynPortDriver-based interface for controllers that support the FRIB LCP
//  protocol.
//
//  Copyright (c) 2017, FRIB/NSCL, Michigan State University
//
//  Authors:
//      Mark Davis (davism50@msu.edu)
//      Martin Konrad (konrad@frib.msu.edu)
//-----------------------------------------------------------------------------
//  Release log (most recent 1st) (see git repository for more details)
//
//
//  2017-01-24 - 2017-xx-xx:
//       (initial development of completely new version)
//
//-----------------------------------------------------------------------------
#ifndef DRVFGPDB_H
#define DRVFGPDB_H

#include <map>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <thread>
#include <atomic>

#include <asynPortDriver.h>

#include "asynOctetSyncIOInterface.h"
#include "ParamInfo.h"
#include "LCPProtocol.h"

// Bit usage for diagFlags parameter

const uint32_t  ShowPackets_    = 0x00000001;
const uint32_t  ShowContents_   = 0x00000002;
const uint32_t  ShowRegWrites_  = 0x00000004;
const uint32_t  ShowRegReads_   = 0x00000008;

const uint32_t  ShowWaveReads_  = 0x00000010;
const uint32_t  ShowBlkWrites_  = 0x00000020;
const uint32_t  ShowBlkReads_   = 0x00000040;
const uint32_t  ShowBlkErase_   = 0x00000080;

const uint32_t  ShowErrors_     = 0x00000100;
const uint32_t  ShowParamState_ = 0x00000200;
const uint32_t  ForSyncThread_  = 0x00000400;
const uint32_t  ForAsyncThread_ = 0x00000800;

const uint32_t  ShowInit_       = 0x00001000;
const uint32_t  TestMode_       = 0x00002000;
const uint32_t  DebugTrace_     = 0x00004000;
const uint32_t  DisableStreams_ = 0x00008000;


//-----------------------------------------------------------------------------
class ProcGroup {
  public:
    std::vector<int>  paramIDs;  // offset to paramID map
};

//-----------------------------------------------------------------------------
class RequiredParam {
  public:
    int         *id;      // addr of int value to save the paramID
    void        *drvVal;  // for driver-only params
    std::string  def;     // string that defines the parameter
};


//-----------------------------------------------------------------------------
class drvFGPDB : public asynPortDriver {

  public:
    drvFGPDB(const std::string &drvPortName,
             std::shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
             const std::string &udpPortName, uint32_t startupDiagFlags);
    ~drvFGPDB();


    // driver-specific versions of asynPortDriver virtual functions
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize)
                                     override;

    void startCommunication();

    virtual asynStatus getIntegerParam(int list, int index, int *value)
                                       override;

    virtual asynStatus getDoubleParam(int list, int index, double * value)
                                      override;

    virtual asynStatus getUIntDigitalParam(int list, int index,
                                           epicsUInt32 *value, epicsUInt32 mask);

    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 newVal)
                                  override;

    virtual asynStatus writeUInt32Digital(asynUser *pasynUser,
                                          epicsUInt32 newVal, epicsUInt32 mask)
                                         override;

    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 newVal)
                                    override;


    virtual asynStatus readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                     size_t nElements, size_t *nIn) override;

    virtual asynStatus writeInt8Array(asynUser *pasynUser, epicsInt8 *values,
                                      size_t nElements) override;

    uint numParams(void) { return params.size(); }

    static void setIfNewError(asynStatus &curStat, asynStatus newStat)
                  { if (curStat == asynSuccess)  curStat = newStat; }

    void setDiagFlags(uint32_t val) { diagFlags = val; };


#ifndef TEST_DRVFGPDB
  private:
#endif
    void syncComLCP(void);

    void checkComStatus(void);
    void resetParamStates(void);

    asynStatus getWriteAccess(void);

    int processPendingWrites(void);

    bool validParamID(int paramID)  {
      return ((uint)paramID < params.size()); }

    bool inDefinedRegRange(uint firstReg, uint numRegs);

    int addNewParam(const ParamInfo &newParam);

    int processParamDef(const std::string &paramDef);

    asynStatus addRequiredParams(void);

    asynStatus sendMsg(asynUser *pComPort, std::vector<uint32_t> &cmdBuf);
    asynStatus readResp(asynUser *pComPort, std::vector<uint32_t> &respBuf,
                        size_t expectedRespSize);

    asynStatus sendCmdGetResp(asynUser *pComPort,
                              std::vector<uint32_t> &cmdBuf,
                              std::vector<uint32_t> &respBuf,
                              LCPStatus  &respStatus);

    asynStatus readRegs(epicsUInt32 firstReg, uint numRegs);
    asynStatus writeRegs(epicsUInt32 firstReg, uint numRegs);

    asynStatus postNewReadVal(int paramID);

    asynStatus updateReadValues();
    asynStatus postNewReadValues();


    // clients should use asynPortDriver::findParam() instead
    int findParamByName(const std::string &name);

    std::pair<asynStatus, ParamInfo> getParamInfo(int paramID);

    asynStatus updateRegMap(int paramID);

    bool isValidWritableParam(const char *funcName, asynUser *pasynUser);

    void applyNewParamSetting(ParamInfo &param, uint32_t setVal);


    asynStatus readBlock(uint chipNum, uint32_t blockSize, uint32_t blockNum,
                          std::vector<uint8_t> &rwBuf);

    asynStatus eraseBlock(uint chipNum, uint32_t blockSize, uint32_t blockNum);

    asynStatus writeBlock(uint chipNum, uint32_t blockSize, uint32_t blockNum,
                          std::vector<uint8_t> &rwBuf);

    asynStatus readNextBlock(ParamInfo &param);
    asynStatus writeNextBlock(ParamInfo &param);

    asynStatus setArrayOperStatus(ParamInfo &param, uint32_t percDone);


    static const int MaxAddr = 1;
    static const int InterfaceMask = asynInt8ArrayMask | asynInt32Mask |
                                     asynInt32ArrayMask |
                                     asynUInt32DigitalMask | asynFloat64Mask |
                                     asynFloat64ArrayMask | asynOctetMask |
                                     asynDrvUserMask;
    static const int InterruptMask = asynInt8ArrayMask | asynInt32Mask |
                                     asynInt32ArrayMask |
                                     asynUInt32DigitalMask | asynFloat64Mask |
                                     asynFloat64ArrayMask | asynOctetMask;
    static const int AsynFlags = 0;   // This driver does not block and it is not multi-device
    static const int AutoConnect = 1;
    static const int Priority = 0;    // 0 -> epicsThreadPriorityMedium (default value) Only used if ASYN_CANBLOCK
    static const int StackSize = 0;   // 0 -> epicsThreadStackMedium (default value) Only used if ASYN_CANBLOCK


    std::shared_ptr<asynOctetSyncIOInterface> syncIO;

    std::array<ProcGroup, ProcessGroupSize> procGroup;
    ProcGroup & getProcGroup(uint groupID);
    int procGroupSize(uint groupID)  {
           return getProcGroup(groupID).paramIDs.size(); }

    std::vector<ParamInfo> params;
    uint ParamID(ParamInfo &param)  { return (&param - params.data()); }

    asynUser *pAsynUserUDP;   // asynUser for UDP asyn port

    std::atomic<bool> initComplete;
    std::atomic<bool> exitDriver;

    std::thread syncThread;

    std::atomic<bool> writeAccess;

    bool  unfinishedArrayRWs;
    bool  updateRegs;

    bool  connected;
    std::chrono::system_clock::time_point  lastRespTime;


    //=== paramIDs for required parameters ===
    // reg values the ctlr must support
    int idUpSecs;         uint32_t upSecs, prevUpSecs;
    int idSessionID;      LCP::sessionId sessionID;

    //driver-only values
    int idSyncPktID ;     uint32_t syncPktID;
    int idSyncPktsSent;   uint32_t syncPktsSent;
    int idSyncPktsRcvd;   uint32_t syncPktsRcvd;

    int idAsyncPktID;     uint32_t asyncPktID;
    int idAsyncPktsSent;  uint32_t asyncPktsSent;
    int idAsyncPktsRcvd;  uint32_t asyncPktsRcvd;

    int idStateFlags;     uint32_t stateFlags;

    uint32_t diagFlags;

    bool ShowPackets()      { return diagFlags & ShowPackets_;    }
    bool ShowContents()     { return diagFlags & ShowContents_;   }
    bool ShowRegWrites()    { return diagFlags & ShowRegWrites_;  }
    bool ShowRegReads()     { return diagFlags & ShowRegReads_;   }

    bool ShowWaveReads()    { return diagFlags & ShowWaveReads_;  }
    bool ShowBlkWrites()    { return diagFlags & ShowBlkWrites_;  }
    bool ShowBlkReads()     { return diagFlags & ShowBlkReads_;   }
    bool ShowBlkErase()     { return diagFlags & ShowBlkErase_;   }

    bool ShowErrors()       { return diagFlags & ShowErrors_;     }
    bool ShowParamState()   { return diagFlags & ShowParamState_; }
    bool ForSyncThread()    { return diagFlags & ForSyncThread_;  }
    bool ForAsyncThread()   { return diagFlags & ForAsyncThread_; }

    bool ShowInit()         { return diagFlags & ShowInit_;       }
    bool TestMode()         { return diagFlags & TestMode_;       }
    bool DebugTrace()       { return diagFlags & DebugTrace_;     }
//  bool DisableStreams()   { return (diagFlags & _DisableStreams_)
//                                                 or readOnlyMode; }

    const std::list<RequiredParam> requiredParamDefs = {
       //--- reg values the ctlr must support ---
       // Use addr 0x0 for LCP reg values (LCP addr is supplied by EPICS recs)
       //ptr-to-paramID    drvVal          param name     addr asyn  ctlr
       { &idUpSecs,        &upSecs,        "upSecs         0x0 Int32 U32"     },
       { nullptr,          nullptr,        "upMs           0x0 Int32 U32"     },

       { nullptr,          nullptr,        "writerIP       0x0 Int32 U32"     },
       { nullptr,          nullptr,        "writerPort     0x0 Int32 U32"     },

       { &idSessionID,     &sessionID.sId, "sessionID      0x0 Int32 U32"     }, //FIXME: replace drvVal by get/set functions

       //--- driver-only values ---
       // addr 0x1 == Read-Only, 0x2 = Read/Write
       { &idSyncPktID,     &syncPktID,     "syncPktID      0x1 Int32"         },
       { &idSyncPktsSent,  &syncPktsSent,  "syncPktsSent   0x1 Int32"         },
       { &idSyncPktsRcvd,  &syncPktsRcvd,  "syncPktsRcvd   0x1 Int32"         },

       { &idAsyncPktID,    &asyncPktID,    "asyncPktID     0x1 Int32"         },
       { &idAsyncPktsSent, &asyncPktsSent, "asyncPktsSent  0x1 Int32"         },
       { &idAsyncPktsRcvd, &asyncPktsRcvd, "asyncPktsRcvd  0x1 Int32"         },

       { &idStateFlags,    &stateFlags,    "stateFlags     0x1 UInt32Digital" },
     };

};

//-----------------------------------------------------------------------------
#endif // DRVFGPDB_H
