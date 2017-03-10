#ifndef PARAMINFO_H
#define PARAMINFO_H

#include <regex>
#include <map>

#include <asynPortDriver.h>

// Definition of the protocol for FGPDB devices.

enum class CtlrDataFmt {
  NotDefined,
  S32,       // signed 32-bit int
  U32,       // unsigned 32-bit int
  F32,       // 32-bit float
  U16_16,    // = (uint) (value * 2.0^16)
  PHASE      // = (int) (degrees * (2.0^32) / 360.0)
};

enum class SetState {
  Undefined,  // no value written to the parameter yet
  Pending,    // new value waiting to be sent
  Sent        // ctlr ack'd write cmd (but not necessarily the new value)
};

enum class ReadState {
  Undefined,  // no value read from ctlr yet
  Current,    // value recently read from controller
};


/* For probable future use
//===== values for StaticRegInfo.syncMode =====
//
//  These values are used to specify what should be done for each writable
//  value when the IOC or the Ctlr has been restarted.
//
//  SM_DN:
//        Don't do anything special to the set value when resyncing the IOC and ctlr states.
//
//  SM_EQ:
//        These are values that, once the IOC has successfully sent the value
//        to the ctlr, should always be the same on the IOC and the ctlr. This
//        means that, if one of them is restarted after they were in sync, the
//        values can be restored from the one that did not restart.
//
//  SM_CM:
//        These are values that must be "reset" whenever the ctlr is restarted.
//        In this case, "reset" means changing the IOC's value to match the
//        ctlrs.  This is needed for values that control the things like
//        on/off, enabled/disabled, etc. where we do NOT want the previous
//        state to be reasserted automatically without human intervention when
//        a ctlr is restarted.
//
//  SM_IM:
//        These are values for which the readback from the controller may not
//        match the last setting.  For example, the IOC sends a new value for a
//        setpoint and the readback value indicates the currently "applied"
//        setpoint as it ramps toward the specified setpoint.  For these
//        values, the IOC copy cannot be (reliably) restored from the ctlr, so
//        a restarted IOC must restore the most recently saved value from the
//        persistent data files.
//
//  Of course, there is no way to guarantee that last saved value is equal to
//  the most recent user-supplied setpoint, but updating the saved copy
//  frequently will minimize the possiblity for a mismatch.
//----------------------------------------------
enum class SyncMode {
  NotDefined,
  SM_DN,   // Do Nothing (just leave default startup values)
  SM_EQ,   // IOC and ctlr values should be EQual
  SM_CM,   // Ctlr Master:  If ctlr restarted, use ctlr value
  SM_IM,   // IOC Master:  If IOC restarted, restore from file
};
*/

//-----------------------------------------------------------------------------
// Information the driver keeps about each parameter.  This list is generated
// during IOC startup from the data in the INP/OUT fields in the EPICS records
// that are linked to these parameters.
//-----------------------------------------------------------------------------
class ParamInfo {
  public:
    ParamInfo() :
      regAddr(0),
      asynType(asynParamNotDefined),
      ctlrFmt(CtlrDataFmt::NotDefined),
      readOnly(true),
      ctlrValSet(0),
      setState(SetState::Undefined),
      ctlrValRead(0),
      readState(ReadState::Undefined)
    {};

    ParamInfo(const ParamInfo &info) :
      name(info.name),
      regAddr(info.regAddr),
      asynType(info.asynType),
      ctlrFmt(info.ctlrFmt),
      readOnly(info.readOnly),
      ctlrValSet(info.ctlrValSet),
      setState(info.setState),
      ctlrValRead(info.ctlrValRead),
      readState(info.readState)
    {};

    ParamInfo(const std::string& paramStr, const std::string& portName);


    static asynParamType strToAsynType(const std::string &typeName);

    static CtlrDataFmt strToCtlrFmt(const std::string &fmtName);


    std::string    name;
    uint           regAddr;     // LCP reg addr or driver param group
    asynParamType  asynType;    // format of value used by driver
    CtlrDataFmt    ctlrFmt;     // format of value sent to/read from controller
  //SyncMode       syncMode;    // relation between set and read values

    bool           readOnly;    // clients cannot write to the value

    epicsUInt32    ctlrValSet;  // value to write to ctlr (in ctlr fmt, host byte order)
    SetState       setState;    // state of ctlrValSet

    epicsUInt32    ctlrValRead; // most recently read value (in ctlr fmt, host byte order)
    ReadState      readState;   // state of ctlrValRead

#ifndef TEST_DRVFGPDB
  private:
#endif

    static const std::regex& generateParamStrRegex();

    static const std::map<std::string, asynParamType> asynTypes;
    static const std::map<std::string, CtlrDataFmt> ctlrFmts;
};


//-----------------------------------------------------------------------------
// Return a string with the set of key values from a map
//-----------------------------------------------------------------------------
template <typename T>
std::string joinMapKeys(const std::map<std::string, T>& map,
                        const std::string& separator) {
  std::string joinedKeys;
  bool first = true;
  for(auto const& x : map) {
    if(!first)  joinedKeys += separator;
    first = false;
    joinedKeys += x.first;
  }
  return joinedKeys;
}

#endif // PARAMINFO_H