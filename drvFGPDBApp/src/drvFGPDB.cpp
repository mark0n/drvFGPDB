//----- drvFGPDB.cpp ----- 02/24/17 --- (01/24/17)----

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

#include <iostream>
#include <string>

#include <arpa/inet.h>

#include <iocsh.h>
#include <epicsExport.h>

#include "drvFGPDB.h"
#include "LCPProtocol.h"

using namespace std;

// Much more compact, easily read names for freq used types
typedef  epicsInt8      I8;
typedef  epicsInt16     I16;
typedef  epicsInt32     I32;

typedef  epicsUInt8     U8;
typedef  epicsUInt16    U16;
typedef  epicsUInt32    U32;

typedef  epicsFloat32   F32;
typedef  epicsFloat64   F64;

typedef  unsigned int   uint;
typedef  unsigned char  uchar;


static list<drvFGPDB *> drvList;

// list of driver values accessable via asyn interface
static const list<string> driverParamDefs = {
  "devName     0x1 Octet",
  "syncPktID   0x1 Int32",
  "asyncPktID  0x1 Int32",
  "ctlrAddr    0x1 Int32",
  "stateFlags  0x1 UInt32Digital",
  "diagFlags   0x2 UInt32Digital"
};


static void syncComLCP(void *drvPvt);

static bool  initComplete;

//-----------------------------------------------------------------------------
// Return the time as a millisecond counter that wraps around
//-----------------------------------------------------------------------------
ulong GetMS(void)
{
  struct timespec  timeData;
  clock_gettime(CLOCK_REALTIME, &timeData);
  return (timeData.tv_sec * 1000L) + (timeData.tv_nsec / 1000000L);
}

ulong MSSince(ulong msTime) { return GetMS() - msTime; }


//=============================================================================
drvFGPDB::drvFGPDB(const string &drvPortName,
                   shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
                   const string &udpPortName, int maxParams_) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, maxParams_, InterfaceMask,
                   InterruptMask, AsynFlags, AutoConnect, Priority, StackSize),
    syncIO(syncIOWrapper),
    maxParams(maxParams_),
    packetID(0),
    syncThreadInitialized(false),
    stopProcessing(false)
{
  initHookRegister(drvFGPDB_initHookFunc);

//  cout << "Adding drvPGPDB '" << portName << " to drvList[]" << endl;  //tdebug
  drvList.push_back(this);

  addDriverParams();

  // Create a pAsynUser and connect it to the asyn port that was created by
  // the startup script for communicating with the LCP controller
  auto stat = pasynOctetSyncIO->connect(udpPortName.c_str(), 0,
                                        &pAsynUserUDP, nullptr);

  if (stat) {
    cout << "  asyn driver for: " << drvPortName
         << " unable to connect to asyn UDP port: " << udpPortName
         << endl << endl;
    throw invalid_argument("Invalid asyn UDP port name");
  }

//  cout << "  asyn driver for: " << drvPortName
//       << " connected to asyn UDP port: " << udpPortName << endl;

  startThread(string("sync"), (EPICSTHREADFUNC)::syncComLCP);
}

//-----------------------------------------------------------------------------
drvFGPDB::~drvFGPDB()
{
  pasynOctetSyncIO->disconnect(pAsynUserUDP);
//  pasynManager->freeAsynUser(pAsynUserUDP);  // results in a segment fault...

  drvList.remove(this);
}

//-----------------------------------------------------------------------------
// Create a thread to manage ongoing communication with the controller
//-----------------------------------------------------------------------------
void drvFGPDB::startThread(const string &prefix, EPICSTHREADFUNC funcPtr)
{
  string threadName(prefix + portName);

  if ( epicsThreadCreate(threadName.c_str(),
                         epicsThreadPriorityMedium,
                         epicsThreadGetStackSize(epicsThreadStackMedium),
                         funcPtr, this) == NULL)  {
    cout << typeid(this).name() << "::" << __func__ << ": "
         << "epicsThreadCreate failure" << endl;
    throw runtime_error("Unable to start new thread");
  }

  ulong startTime = GetMS();
  while (!syncThreadInitialized)  {
    if (MSSince(startTime) > 10000)  {
      cout << typeid(this).name() << "::" << __func__ << ": "
           "Thread " << threadName << " failed to initialize" << endl;
      throw runtime_error("New thread did not initialize");
    }
    if (syncThreadInitialized)  break;
    epicsThreadSleep(0.010);
  }

}

//-----------------------------------------------------------------------------
static void syncComLCP(void *drvPvt)
{
  drvFGPDB *pdrv = (drvFGPDB *)drvPvt;

  pdrv->syncComLCP();
}

//-----------------------------------------------------------------------------
//  Task run in a separate thread to manage synchronous communication with a
//  controller that supports LCP.
//-----------------------------------------------------------------------------
void drvFGPDB::syncComLCP(void)
{
  syncThreadInitialized = true;


  cout << endl << portName << ": sync com thread started" <<endl;

  while (!stopProcessing)  {
    epicsThreadSleep(0.200);

    if (!initComplete)  continue;

    readRegs(0x10000, regGroup.at(ProcGroup_LCP_RO-1).maxOffset);
    readRegs(0x20000, regGroup.at(ProcGroup_LCP_WA-1).maxOffset);

    processPendingWrites();
  }
}


//-----------------------------------------------------------------------------
//  Send any pending write values for the specified group of LCP registers.
//  Returns the # of ack'd writes or < 0 if an error.
//-----------------------------------------------------------------------------
int drvFGPDB::processPendingWrites(void)
{
  //ToDo: Use a linked-list of params with pending writes

  int ackdCount = 0;
  for (auto &param : paramList)  {
    if (param.setState != SetState::Pending)  continue;

    if (writeRegs(param.regAddr, 1) != asynSuccess)  continue;

    param.setState = SetState::Sent;
    ++ackdCount;
  }

  return ackdCount;
}


//-----------------------------------------------------------------------------
// Add parameters for externally-accessable driver values
//-----------------------------------------------------------------------------
void drvFGPDB::addDriverParams(void)
{
  for (auto const &str : driverParamDefs) {
    if (paramList.size() >= (uint)maxParams)
      throw invalid_argument("# driver params > maxParams");
    ParamInfo param(str, portName);
    paramList.push_back(param);
  }
}

//-----------------------------------------------------------------------------
//  Search the driver's list of parameters for an entry with the given name.
//  Unlike asynPortDriver::findParam(), this func works during IOC startup.
//-----------------------------------------------------------------------------
int drvFGPDB::findParamByName(const string &name)
{
  auto it = find_if(paramList.begin(), paramList.end(), [&] (const ParamInfo& p)
                    { return p.name == name; } );
  return (it != paramList.end()) ? it - paramList.begin() : -1;
}

//-----------------------------------------------------------------------------
//  Return a copy of the ParamInfo struct for a parameter
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::getParamInfo(int paramID, ParamInfo &paramInfo)
{
  if ((uint)paramID >= paramList.size())  return asynError;

  paramInfo = paramList.at(paramID);  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Update the properties for an existing parameter.
//
//  Checks for conflicts and updates any missing property values using ones
//  from the new set of properties.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::updateParamDef(int paramID, const ParamInfo &newParam)
{
  if ((uint)paramID >= (uint)maxParams)  return asynError;  //msg

  ParamInfo  &curParam = paramList.at(paramID);

  if (curParam.name != newParam.name)  return asynError;

#define UpdateProp(prop, NotDef)  \
  if (curParam.prop == (NotDef))  \
    curParam.prop = newParam.prop;  \
  else  \
    if ((newParam.prop != (NotDef)) and (curParam.prop != newParam.prop))  \
      return asynError;  //msg

  UpdateProp(regAddr, 0);
  UpdateProp(asynType, asynParamNotDefined);
  UpdateProp(ctlrFmt, CtlrDataFmt::NotDefined);
//UpdateProp(syncMode, SyncMode::NotDefined);

  return asynSuccess;
}

//-----------------------------------------------------------------------------
// This func is called during IOC statup to to get the ID for a parameter for a
// given "port" (device) and "addr" (sub-device).  At least one of the calls
// for each parameter must include all the properties that define a parameter.
// For EPICS records, the strings come from the record's INP or OUT field
// (everything after the "@asyn(port, addr, timeout)" prefix).
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                   __attribute__((unused)) const char
                                   **pptypeName,
                                   __attribute__((unused)) size_t *psize)
{
  string paramCfgStr = string(drvInfo);
  ParamInfo  param(paramCfgStr, portName);

  if (param.name.empty())  return asynError;

  // If the parameter is not already in the list, then add it
  int  paramID = findParamByName(param.name);
  if (paramID < 0)  {
    if (paramList.size() >= (uint)maxParams)  return asynError;
    paramList.push_back(param);
    pasynUser->reason = paramList.size()-1;
    return asynSuccess;
  }

  pasynUser->reason = paramID;

  return updateParamDef(paramID, param);  // in case prev def was incomplete
}

//-----------------------------------------------------------------------------
// Create the asyn params for the driver
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::createAsynParams(void)
{
//cout << endl
//     << "create asyn params for: [" << portName << "]" << endl;

  int paramID;
  asynStatus stat;
  for (auto param = paramList.cbegin(); param != paramList.cend(); ++param) {
    if (param->asynType == asynParamNotDefined)  {
      cout << endl
           <<"  *** " << portName << ":" << param->name
           << " [" << (param - paramList.begin()) << "]: "
              "No asyn type specified in INP/OUT field ***" << endl;
      throw invalid_argument("Configuration error");
    }
    stat = createParam(param->name.c_str(), param->asynType, &paramID);
    if (stat != asynSuccess)  return stat;
//  cout << "  created " << param->name << " [" << paramID << "]" << endl; //tdebug
  }
  cout << endl;

  return asynSuccess;
}

//-----------------------------------------------------------------------------
// Returns a reference to a RegGroup object for the specified groupID.
//-----------------------------------------------------------------------------
RegGroup & drvFGPDB::getRegGroup(uint groupID)
{
  uint groupIdx = groupID - 1;
  if (groupIdx >= regGroup.size())
    throw out_of_range("Invalid LCP register group ID");

  return regGroup.at(groupIdx);
}

//-----------------------------------------------------------------------------
// Returns true if the range of LCP addresses is within the defined ones
//-----------------------------------------------------------------------------
bool drvFGPDB::inDefinedRegRange(uint firstReg, uint numRegs)
{
  if (!LCPUtil::validRegAddr(firstReg))  return false;

  uint groupID = LCPUtil::addrGroupID(firstReg);
  uint offset = LCPUtil::addrOffset(firstReg);
  const RegGroup &group = getRegGroup(groupID);

  return ((offset + numRegs - 1) <= group.maxOffset);
}

//-----------------------------------------------------------------------------
// Determine the range of addresses in each LCP register group
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::determineAddrRanges(void)
{
  asynStatus stat = asynSuccess;

  for (auto const& param : paramList) {

    auto addr = param.regAddr;
    uint groupID = LCPUtil::addrGroupID(addr);
    uint offset = LCPUtil::addrOffset(addr);

    if (LCPUtil::validRegAddr(addr))  {
      RegGroup &group = getRegGroup(groupID);
      if (offset > group.maxOffset)  group.maxOffset = offset;
    } else {
      if (groupID == 0)  continue;  // Driver parameter
      cout << "Invalid addr/group ID for parameter: " << param.name << endl;
      stat = asynError;
    }
  }

  return stat;
}

//-----------------------------------------------------------------------------
// Create the lists that map reg addrs to params
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::createAddrToParamMaps(void)
{
  asynStatus stat = asynSuccess;

  for(auto& group : regGroup) {
    if (group.maxOffset < 1)  continue;
    group.paramIDs = vector<int>(group.maxOffset + 1, -1);
  }


  for (auto param = paramList.cbegin(); param != paramList.cend(); ++param) {

    auto addr = param->regAddr;
    if (!LCPUtil::validRegAddr(addr))  continue;

    int paramID = param - paramList.begin();

    uint groupID = LCPUtil::addrGroupID(addr);
    uint offset = LCPUtil::addrOffset(addr);

    vector<int> &paramIDs = getRegGroup(groupID).paramIDs;

    if (paramIDs.at(offset) >= 0)  {
      cout << "Device: " << portName << ": "
           "*** Multiple params with same LCP reg addr ***" << endl
           << "  " << paramList.at(paramIDs.at(offset)).name
           << " and " << paramList.at(paramID).name << endl;
      stat = asynError;
    }

    paramIDs.at(offset) = paramID;
  }

  return stat;
}

//-----------------------------------------------------------------------------
// Callback function for EPICS IOC initialization steps.  Used to trigger
// startup/initialization processes in the correct order and at the correct
// times.
//-----------------------------------------------------------------------------
void drvFGPDB_initHookFunc(initHookState state)
{
  cout << endl << "initHookState: " << state << endl << endl;

  if (state >= initHookAfterIocRunning)  { initComplete = true;  return; }

  if (state != initHookAfterInitDatabase)  return;

  for(drvFGPDB *drv : drvList) {
    if (drv->createAsynParams() != asynSuccess)  continue;
    if (drv->determineAddrRanges() != asynSuccess)  continue;
    if (drv->createAddrToParamMaps() != asynSuccess)  continue;
  }

}

//----------------------------------------------------------------------------
//  Post a new read value for a parameter
//----------------------------------------------------------------------------
asynStatus drvFGPDB::postNewReadVal(uint paramID)
{
  ParamInfo &param = paramList.at(paramID);
  asynStatus stat = asynError;


  switch (param.asynType)  {

    case asynParamInt32:
      stat = setIntegerParam((int)paramID, param.ctlrValRead);
      break;

    case asynParamUInt32Digital:
      stat = setUIntDigitalParam((int)paramID, param.ctlrValRead, 0xFFFFFFFF);
      break;

    case asynParamFloat64:
      //finish - remember to convert from ctlr to asyn format
      break;

    default:
      break;
  }

  return stat;
}

//----------------------------------------------------------------------------
// Read the controller's current values for one or more LCP registers
//----------------------------------------------------------------------------
asynStatus drvFGPDB::readRegs(U32 firstReg, uint numRegs)
{
  int  eomReason;
<<<<<<< HEAD
  asynStatus stat, returnStat = asynSuccess;
  size_t  pktSize, sent, rcvd;
=======
  asynStatus stat;
  size_t  rcvd;
>>>>>>> e128c4b60b08c448adb2879e84b9275787eabd06
  char  *pBuf;
  char  respBuf[1024];

<<<<<<< HEAD
  //cout << endl << portName << ":readRegs()" << endl  //tdebug
  //     << "  firstReg: " << hex << firstReg << "  numRegs: " << numRegs << endl;  //tdebug

=======
>>>>>>> e128c4b60b08c448adb2879e84b9275787eabd06
  if (!inDefinedRegRange(firstReg, numRegs))  return asynError;

  //cout << "  range OK" << endl;  //tdebug

  size_t expectedRespSize = numRegs * 4 + 20;
  if (expectedRespSize > sizeof(respBuf))  {
    cout << portName << ": readRegs() respBuf[] too small" << endl;
    return asynError;
  }

  vector<uint32_t> cmdBuf;
  cmdBuf.reserve(5);
  cmdBuf.push_back(htonl(packetID));
  cmdBuf.push_back(htonl(static_cast<int32_t>(LCPCommand::READ_REGS)));
  cmdBuf.push_back(htonl(firstReg));
  cmdBuf.push_back(htonl(numRegs));
  cmdBuf.push_back(htonl(0));

  size_t bytesToSend = cmdBuf.size() * sizeof(cmdBuf[0]);
  size_t bytesSent;

  stat = pasynOctetSyncIO->write(pAsynUserUDP,
                                 reinterpret_cast<char *>(cmdBuf.data()),
                                 bytesToSend, 2.0, &bytesSent);
  if (stat != asynSuccess)  return stat;
  if (bytesSent != bytesToSend)  return asynError;

  ++packetID;

  rcvd = 0;
  pasynOctetSyncIO->read(pAsynUserUDP, respBuf, sizeof(respBuf), 2.0, &rcvd,
                         &eomReason);
  if (rcvd != expectedRespSize)  return asynError;

  //todo:  Check header values in returned packet
  pBuf = respBuf + 20;

  uint groupID = LCPUtil::addrGroupID(firstReg);
  uint offset = LCPUtil::addrOffset(firstReg);

  RegGroup &group = getRegGroup(groupID);

  bool chgsToBePosted = false;

  for (uint u=0; u<numRegs; ++u,++offset)  {
    uint paramID = group.paramIDs.at(offset);
    U32 justReadVal = ntohl(*(U32 *)pBuf);  pBuf += 4;

    if (paramID > paramList.size())  continue;

    ParamInfo &param = paramList.at(paramID);
    if ((justReadVal == param.ctlrValRead)
      and (param.readState == ReadState::Current))  continue;

    param.ctlrValRead = justReadVal;
    param.readState = ReadState::Current;
    auto stat = postNewReadVal(paramID);
    if (stat == asynSuccess)
      chgsToBePosted = true;
    else
      if (returnStat == asynSuccess)  returnStat = stat;
  }

  if (chgsToBePosted)  callParamCallbacks();

  return returnStat;
}


//----------------------------------------------------------------------------
// Send the driver's current value for one or more writeable LCP registers to
// the LCP controller
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeRegs(uint firstReg, uint numRegs)
{
  int  eomReason;
  asynStatus stat;
  size_t rcvd;
  char  respBuf[32];


  if (!inDefinedRegRange(firstReg, numRegs))  return asynError;
  if (LCPUtil::readOnlyAddr(firstReg))  return asynError;

  vector<uint32_t> cmdBuf;
  cmdBuf.reserve(4 + numRegs);
  cmdBuf.push_back(htonl(packetID));
  cmdBuf.push_back(htonl(static_cast<int32_t>(LCPCommand::WRITE_REGS)));
  cmdBuf.push_back(htonl(firstReg));
  cmdBuf.push_back(htonl(numRegs));

  uint groupID = LCPUtil::addrGroupID(firstReg);
  uint offset = LCPUtil::addrOffset(firstReg);

  RegGroup &group = getRegGroup(groupID);

  for (uint u=0; u<numRegs; ++u,++offset)  {
    uint paramID = group.paramIDs.at(offset);
    if (paramID > paramList.size())  return asynError;
<<<<<<< HEAD
    ParamInfo &param = paramList.at(paramID);
    *(U32 *)pBuf = htonl(param.ctlrValSet);  pBuf += 4;
=======
    ParamInfo param = paramList.at(paramID);
    cmdBuf.push_back(htonl(param.ctlrValSet));
>>>>>>> e128c4b60b08c448adb2879e84b9275787eabd06
  }

  size_t bytesToSend = cmdBuf.size() * sizeof(cmdBuf[0]);
  size_t bytesSent;

  stat = pasynOctetSyncIO->write(pAsynUserUDP,
                                 reinterpret_cast<char *>(cmdBuf.data()),
                                 bytesToSend, 2.0, &bytesSent);
  if (stat != asynSuccess)  return stat;
  if (bytesSent != bytesToSend)  return asynError;

  ++packetID;

  pasynOctetSyncIO->read(pAsynUserUDP, respBuf, sizeof(respBuf), 2.0, &rcvd,
                         &eomReason);
  if (rcvd != 20)  return asynError;

  //todo:
  //  - Check header values in returned packet
  //  - Update the setState of the params

  return asynSuccess;
}

//----------------------------------------------------------------------------
asynStatus drvFGPDB::getIntegerParam(int list, int index, int *value)
{  return asynDisconnected; }

asynStatus drvFGPDB::getDoubleParam(int list, int index, double * value)
{  return asynDisconnected; }

asynStatus drvFGPDB::getUIntDigitalParam(int list, int index,
                                         epicsUInt32 *value, epicsUInt32 mask)
{  return asynDisconnected; }


//----------------------------------------------------------------------------
// Process a request to write to one of the Int32 parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeInt32(asynUser *pasynUser, epicsInt32 newVal)
{
  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = paramList.at(paramID);


  do {
    if ((uint)paramID >= paramList.size())  {
      cout << __func__ << " for " << portName << ": "
           " *** paramID out of bounds ***" << endl;
      stat = asynError;  break;
    }

    if (param.asynType != asynParamInt32)  {
      cout << __func__ << " for " << portName << ":" << param.name << ": "
           " *** invalid parameter type ***" << endl;
      stat = asynError;  break;
    }

    if (param.readOnly)  {
      cout << __func__ << " for " << portName << ":" << param.name << ": "
           " *** param is read-only ***" << endl;
      stat = asynError;  break;
    }

    param.ctlrValSet = newVal;
    param.setState = SetState::Pending;

  //ToDo: Add param to a linked-list of params with pending writes

  } while (0);


  if (stat != asynSuccess)
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s::%s: status=%d, paramID=%d, name=%s, value=%d",
                  typeid(this).name(), __func__,
                  stat, paramID, param.name.c_str(), newVal);
  else
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s::%s():  paramID=%d, name=%s, value=%d\n",
              typeid(this).name(), __func__,
              paramID, param.name.c_str(), newVal);

  return stat;
}


//=============================================================================
// Configuration routine.  Called directly, or from the iocsh function below
//=============================================================================

extern "C" {

//-----------------------------------------------------------------------------
// EPICS iocsh callable func to call constructor for the drvFGPDB class.
//
//  \param[in] drvPortName The name of the asyn port driver to be created and
//             that this module extends.
//  \param[in] udpPortName The name of the asyn port for the UDP connection to
//             the device.
//  \maxParams[in] max # of asyn parameters that can be defined.
//-----------------------------------------------------------------------------
int drvFGPDB_Config(char *drvPortName, char *udpPortName, int maxParams)
{
  new drvFGPDB(string(drvPortName),
                   NULL, //new asynOctetSyncIOInterface,
                   string(udpPortName), maxParams);

  return 0;
}


//-----------------------------------------------------------------------------
// EPICS iocsh shell commands
//-----------------------------------------------------------------------------

//=== Argment definitions: Description and type for each one ===
static const iocshArg config_Arg0 = { "drvPortName", iocshArgString };
static const iocshArg config_Arg1 = { "udpPortName", iocshArgString };
static const iocshArg config_Arg2 = { "maxParams",   iocshArgInt    };

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

