
#include <string>
#include <vector>

#include <asynPortDriver.h>


using namespace std;



enum class ParamGroup {
  NotDefined,
  LCP_RO,  // Read-Only LCP register
  LCP_WA,  // Write-Anytime LCP register
  LCP_WO,  // Write-Once LCP register
  DRV_RO,  // Read-Only driver value
  DRV_RW,  // Read-Write driver value
};

enum class CtlrDataFmt {
  NotDefined,
  S32,       // signed 32-bit int
  U32,       // unsigned 32-bit int
  F32,       // 32-bit float
  U16_16,    // = (uint) (value * 2.0^16)
  PHASE      // = (int) (degrees * (2.0^32) / 360.0)
};

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


// Information the driver keeps about each parameter.  This list is generated
// during IOC startup from the data in the INP/OUT fields in the EPICS records
// that are linked to these parameters.
class ParamInfo {
  public:
    ParamInfo()  {
      group    = ParamGroup::NotDefined;
      regNum   = 0;
      asynType = asynParamNotDefined;
      ctlrFmt  = CtlrDataFmt::NotDefined;
      syncMode = SyncMode::NotDefined;
    };

    ParamInfo(const ParamInfo &info) {
      name     = info.name;
      group    = info.group;
      regNum   = info.regNum;
      asynType = info.asynType;
      ctlrFmt  = info.ctlrFmt;
      syncMode = info.syncMode;
    };

    string         name;
    ParamGroup     group;      // what group the param belongs to
    uint           regNum;     // reg # for LCP_xx group params
    asynParamType  asynType;   // format of value used by driver
    CtlrDataFmt    ctlrFmt;    // format of value sent to/read from controller
    SyncMode       syncMode;   // relation between set and read values
};


//-----------------------------------------------------------------------------
class drvFGPDB : public asynPortDriver {

  public:
    drvFGPDB(const string &drvPortName);

    // driver-specific versions of asynPortDriver virtual functions
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);


    // functions unique to this driver
    asynStatus findParamByName(const string &name, int *paramID);

    asynStatus getParamInfo(int paramID, ParamInfo &paramInfo);


    ParamGroup strToParamGroup(const string &groupName);

    asynParamType strToAsynType(const string &typeName);

    CtlrDataFmt strToCtlrFmt(const string &fmtName);

    SyncMode strToSyncMode(const string &modeName);

    asynStatus extractProperties(vector <string> &properties,
                                 ParamInfo &paramInfo);

    asynStatus updateParam(int paramID, const ParamInfo &newParam);


  private:
    static const int MaxAddr = 1;
    static const int MaxParams = 200;
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


    int  numParams;
    ParamInfo  paramList[MaxParams];

};

//-----------------------------------------------------------------------------
