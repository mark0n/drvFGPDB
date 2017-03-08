#include <iostream>

#include <asynPortDriver.h>

#include "LCPProtocol.h"

#include "ParamInfo.h"

using namespace std;


const std::map<std::string, asynParamType> ParamInfo::asynTypes = {
  { "Int32",         asynParamInt32         },
  { "UInt32Digital", asynParamUInt32Digital },
  { "Float64",       asynParamFloat64       },
  { "Octet",         asynParamOctet         }
};

const std::map<std::string, CtlrDataFmt> ParamInfo::ctlrFmts = {
  { "S32",    CtlrDataFmt::S32    },
  { "U32",    CtlrDataFmt::U32    },
  { "F32",    CtlrDataFmt::F32    },
  { "U16_16", CtlrDataFmt::U16_16 }
};

//-----------------------------------------------------------------------------
// Construct a ParamInfo object from a string description of the form:
//
//  name addr asynType ctlrFmt
//-----------------------------------------------------------------------------
ParamInfo::ParamInfo(const string& paramStr, const string& portName)
         : ParamInfo()
{
  if (!regex_match(paramStr, generateParamStrRegex()))  {
    cout << "*** Param def error: Device: " << portName << " ***" << endl
         << "[" << paramStr << "]" << endl;
    throw invalid_argument("Invalid parameter definition.");
  }

  stringstream paramStream(paramStr);
  string asynTypeName, ctlrFmtName;

  paramStream >> name
              >> hex >> regAddr
              >> asynTypeName
              >> ctlrFmtName;

  asynType = strToAsynType(asynTypeName);
  ctlrFmt = strToCtlrFmt(ctlrFmtName);
  readOnly = LCPUtil::readOnlyAddr(regAddr);
  ctlrValSet = 0;
  setState = SetState::Undefined;
}

//-----------------------------------------------------------------------------
// Generate a regex for basic validation of strings that define a parameter
//-----------------------------------------------------------------------------
const regex& ParamInfo::generateParamStrRegex()
{
  const string paramName    = "\\w+";
  const string whiteSpaces  = "\\s+";
  const string address      = "0x[0-9a-fA-F]+";
  const string asynType     = "(" + joinMapKeys(asynTypes, "|") + ")";
  const string ctlrFmt      = "(" + joinMapKeys(ctlrFmts,  "|") + ")";
  const string optionalPart = "(" + whiteSpaces + address
                                  + whiteSpaces + asynType
                                  + "(" + whiteSpaces + ctlrFmt + ")?)?";
  static const regex re(paramName + optionalPart);
  return re;
}

//-----------------------------------------------------------------------------
//  Return the asynParamType associated with a string
//-----------------------------------------------------------------------------
asynParamType ParamInfo::strToAsynType(const string &typeName)
{
  auto it = asynTypes.find(typeName);
  return it == asynTypes.end() ? asynParamNotDefined : it->second;
}

//-----------------------------------------------------------------------------
//  Return the CtlrDataFmt associated with a string
//-----------------------------------------------------------------------------
CtlrDataFmt ParamInfo::strToCtlrFmt(const string &fmtName)
{
  auto it = ctlrFmts.find(fmtName);
  return it == ctlrFmts.end() ? CtlrDataFmt::NotDefined : it->second;
}


