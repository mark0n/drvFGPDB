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
  { "U16_16", CtlrDataFmt::U16_16 },
  { "PHASE",  CtlrDataFmt::PHASE  }
};


static string NotDefined("<NotDefined>");


//-----------------------------------------------------------------------------
// Construct a ParamInfo object from a string description of the form:
//
//  name addr asynType ctlrFmt
//-----------------------------------------------------------------------------
ParamInfo::ParamInfo(const string& paramStr, const string& portName)
         : ParamInfo()
{
  if (!regex_match(paramStr, generateParamStrRegex()))  {
    cout << endl
         << "*** Param def error: Device: " << portName << " ***" << endl
         << "    [" << paramStr << "]" << endl;
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
//  Return a string definition for a parameter
//-----------------------------------------------------------------------------
ostream& operator<<(ostream& os, const ParamInfo &param)
{
  os << param.name << " 0x" << hex << param.regAddr << " "
     << ParamInfo::asynTypeToStr(param.asynType) << " "
     << ParamInfo::ctlrFmtToStr(param.ctlrFmt);
  return os;
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

//-----------------------------------------------------------------------------
//  Return the string associated with an asynParamType
//-----------------------------------------------------------------------------
const string & ParamInfo::asynTypeToStr(const asynParamType asynType)
{
  for (auto& x: asynTypes)  if (x.second == asynType)  return x.first;

  return NotDefined;
}

//-----------------------------------------------------------------------------
//  Return the string associated with a ClrDataFmt
//-----------------------------------------------------------------------------
const string & ParamInfo::ctlrFmtToStr(const CtlrDataFmt ctlrFmt)
{
  for (auto& x: ctlrFmts)  if (x.second == ctlrFmt)  return x.first;

  return NotDefined;
}

//-----------------------------------------------------------------------------
void conflictingParamDefs(const string &context,
                          const ParamInfo &curDef, const ParamInfo &newDef)
{
  cout << "*** " << context << ":" << curDef.name << ": "
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
asynStatus ParamInfo::updateParamDef(const string &context,
                                     const ParamInfo &newParam)
{
  cout << endl  //tdebug
       << "  update: " << this << endl  //tdebug
       << "   using: " << newParam << endl;  //tdebug

  if (name != newParam.name)  return asynError;

#define UpdateProp(prop, NotDef)  \
  if (prop == (NotDef))  \
    prop = newParam.prop;  \
  else  \
    if ((newParam.prop != (NotDef)) and (prop != newParam.prop))  { \
      conflictingParamDefs(context, *this, newParam);  \
      return asynError; \
    }

  UpdateProp(regAddr,  0);
  UpdateProp(asynType, asynParamNotDefined);
  UpdateProp(ctlrFmt,  CtlrDataFmt::NotDefined);
//UpdateProp(syncMode, SyncMode::NotDefined);

  return asynSuccess;
}


