#ifndef PARAMINFO_H
#define PARAMINFO_H

#include <regex>
#include <map>
#include <iostream>

#include <asynPortDriver.h>

// Definition of the protocol for FGPDB devices.

enum class CtlrDataFmt {
  NotDefined,
  S32,       // signed 32-bit int
  U32,       // unsigned 32-bit int
  F32,       // 32-bit float
  U16_16,    // = (uint) (value * 2.0^16)
};

// Be sure to update ParamInfo::setStates map in ParamInfo.cpp
enum class SetState {
  Undefined,  // no value written to the parameter yet
  Pending,    // new setting ready to be processed
  Processing, // in the middle of processing a write
  Sent        // ack'd by ctlr or driver-only value updated
};

// Be sure to update ParamInfo::readStates map in ParamInfo.cpp
enum class ReadState {
  Undefined,  // no value read from ctlr yet
  Pending,    // new reading ready to be posted
  Current,    // most recent value posted to asyn layer
};

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
      readState(ReadState::Undefined),
      drvValue(nullptr),
      chipNum(0),
      blockSize(0),
      eraseReq(false),
      offset(0),
      length(0),
      statusParamID(-1)
    {};

    ParamInfo(const std::string& paramStr, const std::string& portName);


    asynStatus updateParamDef(const std::string &context,
                              const ParamInfo &newParam);


    static asynParamType strToAsynType(const std::string &typeName);
    static CtlrDataFmt strToCtlrFmt(const std::string &fmtName);

    static const std::string & asynTypeToStr(const asynParamType asynType);
    static const std::string & ctlrFmtToStr(const CtlrDataFmt ctlrFmt);

    const std::string & setStateToStr(void);
    const std::string & readStateToStr(void);

    static double ctlrFmtToDouble(uint32_t ctlrVal, CtlrDataFmt ctlrFmt);

    static uint32_t doubleToCtlrFmt(double dval, CtlrDataFmt ctlrFmt);
    static uint32_t int32ToCtlrFmt(int32_t ival, CtlrDataFmt ctlrFmt);

    friend std::ostream& operator<<(std::ostream& os, const ParamInfo &param);

    bool isScalarParam()  {
      return (asynType == asynParamInt32)
          or (asynType == asynParamUInt32Digital)
          or (asynType == asynParamFloat64);
    }



    std::string    name;

    uint           regAddr;     // LCP reg addr or driver param group

    asynParamType  asynType;    // format used by asyn interface
    CtlrDataFmt    ctlrFmt;     // format of run-time value in ctlr or driver

    bool           readOnly;    // clients cannot write to the value

    epicsUInt32    ctlrValSet;  // value to write to ctlr (in ctlr fmt, host byte order)
    SetState       setState;    // state of ctlrValSet

    epicsUInt32    ctlrValRead; // most recently read value (in ctlr fmt, host byte order)
    ReadState      readState;   // state of ctlrValRead

    uint32_t      *drvValue;    // run-time value for driver-only scalar params

    // properties for pmem (array) parameters
    std::vector<uint8_t> arrayValSet;
    std::vector<uint8_t> arrayValRead;

    uint           chipNum;
    ulong          blockSize;
    bool           eraseReq;
    ulong          offset;
    ulong          length;

    std::string    statusParamName;
    int            statusParamID;

    // state data for in-progress write of an array value
    uint32_t       fromOffset;
    uint32_t       blockNum;
    uint32_t       dataOffset;
    uint32_t       bytesLeft;
    uint           wrCount;

    std::vector<uint8_t> rwBuf;

#ifndef TEST_DRVFGPDB
  private:
#endif

    static const std::regex& scalarParamDefRegex();
    static const std::regex& pmemParamDefRegex();

    static const std::map<std::string, asynParamType> asynTypes;
    static const std::map<std::string, CtlrDataFmt> ctlrFmts;
    static const std::map<SetState, std::string> setStates;
    static const std::map<ReadState, std::string> readStates;
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
