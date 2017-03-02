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
  "syncPktID   0x1 Int32",
  "asyncPktID  0x1 Int32",
  "ctlrAddr    0x1 Int32",
  "stateFlags  0x1 UInt32Digital",
  "diagFlags   0x2 UInt32Digital"
};


static void syncComLCP(void *drvPvt);

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

//-----------------------------------------------------------------------------
static uint addrGroupID(uint addr)   { return ((addr >> 16) & 0xFFFF); }

static uint addrOffset(uint addr)  { return addr & 0xFFFF; }

static bool validRegAddr(uint addr)  {
  uint groupID = addrGroupID(addr);
  return (groupID > 0) and (groupID < 4);
}

// Indexes into regGroup[]
#define  LCP_RO  0  // Read-Only registers
#define  LCP_WA  1  // Write-Anytime registers
#define  LCP_WO  2  // Write-Once registers


//=============================================================================
drvFGPDB::drvFGPDB(const string &drvPortName,
                   shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
                   const string &udpPortName, int maxParams_) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, maxParams_, InterfaceMask,
                   InterruptMask, AsynFlags, AutoConnect, Priority, StackSize),
    syncIO(syncIOWrapper),
    maxParams(maxParams_),
    packetID(0),
    syncThreadInitialized(false)
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

  cout << "  asyn driver for: " << drvPortName
       << " connected to asyn UDP port: " << udpPortName << endl;

  startThread(string("sync"), (EPICSTHREADFUNC)::syncComLCP);
}

//-----------------------------------------------------------------------------
drvFGPDB::~drvFGPDB()
{
  pasynOctetSyncIO->disconnect(pAsynUserUDP);
//  pasynManager->freeAsynUser(pAsynUserUDP);  // results in a segment fault...

  for (auto it = drvList.begin(); it != drvList.end(); ++it)
    if ((*it) == this) { drvList.erase(it);  break; }
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


  for (;;)  {
    //readRegs(0, regGroup.at(LCP_RO).maxOffset);
    //readRegs(0, regGroup.at(LCP_WA).maxOffset);

    //processPendingWrites(LCP_WA);
    //processPendingWrites(LCP_WO);

    epicsThreadSleep(0.200);
  }
}


//-----------------------------------------------------------------------------
//  Send any pending write values for the specified group of LCP registers.
//  Returns the # of ack'd writes or < 0 if an error.
//-----------------------------------------------------------------------------
int drvFGPDB::processPendingWrites(int groupIdx)
{
  if ((groupIdx != LCP_WA) and (groupIdx != LCP_WO))  return -1;

  RegGroup &group = regGroup.at(groupIdx);
  vector<int> &idMap = group.paramIDs;

  int  paramID, ackdCount = 0;
  for (auto it = idMap.begin(); it != idMap.end(); ++it)  {
    if ((paramID = *it) < 0)  continue;

    ParamInfo &param = paramList.at(paramID);
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
  for (auto str=driverParamDefs.begin(); str!=driverParamDefs.end(); ++str) {
    if (paramList.size() >= (uint)maxParams)
      throw invalid_argument("# driver params > maxParams");
    ParamInfo param(*str, portName);
    paramList.push_back(param);
  }
}

//-----------------------------------------------------------------------------
//  Search the driver's list of parameters for an entry with the given name.
//  Unlike asynPortDriver::findParam(), this func works during IOC startup.
//-----------------------------------------------------------------------------
int drvFGPDB::findParamByName(const string &name)
{
  for (auto param = paramList.begin(); param != paramList.end(); ++param)
    if (param->name == name)  return param - paramList.begin();

  return -1;
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
    pasynUser->reason = paramList.size()-1;  return asynSuccess;
  }

  pasynUser->reason = paramID;

  // Update the existing entry (in case the old one was incomplete)
  return updateParamDef(paramID, param);
}

//-----------------------------------------------------------------------------
// Create the asyn params for the driver
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::createAsynParams(void)
{
  cout << endl
       << "create asyn params for: [" << portName << "]" << endl;  //tdebug

  int paramID;
  asynStatus stat;
  for (auto param = paramList.begin(); param != paramList.end(); ++param)  {
    stat = createParam(param->name.c_str(), param->asynType, &paramID);
    if (stat != asynSuccess)  return stat;
    cout << "  created '" << param->name << "' [" << paramID << "]" << endl;  //tdebug
  }
  cout << endl;  //tdebug

  return asynSuccess;
}

//-----------------------------------------------------------------------------
// Determine the range of addresses in each LCP register group
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::determineAddrRanges(void)
{
  asynStatus stat = asynSuccess;

  for (auto param = paramList.begin(); param != paramList.end(); ++param)  {

    auto addr = param->regAddr;
    uint groupID = addrGroupID(addr);  uint offset = addrOffset(addr);

    if (validRegAddr(addr))  {
      RegGroup &group = regGroup.at(groupID - 1);
      if (offset > group.maxOffset)  group.maxOffset = offset;
    } else {
      if (groupID == 0)  continue;  // Driver parameter
      cout << "Invalid addr/group ID for parameter: " << param->name << endl;
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

  for (auto group = regGroup.begin(); group != regGroup.end(); ++group)  {
    if (group->maxOffset < 1)  continue;
    group->paramIDs = vector<int>(group->maxOffset+1, -1);
  }


  for (auto param = paramList.begin(); param != paramList.end(); ++param)  {

    auto addr = param->regAddr;
    if (!validRegAddr(addr))  continue;

    int paramID = param - paramList.begin();

    uint groupID = addrGroupID(addr);  uint offset = addrOffset(addr);

    vector<int> &paramIDs = regGroup[groupID-1].paramIDs;

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
  if (state != initHookAfterInitDatabase)  return;

  for (auto it = drvList.begin(); it != drvList.end(); ++it)  {
    drvFGPDB *drv = *it;

    if (drv->createAsynParams() != asynSuccess)  continue;
    if (drv->determineAddrRanges() != asynSuccess)  continue;
    if (drv->createAddrToParamMaps() != asynSuccess)  continue;
  }
}

//----------------------------------------------------------------------------
// Read the controller's current values for one or more LCP registers
//----------------------------------------------------------------------------
asynStatus drvFGPDB::readRegs(U32 firstReg, uint numRegs)
{
  int  eomReason;
  asynStatus stat;
  size_t  pktSize, sent, rcvd;
  char  *pBuf;
  char  cmdBuf[32];
  char  respBuf[1024];


  size_t expectedRespSize = numRegs * 4 + 20;

  if (expectedRespSize > sizeof(respBuf))  return asynError;

  pBuf = cmdBuf;

  *(U32 *)pBuf = htonl(packetID);   pBuf += 4;
  *(U32 *)pBuf = htonl(static_cast<int32_t>(LCPCommand::READ_REGS));  pBuf += 4;
  *(U32 *)pBuf = htonl(firstReg);   pBuf += 4;
  *(U32 *)pBuf = htonl(numRegs);    pBuf += 4;
  *(U32 *)pBuf = htonl(0);          pBuf += 4;

  pktSize = pBuf - cmdBuf;

  stat = pasynOctetSyncIO->write(pAsynUserUDP, cmdBuf, pktSize, 2.0, &sent);
  if (stat != asynSuccess)  return stat;
  if (sent != pktSize)  return asynError;

  ++packetID;

  rcvd = 0;
  pasynOctetSyncIO->read(pAsynUserUDP, (char *)respBuf, sizeof(respBuf),
                         2.0, &rcvd, &eomReason);
  if (rcvd != expectedRespSize)  return asynError;

  return asynSuccess;
}

//----------------------------------------------------------------------------
// Send the driver's current value for one or more writeable LCP registers to
// the LCP controller
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeRegs(uint firstReg, uint numRegs)
{
  int  eomReason;
  asynStatus stat;
  size_t  pktSize, sent, rcvd;
  char  *pBuf;
  char  cmdBuf[1024];
  char  respBuf[32];


  uint maxNumRegs = sizeof(cmdBuf) / 4 - 4;

  if (numRegs > maxNumRegs)  return asynError;

  pBuf = cmdBuf;

  *(U32 *)pBuf = htonl(packetID);    pBuf += 4;
  *(U32 *)pBuf = htonl(static_cast<int32_t>(LCPCommand::WRITE_REGS));  pBuf += 4;
  *(U32 *)pBuf = htonl(firstReg);    pBuf += 4;
  *(U32 *)pBuf = htonl(numRegs);     pBuf += 4;

  for (uint u=0; u<numRegs; ++u)  {
    *(U32 *)pBuf = htonl(u);  pBuf += 4; }  //test-only

  pktSize = pBuf - cmdBuf;


  stat = pasynOctetSyncIO->write(pAsynUserUDP, cmdBuf, pktSize, 2.0, &sent);
  if (stat != asynSuccess)  return stat;
  if (sent != pktSize)  return asynError;

  ++packetID;

  pasynOctetSyncIO->read(pAsynUserUDP, (char *)respBuf, sizeof(respBuf),
                         2.0, &rcvd, &eomReason);
  if (rcvd != 20)  return asynError;

  return asynSuccess;
}

//----------------------------------------------------------------------------
// Process a request to write to one of the Int32 parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeInt32(asynUser *pasynUser, epicsInt32 newVal)
{
  asynStatus  stat = asynSuccess;
  const char  *paramName;
  int  paramID = pasynUser->reason;


  getParamName(paramID, &paramName);

  do {
    if ((uint)paramID >= paramList.size())  {
      cout << "*** paramID out of bounds ***" << endl;
      stat = asynError;  break;
    }

    ParamInfo &param = paramList.at(paramID);
    if (param.asynType != asynParamInt32)  {
      cout << "*** invalid parameter type ***" << endl;
      stat = asynError;  break;
    }

    param.ctlrValSet = newVal;
    param.setState = SetState::Pending;
  } while (0);


  if (stat != asynSuccess)
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s::%s: status=%d, paramID=%d, name=%s, value=%d",
                  typeid(this).name(), __func__,
                  stat, paramID, paramName, newVal);
  else
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s::%s():  paramID=%d, name=%s, value=%d\n",
              typeid(this).name(), __func__, paramID, paramName, newVal);

  return stat;
}

//-----------------------------------------------------------------------------
