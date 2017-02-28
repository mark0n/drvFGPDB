//----- drvFGPDB.h ----- 02/17/17 --- (01/24/17)----

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

#include <asynPortDriver.h>
#include <asynOctetSyncIO.h>
#include <initHooks.h>
#include "asynOctetSyncIOInterface.h"
#include "ParamInfo.h"

void drvFGPDB_initHookFunc(initHookState state);

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

    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 newVal);


#ifndef TEST_DRVFGPDB
  private:
#endif
    friend void drvFGPDB_initHookFunc(initHookState state);

    void addDriverParams(void);

    asynStatus readRegs(epicsUInt32 firstReg, uint numRegs);
    asynStatus writeRegs(epicsUInt32 firstReg, uint numRegs);

    // clients should use asynPortDriver::findParam() instead
    asynStatus findParamByName(const std::string &name, int *paramID);

    asynStatus getParamInfo(int paramID, ParamInfo &paramInfo);

    asynStatus updateParamDef(int paramID, const ParamInfo &newParam);

    asynStatus createAsynParams(void);

    asynStatus determineGroupRanges(void);
    void createProcessingGroups(void);
    int addParamToGroup(std::vector<int> &groupList, uint idx, int paramID);
    asynStatus sortParams(void);



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

    int maxParams;

    uint max_LCP_RO;  // largest regAddr for LCP_RO group of params
    uint max_LCP_WA;  // largest regAddr for LCP_WA group of params
    uint max_LCP_WO;  // largest regAddr for LCP_WO group of params
    uint num_DRV_RO;  // number of driver RO params
    uint num_DRV_RW;  // number of driver RW params

    epicsUInt32 packetID;

    std::vector<ParamInfo> paramList;

    // lists of the elements in paramList sorted by processing group
    std::vector<int> LCP_RO_group;  // LCP Read-Only parameters
    std::vector<int> LCP_WA_group;  // LCP Write-Anytime parameters
    std::vector<int> LCP_WO_group;  // LCP Write-Once parameters
    std::vector<int> DRV_RO_group;  // Driver Read-Only parameters
    std::vector<int> DRV_RW_group;  // Driver Read/Write parameters
    asynUser *pAsynUserUDP;   // asynUser for UDP asyn port
};

//-----------------------------------------------------------------------------
#endif // DRVFGPDB_H
