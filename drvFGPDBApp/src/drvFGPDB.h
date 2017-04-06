//----- drvFGPDB.h ----- 03/10/17 --- (01/24/17)----

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

#include <initHooks.h>

#include "asynOctetSyncIOInterface.h"
#include "ParamInfo.h"

void drvFGPDB_initHookFunc(initHookState state);


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
class RegGroup {
  public:
    std::vector<int>  paramIDs;  // offset to paramID map
};

//-----------------------------------------------------------------------------
class RequiredParam {
  public:
    int         *id;    // addr of int value to save the paramID
    std::string  def;   // string that defines the parameter
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
                                     const char **pptypeName, size_t *psize);

    virtual asynStatus getIntegerParam(int list, int index, int *value);

    virtual asynStatus getDoubleParam(int list, int index, double * value);

    virtual asynStatus getUIntDigitalParam(int list, int index,
                                           epicsUInt32 *value, epicsUInt32 mask);

    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 newVal);

    virtual asynStatus writeUInt32Digital(asynUser *pasynUser,
                                          epicsUInt32 newVal, epicsUInt32 mask);

    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 newVal);

    // functions that clients can use safely
    bool isWritableTypeOf(const std::string &caller,
                          int paramID, asynParamType asynType);

    uint numParams(void) { return paramList.size(); }



#ifndef TEST_DRVFGPDB
  private:
#endif
    void syncComLCP(void);

    asynStatus getWriteAccess(void);

    int processPendingWrites(void);

    bool validParamID(int paramID)  {
      return ((uint)paramID < paramList.size()); }

    RegGroup & getRegGroup(uint groupID);
    bool inDefinedRegRange(uint firstReg, uint numRegs);

    int addNewParam(const ParamInfo &newParam);

    int processParamDef(const std::string &paramDef);

    asynStatus addRequiredParams(void);

    asynStatus readRegs(epicsUInt32 firstReg, uint numRegs);
    asynStatus writeRegs(epicsUInt32 firstReg, uint numRegs);

    asynStatus postNewReadVal(int paramID);

    // clients should use asynPortDriver::findParam() instead
    int findParamByName(const std::string &name);

    std::pair<asynStatus, ParamInfo> getParamInfo(int paramID);

    asynStatus updateRegMap(int paramID);


    static const int MaxAddr = 1;
    static const int InterfaceMask = asynInt8ArrayMask | asynInt32Mask |
                                     asynUInt32DigitalMask | asynFloat64Mask |
                                     asynFloat64ArrayMask | asynOctetMask |
                                     asynDrvUserMask;
    static const int InterruptMask = asynInt8ArrayMask | asynInt32Mask |
                                     asynUInt32DigitalMask | asynFloat64Mask |
                                     asynFloat64ArrayMask | asynOctetMask;
    static const int AsynFlags = 0;
    static const int AutoConnect = 1;
    static const int Priority = 0;
    static const int StackSize = 0;


    std::shared_ptr<asynOctetSyncIOInterface> syncIO;

    std::array<RegGroup,3> regGroup;

    epicsUInt32 packetID;

    std::vector<ParamInfo> paramList;

    asynUser *pAsynUserUDP;   // asynUser for UDP asyn port

    std::atomic<bool> exitDriver;

    std::thread syncThread;

    std::atomic<bool> writeAccess;

    //=== paramIDs for required parameters ===
    // reg values the ctlr must support
    int idSessionID;

    //driver-only values
    int idDevName;

    int idSyncPktID;
    int idSyncPktsSent;
    int idSyncPktsRcvd;

    int idAsyncPktID;
    int idAsyncPktsSent;
    int idAsyncPktsRcvd;

    int idCtlrAddr;

    int idStateFlags;

    int idDiagFlags;


    uint32_t  startupDiagFlags;


    uint32_t DiagFlags()  {
      return (idDiagFlags < 0) ? startupDiagFlags :
                                 paramList.at(idDiagFlags).ctlrValSet;
    }

    bool ShowPackets()      { return DiagFlags() & ShowPackets_;    }
    bool ShowContents()     { return DiagFlags() & ShowContents_;   }
    bool ShowRegWrites()    { return DiagFlags() & ShowRegWrites_;  }
    bool ShowRegReads()     { return DiagFlags() & ShowRegReads_;   }

    bool ShowWaveReads()    { return DiagFlags() & ShowWaveReads_;  }
    bool ShowBlkWrites()    { return DiagFlags() & ShowBlkWrites_;  }
    bool ShowBlkReads()     { return DiagFlags() & ShowBlkReads_;   }
    bool ShowBlkErase()     { return DiagFlags() & ShowBlkErase_;   }

    bool ShowErrors()       { return DiagFlags() & ShowErrors_;     }
    bool ShowParamState()   { return DiagFlags() & ShowParamState_; }
    bool ForSyncThread()    { return DiagFlags() & ForSyncThread_;  }
    bool ForAsyncThread()   { return DiagFlags() & ForAsyncThread_; }

    bool ShowInit()         { return DiagFlags() & ShowInit_;       }
    bool TestMode()         { return DiagFlags() & TestMode_;       }
    bool DebugTrace()       { return DiagFlags() & DebugTrace_;     }
//    bool DisableStreams()   { return (DiagFlags() & _DisableStreams_)
//                                                    or readOnlyMode; }

    const std::list<RequiredParam> requiredParamDefs = {
       //--- reg values the ctlr must support ---
       { nullptr,          "hardVersion    0x0 Int32 U32"     },
       { nullptr,          "firmVersion    0x0 Int32 U32"     },

       { nullptr,          "flashId        0x0 Int32 U32"     },

       { nullptr,          "devType        0x0 Int32 U32"     },
       { nullptr,          "devID          0x0 Int32 U32"     },

       { nullptr,          "upSecs         0x0 Int32 U32"     },
       { nullptr,          "upMs           0x0 Int32 U32"     },

       { nullptr,          "writerIP       0x0 Int32 U32"     },
       { nullptr,          "writerPort     0x0 Int32 U32"     },

       { &idSessionID,     "sessionID      0x0 Int32 U32"     },

       //--- driver-only values ---
       { &idDevName,       "devName        0x1 Octet"         },

       { &idSyncPktID,     "syncPktID      0x1 Int32"         },
       { &idSyncPktsSent,  "syncPktsSent   0x1 Int32"         },
       { &idSyncPktsRcvd,  "syncPktsRcvd   0x1 Int32"         },

       { &idAsyncPktID,    "asyncPktID     0x1 Int32"         },
       { &idAsyncPktsSent, "asyncPktsSent  0x1 Int32"         },
       { &idAsyncPktsRcvd, "asyncPktsRcvd  0x1 Int32"         },

       { &idCtlrAddr,      "ctlrAddr       0x1 Int32"         },

       { &idStateFlags,    "stateFlags     0x1 UInt32Digital" },

       { &idDiagFlags,     "diagFlags      0x2 UInt32Digital" }
     };

};

//-----------------------------------------------------------------------------
#endif // DRVFGPDB_H
