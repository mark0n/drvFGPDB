//----- drvFGPDB_IOC.cpp ----- 04/06/17 --- (01/24/17)----

#include <memory>

#include <iocsh.h>
#include <epicsExport.h>

#include "drvFGPDB.h"
#include "asynOctetSyncIOWrapper.h"

using namespace std;

static shared_ptr<asynOctetSyncIOWrapper> syncIOWrapper;

extern "C" {

//-----------------------------------------------------------------------------
// EPICS iocsh callable func to call constructor for the drvFGPDB class.
//
//  \param[in] drvPortName The name of the asyn port driver to be created and
//             that this module extends.
//  \param[in] udpPortName The name of the asyn port for the UDP connection to
//             the device.
//-----------------------------------------------------------------------------
int drvFGPDB_Config(char *drvPortName, char *udpPortName,
                    int startupDiagFlags_)
{
  if (!syncIOWrapper) {
    syncIOWrapper = make_shared<asynOctetSyncIOWrapper>();
  }
  new drvFGPDB(string(drvPortName), syncIOWrapper, string(udpPortName),
               startupDiagFlags_);

  return 0;
}


//-----------------------------------------------------------------------------
// EPICS iocsh shell commands
//-----------------------------------------------------------------------------

//=== Argment definitions: Description and type for each one ===
static const iocshArg config_Arg0 = { "drvPortName", iocshArgString };
static const iocshArg config_Arg1 = { "udpPortName", iocshArgString };
static const iocshArg config_Arg2 = { "startupDiag", iocshArgInt    };

//=== A list of the argument definitions ===
static const iocshArg * const config_Args[] = {
  &config_Arg0,
  &config_Arg1,
  &config_Arg2
};

//=== Func def struct:  Pointer to func, # args, arg list ===
static const iocshFuncDef config_FuncDef =
  { "drvFGPDB_Config", 3, config_Args };

//=== Func to call the func using elements from the generic argument list ===
static void config_CallFunc(const iocshArgBuf *args)
{
  drvFGPDB_Config(args[0].sval, args[1].sval, args[2].ival);
}


//-----------------------------------------------------------------------------
//  The function that registers the IOC shell functions
//-----------------------------------------------------------------------------
void drvFGPDB_Register(void)
{
  static bool firstTime = true;

  if (firstTime)
  {
    iocshRegister(&config_FuncDef, config_CallFunc);
    firstTime = false;
  }
}

epicsExportRegistrar(drvFGPDB_Register);

}  // extern "C"

//-----------------------------------------------------------------------------

