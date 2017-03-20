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
#include <asynOctetSyncIO.h>

#include <initHooks.h>

#include "asynOctetSyncIOInterface.h"
#include "ParamInfo.h"

void drvFGPDB_initHookFunc(initHookState state);

//-----------------------------------------------------------------------------
class RegGroup {
  public:
    RegGroup() :
      maxOffset(0)
    { };

    uint  maxOffset;             // largest addr offset (0 - 0xFFFF)
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
             const std::string &udpPortName, int maxParams);
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
    friend void drvFGPDB_initHookFunc(initHookState state);

    void syncComLCP(void);

    int getWriteAccess(void);

    int processPendingWrites(void);

    bool validParamID(int paramID)  {
      return ((uint)paramID < paramList.size()); }

    RegGroup & getRegGroup(uint groupID);
    bool inDefinedRegRange(uint firstReg, uint numRegs);

    void addRequiredParams(void);

    asynStatus readRegs(epicsUInt32 firstReg, uint numRegs);
    asynStatus writeRegs(epicsUInt32 firstReg, uint numRegs);

    asynStatus postNewReadVal(uint paramID);

    // clients should use asynPortDriver::findParam() instead
    int findParamByName(const std::string &name);

    asynStatus getParamInfo(int paramID, ParamInfo &paramInfo);

    asynStatus updateParamDef(int paramID, const ParamInfo &newParam);

    asynStatus createAsynParams(void);

    asynStatus determineAddrRanges(void);
    asynStatus createAddrToParamMaps(void);


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

    int maxParams;  // upper limit on total # params

    std::array<RegGroup,3> regGroup;

    epicsUInt32 packetID;

    std::vector<ParamInfo> paramList;

    asynUser *pAsynUserUDP;   // asynUser for UDP asyn port

    std::atomic<bool> stopProcessing;

    std::thread syncThread;

    std::atomic<bool> writeAccess;

    // paramIDs for required parameters
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

    int idSessionID;

    const std::list<RequiredParam> requiredParamDefs = {
       { &idDevName,       "devName        0x1 Octet"         },

       { &idSyncPktID,     "syncPktID      0x1 Int32"         },
       { &idSyncPktsSent,  "syncPktsSent   0x1 Int32"         },
       { &idSyncPktsRcvd,  "syncPktsRcvd   0x1 Int32"         },

       { &idAsyncPktID,    "asyncPktID     0x1 Int32"         },
       { &idAsyncPktsSent, "asyncPktsSent  0x1 Int32"         },
       { &idAsyncPktsRcvd, "asyncPktsRcvd  0x1 Int32"         },

       { &idCtlrAddr,      "ctlrAddr       0x1 Int32"         },

       { &idStateFlags,    "stateFlags     0x1 UInt32Digital" },
       { &idDiagFlags,     "diagFlags      0x2 UInt32Digital" },

       { &idSessionID,     "sessionID"  }
     };
;
};

//-----------------------------------------------------------------------------
#endif // DRVFGPDB_H
