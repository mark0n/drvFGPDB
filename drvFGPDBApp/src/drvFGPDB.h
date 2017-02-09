
#include <string>
#include <vector>

#include <asynPortDriver.h>

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

// Information the driver keeps about each parameter.  This list is generated
// during IOC startup from the data in the INP/OUT fields in the EPICS records
// that are linked to these parameters.
class ParamInfo {
  public:
    ParamInfo()  {
      group    = ParamGroup::NotDefined;
      regAddr  = 0;
      asynType = asynParamNotDefined;
      ctlrFmt  = CtlrDataFmt::NotDefined;
    };

    ParamInfo(const ParamInfo &info) {
      name     = info.name;
      group    = info.group;
      regAddr  = info.regAddr;
      asynType = info.asynType;
      ctlrFmt  = info.ctlrFmt;
    };

    std::string    name;
    ParamGroup     group;      // what group the param belongs to
    uint           regAddr;    // register address for parameter
    asynParamType  asynType;   // format of value used by driver
    CtlrDataFmt    ctlrFmt;    // format of value sent to/read from controller
};


//-----------------------------------------------------------------------------
class drvFGPDB : public asynPortDriver {

  public:
    drvFGPDB(const std::string &drvPortName);

    // driver-specific versions of asynPortDriver virtual functions
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);


    // functions unique to this driver
    asynStatus findParamByName(const std::string &name, int *paramID);

    asynStatus getParamInfo(int paramID, ParamInfo &paramInfo);


    ParamGroup strToParamGroup(const std::string &groupName);

    asynParamType strToAsynType(const std::string &typeName);

    CtlrDataFmt strToCtlrFmt(const std::string &fmtName);

    asynStatus extractProperties(std::vector <std::string> &properties,
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
