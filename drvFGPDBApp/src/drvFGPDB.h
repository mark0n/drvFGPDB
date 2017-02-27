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
#include <regex>
#include <list>

#include <asynPortDriver.h>
#include <asynOctetSyncIO.h>

#include <initHooks.h>

//-----------------------------------------------------------
enum class CtlrDataFmt {
  NotDefined,
  S32,       // signed 32-bit int
  U32,       // unsigned 32-bit int
  F32,       // 32-bit float
  U16_16,    // = (uint) (value * 2.0^16)
  PHASE      // = (int) (degrees * (2.0^32) / 360.0)
};

//-----------------------------------------------------------
enum class ParamGroup {
  Invalid,
  LCP_RO,  // Read-Only LCP register
  LCP_WA,  // Write-Anytime LCP register
  LCP_WO,  // Write-Once LCP register
  DRV_RO,  // Read-Only Driver parameter
  DRV_RW   // Read-Write Driver parameter
};


//-----------------------------------------------------------
/* For probable future use
//===== values for StaticRegInfo.syncMode =====
//
//  These values are used to specify what should be done for each writable
//  value when the IOC or the Ctlr has been restarted.
//
//  SM_DN:
//        Don't do anything special to the set value when resyncing the IOC and ctlr states.
//
//  SM_EQ:
//        These are values that, once the IOC has successfully sent the value
//        to the ctlr, should always be the same on the IOC and the ctlr. This
//        means that, if one of them is restarted after they were in sync, the
//        values can be restored from the one that did not restart.
//
//  SM_CM:
//        These are values that must be "reset" whenever the ctlr is restarted.
//        In this case, "reset" means changing the IOC's value to match the
//        ctlrs.  This is needed for values that control the things like
//        on/off, enabled/disabled, etc. where we do NOT want the previous
//        state to be reasserted automatically without human intervention when
//        a ctlr is restarted.
//
//  SM_IM:
//        These are values for which the readback from the controller may not
//        match the last setting.  For example, the IOC sends a new value for a
//        setpoint and the readback value indicates the currently "applied"
//        setpoint as it ramps toward the specified setpoint.  For these
//        values, the IOC copy cannot be (reliably) restored from the ctlr, so
//        a restarted IOC must restore the most recently saved value from the
//        persistent data files.
//
//  Of course, there is no way to guarantee that last saved value is equal to
//  the most recent user-supplied setpoint, but updating the saved copy
//  frequently will minimize the possiblity for a mismatch.
//----------------------------------------------
enum class SyncMode {
  NotDefined,
  SM_DN,   // Do Nothing (just leave default startup values)
  SM_EQ,   // IOC and ctlr values should be EQual
  SM_CM,   // Ctlr Master:  If ctlr restarted, use ctlr value
  SM_IM,   // IOC Master:  If IOC restarted, restore from file
};
*/

class drvFGPDB;

//-----------------------------------------------------------------------------
// Information the driver keeps about each parameter.  This list is generated
// during IOC startup from the data in the INP/OUT fields in the EPICS records
// that are linked to these parameters.
//-----------------------------------------------------------------------------
class ParamInfo {
  friend drvFGPDB;

  public:
    ParamInfo() :
      regAddr(0),
      asynType(asynParamNotDefined),
      ctlrFmt(CtlrDataFmt::NotDefined),
      group(ParamGroup::Invalid)  {};

    ParamInfo(const ParamInfo &info) :
      name(info.name),
      regAddr(info.regAddr),
      asynType(info.asynType),
      ctlrFmt(info.ctlrFmt),
      group(info.group)   {};

    ParamInfo(const std::string& paramStr, const std::string& portName);


    static asynParamType strToAsynType(const std::string &typeName);

    static CtlrDataFmt strToCtlrFmt(const std::string &fmtName);

    static ParamGroup regAddrToParamGroup(const uint regAddr);


    std::string    name;
    uint           regAddr;    // LCP reg addr or driver param group
    asynParamType  asynType;   // format of value used by driver
    CtlrDataFmt    ctlrFmt;    // format of value sent to/read from controller
  //SyncMode       syncMode;   // relation between set and read values

    ParamGroup     group;      // what processing group does param belong to
#ifndef TEST_DRVFGPDB
  private:
#endif
    static std::regex generateParamStrRegex();

    static const std::map<std::string, asynParamType> asynTypes;
    static const std::map<std::string, CtlrDataFmt> ctlrFmts;
};


//-----------------------------------------------------------------------------
// Return a string with the set of key values from a map
//-----------------------------------------------------------------------------
template <typename T>
std::string joinMapKeys(const std::map<std::string, T>& map,
                        const std::string& separator) {
  std::string joinedKeys;
  bool first = true;
  for(auto const& x : map) {
    if(!first)  joinedKeys += separator;
    first = false;
    joinedKeys += x.first;
  }
  return joinedKeys;
}


//-----------------------------------------------------------------------------


void drvFGPDB_initHookFunc(initHookState state);

//-----------------------------------------------------------------------------
class drvFGPDB : public asynPortDriver {

  public:
    drvFGPDB(const std::string &drvPortName, const std::string &udpPortName,
             int maxParams);
    ~drvFGPDB();


    // driver-specific versions of asynPortDriver virtual functions
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);

    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 newVal);


    // WARNING: The following functions are public ONLY for the purpose of
    // avoiding unnecessary complexity when writing tests.  These should NOT be
    // used outside of this class EXCEPT by those tests (i.e. the authors will
    // make changes to these functions as needed, so dependening on them to NOT
    // change is dangerous and will likely result in broken clients.

    asynStatus readRegs(epicsUInt32 firstReg, uint numRegs);
    asynStatus writeRegs(epicsUInt32 firstReg, uint numRegs);


    // clients should use asynPortDriver::findParam() instead
    asynStatus findParamByName(const std::string &name, int *paramID);

    asynStatus getParamInfo(int paramID, ParamInfo &paramInfo);

    asynStatus extractProperties(std::vector <std::string> &properties,
                                 ParamInfo &paramInfo);

    asynStatus updateParam(int paramID, const ParamInfo &newParam);

    asynStatus createAsynParams(void);

    asynStatus determineGroupRanges(void);
    void createProcessingGroups(void);
    int addParamToGroup(std::vector<int> &groupList, uint idx, int paramID);
    asynStatus sortParams(void);



//private:
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