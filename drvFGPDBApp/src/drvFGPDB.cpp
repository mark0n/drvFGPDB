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

#include <epicsThread.h>

using namespace std;


typedef chrono::milliseconds ms;
typedef chrono::seconds  secs;


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


static const double writeTimeout = 0.5;
static const double readTimeout  = 0.5;

//-----------------------------------------------------------------------------
//  print the specified date/time in YYY-MM-DD HH:MM:SS format
//-----------------------------------------------------------------------------
void showDateTime(const time_t dateTime)
{
  std::tm *lt = localtime(&dateTime);

  // Work around missing std::put_time() function (not supported by g++ <5).
  // The following statement can be replaced by 'cout << put_time(lt, "%F %T.")'
  // when we don't need to support Debian Jessie anymore.
  cout << 1900 + lt->tm_year << "-"
       << std::setw(2) << std::setfill('0') << 1 + lt->tm_mon << "-"
       << std::setw(2) << std::setfill('0') << lt->tm_mday << " "
       << std::setw(2) << std::setfill('0') << lt->tm_hour << ":"
       << std::setw(2) << std::setfill('0') << lt->tm_min << ":"
       << std::setw(2) << std::setfill('0') << lt->tm_sec;
}

//-----------------------------------------------------------------------------
//  print the current date/time incl milliseconds
//-----------------------------------------------------------------------------
void logMsgHdr(const string& prefix)
{
  auto now = chrono::system_clock::now();
  auto now_c = chrono::system_clock::to_time_t(now);
  auto msec = chrono::duration_cast<ms>(now.time_since_epoch()).count() % 1000;

  cout << prefix;  showDateTime(now_c);

  cout << "." << std::setw(3) << std::setfill('0') << msec << " "
       << epicsThreadGetNameSelf() << "[" << this_thread::get_id() << "]: ";
}

//-----------------------------------------------------------------------------
drvFGPDB::drvFGPDB(const string &drvPortName,
                   shared_ptr<asynOctetSyncIOInterface> syncIOWrapper,
                   const string &udpPortName, uint32_t startupDiagFlags,
                   uint32_t resendMode_) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, InterfaceMask, InterruptMask,
                   AsynFlags, AutoConnect, Priority, StackSize),
    timerQueue(epicsTimerQueueActive::allocate(false, TimerThreadPriority)),
    writeAccessTimer(eventTimer(bind(&drvFGPDB::keepWriteAccess, this),
                                2.000, timerQueue)),
    scalarReadsTimer(eventTimer(bind(&drvFGPDB::processScalarReads, this),
                                0.200, timerQueue)),
    scalarWritesTimer(eventTimer(bind(&drvFGPDB::processScalarWrites, this),
                                 0.200, timerQueue)),
    arrayReadsTimer(eventTimer(bind(&drvFGPDB::processArrayReads, this),
                               0.020, timerQueue)),
    arrayWritesTimer(eventTimer(bind(&drvFGPDB::processArrayWrites, this),
                                0.020, timerQueue)),
    postNewReadingsTimer(eventTimer(bind(&drvFGPDB::postNewReadings, this),
                                    0.200, timerQueue)),
    comStatusTimer(eventTimer(bind(&drvFGPDB::checkComStatus, this),
                              1.000, timerQueue)),
    callback_thread_id(0),
    syncIO(syncIOWrapper),
    initComplete(false),
    exitDriver(false),
    writeAccess(false),
    arrayWritesInProgress(false),
    arrayReadsInProgress(false),
    updateRegs(true),
    firstRestartCheck(true),
    connected(false),
    lastRespTime(0s),
    lastWriteTime(0s),
    idUpSecs(-1),
    upSecs(0),
    idSessionID(-1),
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
    idStateFlags(-1),
    stateFlags(0),
    idCtlrUpSince(-1),
    ctlrUpSince(0),
    resendMode(static_cast<ResendMode>(resendMode_)),
    diagFlags(startupDiagFlags)
{
  if (addRequiredParams() != asynSuccess)  {
    logMsgHdr("\n");
    cout << "*** " << drvPortName << ": Req Params Config error ***" << endl;
    //Exit thread body safely
    exitDriver = true;
    //ToDo: destroy() for all the eventTimers (?)
    throw invalid_argument("Invalid Req Params config");
  }

  // Create a pAsynUser and connect it to the asyn port that was created by
  // the startup script for communicating with the LCP controller
  auto stat = syncIO->connect(udpPortName.c_str(), 0, &pAsynUserUDP, nullptr);

  if (stat) {
    logMsgHdr("\n");
    cout << "***  asyn driver for: " << drvPortName
         << " unable to connect to asyn UDP port: " << udpPortName << " ***"
         << endl << endl;
    throw invalid_argument("Invalid asyn UDP port name");
  }
}

//-----------------------------------------------------------------------------
drvFGPDB::~drvFGPDB()
{
  exitDriver = true;

  writeAccessTimer.destroy();
  scalarReadsTimer.destroy();
  scalarWritesTimer.destroy();
  arrayReadsTimer.destroy();
  arrayWritesTimer.destroy();
  postNewReadingsTimer.destroy();
  comStatusTimer.destroy();

  timerQueue.release();

  syncIO->disconnect(pAsynUserUDP);
}

//-----------------------------------------------------------------------------
void drvFGPDB::startCommunication()
{
  if (verifyReqParams() != asynSuccess)  {
    logMsgHdr("\n");
    cout << "*** " << portName
         << " missing or invalid defs for req params ***" << endl << endl;
    return;
  }

  writeAccessTimer.start();
  scalarReadsTimer.start();
  arrayReadsTimer.start();
  postNewReadingsTimer.start();
  comStatusTimer.start();

  logMsgHdr("\n");
  cout << "=== " << portName << ": initialization complete ===" << endl << endl;

  initComplete = true;
}

//-----------------------------------------------------------------------------
//  Check to make sure timerEvent callbacks don't come from more than 1 thread
//-----------------------------------------------------------------------------
void drvFGPDB::checkCallbackThread(const string &funcName)
{
  thread::id  thisThread = this_thread::get_id();

  if (callback_thread_id == (thread::id)0)  callback_thread_id = thisThread;

  if (ShowCallbacks() or (thisThread != callback_thread_id))  {
    logMsgHdr("\n");
    cout << "====[" << funcName << "]===" << endl;
  }
  if (thisThread != callback_thread_id)  {
    logMsgHdr("\n");
    cout << "*** timer callback from multiple threads!!! ***" << endl << endl;
  }
}

//-----------------------------------------------------------------------------
//  Function invoked by the eventTimer thread to update the driver's copy of
//  the scalar parameter values.
//
//  Returns DefaultInternval or the # of secs until the next call.
//
//  WARNING:  This function should ONLY be called by the thread that manages
//            the eventTimer.  To cause this function to be called by that
//            thread ASAP, call scalarReadsTimer.wakeUp()
//-----------------------------------------------------------------------------
double drvFGPDB::processScalarReads()
{
  checkCallbackThread(__func__);

  if (exitDriver)  return DontReschedule;

  updateScalarReadValues();  postNewReadingsTimer.wakeUp();

  return DefaultInterval;
}

//-----------------------------------------------------------------------------
//  Function invoked by the eventTimer thread to process any Pending write
//  operations for scalar values.
//
//  Returns DefaultInternval or the # of secs until the next call.
//
//  WARNING:  This function should ONLY be called by the thread that manages
//            the eventTimer.  To cause this function to be called by that
//            thread ASAP, call scalarWritesTimer.wakeUp()
//-----------------------------------------------------------------------------
double drvFGPDB::processScalarWrites(void)
{
  checkCallbackThread(__func__);

  if (exitDriver)  return DontReschedule;

  //ToDo: Use a list of scalar params with pending writes to improve efficiency

  bool writeErrors = false;

  for (auto &param : params)  {

    if (!param.isScalarParam())  continue;

    SetState setState;
    {
      lock_guard<drvFGPDB> asynLock(*this);
      setState = param.setState;
    }
    if (setState != SetState::Pending)  continue;

    // LCP reg param: Write new setting to the controller
    if (LCPUtil::isLCPRegParam(param.getRegAddr()))  {
      if (!connected or !writeAccess)  continue;
      if (writeRegs(param.getRegAddr(), 1) != asynSuccess)
        writeErrors = true;
      else if (param.drvValue)  {  // also update local var if one specified
        lock_guard<drvFGPDB> asynLock(*this);
        *param.drvValue = param.ctlrValSet;
      }
      continue;
    }

    // Driver-only params: Write new setting to local variable
    if (param.drvValue)  {
      lock_guard<drvFGPDB> asynLock(*this);
      *param.drvValue = param.ctlrValSet;
      param.setState = SetState::Sent;
    }
  }

  return (writeErrors ? DefaultInterval : 5.0);
}


//-----------------------------------------------------------------------------
//  Function invoked by the eventTimer thread to read the next block for each
//  active array read operation.
//
//  Returns DefaultInternval or the # of secs until the next call.
//
//  WARNING:  This function should ONLY be called by the thread that manages
//            the eventTimer.  To cause this function to be called by that
//            thread ASAP, call arrayReadsTimer.wakeUp()
//-----------------------------------------------------------------------------
double drvFGPDB::processArrayReads(void)
{
  checkCallbackThread(__func__);

  if (exitDriver)  return DontReschedule;
  if (!connected)  return 1.0;

  //ToDo: Use a list of array params with pending reads to improve efficiency

  arrayReadsInProgress = false;

  for (auto &param : params)  {

    if (! param.isArrayParam())  continue;

    if (param.readState != ReadState::Update)  continue;

    SetState setState;
    {
      lock_guard<drvFGPDB> asynLock(*this);
      setState = param.setState;
    }
    // Wait if a new or in-progress write operation for this array
    if ((setState == SetState::Pending) or
        (setState == SetState::Processing))  continue;

    // start or continue processing an array value
    if (readNextBlock(param) != asynSuccess)  initArrayReadback(param);
  }

  return (arrayReadsInProgress ? DefaultInterval : 2.0);
}


//-----------------------------------------------------------------------------
//  Function invoked by the eventTimer thread to send the next block for each
//  active array write operation.
//
//  Returns DefaultInternval or the # of secs until the next call.
//
//  WARNING:  This function should ONLY be called by the thread that manages
//            the eventTimer.  To cause this function to be called by that
//            thread ASAP, call arrayWritesTimer.wakeUp()
//-----------------------------------------------------------------------------
double drvFGPDB::processArrayWrites(void)
{
  checkCallbackThread(__func__);

  if (exitDriver)  return DontReschedule;

  //ToDo: Use a list of array params with pending writes to improve efficiency

  arrayWritesInProgress = false;

  for (auto &param : params)  {

    // start or continue processing an array value
    if (! param.isArrayParam())  continue;

    SetState setState;
    {
      lock_guard<drvFGPDB> asynLock(*this);
      setState = param.setState;
    }
    if ((setState != SetState::Pending) and
        (setState != SetState::Processing))  continue;

    if (!connected or !writeAccess)  continue;

    if (writeNextBlock(param) != asynSuccess)  {
      logMsgHdr("\n");
      cout << "*** " << portName << ":" << param.name << ": "
        "unable to write new array value ***" << endl << endl;
      lock_guard<drvFGPDB> asynLock(*this);
      param.setState = SetState::Error;
      setParamStatus(ParamID(param), asynError);
      // always re-read after a write (especially after a failed one!)
      initArrayReadback(param);
    }
  }

  return (arrayWritesInProgress ? DefaultInterval : 2.0);
}


//-----------------------------------------------------------------------------
//  Function invoked by the eventTimer thread to update the state of they asyn
//  params and post any changes.
//
//  Returns DefaultInternval or the # of secs until the next call.
//
//  WARNING:  This function should ONLY be called by the thread that manages
//            the eventTimer.  To cause this function to be called by that
//            thread ASAP, call postNewReadingsTimer.wakeUp()
//----------------------------------------------------------------------------
double drvFGPDB::postNewReadings(void)
{
  bool  chgsToBePosted = false;
  asynStatus  stat, returnStat = asynSuccess;

  checkCallbackThread(__func__);

  if (exitDriver)  return DontReschedule;


  //ToDo:  Efficiency improvement:
  //       Eliminate the need to scan the entire list of params each time by
  //       using a separate list of params that have new read values that need
  //       to be posted.

  lock_guard<drvFGPDB> asynLock(*this);

  for (int paramID=0; (uint)paramID<params.size(); ++paramID)  {
    ParamInfo &param = params.at(paramID);

    if (param.readState != ReadState::Pending)  continue;

    stat = setAsynParamVal(paramID);

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

  return DefaultInterval;
}


//-----------------------------------------------------------------------------
//  Function invoked by the eventTimer thread to check if the ctlr is connected,
//  disconnected, or was rebooted.
//
//  Returns DefaultInternval or the # of secs until the next call.
//
//  WARNING:  This function should ONLY be called by the thread that manages
//            the eventTimer.  To cause this function to be called by that
//            thread ASAP, call comStatusTimer.wakeUp()
//-----------------------------------------------------------------------------
double drvFGPDB::checkComStatus(void)
{
  checkCallbackThread(__func__);

  lock_guard<drvFGPDB> asynLock(*this);

  if (connected)  {
    if (chrono::system_clock::now() - lastRespTime >= 5s)  {
      logMsgHdr("\n");
      cout << "*** " << portName << " ctlr offline ***" << endl << endl;
      resetReadStates();  connected = false;
    }
  }
  // scan list of RO and WA regs to see if all of them are current
  else  {
    bool  unreadValues = false;
    for (auto &param : params)  {
      auto groupID = LCPUtil::addrGroupID(param.getRegAddr());
      if ((groupID == ProcGroup_LCP_RO) or (groupID == ProcGroup_LCP_WA))
        if (param.readState != ReadState::Current)  {
          unreadValues = true;  break; }
    }
    if (!unreadValues)  {
      logMsgHdr("\n");
      cout << "=== " << portName << " ctlr online ===" << endl << endl;
      connected = true;
      scalarWritesTimer.wakeUp();
    }
  }

  return DefaultInterval;
}

//-----------------------------------------------------------------------------
//  Reset readState of all parameters so we can determine when they have all
//  been reread, and post the status chg to update the state of EPICS records
//-----------------------------------------------------------------------------
void drvFGPDB::resetReadStates(void)
{
  lock_guard<drvFGPDB> asynLock(*this);

  for (int paramID=0; (uint)paramID<params.size(); ++paramID)  {
    ParamInfo &param = params.at(paramID);

    if (param.isScalarParam())
      param.readState = ReadState::Undefined;

    else  if (param.isArrayParam())  {
      param.initBlockRW(param.arrayValRead.size());
      param.readState = ReadState::Update;
    }

    setParamStatus(paramID, asynDisconnected);

    // required to get status change to process for an array param
    if (param.getAsynType() == asynParamInt8Array)
       doCallbacksInt8Array((epicsInt8 *)"", 0, paramID, 0);
  }

  callParamCallbacks();
}

//-----------------------------------------------------------------------------
//  Cause all previously sent WA register values to be resent
//-----------------------------------------------------------------------------
void drvFGPDB::resetSetStates(void)
{
  lock_guard<drvFGPDB> asynLock(*this);

  for (auto &param : params)  {
    if ( (LCPUtil::addrGroupID(param.getRegAddr()) == ProcGroup_LCP_WA) and
        ((param.setState == SetState::Processing) or
         (param.setState == SetState::Restored) or
         (param.setState == SetState::Sent)) )
      param.setState = SetState::Pending;
  }
  scalarWritesTimer.wakeUp();
}

//-----------------------------------------------------------------------------
//  Mark all Restored settings as Sent
//-----------------------------------------------------------------------------
void drvFGPDB::clearSetStates(void)
{
  lock_guard<drvFGPDB> asynLock(*this);

  for (auto &param : params)  {
    if ( (LCPUtil::addrGroupID(param.getRegAddr()) == ProcGroup_LCP_WA) and
        (param.setState == SetState::Restored) )
      param.setState = SetState::Sent;
  }
}

//-----------------------------------------------------------------------------
//  Abort any incomplete array write operations.  Generally called because the
//  controller restarted.
//-----------------------------------------------------------------------------
void drvFGPDB::cancelArrayWrites(void)
{
  //ToDo: Use a list of array params with active writes to improve efficiency

  arrayWritesInProgress = false;

  for (auto &param : params)  {
    if (! param.isArrayParam())  continue;

    {
      lock_guard<drvFGPDB> asynLock(*this);

      if ((param.setState != SetState::Pending) and
          (param.setState != SetState::Processing))  continue;

      param.setState = SetState::Error;
      setParamStatus(ParamID(param), asynError);
    }

    logMsgHdr("\n");
    cout << "*** " << portName << ":" << param.name << ": write canceled ***"
         << endl;
  }

}

//-----------------------------------------------------------------------------
//  If the ctlr was restarted since we were last talking to it, then log a
//  message and (optionally) resend all the settings received since the IOC
//  restarted.
//-----------------------------------------------------------------------------
void drvFGPDB::checkForRestart(uint32_t newUpSecs)
{
  uint32_t readTime = (uint32_t) chrono::system_clock::to_time_t(lastRespTime);

  ParamInfo &upSinceParam = params.at(idCtlrUpSince);

  uint32_t newUpSince = readTime - newUpSecs;
  uint32_t prevUpSince =
     firstRestartCheck ? upSinceParam.ctlrValSet : upSinceParam.ctlrValRead;

  time_t upSince = (time_t) newUpSince;

  // If ctlr restarted, resend all the scalar settings (if configured to do so)
  // and cancel all array writes
  if ((int32_t)(newUpSince - prevUpSince) > 3)  {
    logMsgHdr("\n");
    cout << "*** " << portName << " Controller restarted ***" << endl << endl;
    if (resendMode == ResendMode::AfterCtlrRestart)  resetSetStates();
    cancelArrayWrites();  resetReadStates();
  }
  else {
    // ctlr did not restart, so clear set state for all Restored settings
    if (firstRestartCheck)  {
      logMsgHdr("\n");
      cout << "=== " << portName << " Controller up since: ";
      showDateTime(upSince);
      cout << " ===" << endl << endl;
      clearSetStates();
    }
  }

  firstRestartCheck = false;

  upSinceParam.newReadVal(newUpSince);
}

//-----------------------------------------------------------------------------
//  Attempt to gain write access to the ctlr (by setting the sessionID reg)
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::getWriteAccess(void)
{
  if (exitDriver or !connected)  return asynError;

  ParamInfo &param = params.at(idSessionID);

  for (int attempt = 0; attempt <= 5; ++attempt)  {
    if (attempt)  this_thread::sleep_for(10ms);

    param.ctlrValSet = sessionID.get();
    param.setState = SetState::Pending;

    if (writeRegs(param.getRegAddr(), 1) != asynSuccess)  continue;

    if (!writeAccess)  continue;

    return asynSuccess;
  }

  logMsgHdr("\n");
  cout << "*** " << portName << " failed to get write access ***" << endl;

  return asynError;
}

//-----------------------------------------------------------------------------
//  Called by eventTimer thread as needed to get and to maintain write access.
//
//  Returns DefaultInternval or the # of secs until the next call.
//
//  WARNING:  This function should ONLY be called by the thread that manages
//            the eventTimer.  To cause this function to be called by that
//            thread ASAP, call scalarReadsTimer.wakeUp()
//-----------------------------------------------------------------------------
double drvFGPDB::keepWriteAccess(void)
{
  checkCallbackThread(__func__);

  if (exitDriver)  return DontReschedule;
  if (!connected)  return DefaultInterval;

  lock_guard<drvFGPDB> asynlock(*this);

  if (!writeAccess) {
    if (ShowRegWrites())  {
      logMsgHdr("\n");
      cout << portName << ": getting write access" << endl;
    }
    if (getWriteAccess() != asynSuccess)  return 1.0;
    scalarWritesTimer.wakeUp();
    return DefaultInterval;
  }

  if (ShowRegWrites())  {
    logMsgHdr("\n");
    cout << portName << ": keeping write access" << endl;
  }

  asynStatus stat;
  vector<uint32_t> cmdBuf {
    htonl(syncPktID),
    htonl(static_cast<int32_t>(LCPCommand::WRITE_REGS)),
    0,
    0
  };
  const int respHdrWords = 5;
  vector<uint32_t> respBuf(respHdrWords, 0);
  LCPStatus respStatus;
  stat = sendCmdGetResp(pAsynUserUDP, cmdBuf, respBuf, respStatus);

  if (stat != asynSuccess) return  1.0;
  if (respStatus != LCPStatus::SUCCESS) return 1.0;

  return DefaultInterval;
}

//-----------------------------------------------------------------------------
//  Add params for values the driver expects and/or supports for all devices.
//  NOTE that the values that correspond to LCP registers do not yet have
//  regAddr value assigned to them.  This avoids the need to have fixed LCP
//  addresses assigned to such values.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::addRequiredParams(void)
{
  asynStatus  stat = asynSuccess;

  /**
  * @brief Defines the properties of all parameters required by the driver.
  */
  class RequiredParam {
    public:
      int&         id;       //!< address of int value to save the paramID
      uint32_t    *drvVal;   //!< value for driver-only params
      std::string  def;      //!< string that defines the parameter
      bool         readOnly; //!< write protect this parameter?
  };

  /**
   * @brief List that includes all parameters required by the driver.
   *
   * @note  Register values that the ctlr must support:\n
   *        The final LCP addr is supplied by EPICS records.\n
   *        Use addr 0x0.\n
   *        - upSecs, upMs, writerIP, writerPort and sessionID
   * @n
   * @note  Driver-Only values:\n
   *        Use addr=0x1 for Read-Only, addr=0x2 for Read/Write.\n
   *        - syncPktID, syncPktsSent, syncPktsRcvd, asyncPktID, asyncPktsSent
   *          and asyncPktsRcvd, ctlrUpSince
   */
  const std::list<RequiredParam> requiredParamDefs = {
    //--- reg values the ctlr must support ---
    // Use addr 0x0 for LCP reg values (LCP addr is supplied by EPICS recs)
    //ptr-to-paramID   drvVal          param name     addr asyn-fmt      ctlr-fmt read-only?
    { idUpSecs,        &upSecs,        "upSecs         0x0 Int32         U32",    true  },

    { idSessionID,     nullptr,        "sessionID      0x0 Int32         U32",    true  },

    //--- driver-only values ---
    // addr 0x1 == Read-Only, 0x2 = Read/Write
    { idSyncPktID,     &syncPktID,     "syncPktID      0x1 Int32         U32",    false },
    { idSyncPktsSent,  &syncPktsSent,  "syncPktsSent   0x1 Int32         U32",    false },
    { idSyncPktsRcvd,  &syncPktsRcvd,  "syncPktsRcvd   0x1 Int32         U32",    false },

    { idAsyncPktID,    &asyncPktID,    "asyncPktID     0x1 Int32         U32",    false },
    { idAsyncPktsSent, &asyncPktsSent, "asyncPktsSent  0x1 Int32         U32",    false },
    { idAsyncPktsRcvd, &asyncPktsRcvd, "asyncPktsRcvd  0x1 Int32         U32",    false },

    { idStateFlags,    &stateFlags,    "stateFlags     0x1 UInt32Digital U32",    false },

    { idDiagFlags,     &diagFlags,     "diagFlags      0x2 UInt32Digital U32",    false },

    { idCtlrUpSince,   &ctlrUpSince,   "ctlrUpSince    0x2 Int32         U32",    false },
 };

  for (auto const &paramDef : requiredParamDefs)  {
    int paramID = processParamDef(paramDef.def, paramDef.readOnly);
    if (paramID < 0)  {
      stat = asynError;  continue; }
    paramDef.id = paramID;
    if (paramDef.drvVal)  {
      ParamInfo &param = params.at(paramID);
      param.drvValue = paramDef.drvVal;
    }
  }

  // insure existance and config of critical values
  if (!validParamID(idUpSecs) or
      !validParamID(idSessionID) or
      !validParamID(idCtlrUpSince))  return asynError;

  return stat;
}

//-----------------------------------------------------------------------------
// Verify that no key info is missing for the required params
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::verifyReqParams(void) const
{
  uint  errCount = 0;

  for (auto &param : params)  {
    if (param.getRegAddr() < 1)  {
      logMsgHdr("\n");
      cout << "*** " << portName << ": Incomplete param def ***" << endl
           << "[" << param << "]" << endl;
      ++errCount;  continue;
    }
  }

  return (errCount ? asynError : asynSuccess);
}

//-----------------------------------------------------------------------------
//  Search the driver's list of parameters for an entry with the given name.
//  Unlike asynPortDriver::findParam(), this func works during IOC startup.
//  Returns the paramID (index in to list) or < 0 if param not in the list.
//-----------------------------------------------------------------------------
int drvFGPDB::findParamByName(const string &name) const
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
    lock_guard<drvFGPDB> asynLock(*this);
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

  if (newParam.getAsynType() == asynParamNotDefined)  {
    logMsgHdr("\n");
    cout << "*** " << portName << ": No asyn type specified ***"
         << "[" << newParam << "] ***" << endl;
    return -1;
  }
  stat = createParam(newParam.name.c_str(), newParam.getAsynType(), &paramID);
  if (stat != asynSuccess)  return -1;

  setParamStatus(paramID, asynDisconnected);

  if (ShowInit())  {
    logMsgHdr("\n");
    cout << "created " << portName << ":"
         << newParam.name << " [" << dec << paramID << "]" << endl;
  }

  params.push_back(newParam);

  if ((uint)paramID != params.size() - 1)  {
    logMsgHdr("\n");
    cout << "*** " << portName << ":" << newParam.name << ": "
            "asyn paramID != driver paramID ***" << endl;
    throw runtime_error("mismatching paramIDs");
  }

  if (newParam.getRegAddr()) if (updateRegMap(paramID) != asynSuccess)  return -1;

  return paramID;
}

//-----------------------------------------------------------------------------
//  Process a param def and add it to the driver and asyn layer lists or update
//  its properties.
//-----------------------------------------------------------------------------
int drvFGPDB::processParamDef(const string &paramDef, bool readOnly = false)
{
  asynStatus  stat;
  int  paramID;

  ParamInfo newParam(paramDef, portName, readOnly);
  if (newParam.name.empty())  return -1;

  if ((paramID = findParamByName(newParam.name)) < 0)
    return addNewParam(newParam);

  ParamInfo &curParam = params.at(paramID);
  if (ShowInit())  {
    logMsgHdr("\n");
    cout << "update: [" << curParam << "] using: [" << newParam << "]" << endl;
  }
  stat = curParam.updateParamDef(portName, newParam);
  if (stat != asynSuccess)  return -1;

  if (newParam.getRegAddr())
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

  auto addr = param.getRegAddr();
  uint groupID = LCPUtil::addrGroupID(addr);
  uint offset = LCPUtil::addrOffset(addr);

  if (LCPUtil::isLCPRegParam(addr))  { // ref to LCP register value
    vector<int> &paramIDs = getProcGroup(groupID).paramIDs;

    if (offset >= paramIDs.size())  paramIDs.resize(offset+1, -1);

    if (paramIDs.at(offset) < 0) {  // not set yet
      paramIDs.at(offset) = paramID;  return asynSuccess; }

    if (paramIDs.at(offset) == paramID)  return asynSuccess;

    logMsgHdr("\n");
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

  logMsgHdr("\n");
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
int drvFGPDB::readResp(asynUser *pComPort, vector<uint32_t> &respBuf)
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
  if (stat != asynSuccess)  return -1;
  ++syncPktsRcvd;

  return rcvd;
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
  int  flushedPkts;
  U32  cmdRcvd = 0;

  ++syncPktID;  cmdBuf[0] = htonl(syncPktID);  respStatus = LCPStatus::ERROR;

  const int MaxMsgAttempts = 5;
  for (int attempt=0; attempt<MaxMsgAttempts; ++attempt)  {

    stat = sendMsg(pComPort, cmdBuf);
    if (exitDriver)  return asynError;
    if (stat != asynSuccess)  {
      checkComStatus();  this_thread::sleep_for(100ms);  continue; }

    bool validResp = false;
    for (flushedPkts=0; flushedPkts<100; ++flushedPkts)  {

      int respLen = readResp(pComPort, respBuf);
      if (exitDriver)  return asynError;
      if (respLen <= 0)  break;

      if ((uint)respLen != respBuf.size()*sizeof(respBuf[0]))  continue;

      lastRespTime = chrono::system_clock::now();

      // check values common to all commands
      U32 pktIDSent = ntohl(cmdBuf[0]);  U32 pktIDRcvd = ntohl(respBuf[0]);
      U32 cmdSent = ntohl(cmdBuf[1]);          cmdRcvd = ntohl(respBuf[1]);

      if ((pktIDSent == pktIDRcvd) and (cmdSent == cmdRcvd))  {
        validResp = true;  break; }
    }

    if (flushedPkts)  {
      logMsgHdr("\n");
      cout << "*** flushed " << flushedPkts << " old packets ***" << endl;
    }

    // try sending the cmd again if we did't get a valid resp
    if (!validResp)  {
      checkComStatus();  this_thread::sleep_for(100ms);  continue; }

    U16 statOffset = LCPUtil::statusOffset((int16_t)cmdRcvd);
    U32 sessID_and_status = ntohl(respBuf[statOffset]);

    U32 respSessionID = (sessID_and_status >> 16) & 0xFFFF;
    U32 respStat = sessID_and_status & 0xFFFF;
    respStatus = static_cast<LCPStatus>(respStat);

    bool prevWriteAccess = writeAccess;

    writeAccess = (respSessionID == sessionID.get());

    if (prevWriteAccess != writeAccess)  {
      logMsgHdr("\n");
      if (writeAccess)
        cout << "=== " << portName << " now has write access ===" << endl;
      else
        cout << "*** " << portName << " lost write access ***" << endl;
    }

    return asynSuccess;
  }

  return asynError;
}


//----------------------------------------------------------------------------
//  Update the read state of the scalar ParamInfo objects
//----------------------------------------------------------------------------
asynStatus drvFGPDB::updateScalarReadValues()
{
  // For LCP regs: Read the latest values from the controller
  readRegs(0x10000, procGroupSize(ProcGroup_LCP_RO));
  readRegs(0x20000, procGroupSize(ProcGroup_LCP_WA));
  readRegs(0x30000, procGroupSize(ProcGroup_LCP_WO));

  lock_guard<drvFGPDB> asynLock(*this);

  //ToDo:  Efficiency Improvement:
  //       Use a list of the scalar params with an associated local variable
  //       (drvValue != null) to avoid having to scan the entire list each
  //       time.

  // For driver-only params: Read the latest value from a local variable
  for (auto &param : params)  {
    if (!param.drvValue or !param.isScalarParam())  continue;
    if (LCPUtil::isLCPRegParam(param.getRegAddr()))  continue;

    U32 newValue = *param.drvValue;

    if ((newValue == param.ctlrValRead)
      and (param.readState == ReadState::Current))  continue;

    param.ctlrValRead = newValue;
    param.readState = ReadState::Pending;
  }

  return asynSuccess;
}

//----------------------------------------------------------------------------
//  Update the asyn layer's copy of a parameter value
//----------------------------------------------------------------------------
asynStatus drvFGPDB::setAsynParamVal(int paramID)
{
  ParamInfo &param = params.at(paramID);
  asynStatus stat = asynError;
  double dval;


  switch (param.getAsynType())  {

    case asynParamInt32:
      stat = setIntegerParam(paramID, param.ctlrValRead);
      break;

    case asynParamUInt32Digital:
      stat = setUIntDigitalParam(paramID, param.ctlrValRead, 0xFFFFFFFF);
      break;

    case asynParamFloat64:
      dval = ParamInfo::ctlrFmtToDouble(param.ctlrValRead, param.getCtlrFmt());
      stat = setDoubleParam(paramID, dval);
      break;

    case asynParamInt8Array:
      setParamStatus(paramID, asynSuccess);  // req for doCallbacksXxxArray to work
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
// Read the controller's current values for one or more LCP registers
//----------------------------------------------------------------------------
asynStatus drvFGPDB::readRegs(U32 firstReg, uint numRegs)
{
  asynStatus stat;
  LCPStatus  respStatus;

  if (exitDriver)  return asynError;

  if (ShowRegReads())  {
    logMsgHdr("\n");
    cout << "=== " << portName << ": readRegs(0x" << hex << firstReg
         << ", " << dec << numRegs << ")" << endl;
  }

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

  lock_guard<drvFGPDB> asynLock(*this);

  for (uint u=0; u<numRegs; ++u,++offset)  {
    U32 justReadVal = ntohl(respBuf.at(RespHdrWords+u));

    int paramID = group.paramIDs.at(offset);
    if (!validParamID(paramID))  continue;

    ParamInfo &param = params.at(paramID);
    if ((justReadVal == param.ctlrValRead)
      and (param.readState == ReadState::Current))  continue;

    if (paramID == idUpSecs)  checkForRestart(justReadVal);

    param.newReadVal(justReadVal);
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

  if (ShowRegWrites())  {
    logMsgHdr("\n");
    cout << "=== " << portName << ":" << "writeRegs(0x" << hex << firstReg
         << ", " << dec << numRegs << ")" << endl;
  }

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
    lock_guard<drvFGPDB> asynLock(*this);
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

  lastWriteTime = chrono::system_clock::now();

  if (exitDriver)  return asynError;

  //todo:
  //  - Check cmd-specific header values in returned packet
  //  - Return the status value from the response pkt (change the return type
  //    for the function to LCPStatus?  Or return the rcvd status in another
  //    argument)? Consider what the caller really needs/wants to know:  Just
  //    whether or not the send/rcv worked vs it worked but the controller did
  //    or didn't return an error (especially given that a resp status of
  //    SUCCESS does NOT necessarily mean that all the values written were
  //    accepted as-is)

  offset = LCPUtil::addrOffset(firstReg);

  lock_guard<drvFGPDB> asynLock(*this);
  for (uint u=0; u<numRegs; ++u,++offset)  {
    int paramID = group.paramIDs.at(offset);
    if (!validParamID(paramID))  continue;
    ParamInfo &param = params.at(paramID);
    // in case setState was chgd by another thread
    if (param.setState == SetState::Processing)  {
      param.setState = SetState::Sent;
    }
  }

  writeAccessTimer.restart();  // reset timeout to avoid unnecessary callbacks

  return asynSuccess;
}


//=============================================================================
asynStatus drvFGPDB::getIntegerParam(int list, int index, int *value)
{
  if (ShowInit())  {
    logMsgHdr("\n");
    cout << "=== " << typeid(this).name() << "::"
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
    logMsgHdr("\n");
    cout << "=== " << typeid(this).name() << "::"
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
    logMsgHdr("\n");
    cout << "=== " << typeid(this).name() << "::"
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
  if (param.isReadOnly())  {
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

  if (initComplete or (resendMode == ResendMode::AfterIOCRestart))
    param.setState = SetState::Pending;
  else   if (resendMode == ResendMode::Never)
    param.setState = SetState::Sent;
  else
    param.setState = SetState::Restored;

  // Cause Pending writes to be processed ASAP
  if (initComplete)  scalarWritesTimer.wakeUp();

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

  if (ShowBlkErase())  {
    logMsgHdr("\n");
    cout << "=== eraseBlock(" << dec << chipNum << ", "
         << blockSize << ", " << blockNum << ")" << endl;
  }

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

  writeAccessTimer.restart();  // reset timeout to avoid unnecessary callbacks

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


  if (ShowBlkReads())  {
    logMsgHdr("\n");
    cout << "=== readBlock(" << dec << chipNum << ", "
         << blockSize << ", "  << blockNum << ", "
         << "buf[" << buf.size() << "])" << endl;
  }


  if (blockSize > buf.size())  return asynError;

  etherMTU = 1500;  //ToDo:  How to determine the ACTUAL MTU?


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

  if (ShowBlkWrites())  {
    logMsgHdr("\n");
    cout << "=== writeBlock(" << dec << chipNum << ", "
         << blockSize << ", "  << blockNum << ", "
         << "buf[" << buf.size() << "])" << endl;
  }

  if (buf.size() < blockSize)  return asynError;

  etherMTU = 1500;  //ToDo:  How to determine the ACTUAL MTU?


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

  writeAccessTimer.restart();  // reset timeout to avoid unnecessary callbacks

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Read next block of a PMEM array value from the controller
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::readNextBlock(ParamInfo &param)
{
  U32  ttl, arraySize;
  float  perc;


  SetState setState;
  {
    lock_guard<drvFGPDB> asynLock(*this);
    setState = param.setState;
  }
  // Wait if an unfinished write operation or inactive connection
  if ((setState == SetState::Pending) or (setState == SetState::Processing)
    or !connected)  return asynSuccess;

  if (!param.getBytesLeft())  {
    lock_guard<drvFGPDB> asynLock(*this);
    param.readState = ReadState::Pending;
    postNewReadingsTimer.wakeUp();
    return asynSuccess;
  }

  arrayReadsInProgress = true;

//--- initialize values used in the loop ---
  arraySize = param.arrayValRead.size();

  param.rwBuf.assign(param.getBlockSize(), 0);

  // adjust # of bytes to read from the next block if necessary
  if (param.getRWCount() > param.getBytesLeft())  param.setRWCount(param.getBytesLeft());

  // read the next block of bytes
  if (readBlock(param.getChipNum(), param.getBlockSize(), param.getBlockNum(), param.rwBuf))  {
    printf("*** error reading block %u ***\r\n", param.getBlockNum());
    perror("readBlock()");  return asynError;
  }

  lock_guard<drvFGPDB> asynLock(*this);

  // Copy just read data to the appropriate bytes in the buffer
  memcpy(param.arrayValRead.data() + param.getRWOffset(),
         param.rwBuf.data() + param.getDataOffset(),
         param.getRWCount());

  param.incrementBlockNum();  param.setDataOffset(0);  param.reduceBytesLeftBy(param.getRWCount());
  param.setRWOffset(param.getRWOffset() + param.getRWCount());
  param.setRWCount(param.getBlockSize());

  ttl = arraySize - param.getBytesLeft();  perc = (float)ttl / arraySize * 100.0;

  setArrayOperStatus(param, (uint32_t)perc);  // update the status param

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Send next block of a new array value to the controller
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::writeNextBlock(ParamInfo &param)
{
  U32  ttl, arraySize;
  float  perc;


  {
    lock_guard<drvFGPDB> asynLock(*this);

    if (!param.getBytesLeft())  {
      param.setState = SetState::Sent;
      initArrayReadback(param);  // readback what we just finished sending
      return asynSuccess;
    }

    if (!writeAccess)  return asynError;

    if (!connected)  return asynSuccess;  // wait if inactive connection

    arrayWritesInProgress = true;

    // stops writeXxxArray() funcs from making concurrent changes
    param.setState = SetState::Processing;
  }

  // initialize values used in the loop
  arraySize = param.arrayValSet.size();

  param.rwBuf.assign(param.getBlockSize(), 0);

  // adjust # of bytes to write to the next block if necessary
  if (param.getRWCount() > param.getBytesLeft())  param.setRWCount(param.getBytesLeft());

  // If not replacing all the bytes in the block, then read the existing
  // contents of the block to be modified.
  if (param.getRWCount() != param.getBlockSize())
    if (readBlock(param.getChipNum(), param.getBlockSize(), param.getBlockNum(), param.rwBuf))  {
      printf("*** error reading block %u ***\r\n", param.getBlockNum());
      perror("readBlock()");  /*rwSockMutex->unlock();*/
      return asynError;
    }

  // If required, 1st erase the next block to be written to
  if (param.getEraseReq())
    if (eraseBlock(param.getChipNum(), param.getBlockSize(), param.getBlockNum()))  {
      printf("*** error erasing block %u ***\r\n", param.getBlockNum());
      perror("eraseBlock()");  /*rwSockMutex->unlock();*/
      return asynError;
    }

  // Copy new data in to the appropriate bytes in the buffer
  memcpy(param.rwBuf.data() + param.getDataOffset(),
         param.arrayValSet.data() + param.getRWOffset(),
         param.getRWCount());

  // write the next block of bytes
  if (writeBlock(param.getChipNum(), param.getBlockSize(), param.getBlockNum(), param.rwBuf)) {
    printf("*** error writing block %u ***\r\n", param.getBlockNum());
    perror("writeBlock()");  /*rwSockMutex->unlock()*/;
    return asynError;
  }

  param.incrementBlockNum();  param.setDataOffset(0);  param.reduceBytesLeftBy(param.getRWCount());
  param.setRWOffset(param.getRWOffset() + param.getRWCount());
  param.setRWCount(param.getBlockSize());

  ttl = arraySize - param.getBytesLeft();  perc = (float)ttl / arraySize * 100.0;

  setArrayOperStatus(param, (uint32_t)perc);  // update the status param

  return asynSuccess;
}

//----------------------------------------------------------------------------
void drvFGPDB::initArrayReadback(ParamInfo &param)
{
  param.initBlockRW(param.arrayValRead.size());
  param.readState = ReadState::Update;

  arrayReadsTimer.restart();
}


//----------------------------------------------------------------------------
// Process a request to write to one of the Int32 parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeInt32(asynUser *pasynUser, epicsInt32 newVal)
{
  if (!isValidWritableParam(__func__, pasynUser))  return asynError;

  if (!acceptWrites())  return asynError;

  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  uint32_t setVal = ParamInfo::int32ToCtlrFmt(newVal, param.getCtlrFmt());

  applyNewParamSetting(param, setVal);

  if (ShowRegWrites())  {
    logMsgHdr("\n");
    cout << portName << ":" << __func__ << "(): "
         << newVal << " (0x" << hex << setVal << ")"
         << " to " << param.name << dec << endl;
  }

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s() [%s]:  paramID=%d, name=%s, value=%d\n",
            typeid(this).name(), __func__, portName,
            paramID, param.name.c_str(), newVal);

  return stat;
}


//----------------------------------------------------------------------------
// Process a request to write to one of the UInt32Digital parameters
//----------------------------------------------------------------------------
asynStatus drvFGPDB::writeUInt32Digital(asynUser *pasynUser, epicsUInt32 newVal,
                                        epicsUInt32 mask)
{
  if (!isValidWritableParam(__func__, pasynUser))  return asynError;

  if (!acceptWrites())  return asynError;

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

  if (ShowRegWrites())  {
    logMsgHdr("\n");
    cout << portName << ":" << __func__ << "(): "
         << " 0x" << hex << setVal
         << " (prev:0x" << prevVal
         << ", new:0x" << newVal
         << ", mask:0x" << mask << ")"
         << " to " << param.name << dec << endl;
  }

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

  if (!acceptWrites())  return asynError;

  asynStatus  stat = asynSuccess;
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  uint32_t setVal = ParamInfo::doubleToCtlrFmt(newVal, param.getCtlrFmt());

  applyNewParamSetting(param, setVal);

  if (ShowRegWrites())  {
    logMsgHdr("\n");
    cout << portName << ":" << __func__ << "(): "
         << newVal << " (0x" << hex << setVal << ")"
         << " to " << param.name << dec << endl;
  }

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
        logMsgHdr("\n");
        cout << portName << "::" << __func__ << "(): " << endl
             << "   *** invalid status parameter name: " << param << " ***" << endl;
        return asynError;
      }

  if (param.statusParamID >= 0)  {
    ParamInfo &statusParam = params.at(param.statusParamID);
    lock_guard<drvFGPDB> asynLock(*this);
    statusParam.ctlrValRead = percDone;
    statusParam.readState = ReadState::Pending;
    //postNewReadingsTimer.wakeUp();
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

  if (ShowBlkReads())  {
    logMsgHdr("\n");
    cout << portName << "::" << __func__ << "(): "
         << endl << "   read " << dec << nElements << " elements from: "
         << endl << "   " << param << endl;
  }

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
  int  paramID = pasynUser->reason;
  ParamInfo &param = params.at(paramID);

  if (!isValidWritableParam(__func__, pasynUser))  return asynError;

  if (!acceptWrites())  return asynError;

  asynStatus  stat = asynSuccess;

  if (ShowBlkWrites())  {
    logMsgHdr("\n");
    cout << portName << "::" << __func__ << "(): "
         << endl << "   write " << dec << nElements << " elements to: "
         << endl << "   " << param << endl;
  }

  if (!param.isArrayParam())  return asynError;

  if ((param.setState == SetState::Pending) or
      (param.setState == SetState::Processing))  return asynError;

  setArrayOperStatus(param, 0);  // init the status param

  param.arrayValSet.assign(&values[0], &values[nElements]);

  param.initBlockRW(param.arrayValSet.size());
  param.setState = SetState::Pending;

  asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s() [%s]:  paramID=%d, name=%s, nElements=%lu\n",
            typeid(this).name(), __func__, portName,
            paramID, param.name.c_str(), (ulong)nElements);

  arrayWritesTimer.wakeUp();

  return stat;
}

//-----------------------------------------------------------------------------

