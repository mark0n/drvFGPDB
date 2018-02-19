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
#include <iomanip>
#include <mutex>

#include <arpa/inet.h>

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


static const double writeTimeout = 1.0;
static const double readTimeout  = 1.0;

drvFGPDB::drvFGPDB(const string &drvPortName,
                   shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
                   const string &udpPortName, uint32_t startupDiagFlags) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, InterfaceMask, InterruptMask,
                   AsynFlags, AutoConnect, Priority, StackSize),
    syncIO(syncIOWrapper),
    initComplete(false),
    exitDriver(false),
    syncThread(&drvFGPDB::syncComLCP, this),
    writeAccess(false),
    unfinishedArrayRWs(false),
    updateRegs(true),
    connected(false),
    lastRespTime(0s),
    idUpSecs(-1),
    upSecs(0),
    prevUpSecs(0),
    idSessionID(-1),
    sessionID(0),
    idDevName(-1),
    idSyncPktID(-1),
    syncPktID(0),
    idSyncPktsSent(-1),
    syncPktsSent(0),
    idSyncPktsRcvd(-1),
    syncPktsRcvd(0),
    idAsyncPktID(-1),
    asyncPktID(0),
    idAsyncPktsSent(-1),
    asyncPktsSent(0),
    idAsyncPktsRcvd(-1),
    asyncPktsRcvd(0),
    idCtlrAddr(-1),
    ctlrAddr(0),
    idStateFlags(-1),
    stateFlags(0),
    idDiagFlags(-1),
    diagFlags(startupDiagFlags)
{
  addRequiredParams();

  params.at(idUpSecs).readOnly = true;
  params.at(idSessionID).readOnly = true;


  // Create a pAsynUser and connect it to the asyn port that was created by
  // the startup script for communicating with the LCP controller
  auto stat = syncIO->connect(udpPortName.c_str(), 0, &pAsynUserUDP, nullptr);

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
  exitDriver = true;
  syncThread.join();

  syncIO->disconnect(pAsynUserUDP);
}

void drvFGPDB::startCommunication()
{
  initComplete = true;
}

//-----------------------------------------------------------------------------
//  Task run in a separate thread to manage synchronous communication with a
//  controller that supports LCP.
//-----------------------------------------------------------------------------
void drvFGPDB::syncComLCP()
{
  while (!initComplete and !exitDriver)  this_thread::sleep_for(5ms);

  auto lastRegUpdateTime = chrono::system_clock::now();

  while (!exitDriver)  {
    auto start_time = chrono::system_clock::now();
    unfinishedArrayRWs = false;

    if (!TestMode())  {
      updateRegs = (start_time - lastRegUpdateTime >= 200ms);
      if (updateRegs)  lastRegUpdateTime = chrono::system_clock::now();
      updateReadValues();  postNewReadValues();
      processPendingWrites();
    }

    checkComStatus();

    if (!exitDriver)  {
      auto sleepTime = (unfinishedArrayRWs ? 20ms : 200ms);
      this_thread::sleep_until(start_time + sleepTime);
    }
  }
}

//-----------------------------------------------------------------------------
void drvFGPDB::checkComStatus(void)
{
  bool  ctlrRestarted = false, ctlrDisconnected = false;

  if (connected)  {
    if (chrono::system_clock::now() - lastRespTime >= 5s)  {
      cout << endl << "*** " << portName << " ctlr offline ***" << endl << endl;
      ctlrDisconnected = true;  connected = false;
    }
    else  if (upSecs < prevUpSecs)  {
      cout << endl << "*** " << portName << " ctlr rebooted ***" << endl << endl;
      ctlrRestarted = true;
    }
    prevUpSecs = upSecs;
  }


  if (ctlrRestarted or ctlrDisconnected)  resetParamStates();
}

//-----------------------------------------------------------------------------
void drvFGPDB::resetParamStates(void)
{
  lock_guard<drvFGPDB> lock(*this);

  for (int paramID=0; (uint)paramID<params.size(); ++paramID)  {
    ParamInfo &param = params.at(paramID);

    if (param.isScalarParam())
      param.readState = ReadState::Undefined;

    else  if (param.isArrayParam())  {
      param.initBlockRW(param.arrayValRead.size());
      param.readState = ReadState::Update;
    }

    param.setState = SetState::Undefined;

    setParamStatus(paramID, asynDisconnected);

    // required to get status change to process for array param
    if (param.asynType == asynParamInt8Array)
       doCallbacksInt8Array((epicsInt8 *)"", 0, paramID, 0);
  }

  callParamCallbacks();
}

//-----------------------------------------------------------------------------
//  Attempt to gain write access to the ctlr (by setting the sessionID reg)
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::getWriteAccess(void)
{
  if (exitDriver)  return asynError;

  if (!validParamID(idSessionID))  return asynError;

  ParamInfo &param = params.at(idSessionID);

  for (int attempt = 0; attempt <= 5; ++attempt)  {
    if (attempt)  this_thread::sleep_for(10ms);

    // valid sessionID values are > 0 and < 0xFFFF
    param.ctlrValSet = sessionID = rand() %  0xFFFD + 1;
    param.setState = SetState::Pending;

    if (writeRegs(param.regAddr, 1) != asynSuccess)  continue;

    if (!writeAccess)  continue;

    if (ShowRegWrites())
      cout << "  === " << portName << " now has write access ===" << endl;

    return asynSuccess;
  }

  cout << "  *** " << portName << " failed to get write access ***" << endl;

  return asynError;
}

//-----------------------------------------------------------------------------
int drvFGPDB::processPendingWrites(void)
{
  if (exitDriver)  return -1;

  //ToDo: Use a list of params with pending writes to improve efficiency

  int ackdCount = 0;

  for (auto &param : params)  {
    SetState setState;
    {
      lock_guard<drvFGPDB> lock(*this);
      setState = param.setState;
    }

    // new scalar value to be sent to the controller
    if (LCPUtil::isLCPRegParam(param.regAddr))  {
      if (setState != SetState::Pending)  continue;
      if (!updateRegs)  continue;
      if (!writeAccess)  { getWriteAccess();  if (!writeAccess)  return -1; }
      if (writeRegs(param.regAddr, 1) == asynSuccess)  ++ackdCount;
    }

    // new setting for driver-only scalar value
    else  if (param.isScalarParam())  {
      if (setState != SetState::Pending)  continue;
      lock_guard<drvFGPDB> lock(*this);
      if (param.drvValue)  *param.drvValue = param.ctlrValSet;
      param.setState = SetState::Sent;
      ++ackdCount;
    }

    // start or continue processing an array value
    else  if (param.isArrayParam())  {
      if ((setState != SetState::Pending) and
          (setState != SetState::Processing))  continue;
      if (!connected)  continue;
      if (writeNextBlock(param) != asynSuccess)  {
        cout << endl << "*** " << portName << ":" << param.name << ": "
          "unable to write new array value ***" << endl << endl;
        lock_guard<drvFGPDB> lock(*this);
        param.setState = SetState::Error;
        setParamStatus(ParamID(param), asynError);
        // always re-read after a write (especially after a failed one!)
        param.initBlockRW(param.arrayValRead.size());
        param.readState = ReadState::Update;
      }
    }
  }

  return ackdCount;
}


//-----------------------------------------------------------------------------
asynStatus drvFGPDB::addRequiredParams(void)
{
  asynStatus  stat = asynSuccess;

  for (auto const &paramDef : requiredParamDefs)  {
    int paramID = processParamDef(paramDef.def);
    if (paramID < 0)  {
      stat = asynError;  continue; }
    if (paramDef.id)  *paramDef.id = paramID;
    if (paramDef.drvVal)  {
      ParamInfo &param = params.at(paramID);
      param.drvValue = (uint32_t *)paramDef.drvVal;
    }
  }

  return stat;
}

//-----------------------------------------------------------------------------
//  Search the driver's list of parameters for an entry with the given name.
//  Unlike asynPortDriver::findParam(), this func works during IOC startup.
//  Returns the paramID (index in to list) or < 0 if param not in the list.
//-----------------------------------------------------------------------------
int drvFGPDB::findParamByName(const string &name)
{
  auto it = find_if(params.begin(), params.end(), [&] (const ParamInfo& p)
                    { return p.name == name; } );
  return (it != params.end()) ? it - params.begin() : -1;
}

//-----------------------------------------------------------------------------
//  Return a copy of the ParamInfo struct for a parameter
//-----------------------------------------------------------------------------
std::pair<asynStatus, ParamInfo> drvFGPDB::getParamInfo(int paramID)
{
  ParamInfo paramInfo;
  asynStatus status = asynError;
  if (validParamID(paramID)) {
    lock_guard<drvFGPDB> lock(*this);
    paramInfo = params.at(paramID);
    status = asynSuccess;
  }
  return make_pair(status, paramInfo);
}

//-----------------------------------------------------------------------------
//  Add new parameter to the driver and asyn layer lists
//-----------------------------------------------------------------------------
int drvFGPDB::addNewParam(const ParamInfo &newParam)
{
  asynStatus  stat;
  int paramID;

  if (newParam.asynType == asynParamNotDefined)  {
    cout << endl
         << "  *** " << portName << " [" << newParam << "]" << endl
         << "      No asyn type specified" << endl;
    return -1;
  }
  stat = createParam(newParam.name.c_str(), newParam.asynType, &paramID);
  if (stat != asynSuccess)  return -1;

  setParamStatus(paramID, asynDisconnected);

  if (ShowInit())
    cout << "  created " << portName << ":"
         << newParam.name << " [" << dec << paramID << "]" << endl;

  params.push_back(newParam);

  if ((uint)paramID != params.size() - 1)  {
    cout << endl << "*** " << portName << ":" << newParam.name << ": "
            "asyn paramID != driver paramID ***" << endl << endl;
    throw runtime_error("mismatching paramIDs");
  }

  if (newParam.regAddr) if (updateRegMap(paramID) != asynSuccess)  return -1;

  return paramID;
}

//-----------------------------------------------------------------------------
//  Process a param def and add it to the driver and asyn layer lists or update
//  its properties.
//-----------------------------------------------------------------------------
int drvFGPDB::processParamDef(const string &paramDef)
{
  asynStatus  stat;
  int  paramID;

  ParamInfo newParam(paramDef, portName);
  if (newParam.name.empty())  return -1;

  if ((paramID = findParamByName(newParam.name)) < 0)
    return addNewParam(newParam);

  ParamInfo &curParam = params.at(paramID);
  if (ShowInit())
    cout << endl
         << "  update: " << curParam << endl
         << "   using: " << newParam << endl << endl;
  stat = curParam.updateParamDef(portName, newParam);
  if (stat != asynSuccess)  return -1;

  if (newParam.regAddr)
    if ((stat = updateRegMap(paramID)) != asynSuccess)  return -1;

  return paramID;
}

//-----------------------------------------------------------------------------
// This func is called during IOC statup to get the ID for a parameter for a
// given "port" (device) and "addr" (sub-device).  The first call for any
// parameter must include the name and asynParamType, and at least one of the
// calls for each parameter must include all the properties that define a
// parameter. For EPICS records, the strings come from the record's INP or OUT
// field (everything after the "@asyn(port, addr, timeout)" prefix).
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                               __attribute__((unused)) const char **pptypeName,
                               __attribute__((unused)) size_t *psize)
{
  pasynUser->reason = processParamDef(string(drvInfo));
  return (pasynUser->reason < 0) ? asynError : asynSuccess;
}

//-----------------------------------------------------------------------------
// Returns a reference to a ProcGroup object for the specified groupID.
//-----------------------------------------------------------------------------
ProcGroup & drvFGPDB::getProcGroup(uint groupID)
{
  if (groupID >= procGroup.size())
    throw out_of_range("Invalid LCP register group ID");

  return procGroup.at(groupID);
}

//-----------------------------------------------------------------------------
// Returns true if the range of LCP addrs is within the defined ranges
//-----------------------------------------------------------------------------
bool drvFGPDB::inDefinedRegRange(uint firstReg, uint numRegs)
{
  if (!LCPUtil::isLCPRegParam(firstReg))  return false;

  uint groupID = LCPUtil::addrGroupID(firstReg);
  uint offset = LCPUtil::addrOffset(firstReg);
  const ProcGroup &group = getProcGroup(groupID);

  return ((offset + numRegs) <= group.paramIDs.size());
}

//-----------------------------------------------------------------------------
// Update the regAddr to paramID maps based on the specified param def.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::updateRegMap(int paramID)
{
  if (!validParamID(paramID))  return asynError;

  ParamInfo &param = params.at(paramID);

  auto addr = param.regAddr;
  uint groupID = LCPUtil::addrGroupID(addr);
  uint offset = LCPUtil::addrOffset(addr);

  if (LCPUtil::isLCPRegParam(addr))  { // ref to LCP register value
    vector<int> &paramIDs = getProcGroup(groupID).paramIDs;

    if (offset >= paramIDs.size())  paramIDs.resize(offset+1, -1);

    if (paramIDs.at(offset) < 0) {  // not set yet
      paramIDs.at(offset) = paramID;  return asynSuccess; }

    if (paramIDs.at(offset) == paramID)  return asynSuccess;

    cout << "Device: " << portName << ": "
         "*** Multiple params with same LCP reg addr ***" << endl
         << "  [" << params.at(paramIDs.at(offset)) << "]"
            " and [" << params.at(paramID) << "]" << endl;
    return asynError;
  }

  if (groupID == ProcGroup_Driver)  { // ref to driver-only value
    vector<int> &paramIDs = getProcGroup(groupID).paramIDs;
    paramIDs.push_back(paramID);
    return asynSuccess;
  }

  cout << "Invalid addr/group ID for parameter: " << param.name << endl;

  return asynError;
}

//-----------------------------------------------------------------------------
asynStatus drvFGPDB::sendMsg(asynUser *pComPort, vector<uint32_t> &cmdBuf)
{
  size_t bytesToSend = cmdBuf.size() * sizeof(cmdBuf[0]);
  size_t bytesSent;

  writeData outData {
    .write_buffer = reinterpret_cast<char *>(cmdBuf.data()),
    .write_buffer_len = bytesToSend,
    .nbytesOut = &bytesSent
  };
  asynStatus stat = syncIO->write(pComPort, outData, writeTimeout);
  ++syncPktsSent;
  if (stat != asynSuccess)  return stat;
  if (bytesSent != bytesToSend)  return asynError;

  return asynSuccess;
}

//----------------------------------------------------------------------------
asynStatus drvFGPDB::readResp(asynUser *pComPort, vector<uint32_t> &respBuf,
                              size_t expectedRespSize)
{
  asynStatus stat;
  int  eomReason;

  size_t rcvd = 0;
  readData inData {
    .read_buffer = reinterpret_cast<char *>(respBuf.data()),
    .read_buffer_len = respBuf.size() * sizeof(respBuf[0]),
    .nbytesIn = &rcvd
  };
  stat = syncIO->read(pComPort, inData, readTimeout, &eomReason);
  if (stat != asynSuccess)  return stat;
  ++syncPktsRcvd;
  if (rcvd != expectedRespSize)  return asynError;

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  For use by synchronous (1 resp for each cmd) thread only!
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::sendCmdGetResp(asynUser *pComPort,
                                    vector<uint32_t> &cmdBuf,
                                    vector<uint32_t> &respBuf,
                                    LCPStatus &respStatus)
{
  asynStatus  stat;


  ++syncPktID;  cmdBuf[0] = htonl(syncPktID);  respStatus = LCPStatus::ERROR;

  const int MaxMsgAttempts = 5;
  for (int attempt=0; attempt<MaxMsgAttempts; ++attempt)  {

    stat = sendMsg(pComPort, cmdBuf);
    if (stat != asynSuccess)  { this_thread::sleep_for(200ms);  continue; }
    if (exitDriver)  return asynError;

    stat = readResp(pComPort, respBuf, respBuf.size()*sizeof(respBuf[0]));
    if (stat != asynSuccess)  { this_thread::sleep_for(200ms);  continue; }
    if (exitDriver)  return asynError;

    lastRespTime = chrono::system_clock::now();

    if (!connected)  {
      cout << endl << "=== " << portName << " ctlr online ===" << endl << endl;
      connected = true;
    }

    // check values common to all commands
    U32 pktIDSent = ntohl(cmdBuf[0]);  U32 pktIDRcvd = ntohl(respBuf[0]);
    U32 cmdSent = ntohl(cmdBuf[1]);    U32 cmdRcvd = ntohl(respBuf[1]);

    if ((pktIDSent != pktIDRcvd) or (cmdSent != cmdRcvd))  return asynError;

    U16 statOffset = LCPUtil::statusOffset((int16_t)cmdRcvd);
    U32 sessID_and_status = ntohl(respBuf[statOffset]);

    U32 respSessionID = (sessID_and_status >> 16) & 0xFFFF;
    U32 respStat = sessID_and_status & 0xFFFF;
    respStatus = static_cast<LCPStatus>(respStat);

    writeAccess = (sessionID and (respSessionID == sessionID));

//    if (!writeAccess)  //tdebug
//      cout << "0x" << hex << sessionID << " != 0x" << respSessionID << dec << endl;

    return asynSuccess;
  }

  return asynError;
}


//----------------------------------------------------------------------------
//  Update the read state and value for ParamInfo objects
//----------------------------------------------------------------------------
asynStatus drvFGPDB::updateReadValues()
{
  // LCP reg values will normally be read by the async/streaming thread...
   if (!TestMode() and updateRegs)  {
    readRegs(0x10000, procGroupSize(ProcGroup_LCP_RO));
    readRegs(0x20000, procGroupSize(ProcGroup_LCP_WA));
  }


  lock_guard<drvFGPDB> lock(*this);

  for (auto &param : params)  {
    // Is there a driver var for this param, and did it chg?
    if (param.isScalarParam())  {
      if (!param.drvValue)  continue;

      U32 newValue = *param.drvValue;

      if ((newValue == param.ctlrValRead)
        and (param.readState == ReadState::Current))  continue;

      param.ctlrValRead = newValue;
      param.readState = ReadState::Pending;
    }
    // start or continue updating a PMEM array value
    else  if (param.isArrayParam())  {
      if ((param.setState == SetState::Pending) or
          (param.setState == SetState::Processing))  continue;  // write in progress
      if (param.readState != ReadState::Update)  continue;
      if (!connected)  continue;
      if (readNextBlock(param) != asynSuccess)  {
        param.initBlockRW(param.arrayValRead.size());  // restart read
        param.readState = ReadState::Update;
      }
    }
  }

  return asynSuccess;
}

//----------------------------------------------------------------------------
//  Post a new read value for a parameter
//----------------------------------------------------------------------------
asynStatus drvFGPDB::postNewReadVal(int paramID)
{
  ParamInfo &param = params.at(paramID);
  asynStatus stat = asynError;
  double dval;


  switch (param.asynType)  {

    case asynParamInt32:
      stat = setIntegerParam(paramID, param.ctlrValRead);
      break;

    case asynParamUInt32Digital:
      stat = setUIntDigitalParam(paramID, param.ctlrValRead, 0xFFFFFFFF);
      break;

    case asynParamFloat64:
      dval = ParamInfo::ctlrFmtToDouble(param.ctlrValRead, param.ctlrFmt);
      stat = setDoubleParam(paramID, dval);
      break;

    case asynParamInt8Array:
      setParamStatus(paramID, asynSuccess);  // req for callback to work
      stat = doCallbacksInt8Array((epicsInt8 *)param.arrayValRead.data(),
                                   param.arrayValRead.size(), paramID, 0);
      break;

    default:
      break;
  }

  if (setParamStatus(paramID, stat) != asynSuccess)  return asynError;

  return stat;
}

//----------------------------------------------------------------------------
asynStatus drvFGPDB::postNewReadValues(void)
{
  bool  chgsToBePosted = false;
  asynStatus  stat, returnStat = asynSuccess;

  lock_guard<drvFGPDB> lock(*this);

  for (int paramID=0; (uint)paramID<params.size(); ++paramID)  {
    ParamInfo &param = params.at(paramID);

    if (param.readState != ReadState::Pending)  continue;

    stat = postNewReadVal(paramID);

    if (stat == asynSuccess)  {
      param.readState = ReadState::Current;
      chgsToBePosted = true;
    }
    setIfNewError(returnStat, stat);
  }

  if (chgsToBePosted)  {
    stat = callParamCallbacks();
    setIfNewError(returnStat, stat);
  }

  return returnStat;
}

//----------------------------------------------------------------------------
// Read the controller's current values for one or more LCP registers
//----------------------------------------------------------------------------
asynStatus drvFGPDB::readRegs(U32 firstReg, uint numRegs)
{
  asynStatus stat;
  LCPStatus  respStatus;

  if (exitDriver)  return asynError;

  if (ShowRegReads())
    cout << endl << "  === " << portName << ":" << "readRegs("
         "0x" << hex << firstReg << ", "
         << dec << numRegs << ")" << endl;

  if (!inDefinedRegRange(firstReg, numRegs))  return asynError;


  const int CmdHdrWords = 5;
  vector<uint32_t> cmdBuf(CmdHdrWords, 0);

  const int RespHdrWords = 5;
  vector<uint32_t> respBuf(RespHdrWords + numRegs, 0);

  // packet ID is set by sendCmdGetResp()
  cmdBuf[1] = htonl(static_cast<int32_t>(LCPCommand::READ_REGS));
  cmdBuf[2] = htonl(firstReg);
  cmdBuf[3] = htonl(numRegs);
  cmdBuf[4] = htonl(0);

  stat = sendCmdGetResp(pAsynUserUDP, cmdBuf, respBuf, respStatus);
  if (stat != asynSuccess)  return stat;

  //todo:  Check cmd-specific header values in returned packet

  uint groupID = LCPUtil::addrGroupID(firstReg);
  uint offset = LCPUtil::addrOffset(firstReg);

  ProcGroup &group = getProcGroup(groupID);

  lock_guard<drvFGPDB> lock(*this);

  for (uint u=0; u<numRegs; ++u,++offset)  {
    U32 justReadVal = ntohl(respBuf.at(RespHdrWords+u));

    int paramID = group.paramIDs.at(offset);
    if (!validParamID(paramID))  continue;

    ParamInfo &param = params.at(paramID);
    if ((justReadVal == param.ctlrValRead)
      and (param.readState == ReadState::Current))  continue;

    param.ctlrValRead = justReadVal;
    param.readState = ReadState::Pending;

    if (param.drvValue)  *param.drvValue = justReadVal;
  }

  return asynSuccess;
}


//----------------------------------------------------------------------------
// Send the driver's current value for one or more writeable LCP registers to
// the LCP controller
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeRegs(uint firstReg, uint numRegs)
{
  asynStatus stat;
  LCPStatus  respStatus;


  if (exitDriver)  return asynError;

  if (ShowRegWrites())
    cout << endl << "  === " << portName << ":" << "writeRegs("
         "0x" << hex << firstReg << ", "
         << dec << numRegs << ")" << endl;

  if (!inDefinedRegRange(firstReg, numRegs))  return asynError;
  if (LCPUtil::readOnlyAddr(firstReg))  return asynError;


  const int CmdHdrWords = 4;
  vector<uint32_t> cmdBuf(CmdHdrWords + numRegs, 0);

  const int RespHdrWords = 5;
  vector<uint32_t> respBuf(RespHdrWords, 0);

  // packet ID is set by sendCmdGetResp()
  cmdBuf[1] = htonl(static_cast<int32_t>(LCPCommand::WRITE_REGS));
  cmdBuf[2] = htonl(firstReg);
  cmdBuf[3] = htonl(numRegs);

  uint groupID = LCPUtil::addrGroupID(firstReg);
  uint offset = LCPUtil::addrOffset(firstReg);

  ProcGroup &group = getProcGroup(groupID);

  uint16_t idx = CmdHdrWords;
  {
    lock_guard<drvFGPDB> lock(*this);
    for (uint u=0; u<numRegs; ++u,++offset,++idx)  {
      int paramID = group.paramIDs.at(offset);
      if (!validParamID(paramID))  { return asynError; }
      ParamInfo &param = params.at(paramID);
      cmdBuf[idx] = htonl(param.ctlrValSet);
      param.setState = SetState::Processing;
    }
  }

  stat = sendCmdGetResp(pAsynUserUDP, cmdBuf, respBuf, respStatus);
  if (stat != asynSuccess)  return stat;

  if (respStatus != LCPStatus::SUCCESS)  return asynError;

  if (exitDriver)  return asynError;

  //todo:
  //  - Check cmd-specific header values in returned packet
  //  - Finish logic for updating the setState of the params
  //  - Return the status value from the response pkt (change the return type
  //    for the function to LCPStatus?  Or return the rcvd status in another
  //    argument)? Consider what the caller really needs/wants to know:  Just
  //    whether or not the send/rcv worked vs it worked but the controller did
  //    or didn't return an error (especially given that a resp status of
  //    SUCCESS does NOT necessarily mean that all the values written were
  //    accepted as-is)

  offset = LCPUtil::addrOffset(firstReg);

  lock_guard<drvFGPDB> lock(*this);
  for (uint u=0; u<numRegs; ++u,++offset)  {
    int paramID = group.paramIDs.at(offset);
    if (!validParamID(paramID))  continue;
    ParamInfo &param = params.at(paramID);
    // in case setState was chgd by another thread
    if (param.setState == SetState::Processing)
      param.setState = SetState::Sent;
  }

  return asynSuccess;
}

//=============================================================================
asynStatus drvFGPDB::getIntegerParam(int list, int index, int *value)
{
  if (ShowInit())  {
    cout << endl << "  === " << typeid(this).name() << "::"
         << __func__ << "()  [" << portName << "]: "
         << " list:" << dec << list;
    if (validParamID(index))  { cout << " " << params.at(index).name; }
    cout << " index:" << dec << index << "/" << params.size() << endl;
  }
  return asynPortDriver::getIntegerParam(list, index, value);
}

//----------------------------------------------------------------------------
asynStatus drvFGPDB::getDoubleParam(int list, int index, double * value)
{
  if (ShowInit())  {
    cout << endl << "  === " << typeid(this).name() << "::"
         << __func__ << "()  [" << portName << "]: "
         << " list:" << dec << list;
    if (validParamID(index))  { cout << " " << params.at(index).name; }
    cout << " index:" << dec << index << "/" << params.size() << endl;
  }
  return asynPortDriver::getDoubleParam(list, index, value);
};

//----------------------------------------------------------------------------
asynStatus drvFGPDB::getUIntDigitalParam(int list, int index,
                                         epicsUInt32 *value, epicsUInt32 mask)
{
  if (ShowInit())  {
    cout << endl << "  === " << typeid(this).name() << "::"
         << __func__ << "()  [" << portName << "]: "
         << " list:" << dec << list;
    if (validParamID(index))  { cout << " " << params.at(index).name; }
    cout << " index:" << dec << index << "/" << params.size() << endl;
  }
  return asynPortDriver::getUIntDigitalParam(list, index, value, mask);
};

//=============================================================================
bool drvFGPDB::isValidWritableParam(const char *funcName, asynUser *pasynUser)
{
  int  paramID = pasynUser->reason;

  if (!validParamID(paramID))  {
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "\n%s::%s() [%s]  Called with invalid param ref: %d",
                  typeid(this).name(), funcName, portName, paramID);
    return false;
  }

  ParamInfo &param = params.at(paramID);
  if (param.readOnly)  {
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                  "\n%s::%s() [%s]  Called for read-only param: %s [%d]",
                  typeid(this).name(), funcName, portName,
                  param.name.c_str(), paramID);
    return false;
  }

  return true;
}

//----------------------------------------------------------------------------
void drvFGPDB::applyNewParamSetting(ParamInfo &param, uint32_t setVal)
{
  param.ctlrValSet = setVal;
  param.setState = SetState::Pending;

  //ToDo: Add param to a list of params with pending writes
}


//-----------------------------------------------------------------------------
//  Erase a block of data in Flash or one of the EEPROMs on the controller
//
//  NOTES:
//    - blockSize must be a power of 2
//    - blockNum is relative to blockSize (i.e. the first byte read is at
//      offset blockSize * blockNum)
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::eraseBlock(uint chipNum, U32 blockSize, U32 blockNum)
{
  asynStatus  stat = asynError;
  LCPStatus  respStatus;

  if (ShowBlkErase())
    cout << "=== eraseBlock(" << dec << chipNum << ", "
         << blockSize << ", "
         << blockNum << ")" << endl;

  const int CmdHdrWords = 5;
  vector<uint32_t> cmdBuf(CmdHdrWords, 0);

  const int RespHdrWords = 6;
  vector<uint32_t> respBuf(RespHdrWords, 0);

  // packet ID is set by sendCmdGetResp()
  cmdBuf[1] = htonl(static_cast<int32_t>(LCPCommand::ERASE_BLOCK));
  cmdBuf[2] = htonl(chipNum);
  cmdBuf[3] = htonl(blockSize);
  cmdBuf[4] = htonl(blockNum);

  stat = sendCmdGetResp(pAsynUserUDP, cmdBuf, respBuf, respStatus);
  if (stat != asynSuccess)  return stat;

  //todo:  Check cmd-specific header values in returned packet

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Read a block of data from Flash or one of the EEPROMs on the controller
//
//  NOTES:
//    - blockSize must be a power of 2
//    - blockNum is relative to blockSize (i.e. the first byte read is at
//      offset blockSize * blockNum)
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::readBlock(uint chipNum, U32 blockSize, U32 blockNum,
                               vector<uint8_t> &buf)
{
  asynStatus  stat;
  LCPStatus  respStatus;
  uint  subBlocks;
  U32  useBlockSize, useBlockNum, etherMTU;
  uint8_t *blockData;


  if (ShowBlkReads())
    cout << "=== readBlock(" << dec << chipNum << ", "
         << blockSize << ", "
         << blockNum << ", "
         << "buf[" << buf.size() << "])" << endl;


  if (blockSize > buf.size())  return asynError;

  etherMTU = 1500;  //finish


  // Split the read of the requested blockSize # of bytes in to multiple read
  // requests if necessary to fit in the ethernet MTU size ---
  useBlockSize = blockSize;  useBlockNum = blockNum;  subBlocks = 1;
  while (useBlockSize + 30 > etherMTU) {
    useBlockSize /= 2;  useBlockNum *= 2;  subBlocks *= 2; }

  if (useBlockSize * subBlocks != blockSize)  return asynError;


  // Read the sub-blocks in reverse order, "back-filling" blockDataBuf so we
  // end up with a full blockSize # of contiguous bytes when done (each read
  // after the 1st overwriting the header rcvd for the prev read)

  blockData = buf.data();

  const int CmdHdrWords = 5;
  vector<uint32_t> cmdBuf(CmdHdrWords, 0);

  const int RespHdrWords = 6;
  vector<uint32_t> respBuf(RespHdrWords + useBlockSize/4, 0);

  // packet ID is set by sendCmdGetResp()
  cmdBuf[1] = htonl(static_cast<int32_t>(LCPCommand::READ_BLOCK));
  cmdBuf[2] = htonl(chipNum);
  cmdBuf[3] = htonl(useBlockSize);

  while (subBlocks)  {
    cmdBuf[4] = htonl(useBlockNum);

    stat = sendCmdGetResp(pAsynUserUDP, cmdBuf, respBuf, respStatus);
    if (stat != asynSuccess)  return stat;

    //todo:  Check cmd-specific header values in returned packet
    //       (the respStatus in particular!)

    memcpy(blockData, respBuf.data() + RespHdrWords, useBlockSize);

    blockData += useBlockSize;  ++useBlockNum;  --subBlocks;
  }

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Write a block of data to Flash or one of the EEPROMs on the controller
//
//  NOTES:
//    - blockSize must be a power of 2
//    - blockNum is relative to blockSize (i.e. the first byte written is at
//      offset blockSize * blockNum)
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::writeBlock(uint chipNum, U32 blockSize, U32 blockNum,
                                vector<uint8_t> &buf)
{
  asynStatus  stat = asynError;
  LCPStatus  respStatus;
  uint  subBlocks;
  U32  useBlockSize, useBlockNum, etherMTU;
  uint8_t  *blockData;

  if (ShowBlkWrites())
    cout << "=== writeBlock(" << dec << chipNum << ", "
         << blockSize << ", "
         << blockNum << ", "
         << "buf[" << buf.size() << "])" << endl;

  if (buf.size() < blockSize)  return asynError;

  etherMTU = 1500;  //finish


  // Split the read of the requested blockSize # of bytes in to multiple
  // write requests if necessary to fit in the ethernet MTU size ---
  useBlockSize = blockSize;  useBlockNum = blockNum;  subBlocks = 1;
  while (useBlockSize + 30 > etherMTU) {
    useBlockSize /= 2;  useBlockNum *= 2;  subBlocks *= 2; }

  if (useBlockSize * subBlocks != blockSize)  return asynError;


  blockData = buf.data();

  const int CmdHdrWords = 5;
  vector<uint32_t> cmdBuf(CmdHdrWords + useBlockSize/4, 0);

  const int RespHdrWords = 6;
  vector<uint32_t> respBuf(RespHdrWords, 0);

  // packet ID is set by sendCmdGetResp()
  cmdBuf[1] = htonl(static_cast<int32_t>(LCPCommand::WRITE_BLOCK));
  cmdBuf[2] = htonl(chipNum);
  cmdBuf[3] = htonl(useBlockSize);

  while (subBlocks)  {
    cmdBuf[4] = htonl(useBlockNum);

    memcpy(cmdBuf.data() + CmdHdrWords, blockData, useBlockSize);

    stat = sendCmdGetResp(pAsynUserUDP, cmdBuf, respBuf, respStatus);
    if (stat != asynSuccess)  return stat;

    //todo:  Check cmd-specific header values in returned packet
    //       (the respStatus in particular!)

    blockData += useBlockSize;  ++useBlockNum;  --subBlocks;
  }

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Read next block of a PMEM array value from the controller
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::readNextBlock(ParamInfo &param)
{
  U32  ttl, arraySize;
  float  perc;


  // Wait if an unfinished write operation
  if ((param.setState == SetState::Pending) or
      (param.setState == SetState::Processing))  return asynSuccess;

  if (!param.bytesLeft)  {
    lock_guard<drvFGPDB> lock(*this);
    param.readState = ReadState::Pending;
    return asynSuccess;
  }

//--- initialize values used in the loop ---
  arraySize = param.arrayValRead.size();

  param.rwBuf.assign(param.blockSize, 0);

  // adjust # of bytes to read from the next block if necessary
  if (param.rwCount > param.bytesLeft)  param.rwCount = param.bytesLeft;

  // read the next block of bytes
  if (readBlock(param.chipNum, param.blockSize, param.blockNum, param.rwBuf))  {
    printf("*** error reading block %u ***\r\n", param.blockNum);
    perror("readBlock()");  return asynError;
  }

  lock_guard<drvFGPDB> lock(*this);

  // Copy just read data to the appropriate bytes in the buffer
  memcpy(param.arrayValRead.data() + param.rwOffset,
         param.rwBuf.data() + param.dataOffset,
         param.rwCount);

  ++param.blockNum;  param.dataOffset = 0;  param.bytesLeft -= param.rwCount;
  param.rwOffset += param.rwCount;  param.rwCount = param.blockSize;

  ttl = arraySize - param.bytesLeft;  perc = (float)ttl / arraySize * 100.0;

  setArrayOperStatus(param, (uint32_t)perc);

  unfinishedArrayRWs = true;

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Send next block of a new array value to the controller
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::writeNextBlock(ParamInfo &param)
{
  U32  ttl, arraySize;
  float  perc;


  if (!param.bytesLeft)  {
    lock_guard<drvFGPDB> lock(*this);
    param.setState = SetState::Sent;
    param.initBlockRW(param.arrayValRead.size());  // read back what we just wrote
    param.readState = ReadState::Update;
    return asynSuccess;
  }

  {
    lock_guard<drvFGPDB> lock(*this);
    param.setState = SetState::Processing;
  }

//--- initialize values used in the loop ---
  arraySize = param.arrayValSet.size();

  param.rwBuf.assign(param.blockSize, 0);

  if (!writeAccess)  { getWriteAccess();  if (!writeAccess)  return asynError; }


  // adjust # of bytes to write to the next block if necessary
  if (param.rwCount > param.bytesLeft)  param.rwCount = param.bytesLeft;

  // If not replacing all the bytes in the block, then read the existing
  // contents of the block to be modified.
  if (param.rwCount != param.blockSize)
    if (readBlock(param.chipNum, param.blockSize, param.blockNum, param.rwBuf))  {
      printf("*** error reading block %u ***\r\n", param.blockNum);
      perror("readBlock()");  /*rwSockMutex->unlock();*/
      return asynError;
    }

  // If required, 1st erase the next block to be written to
  if (param.eraseReq)
    if (eraseBlock(param.chipNum, param.blockSize, param.blockNum))  {
      printf("*** error erasing block %u ***\r\n", param.blockNum);
      perror("eraseBlock()");  /*rwSockMutex->unlock();*/
      return asynError;
    }

  // Copy new data in to the appropriate bytes in the buffer
  memcpy(param.rwBuf.data() + param.dataOffset,
         param.arrayValSet.data() + param.rwOffset,
         param.rwCount);

  // write the next block of bytes
  if (writeBlock(param.chipNum, param.blockSize, param.blockNum, param.rwBuf)) {
    printf("*** error writing block %u ***\r\n", param.blockNum);
    perror("writeBlock()");  /*rwSockMutex->unlock()*/;
    return asynError;
  }

  ++param.blockNum;  param.dataOffset = 0;  param.bytesLeft -= param.rwCount;
  param.rwOffset += param.rwCount;  param.rwCount = param.blockSize;

  ttl = arraySize - param.bytesLeft;  perc = (float)ttl / arraySize * 100.0;

  // update the status param
  if (param.statusParamID >= 0)  {
    ParamInfo &statusParam = params.at(param.statusParamID);
    lock_guard<drvFGPDB> lock(*this);
    statusParam.ctlrValRead = (uint32_t)perc;
    statusParam.readState = ReadState::Pending;
  }

  unfinishedArrayRWs = true;

  return asynSuccess;
}

//----------------------------------------------------------------------------
// Process a request to write to one of the Int32 parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeInt32(asynUser *pasynUser, epicsInt32 newVal)
{
  if (!isValidWritableParam(__func__, pasynUser))  return asynError;

  if (initComplete and !connected)  return asynDisconnected;

  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  uint32_t setVal = ParamInfo::int32ToCtlrFmt(newVal, param.ctlrFmt);

  applyNewParamSetting(param, setVal);

  if (ShowRegWrites())
    cout << typeid(this).name() << "::" << __func__ << "()  [" << portName << "]: "
         << newVal << " (0x" << hex << setVal << ")"
         << " to " << param.name << dec << endl;

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s() [%s]:  paramID=%d, name=%s, value=%d\n",
            typeid(this).name(), __func__, portName,
            paramID, param.name.c_str(), newVal);

  return stat;
}


//----------------------------------------------------------------------------
// Process a request to write to one of the UInt32Digital parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::
  writeUInt32Digital(asynUser *pasynUser, epicsUInt32 newVal, epicsUInt32 mask)
{
  if (!isValidWritableParam(__func__, pasynUser))  return asynError;

  if (initComplete and !connected)  return asynDisconnected;

  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  // Compute result of applying specified changes
  uint32_t  setVal;
  setVal = param.ctlrValSet;   // start with cur value
  setVal |= (newVal & mask);   // set bits set in newVal and mask
  setVal &= (newVal | ~mask);  // clear bits clear in newVal but set in mask

  uint32_t  prevVal = param.ctlrValSet;
  applyNewParamSetting(param, setVal);

  if (ShowRegWrites())
    cout << typeid(this).name() << "::" << __func__ << "()  [" << portName << "]: "
         << " 0x" << hex << setVal
         << " (prev:0x" << prevVal
         << ", new:0x" << newVal
         << ", mask:0x" << mask << ")"
         << " to " << param.name << dec << endl;

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s() [%s]:  paramID=%d, name=%s, value=0x%08X\n",
            typeid(this).name(), __func__, portName,
            paramID, param.name.c_str(), newVal);

  return stat;
}

//----------------------------------------------------------------------------
// Process a request to write to one of the Float64 parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeFloat64(asynUser *pasynUser, epicsFloat64 newVal)
{
  if (!isValidWritableParam(__func__, pasynUser))  return asynError;

  if (initComplete and !connected)  return asynDisconnected;

  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  uint32_t setVal = ParamInfo::doubleToCtlrFmt(newVal, param.ctlrFmt);

  applyNewParamSetting(param, setVal);

  if (ShowRegWrites())
    cout << typeid(this).name() << "::" << __func__ << "()  [" << portName << "]: "
         << newVal << " (0x" << hex << setVal << ")"
         << " to " << param.name << dec << endl;

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s() [%s]:  paramID=%d, name=%s, value=%e\n",
            typeid(this).name(), __func__, portName,
            paramID, param.name.c_str(), newVal);

  return stat;
}

//-----------------------------------------------------------------------------
//  If a status param has been provided for an array param, then set it to the
//  percentage done for the current read or write operation for the array.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::setArrayOperStatus(ParamInfo &param, uint32_t percDone)
{
  if (param.statusParamID < 0)
    if (param.statusParamName.size() > 0)
      if ((param.statusParamID = findParamByName(param.statusParamName)) < 0)  {
        cout << typeid(this).name() << "::" << __func__
             << "()  [" << portName << "]: " << endl
             << "   invalid status parameter name: " << param << endl;
        return asynError;
      }

  if (param.statusParamID >= 0)  {
    ParamInfo &statusParam = params.at(param.statusParamID);
    statusParam.ctlrValRead = percDone;
    statusParam.readState = ReadState::Pending;
    postNewReadValues();
  }

  return asynSuccess;
}


//----------------------------------------------------------------------------
//  Only called during init for records with PINI set to "1" (?)
//----------------------------------------------------------------------------
asynStatus drvFGPDB::readInt8Array(asynUser *pasynUser, epicsInt8 *value,
                                   size_t nElements, size_t *nIn)
{
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  if (ShowBlkReads())
    cout << typeid(this).name() << "::" << __func__ << "()  [" << portName << "]: "
         << endl << "   read " << dec << nElements << " elements from: "
         << endl << "   " << param << endl;

  size_t count = nElements;
  if (count > param.arrayValRead.size())  count = param.arrayValRead.size();

  memcpy(value, param.arrayValRead.data(), count);

  *nIn = count;

//  return asynPortDriver::readInt8Array(pasynUser, value, nElements, nIn);
  return asynSuccess;
}

//-----------------------------------------------------------------------------
asynStatus drvFGPDB::writeInt8Array(asynUser *pasynUser, epicsInt8 *values,
                                    size_t nElements)
{
  if (!isValidWritableParam(__func__, pasynUser))  return asynError;

  if (initComplete and !connected)  return asynDisconnected;

  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  if (!param.isArrayParam())  return asynError;

  if ((param.setState == SetState::Pending) or
      (param.setState == SetState::Processing))  return asynError;

  setArrayOperStatus(param, 0);

  // ToDo:  Use shared buffers, so when someone is updating the firmware for N
  // controllers at once, we don't end up with N MORE unique huge buffers, each
  // with the same information (we will ALREADY have that for the records
  // themselves - no need to compound that problem in the driver...)
  param.arrayValSet.assign(&values[0], &values[nElements]);

  param.initBlockRW(param.arrayValSet.size());
  param.setState = SetState::Pending;

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s() [%s]:  paramID=%d, name=%s, nElements=%lu\n",
            typeid(this).name(), __func__, portName,
            paramID, param.name.c_str(), (ulong)nElements);

  return stat;
}

//-----------------------------------------------------------------------------

