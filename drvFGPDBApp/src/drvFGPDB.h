
#include <string>

#include <asynPortDriver.h>


using namespace std;


typedef struct {
  string         name;
/*
  const char    *setName;    // only for writable registers
  bool           logChgs;    // log changes to the readback value
  int            syncMode;   // relation between set and read values
  dataTypeCtlr   ctlrType;   // format of value sent to/read from controller
  asynParamType  asynType;   // format of value used by driver
  bool           readOnly;   // read-only value
*/
} ParamInfo;


//-----------------------------------------------------------------------------
class drvFGPDB : public asynPortDriver {

  public:
    drvFGPDB(std::string drvPortName);

    // driver-specific versions of asynPortDriver virtual functions
    virtual asynStatus drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                     const char **pptypeName, size_t *psize);

    // functions unique to this driver
    asynStatus findParamByName(const string &name, int *index);


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
    ParamInfo  paramInfo[MaxParams];

};

//-----------------------------------------------------------------------------
