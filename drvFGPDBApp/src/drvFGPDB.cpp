//----- drvFGPDB.cpp ----- 03/16/17 --- (01/24/17)----

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
#include <utility>

#include <arpa/inet.h>

#include <iocsh.h>
#include <epicsExport.h>

#include "drvFGPDB.h"
#include "LCPProtocol.h"

using namespace std;

// Much more compact, easily read names for freq used types
typedef  epicsInt8      S8;
typedef  epicsInt16     S16;
typedef  epicsInt32     S32;

typedef  epicsUInt8     U8;
typedef  epicsUInt16    U16;
typedef  epicsUInt32    U32;

typedef  epicsFloat32   F32;
typedef  epicsFloat64   F64;

typedef  unsigned int   uint;
typedef  unsigned char  uchar;


static list<drvFGPDB *> drvList;


static const double PhaseConvFactor32 = 83.8190317e-9; // ~ 360 / 2^32

static bool  initComplete;

//-----------------------------------------------------------------------------
// Return the time as a millisecond counter that wraps around
//-----------------------------------------------------------------------------
ulong getSystemClockInMS(void)
{
  using namespace std::chrono;
  auto duration = system_clock::now().time_since_epoch();
  return duration_cast<milliseconds>(duration).count();
}

//=============================================================================
drvFGPDB::drvFGPDB(const string &drvPortName,
                   shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
                   const string &udpPortName) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, 0, InterfaceMask,
                   InterruptMask, AsynFlags, AutoConnect, Priority, StackSize),
    syncIO(syncIOWrapper),
    packetID(0),
    stopProcessing(false),
    syncThread(&drvFGPDB::syncComLCP, this),
    writeAccess(false),
    idDevName(-1),
    idSyncPktID(-1),
    idSyncPktsRcvd(-1),
    idAsyncPktID(-1),
    idAsyncPktsRcvd(-1),
    idCtlrAddr(-1),
    idStateFlags(-1),
    idDiagFlags(-1),
    idSessionID(-1)
{
  initHookRegister(drvFGPDB_initHookFunc);

//  cout << "Adding drvPGPDB '" << portName << " to drvList[]" << endl;  //tdebug
  drvList.push_back(this);

  addRequiredParams();

  paramList.at(idSessionID).readOnly = true;


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
}

//-----------------------------------------------------------------------------
drvFGPDB::~drvFGPDB()
{
  stopProcessing = true;
  syncThread.join();

  pasynOctetSyncIO->disconnect(pAsynUserUDP);

  drvList.remove(this);
}

//-----------------------------------------------------------------------------
//  Task run in a separate thread to manage synchronous communication with a
//  controller that supports LCP.
//-----------------------------------------------------------------------------
void drvFGPDB::syncComLCP()
{
  while (!stopProcessing)  {
    if (!initComplete)  { this_thread::sleep_for(5ms);  continue; }

    auto start_time = chrono::system_clock::now();

    readRegs(0x10000, getRegGroup(ProcGroup_LCP_RO).maxOffset+1);
    readRegs(0x20000, getRegGroup(ProcGroup_LCP_WA).maxOffset+1);

    if (!writeAccess)  getWriteAccess();

    processPendingWrites();

    this_thread::sleep_until(start_time + 200ms);
  }
}


//-----------------------------------------------------------------------------
//  Attempt to gain write access to the ctlr (by setting the sessionID reg)
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::getWriteAccess(void)
{
  if (!validParamID(idSessionID))  return asynError;

  ParamInfo &sessionID = paramList.at(idSessionID);

  for (int attempt = 0; attempt <= 5; ++attempt)  {
    if (attempt)  this_thread::sleep_for(10ms);

    // valid sessionID values are > 0 and < 0xFFFF
    sessionID.ctlrValSet = getSystemClockInMS() & 0xFFFE;
    if (!sessionID.ctlrValSet)  sessionID.ctlrValSet = 1;
    sessionID.setState = SetState::Pending;

    if (writeRegs(sessionID.regAddr, 1) != asynSuccess)  continue;

    //temp: For now we just read the reg value back.  Eventually the
    //      sessionID value in the hdr of every resp pkt will be used to insure
    //      that we set the correct register value and the ctlr really is using
    //      the value we sent (and that we have write access)
    sessionID.readState = ReadState::Undefined;
    if (readRegs(sessionID.regAddr, 1) != asynSuccess)  continue;

    if ((sessionID.ctlrValRead == sessionID.ctlrValSet) and
        (sessionID.setState == SetState::Sent) and
        (sessionID.readState == ReadState::Current))  writeAccess = true;

    if (writeAccess)  {
      cout << "  === " << portName << " now has write access ===" << endl;
      return asynSuccess;
    }

  }

  cout << "  *** " << portName << " failed to get write access ***" << endl;

  return asynError;
}

//-----------------------------------------------------------------------------
//  Send any pending write values for the specified group of LCP registers.
//  Returns the # of ack'd writes or < 0 if an error.
//-----------------------------------------------------------------------------
int drvFGPDB::processPendingWrites(void)
{
  asynStatus  stat = asynError;

  //ToDo: Use a linked-list of params with pending writes

  if (!writeAccess)  return -1;

  int ackdCount = 0;
  for (auto &param : paramList)  {
    if (param.setState != SetState::Pending)  continue;

    for (int attempt = 0; attempt < 3; ++attempt)  {
      if ((stat = writeRegs(param.regAddr, 1)) == asynSuccess)  break;
      if (!writeAccess)  getWriteAccess();
    }
    if (stat != asynSuccess)  continue;

    param.setState = SetState::Sent;
    ++ackdCount;
  }

  return ackdCount;
}


//-----------------------------------------------------------------------------
// Add parameters for parameters required by the driver
//-----------------------------------------------------------------------------
void drvFGPDB::addRequiredParams(void)
{
  for (auto const &paramDef : requiredParamDefs) {
    ParamInfo param(paramDef.def, portName);
    paramList.push_back(param);
    *paramDef.id = paramList.size() - 1;
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
std::pair<asynStatus, ParamInfo> drvFGPDB::getParamInfo(int paramID)
{
  ParamInfo paramInfo;
  asynStatus status = asynError;
  if (validParamID(paramID)) {
    paramInfo = paramList.at(paramID);
    status = asynSuccess;
  }
  return make_pair(status, paramInfo);
}

//-----------------------------------------------------------------------------
void conflictingParamDefs(const string &portName,
                          const ParamInfo &curDef, const ParamInfo &newDef)
{
  cout << "*** " << portName << ":" << curDef.name << ": "
          "Conflicting parameter definitions ***" << endl
       << "  cur: " << curDef << endl
       << "  new: " << newDef << endl;
}

//-----------------------------------------------------------------------------
//  Update the properties for an existing parameter.
//
//  Checks for conflicts and updates any missing property values using ones
//  from the new set of properties.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::updateParamDef(int paramID, const ParamInfo &newParam)
{
  if ((uint)paramID >= paramList.size())  return asynError;  //msg

  ParamInfo &curParam = paramList.at(paramID);

  cout << endl  //tdebug
       << "  update: " << curParam << endl  //tdebug
       << "   using: " << newParam << endl;  //tdebug

  if (curParam.name != newParam.name)  return asynError;

#define UpdateProp(prop, NotDef)  \
  if (curParam.prop == (NotDef))  \
    curParam.prop = newParam.prop;  \
  else  \
    if ((newParam.prop != (NotDef)) and (curParam.prop != newParam.prop))  { \
      conflictingParamDefs(portName, curParam, newParam);  \
      return asynError; \
    }

  UpdateProp(regAddr,  0);
  UpdateProp(asynType, asynParamNotDefined);
  UpdateProp(ctlrFmt,  CtlrDataFmt::NotDefined);
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
    paramList.push_back(param);
    pasynUser->reason = paramList.size()-1;
    cout << endl << "    add: [" << paramCfgStr << "] " //tdebug
                    "[" << dec << pasynUser->reason << "]" << endl;  //tdebug
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
           << "  *** " << portName << " [" << *param << "]" << endl
           << "      No asyn type specified in INP/OUT field" << endl;
      throw invalid_argument("Configuration error");
    }
    stat = createParam(param->name.c_str(), param->asynType, &paramID);
    if (stat != asynSuccess)  return stat;
    cout << "  created " << param->name << " [" << dec << paramID << "]" << endl; //tdebug
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
// Returns true if the range of LCP addrs is within the defined ranges
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
// Create the lists that maps reg addrs to params
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
           << "  [" << paramList.at(paramIDs.at(offset)) << "]"
              " and [" << paramList.at(paramID) << "]" << endl;
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
  cout << endl << "initHookState: " << dec << state << endl
       << endl;

  switch(state) {
    case initHookAfterInitDatabase:
      for(drvFGPDB *drv : drvList) {
        if (drv->createAsynParams() != asynSuccess)  continue;
        if (drv->determineAddrRanges() != asynSuccess)  continue;
        if (drv->createAddrToParamMaps() != asynSuccess)  continue;
      }
      break;
    case initHookAfterIocRunning:
      initComplete = true;
      break;
    default:
      break;
  }
}

//----------------------------------------------------------------------------
//  Convert a value from the format used the controller to a float
//----------------------------------------------------------------------------
double cltrFmtToDouble(U32 ctlrVal, CtlrDataFmt ctlrFmt)
{
  float dval = 0.0;

  switch (ctlrFmt)  {
    case CtlrDataFmt::NotDefined:
      break;

    case CtlrDataFmt::S32:
      dval = (S32)ctlrVal;  break;

    case CtlrDataFmt::U32:
      dval = ctlrVal;  break;

    case CtlrDataFmt::F32:
      dval = *((F32 *)&ctlrVal);  break;

    case CtlrDataFmt::U16_16:
      dval = (float)((double)ctlrVal / 65536.0);  break;

    case CtlrDataFmt::PHASE:
      dval = (double)ctlrVal * PhaseConvFactor32;  break;
  }

  return dval;
}

//----------------------------------------------------------------------------
//  Convert a value from a floating point to the format used the controller
//----------------------------------------------------------------------------
uint32_t doubleToCtlrFmt(double dval, CtlrDataFmt ctlrFmt)
{
  uint32_t  ctlrVal = 0;
  F32  f32val = 0.0;

  switch (ctlrFmt)  {
    case CtlrDataFmt::NotDefined:  break;

    case CtlrDataFmt::S32:
      dval = (int32_t) ctlrVal;  break;

    case CtlrDataFmt::U32:
      dval = (uint32_t) ctlrVal;  break;

    case CtlrDataFmt::F32:
      f32val = dval;  ctlrVal = *((uint32_t *)&f32val);  break;

    case CtlrDataFmt::U16_16:
      ctlrVal = (uint32_t) (dval * 65536.0);  break;

    case CtlrDataFmt::PHASE:
      ctlrVal = (uint32_t) (dval / PhaseConvFactor32);  break;
  }

  return ctlrVal;
}

//----------------------------------------------------------------------------
//  Post a new read value for a parameter
//----------------------------------------------------------------------------
asynStatus drvFGPDB::postNewReadVal(uint paramID)
{
  ParamInfo &param = paramList.at(paramID);
  asynStatus stat = asynError;
  double dval;


  switch (param.asynType)  {

    case asynParamInt32:
      stat = setIntegerParam((int)paramID, param.ctlrValRead);
      break;

    case asynParamUInt32Digital:
      stat = setUIntDigitalParam((int)paramID, param.ctlrValRead, 0xFFFFFFFF);
      break;

    case asynParamFloat64:
      dval = cltrFmtToDouble(param.ctlrValRead, param.ctlrFmt);
      stat = setDoubleParam((int)paramID, dval);
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
  asynStatus stat;
  int  eomReason;
  char  *pBuf;
  char  respBuf[1024];


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


  asynStatus returnStat = asynSuccess;  size_t rcvd = 0;
  stat = pasynOctetSyncIO->read(pAsynUserUDP, respBuf, sizeof(respBuf), 2.0,
                                &rcvd, &eomReason);
  if (stat != asynSuccess)  return stat;
  if (rcvd != expectedRespSize)  return asynError;

  //todo:  Check header values in returned packet
  pBuf = respBuf + 20;

  uint groupID = LCPUtil::addrGroupID(firstReg);
  uint offset = LCPUtil::addrOffset(firstReg);

  RegGroup &group = getRegGroup(groupID);

  bool chgsToBePosted = false;

  for (uint u=0; u<numRegs; ++u,++offset)  {
    U32 justReadVal = ntohl(*(U32 *)pBuf);  pBuf += 4;

    int paramID = group.paramIDs.at(offset);
    if (!validParamID(paramID))  continue;

    ParamInfo &param = paramList.at(paramID);
    if ((justReadVal == param.ctlrValRead)
      and (param.readState == ReadState::Current))  continue;

    param.ctlrValRead = justReadVal;
    param.readState = ReadState::Current;
    stat = postNewReadVal(paramID);
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
  uint32_t  respSessionID = 0, respStatus = -999;
  ParamInfo &sessionID = paramList.at(idSessionID);


  if (!inDefinedRegRange(firstReg, numRegs))  return asynError;
  if (LCPUtil::readOnlyAddr(firstReg))  return asynError;


  ++packetID;

  // Construct cmd packet
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
    int paramID = group.paramIDs.at(offset);
    if (!validParamID(paramID))  return asynError;
    ParamInfo &param = paramList.at(paramID);
    cmdBuf.push_back(htonl(param.ctlrValSet));
    cout << "  === send value 0x" << hex << param.ctlrValSet   //tdebug
         << " for " << param.name << " ===" << dec << endl;  //tdebug
  }

  size_t bytesToSend = cmdBuf.size() * sizeof(cmdBuf[0]);
  size_t bytesSent;

  // Send the cmd pkt
  stat = pasynOctetSyncIO->write(pAsynUserUDP,
                                 reinterpret_cast<char *>(cmdBuf.data()),
                                 bytesToSend, 2.0, &bytesSent);
  if ((stat != asynSuccess) or (bytesSent != bytesToSend))  return asynError;


  // Read and process response packet
  vector<uint32_t> respBuf(5, 0);
  size_t expectedRespLen = respBuf.size() * sizeof(respBuf[0]);
  pasynOctetSyncIO->read(pAsynUserUDP,
                         reinterpret_cast<char *>(respBuf.data()),
                         expectedRespLen, 2.0, &rcvd, &eomReason);
  if (rcvd != expectedRespLen)  return asynError;

  uint32_t sessID_and_status = ntohl(respBuf[4]);
  respSessionID = (sessID_and_status >> 16) & 0xFFFF;
  respStatus = sessID_and_status & 0xFFFF;

  writeAccess = (respSessionID == sessionID.ctlrValSet);

  if (respStatus)  return asynError;

  //todo:
  //  - Check all header values in returned packet
  //  - Finish logic for updating the setState of the params
  //  - Return the status value from the response pkt (change the return type
  //    for the function to LCPStatus?  Or return the rcvd status in another
  //    argument)? Consider what the caller really needs/wants to know:  Just
  //    whether or not the send/rcv worked vs it worked but the controller did
  //    or didn't return an error (especially given that a resp status of
  //    SUCCESS does NOT necessarily meant that all the values written were
  //    accepted as-is)

  offset = LCPUtil::addrOffset(firstReg);

  for (uint u=0; u<numRegs; ++u,++offset)  {
    int paramID = group.paramIDs.at(offset);
    if (!validParamID(paramID))  continue;
    ParamInfo &param = paramList.at(paramID);
    param.setState = SetState::Sent;
  }

  return asynSuccess;
}

//----------------------------------------------------------------------------
asynStatus drvFGPDB::getIntegerParam(__attribute__((unused)) int list,
                                     __attribute__((unused)) int index,
                                     __attribute__((unused)) int *value)
{
  cout << "  " << __func__ << " list: " << dec << list << " index: " << index << endl;
  return asynDisconnected; }

asynStatus drvFGPDB::getDoubleParam(__attribute__((unused)) int list,
                                    __attribute__((unused)) int index,
                                    __attribute__((unused)) double * value)
{
  cout << "  " << __func__ << " list: " << dec << list << " index: " << index << endl;
  return asynDisconnected; }

asynStatus drvFGPDB::getUIntDigitalParam(__attribute__((unused)) int list,
                                         __attribute__((unused)) int index,
                                         __attribute__((unused)) epicsUInt32 *value,
                                         __attribute__((unused)) epicsUInt32 mask)
{
  cout << "  " << __func__ << " list: " << dec << list << " index: " << index << endl;
  return asynDisconnected; }


//----------------------------------------------------------------------------
// Verify that a parameter is writeable and of the specified type
//----------------------------------------------------------------------------
bool drvFGPDB::isWritableTypeOf(const string &caller,
                               int paramID, asynParamType asynType)
{
  if (!validParamID(paramID))  {
    cout << portName << ": " << caller << ": "
         " *** paramID [" << paramID << "] is out of bounds ***" << endl;
    return false;
  }

  ParamInfo &param = paramList.at(paramID);

  if (param.asynType != asynType)  {
    cout << portName << ":" <<caller << ": "<< param.name << ": "
         " *** invalid parameter type ***" << endl;
    return false;
  }

  if (param.readOnly)  {
    cout << __func__ << " for " << portName << ":" << param.name << ": "
           " *** param is read-only ***" << endl;
    return false;
  }

  return true;
}


//----------------------------------------------------------------------------
// Process a request to write to one of the Int32 parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeInt32(asynUser *pasynUser, epicsInt32 newVal)
{
  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = paramList.at(paramID);


  cout << endl << "  === write " << newVal << "(0x" << hex << newVal << ")"
       << " to " << param.name << dec << endl;

  do {
    if (!isWritableTypeOf(__func__, paramID, asynParamInt32))  {
      stat = asynError;  break; }

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


//----------------------------------------------------------------------------
// Process a request to write to one of the UInt32Digital parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::
  writeUInt32Digital(asynUser *pasynUser, epicsUInt32 newVal, epicsUInt32 mask)
{
  asynStatus  stat = asynSuccess;
  uint32_t  setVal;
  int  paramID = pasynUser->reason;
  ParamInfo &param = paramList.at(paramID);


  cout << endl << "  === write " << newVal << "(0x" << hex << newVal << ")"
       << " to " << param.name << dec << endl;

  do {
    if (!isWritableTypeOf(__func__, paramID, asynParamUInt32Digital))  {
      stat = asynError;  break; }

    // Compute result of applying specified changes
    setVal = param.ctlrValSet;   // start with cur value of all the bits
    setVal |= (newVal & mask);   // set bits set in value and the mask
    setVal &= (newVal | ~mask);  // clear bits clear in value but set in mask

    param.ctlrValSet = setVal;
    param.setState = SetState::Pending;

  //ToDo: Add param to a linked-list of params with pending writes

  } while (0);


  if (stat != asynSuccess)
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s::%s: status=%d, paramID=%d, name=%s, value=0x%08X",
                  typeid(this).name(), __func__,
                  stat, paramID, param.name.c_str(), newVal);
  else
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s::%s():  paramID=%d, name=%s, value=0x%08X\n",
              typeid(this).name(), __func__,
              paramID, param.name.c_str(), newVal);

  return stat;
}

//----------------------------------------------------------------------------
// Process a request to write to one of the Float64 parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeFloat64(asynUser *pasynUser, epicsFloat64 newVal)
{
  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = paramList.at(paramID);


  uint32_t ctlrVal = doubleToCtlrFmt(newVal, param.ctlrFmt);

  cout << endl << "  === write " << newVal << " (0x" << hex << ctlrVal << ")"
       << " to " << param.name << dec << endl;

  do {
    if (!isWritableTypeOf(__func__, paramID, asynParamFloat64))  {
      stat = asynError;  break; }

    param.ctlrValSet = ctlrVal;
    param.setState = SetState::Pending;

  //ToDo: Add param to a linked-list of params with pending writes

  } while (0);


  if (stat != asynSuccess)
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "%s::%s: status=%d, paramID=%d, name=%s, value=%e",
                  typeid(this).name(), __func__,
                  stat, paramID, param.name.c_str(), newVal);
  else
    asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
              "%s::%s():  paramID=%d, name=%s, value=%e\n",
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
//-----------------------------------------------------------------------------
int drvFGPDB_Config(char *drvPortName, char *udpPortName)
{
  new drvFGPDB(string(drvPortName),
                   NULL, //new asynOctetSyncIOInterface,
                   string(udpPortName));

  return 0;
}


//-----------------------------------------------------------------------------
// EPICS iocsh shell commands
//-----------------------------------------------------------------------------

//=== Argment definitions: Description and type for each one ===
static const iocshArg config_Arg0 = { "drvPortName", iocshArgString };
static const iocshArg config_Arg1 = { "udpPortName", iocshArgString };

//=== A list of the argument definitions ===
static const iocshArg * const config_Args[] = {
  &config_Arg0,
  &config_Arg1,
};

//=== Func def struct:  Pointer to func, # args, arg list ===
static const iocshFuncDef config_FuncDef =
  { "drvFGPDB_Config", 3, config_Args };

//=== Func to call the func using elements from the generic argument list ===
static void config_CallFunc(const iocshArgBuf *args)
{
  drvFGPDB_Config(args[0].sval, args[1].sval);
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

