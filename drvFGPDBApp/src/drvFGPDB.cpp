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

//=============================================================================
drvFGPDB::drvFGPDB(const string &drvPortName,
                   shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
                   const string &udpPortName, int maxParams_) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, maxParams_, InterfaceMask,
                   InterruptMask, AsynFlags, AutoConnect, Priority, StackSize),
    syncIO(syncIOWrapper),
    maxParams(maxParams_),
    max_LCP_RO(0),
    max_LCP_WA(0),
    max_LCP_WO(0),
//  num_DRV_RO(0),
//  num_DRV_RW(0),
    packetID(0)
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
asynStatus drvFGPDB::findParamByName(const string &name, int *paramID)
{
  for (auto param = paramList.begin(); param != paramList.end(); ++param)
    if (param->name == name)  {
      *paramID = param - paramList.begin();  return (asynSuccess); }

  return asynError;
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
  int  paramID;
  if (findParamByName(param.name, &paramID) != asynSuccess)  {
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
// Determine the number of parameters in each processing-type group.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::determineGroupRanges(void)
{
  asynStatus stat = asynSuccess;

  max_LCP_RO = max_LCP_WA = max_LCP_WO = 0;  //num_DRV_RO = num_DRV_RW = 0;

  for (auto param = paramList.begin(); param != paramList.end(); ++param)  {

    auto addr = param->regAddr;

    switch (param->group)  {
      case ParamGroup::Invalid:
        cout << "Invalid addr/group ID for parameter: " << param->name << endl;
        stat = asynError;  break;

      case ParamGroup::LCP_RO:
        if (addr > max_LCP_RO)  max_LCP_RO = addr;
        break;

      case ParamGroup::LCP_WA:
        if (addr > max_LCP_WA)  max_LCP_WA = addr;
        break;

      case ParamGroup::LCP_WO:
        if (addr > max_LCP_WO)  max_LCP_WO = addr;
        break;

      default:
        break;
/*
      case ParamGroup::DRV_RO:
        ++num_DRV_RO;
        break;

      case ParamGroup::DRV_RW:
        ++num_DRV_RW;
        break;
*/
    }
  }

  return stat;
}

//-----------------------------------------------------------------------------
// Create the lists for each processing-type group
//-----------------------------------------------------------------------------
void drvFGPDB::createProcessingGroups(void)
{
  if (max_LCP_RO > 0)  LCP_RO_group = vector<int>(max_LCP_RO-0x10000u, -1);
  if (max_LCP_WA > 0)  LCP_WA_group = vector<int>(max_LCP_WA-0x20000u, -1);
  if (max_LCP_WO > 0)  LCP_WO_group = vector<int>(max_LCP_WO-0x30000u, -1);

//if (num_DRV_RO > 0)  DRV_RO_group = vector<int>(num_DRV_RO, -1);
//if (num_DRV_RW > 0)  DRV_RW_group = vector<int>(num_DRV_RW, -1);
}

//-----------------------------------------------------------------------------
//  Add parameter to specified processing group.  Checks for a conflict in the
//  LCP register number.
//-----------------------------------------------------------------------------
int drvFGPDB::addParamToGroup(std::vector<int> &groupList,
                              uint idx, int paramID)
{
  if (groupList.at(idx) >= 0)  {
    cout << "Device: " << portName << ": "
         "*** Multiple params with same LCP reg # ***" << endl
         << "  " << paramList.at(groupList.at(idx)).name
         << " and " << paramList.at(paramID).name << endl;
    return asynError;
  }

  groupList.at(idx) = paramID;

  return 0;
}

//-----------------------------------------------------------------------------
// Sort the params by which processing-type group they belong to.  A paramID
// value of -1 in the LCP_XXX_group lists indicates an LCP register that is not
// currently referenced by an EPICS record.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::sortParams(void)
{
  asynStatus stat = asynSuccess;

//uint  drvROidx = 0, drvRWidx = 0;

  for (auto param = paramList.begin(); param != paramList.end(); ++param)  {

    auto addr = param->regAddr;
    int id = param - paramList.begin();

    switch (param->group)  {
      case ParamGroup::Invalid:  stat = asynError;  break;

      case ParamGroup::LCP_RO:
        if (addParamToGroup(LCP_RO_group, addr-0x10001u, id)) stat = asynError;
       break;

      case ParamGroup::LCP_WA:
        if (addParamToGroup(LCP_WA_group, addr-0x20001u, id)) stat = asynError;
       break;

      case ParamGroup::LCP_WO:
        if (addParamToGroup(LCP_WO_group, addr-0x30001u, id)) stat = asynError;
        break;

      default:
        break;
/*
      case ParamGroup::DRV_RO:
        DRV_RO_group.at(drvROidx++) = id;  break;

      case ParamGroup::DRV_RW:
        DRV_RW_group.at(drvRWidx++) = id;  break;
*/
    }

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

    if (drv->createAsynParams() != asynSuccess)  break;
    if (drv->determineGroupRanges() != asynSuccess)  break;
    drv->createProcessingGroups();
    if (drv->sortParams() != asynSuccess)  break;
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

  rcvd = 0;
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


  stat = asynError;  //tdebug

  if (stat != asynSuccess)
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s::%s: status=%d, paramID=%d, name=%s, value=%d",
                  typeid(this).name(), __func__, stat, paramID, paramName,
                  newVal);
  else
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s::%s():  paramID=%d, name=%s, value=%d\n",
              typeid(this).name(), __func__, paramID, paramName, newVal);

  char buffer[256];
  strncpy(buffer, "test message", sizeof(buffer));
  const double writeTimeout = 2.0;
  size_t nwrite;
  writeData outData;
  outData.write_buffer = buffer;
  outData.write_buffer_len = strlen(buffer);
  asynStatus status = syncIO->write(pasynUser, outData, writeTimeout);
  return stat;
}

//-----------------------------------------------------------------------------
