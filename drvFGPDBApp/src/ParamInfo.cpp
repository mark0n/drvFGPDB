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

const std::map<SetState, std::string> ParamInfo::setStates = {
  { SetState::Undefined,  "Undefined"  },
  { SetState::Pending,    "Pending"    },
  { SetState::Processing, "Processing" },
  { SetState::Sent,       "Sent"       },
  { SetState::Current,    "Current"    }
};

const std::map<ReadState, std::string> ParamInfo::readStates = {
  { ReadState::Undefined, "Undefined" },
  { ReadState::Current,   "Current"   }
};


static string NotDefined("<NotDefined>");

static const double PhaseConvFactor32 = 83.8190317e-9; // ~ 360 / 2^32

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
    return;  // use values set by default constructor
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
asynParamType ParamInfo::strToAsynType(const string &typeName)
{
  auto it = asynTypes.find(typeName);
  return it == asynTypes.end() ? asynParamNotDefined : it->second;
}

//-----------------------------------------------------------------------------
CtlrDataFmt ParamInfo::strToCtlrFmt(const string &fmtName)
{
  auto it = ctlrFmts.find(fmtName);
  return it == ctlrFmts.end() ? CtlrDataFmt::NotDefined : it->second;
}

//-----------------------------------------------------------------------------
const string & ParamInfo::asynTypeToStr(const asynParamType asynType)
{
  for (auto& x: asynTypes)  if (x.second == asynType)  return x.first;

  return NotDefined;
}

//-----------------------------------------------------------------------------
const string & ParamInfo::ctlrFmtToStr(const CtlrDataFmt ctlrFmt)
{
  for (auto& x: ctlrFmts)  if (x.second == ctlrFmt)  return x.first;

  return NotDefined;
}

//-----------------------------------------------------------------------------
const string & ParamInfo::setStateToStr(void)
{
  return setStates.at(setState);
}

//-----------------------------------------------------------------------------
const string & ParamInfo::readStateToStr(void)
{
  return readStates.at(readState);
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
template <typename T>
asynStatus updateProp(T &curVal, const T &newVal, T notDefined)
{
  if (curVal == notDefined)  { curVal = newVal;  return asynSuccess; }
  if (newVal == notDefined)  return asynSuccess;
  if (newVal != curVal)  return asynError;

  return asynSuccess;
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
  if (name != newParam.name)  return asynError;

  bool conflict = false;

  conflict |= (updateProp(regAddr, newParam.regAddr,
                          (uint32_t)0) != asynSuccess);

  conflict |= (updateProp(asynType, newParam.asynType,
                          asynParamNotDefined) != asynSuccess);

  conflict |= (updateProp(ctlrFmt, newParam.ctlrFmt,
                 CtlrDataFmt::NotDefined) != asynSuccess);

//conflict |= (updateProp(syncMode, newParam.syncMode,
//               SyncMode::NotDefined) != asynSuccess);

  if (!conflict)  return asynSuccess;

  conflictingParamDefs(context, *this, newParam);
  return asynError;
}

//----------------------------------------------------------------------------
//  Convert a value from the format used the controller to a float
//----------------------------------------------------------------------------
double ParamInfo::ctlrFmtToDouble(uint32_t ctlrVal, CtlrDataFmt ctlrFmt)
{
  float dval = 0.0;

  switch (ctlrFmt)  {
    case CtlrDataFmt::NotDefined:
      break;

    case CtlrDataFmt::S32:
      dval = (int32_t)ctlrVal;  break;

    case CtlrDataFmt::U32:
      dval = ctlrVal;  break;

    case CtlrDataFmt::F32:
      dval = *((epicsFloat32 *)&ctlrVal);  break;

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
uint32_t ParamInfo::doubleToCtlrFmt(double dval, CtlrDataFmt ctlrFmt)
{
  uint32_t  ctlrVal = 0;
  epicsFloat32  f32val = 0.0;

  switch (ctlrFmt)  {
    case CtlrDataFmt::NotDefined:  break;

    case CtlrDataFmt::S32:
      ctlrVal = (int32_t)dval;  break;

    case CtlrDataFmt::U32:
      ctlrVal = (uint32_t)dval;  break;

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
//  Convert a value from a floating point to the format used the controller
//----------------------------------------------------------------------------
uint32_t ParamInfo::int32ToCtlrFmt(int32_t ival, CtlrDataFmt ctlrFmt)
{
  uint32_t  ctlrVal = 0;
  epicsFloat32  f32val = 0.0;

  switch (ctlrFmt)  {
    case CtlrDataFmt::NotDefined:  break;

    case CtlrDataFmt::S32:
      ctlrVal = ival;  break;

    case CtlrDataFmt::U32:
      ctlrVal = (uint32_t)ival;  break;

    case CtlrDataFmt::F32:
      f32val = (float)ival;  ctlrVal = *((uint32_t *)&f32val);  break;

    case CtlrDataFmt::U16_16:
      ctlrVal = (uint32_t) ((double)ival * 65536.0);  break;

    case CtlrDataFmt::PHASE:
      ctlrVal = (uint32_t) ((double)ival / PhaseConvFactor32);  break;
  }

  return ctlrVal;
}

//----------------------------------------------------------------------------

