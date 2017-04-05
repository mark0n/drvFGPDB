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


static bool  initComplete;


static const double writeTimeout = 1.0;
static const double readTimeout  = 1.0;

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
                   const string &udpPortName, uint32_t startupDiagFlags_) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, 0, InterfaceMask,
                   InterruptMask, AsynFlags, AutoConnect, Priority, StackSize),
    syncIO(syncIOWrapper),
    packetID(0),
    exitDriver(false),
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
    idSessionID(-1),
    startupDiagFlags(startupDiagFlags_)
{
  initHookRegister(drvFGPDB_initHookFunc);

  addRequiredParams();

  paramList.at(idDiagFlags).ctlrValSet = startupDiagFlags;

  paramList.at(idSessionID).readOnly = true;


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

//-----------------------------------------------------------------------------
//  Task run in a separate thread to manage synchronous communication with a
//  controller that supports LCP.
//-----------------------------------------------------------------------------
void drvFGPDB::syncComLCP()
{
  while (!initComplete and !exitDriver)  this_thread::sleep_for(5ms);

  while (!exitDriver)  {
    auto start_time = chrono::system_clock::now();

    if (!TestMode())  {
      // this will normally be done in the async/streaming thread...
      readRegs(0x10000, getRegGroup(ProcGroup_LCP_RO).paramIDs.size());
      readRegs(0x20000, getRegGroup(ProcGroup_LCP_WA).paramIDs.size());

      processPendingWrites();
    }

    if (!exitDriver)  this_thread::sleep_until(start_time + 200ms);
  }
}


//-----------------------------------------------------------------------------
//  Attempt to gain write access to the ctlr (by setting the sessionID reg)
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::getWriteAccess(void)
{
  if (exitDriver)  return asynError;

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
      if (ShowRegWrites())
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
  if (exitDriver)  return asynError;

  asynStatus  stat = asynError;

  //ToDo: Use a linked-list of params with pending writes

  if (!writeAccess)  getWriteAccess();
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
asynStatus drvFGPDB::addRequiredParams(void)
{
  for (auto const &paramDef : requiredParamDefs)
    if ((*paramDef.id = processParamDef(paramDef.def)) < 0)  return asynError;

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Search the driver's list of parameters for an entry with the given name.
//  Unlike asynPortDriver::findParam(), this func works during IOC startup.
//  Returns the paramID (index in to list) or < 0 if param not in the list.
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

  paramList.push_back(newParam);

  if ((uint)paramID != paramList.size() - 1)  {
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

  ParamInfo &curParam = paramList.at(paramID);
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

  return ((offset + numRegs) <= group.paramIDs.size());
}

//-----------------------------------------------------------------------------
// Callback function for EPICS IOC initialization steps.  Used to trigger
// normal processing by the driver.
//-----------------------------------------------------------------------------
void drvFGPDB_initHookFunc(initHookState state)
{
  if (state == initHookAfterInitDatabase)  initComplete = true;
}

//-----------------------------------------------------------------------------
// Update the LCP reg addr to paramID maps based on the specified param def.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::updateRegMap(int paramID)
{
  if (!validParamID(paramID))  return asynError;

  ParamInfo &param = paramList.at(paramID);

  auto addr = param.regAddr;
  uint groupID = LCPUtil::addrGroupID(addr);
  uint offset = LCPUtil::addrOffset(addr);

  if (LCPUtil::validRegAddr(addr))  {
    vector<int> &paramIDs = getRegGroup(groupID).paramIDs;

    if (offset >= paramIDs.size())  paramIDs.resize(offset+1, -1);
    if (paramIDs.at(offset) < 0)   // not set yet
      paramIDs.at(offset) = paramID;
    else
      if (paramIDs.at(offset) != paramID)  {
      cout << "Device: " << portName << ": "
           "*** Multiple params with same LCP reg addr ***" << endl
           << "  [" << paramList.at(paramIDs.at(offset)) << "]"
              " and [" << paramList.at(paramID) << "]" << endl;
      return asynError;
    }
  } else  if (groupID) {  // not a driver param?
    cout << "Invalid addr/group ID for parameter: " << param.name << endl;
    return asynError;
  }

  return asynSuccess;
}

//----------------------------------------------------------------------------
//  Post a new read value for a parameter
//----------------------------------------------------------------------------
asynStatus drvFGPDB::postNewReadVal(int paramID)
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
      dval = ParamInfo::ctlrFmtToDouble(param.ctlrValRead, param.ctlrFmt);
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


  if (exitDriver)  return asynError;

  if (ShowRegReads())
    cout << endl << "  === " << portName << ":" << "readRegs("
         "0x" << hex << firstReg << ", "
         << dec << numRegs << ")" << endl;

  if (!inDefinedRegRange(firstReg, numRegs))  return asynError;

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

  writeData outData {
    .write_buffer = reinterpret_cast<char *>(cmdBuf.data()),
    .write_buffer_len = bytesToSend,
    .nbytesOut = &bytesSent
  };
  stat = syncIO->write(pAsynUserUDP, outData, writeTimeout);
  if (stat != asynSuccess)  return stat;
  if (bytesSent != bytesToSend)  return asynError;

  ++packetID;

  if (exitDriver)  return asynError;


  asynStatus returnStat = asynSuccess;  size_t rcvd = 0;
  readData inData {
    .read_buffer = respBuf,
    .read_buffer_len = sizeof(respBuf),
    .nbytesIn = &rcvd
  };
  stat = syncIO->read(pAsynUserUDP, inData, readTimeout, &eomReason);
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


  if (exitDriver)  return asynError;

  if (ShowRegWrites())
    cout << endl << "  === " << portName << ":" << "writeRegs("
         "0x" << hex << firstReg << ", "
         << dec << numRegs << ")" << endl;

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
  }

  size_t bytesToSend = cmdBuf.size() * sizeof(cmdBuf[0]);
  size_t bytesSent;

  // Send the cmd pkt
  writeData outData {
    .write_buffer = reinterpret_cast<char *>(cmdBuf.data()),
    .write_buffer_len = bytesToSend,
    .nbytesOut = &bytesSent
  };
  stat = syncIO->write(pAsynUserUDP, outData, writeTimeout);
  if (stat != asynSuccess)  return stat;
  if (bytesSent != bytesToSend)  return asynError;


  if (exitDriver)  return asynError;

  // Read and process response packet
  vector<uint32_t> respBuf(5, 0);
  size_t expectedRespSize = respBuf.size() * sizeof(respBuf[0]);
  readData inData {
    .read_buffer = reinterpret_cast<char *>(respBuf.data()),
    .read_buffer_len = expectedRespSize,
    .nbytesIn = &rcvd
  };
  stat = syncIO->read(pAsynUserUDP, inData, readTimeout, &eomReason);
  if (stat != asynSuccess)  return stat;
  if (rcvd != expectedRespSize)  return asynError;

  uint32_t sessID_and_status = ntohl(respBuf[4]);
  respSessionID = (sessID_and_status >> 16) & 0xFFFF;
  respStatus = sessID_and_status & 0xFFFF;

  writeAccess = (sessionID.ctlrValSet and respSessionID == sessionID.ctlrValSet);

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
asynStatus drvFGPDB::getIntegerParam(int list, int index, int *value)
{
  if (ShowInit())  {
    cout << "  " << __func__ << " list: " << dec << list;
    if (validParamID(index))  { cout << " " << paramList.at(index).name; }
    cout << " index: " << dec << index << "/" << paramList.size() << endl;
  }
  return asynPortDriver::getIntegerParam(list, index, value);
}

//----------------------------------------------------------------------------
asynStatus drvFGPDB::getDoubleParam(int list, int index, double * value)
{
  if (ShowInit())  {
    cout << "  " << __func__ << " list: " << dec << list;
    if (validParamID(index))  { cout << " " << paramList.at(index).name; }
    cout << " index: " << dec << index << "/" << paramList.size() << endl;
  }
  return asynPortDriver::getDoubleParam(list, index, value);
};

//----------------------------------------------------------------------------
asynStatus drvFGPDB::getUIntDigitalParam(int list, int index,
                                         epicsUInt32 *value, epicsUInt32 mask)
{
  if (ShowInit())  {
    cout << "  " << __func__ << " list: " << dec << list;
    if (validParamID(index))  { cout << " " << paramList.at(index).name; }
    cout << " index: " << dec << index << "/" << paramList.size() << endl;
  }
  return asynPortDriver::getUIntDigitalParam(list, index, value, mask);
};


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
    cout << portName << ":" << caller << ": "<< param.name << ": "
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


  if (ShowRegWrites())
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


  if (ShowRegWrites())
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


  uint32_t ctlrVal = ParamInfo::doubleToCtlrFmt(newVal, param.ctlrFmt);

  if (ShowRegWrites())
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
int drvFGPDB_Config(char *drvPortName, char *udpPortName,
                    int startupDiagFlags_)
{
  new drvFGPDB(string(drvPortName),
               NULL, //new asynOctetSyncIOInterface,
               string(udpPortName), startupDiagFlags_);

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

