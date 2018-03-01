//----- drvFGPDB_IOC.cpp ----- 04/06/17 --- (01/24/17)----

#include <memory>

#include <initHooks.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <epicsExit.h>

#include "drvFGPDB.h"
#include "asynOctetSyncIOWrapper.h"

using namespace std;

static shared_ptr<asynOctetSyncIOWrapper> syncIOWrapper;
static unique_ptr<list<drvFGPDB>> drvFGPDBs;

extern "C" {

//-----------------------------------------------------------------------------
// Callback function for EPICS IOC initialization steps.  Used to trigger
// normal processing by the driver.
//-----------------------------------------------------------------------------
void drvFGPDB_initHookFunc(initHookState state)
{
  if (state == initHookAfterInitDatabase) {
    if (drvFGPDBs) {
      for (auto& d : *drvFGPDBs) {
        d.startCommunication();
      }
    }
  }
}

//-----------------------------------------------------------------------------
// EPICS iocsh callable func to call constructor for the drvFGPDB class.
//
//  \param[in] drvPortName The name of the asyn port driver to be created and
//             that this module extends.
//  \param[in] udpPortName The name of the asyn port for the UDP connection to
//             the device.
//-----------------------------------------------------------------------------
int drvFGPDB_Config(char *drvPortName, char *udpPortName, int startupDiagFlags_)
{
  if (!syncIOWrapper) {
    syncIOWrapper = make_shared<asynOctetSyncIOWrapper>();
  }
  if (!drvFGPDBs) {
    drvFGPDBs = make_unique<list<drvFGPDB>>();
  }

  drvFGPDBs->emplace_back(string(drvPortName), syncIOWrapper,
                          string(udpPortName), startupDiagFlags_);

  return 0;
}

static void drvFGPDB_cleanUp(void *)
{
  drvFGPDBs.reset();
  syncIOWrapper.reset();
}

//-----------------------------------------------------------------------------
// EPICS iocsh shell commands
//-----------------------------------------------------------------------------

// IOC-shell command "drvFGPDB_Config"
static const iocshArg config_Arg0 { "drvPortName", iocshArgString };
static const iocshArg config_Arg1 { "udpPortName", iocshArgString };
static const iocshArg config_Arg2 { "startupDiag", iocshArgInt    };

static const iocshArg * const config_Args[] {
  &config_Arg0,
  &config_Arg1,
  &config_Arg2
};

static const iocshFuncDef config_FuncDef {
  "drvFGPDB_Config",
  sizeof(config_Args) / sizeof(iocshArg *),
  config_Args
};

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
    epicsAtExit(drvFGPDB_cleanUp, nullptr);
    initHookRegister(drvFGPDB_initHookFunc);
    iocshRegister(&config_FuncDef, config_CallFunc);
    firstTime = false;
  }
}

epicsExportRegistrar(drvFGPDB_Register);

}  // extern "C"

//-----------------------------------------------------------------------------

