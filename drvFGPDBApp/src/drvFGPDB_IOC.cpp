/**
 * @file  drvFGPDB_IOC.cpp
 * @brief Defines all EPICS IOC Shell functions needed by the FGPDB driver
 */

#include <memory>
#include <map>
#include <unordered_map>

#include <initHooks.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <epicsExit.h>

#include "drvFGPDB.h"
#include "asynOctetSyncIOWrapper.h"

using namespace std;

static shared_ptr<asynOctetSyncIOWrapper> syncIOWrapper;  //!< shared_ptr to the asynOctetSyncIOWrapper
static shared_ptr<logger> pLog {
  make_shared<timeDateDecorator>(
    make_shared<threadIDDecorator>(
      make_shared<epicsLogger>()
    )
  )
}; //!< shared_ptr to a logging class using the EPICS IOC log facilities. We want all log messages to include a timestamp and the thread ID.
static unique_ptr<map<string, drvFGPDB>> drvFGPDBs;       //!< unique_ptr to the map with all FGPDB driver instances created

extern "C" {

/**
 * @brief Callback function for EPICS IOC initialization steps.  Used to trigger
 *        normal processing by the driver.
 *
 * @param[in] state IOC current state
 */
void drvFGPDB_initHookFunc(initHookState state)
{
  if (state == initHookAfterInitDatabase) {
      if (drvFGPDBs) {
        for (auto& d : *drvFGPDBs) {
          d.second.completeArrayParamInit();
        }
      }
  }
  if (state == initHookAtIocRun) {
    if (drvFGPDBs) {
      for (auto& d : *drvFGPDBs) {
        d.second.startCommunication();
      }
    }
  }
}

/**
 * @brief     EPICS IOC Shell func to call the constructor for the drvFGPDB class.
 *
 * @param[in] drvPortName        The name of the asyn port driver to be created and
 *                               that this module extends.
 * @param[in] udpPortName        The name of the asyn port for the UDP connection to
 *                               the device.
 * @param[in] startupDiagFlags_  Debugging flag
 * @param[in] resendMode         Mode used handle controller and IOC restarts
 *
 * @return 0 @warning If any std::exception is catched, program will be terminated
 */
int drvFGPDB_Config(char *drvPortName, char *udpPortName, int startupDiagFlags_,
                    char *resendMode)
{
  if (!syncIOWrapper) {
    syncIOWrapper = make_shared<asynOctetSyncIOWrapper>();
  }
  if (!pLog) {
    pLog = make_shared<epicsLogger>();
  }
  if (!drvFGPDBs) {
    drvFGPDBs = make_unique<map<string, drvFGPDB>>();
  }
  if (!resendMode) {
    cerr << "ERROR: resend mode not specified for port \"" << drvPortName
         << "\"" << endl;
    exit(-1);
  }
  try{
    string portName = string(drvPortName);
    const std::unordered_map<std::string, ResendMode> resendModeMap {
      { "AfterCtlrRestart", ResendMode::AfterCtlrRestart },
      { "AfterIOCRestart",  ResendMode::AfterIOCRestart  },
      { "Never",            ResendMode::Never            }
    };
    drvFGPDBs->emplace(piecewise_construct, forward_as_tuple(portName),
                       forward_as_tuple(portName, syncIOWrapper,
                                        string(udpPortName), startupDiagFlags_,
                                        resendModeMap.at(resendMode), pLog));
  } catch(const std::out_of_range& e) {
    cerr << "ERROR: invalid resend mode \"" << resendMode << "\" for port \""
         << drvPortName << "\"" << endl;
    exit(-1);
  } catch(const std::exception& e){
          cerr << '\n' << "[ERROR] Port: " << drvPortName << " ; " << typeid(e).name()  << " ; " << e.what()<< '\n' << '\n';
          exit(-1);
  }

  return 0;
}
/**
 * @brief      EPICS IOC Shell func to change at runtime the debugging flags
 *             for a given Port
 *
 * @param[in]  drvPortName  The name of the asyn port driver
 * @param[in]  diagFlags_   Debugging flag to be enable
 */
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

/**
 * @brief EPICS IOC Shell func to retrieve the portNames of the different
 *        driver instances created.
 */
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

/**
 * @brief Function registered at epicsAtExit to safely destroy the driver instances
 *        managed by drvFGPDBs and all needed resources.
 */
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
static const iocshArg config_Arg3 { "resendMode",  iocshArgString };

static const iocshArg * const config_Args[] {
  &config_Arg0,
  &config_Arg1,
  &config_Arg2,
  &config_Arg3
};

static const iocshFuncDef config_FuncDef {
  "drvFGPDB_Config",
  sizeof(config_Args) / sizeof(iocshArg *),
  config_Args
};

static void config_CallFunc(const iocshArgBuf *args)
{
  drvFGPDB_Config(args[0].sval, args[1].sval, args[2].ival, args[3].sval);
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

// IOC-shell command "drvFGPDB_Report"
static const iocshFuncDef report_FuncDef {
  "drvFGPDB_Report",
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

/**
 * @brief The function that registers the EPIS IOC Shell functions
 */
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

