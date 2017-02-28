#ifndef PARAMINFO_H
#define PARAMINFO_H

#include <regex>

#include <asynPortDriver.h>

#include "FGPDBProtocol.h"

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
      group(ParamGroup::Invalid)  {};

    ParamInfo(const ParamInfo &info) :
      name(info.name),
      regAddr(info.regAddr),
      asynType(info.asynType),
      ctlrFmt(info.ctlrFmt),
      group(info.group)   {};

    ParamInfo(const std::string& paramStr, const std::string& portName);


    static asynParamType strToAsynType(const std::string &typeName);

    static CtlrDataFmt strToCtlrFmt(const std::string &fmtName);

    static ParamGroup regAddrToParamGroup(const uint regAddr);


    std::string    name;
    uint           regAddr;    // LCP reg addr or driver param group
    asynParamType  asynType;   // format of value used by driver
    CtlrDataFmt    ctlrFmt;    // format of value sent to/read from controller
  //SyncMode       syncMode;   // relation between set and read values

    ParamGroup     group;      // what processing group does param belong to

  private:
    static std::regex generateParamStrRegex();

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