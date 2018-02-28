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
static unique_ptr<map<string, drvFGPDB>> drvFGPDBs;

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
        d.second.startCommunication();
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
    drvFGPDBs = make_unique<map<string, drvFGPDB>>();
  }

  string portName = string(drvPortName);
  drvFGPDBs->emplace(piecewise_construct, forward_as_tuple(portName),
                     forward_as_tuple(portName, syncIOWrapper,
                                      string(udpPortName), startupDiagFlags_));

  return 0;
}

void drvFGPDB_setDiagFlags(char *drvPortName, int diagFlags_)
{
  string portName = string(drvPortName);

  if (!drvFGPDBs) {
    throw runtime_error("List of drvFGPDB objects doesn't exist! You need to "
                        "create at least one driver object before calling this "
                        "function.");
  }

  auto it = drvFGPDBs->find(portName);
  if(it == drvFGPDBs->end()) {
    throw invalid_argument("Can't find drvFGPDB object for port \"" + portName +
                           "\"");
  } else {
    it->second.setDiagFlags(diagFlags_);
  }
}

void drvFGPDB_Report()
{
  if (!drvFGPDBs) {
    throw runtime_error("List of drvFGPDB objects doesn't exist! You need to "
                        "create at least one driver object before calling this "
                        "function.");
  }

  for(auto const& x : *drvFGPDBs) {
    cout << x.first << endl;
  }
}

static void drvFGPDB_cleanUp(void *)
{
  drvFGPDBs.reset();
  syncIOWrapper.reset();
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

// IOC-shell command "drvFGPDB_SetDiagFlags"
static const iocshArg setDiagFlags_Arg0 { "drvPortName", iocshArgString };
static const iocshArg setDiagFlags_Arg1 { "diagFlags",   iocshArgInt    };

static const iocshArg * const setDiagFlags_Args[] {
  &setDiagFlags_Arg0,
  &setDiagFlags_Arg1
};

static const iocshFuncDef setDiagFlags_FuncDef {
  "drvFGPDB_SetDiagFlags",
  sizeof(setDiagFlags_Args) / sizeof(iocshArg *),
  setDiagFlags_Args
};

static void setDiagFlags_CallFunc(const iocshArgBuf *args)
{
  if (args[0].sval == nullptr) {
    cout << setDiagFlags_FuncDef.name << ": ERROR: Parameter "
         << setDiagFlags_Arg0.name << " not specified!" << endl;
    return;
  }
  try {
    drvFGPDB_setDiagFlags(args[0].sval, args[1].ival);
  } catch(exception& e) {
    cout << setDiagFlags_FuncDef.name << ": ERROR: " << e.what() << endl;
  }
}

// IOC-shell command "drvFGPDBReport"
static const iocshFuncDef report_FuncDef {
  "drvFGPDBReport",
  0,
  nullptr
};

static void report_CallFunc(__attribute__((unused)) const iocshArgBuf *args)
{
  try {
    drvFGPDB_Report();
  } catch(exception& e) {
    cout << report_FuncDef.name << ": ERROR: " << e.what() << endl;
  }
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
    iocshRegister(&setDiagFlags_FuncDef, setDiagFlags_CallFunc);
    iocshRegister(&report_FuncDef, report_CallFunc);
    firstTime = false;
  }
}

epicsExportRegistrar(drvFGPDB_Register);

}  // extern "C"

//-----------------------------------------------------------------------------

