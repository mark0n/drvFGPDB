#include <iostream>

#include <asynPortDriver.h>

#include "LCPProtocol.h"

#include "ParamInfo.h"

using namespace std;


const std::map<std::string, asynParamType> ParamInfo::asynTypes = {
  { "Int32",         asynParamInt32         },
  { "UInt32Digital", asynParamUInt32Digital },
  { "Float64",       asynParamFloat64       },
  { "Octet",         asynParamOctet         },
  { "Int8Array",     asynParamInt8Array     }
};

//NotDefined: Init value of every param's ctlrFmt and value of all driver-only param's ctlrFmt
const std::map<std::string, CtlrDataFmt> ParamInfo::ctlrFmts = {
  { "NotDefined", CtlrDataFmt::NotDefined},
  { "S32",        CtlrDataFmt::S32       },
  { "U32",        CtlrDataFmt::U32       },
  { "F32",        CtlrDataFmt::F32       },
  { "U16_16",     CtlrDataFmt::U16_16    }
};

const std::unordered_map<SetState, std::string, EnumClassHash> ParamInfo::setStates = {
  { SetState::Undefined,  "Undefined"  },
  { SetState::Restored,   "Restored"   },
  { SetState::Pending,    "Pending"    },
  { SetState::Processing, "Processing" },
  { SetState::Sent,       "Sent"       },
  { SetState::Error,      "Error"      }
};

const std::unordered_map<ReadState, std::string, EnumClassHash> ParamInfo::readStates = {
  { ReadState::Undefined, "Undefined" },
  { ReadState::Pending,   "Pending"   },
  { ReadState::Update,    "Update"    },
  { ReadState::Current,   "Current"   }
};

static string NotDefined("<NotDefined>");

//-----------------------------------------------------------------------------
// Construct a ParamInfo object from a string description of the form:
//
//  name addr asynType ctlrFmt
//    OR
//  name addr chipID blockSize eraseReq offset len readStatusParam writeStatusParam
//-----------------------------------------------------------------------------
ParamInfo::ParamInfo(const string& paramStr)
         : regAddr(0),
           asynType(asynParamNotDefined),
           ctlrFmt(CtlrDataFmt::NotDefined),
           chipNum(0),
           blockSize(0),
           eraseReq(false),
           offset(0),
           length(0),
           rwOffset(0),
           blockNum(0),
           dataOffset(0),
           bytesLeft(0),
           rwCount(0),
           setState(SetState::Undefined),
           readState(ReadState::Undefined),
           ctlrValSet(0),
           ctlrValRead(0),
           drvValue(nullptr),
           rdStatusParamID(-1),
           wrStatusParamID(-1)
{
  stringstream paramStream(paramStr);

  if (regex_match(paramStr, scalarParamDefRegex())) {
    string asynTypeName, ctlrFmtName;
    paramStream >> name
                >> hex >> regAddr
                >> asynTypeName
                >> ctlrFmtName;
    asynType = strToAsynType(asynTypeName);
    ctlrFmt = strToCtlrFmt(ctlrFmtName);
    m_readOnly = LCPUtil::readOnlyAddr(regAddr);
  } else if (regex_match(paramStr, pmemParamDefRegex())) {
    string eraseReqStr;
    paramStream >> name
                >> hex >> regAddr
                >> dec >> chipNum
                >> blockSize
                >> eraseReqStr
                >> hex >> offset >> length
                >> rdStatusParamName
                >> wrStatusParamName;
    eraseReq = (eraseReqStr.at(0) == 'Y');
    asynType = asynParamInt8Array;
    m_readOnly = LCPUtil::readOnlyAddr(regAddr);
    arrayValRead.assign(length, 0);
    initBlockRW(arrayValRead.size());
    readState = ReadState::Update;
  } else {
    throw invalid_argument("Invalid parameter definition string \"" + paramStr + "\"");
  }
}


//-----------------------------------------------------------------------------
void ParamInfo::initBlockRW(uint32_t ttlNumBytes)
{
  if (!ttlNumBytes or !blockSize)  return;

  rwOffset = 0;
  blockNum = offset / blockSize;
  dataOffset = offset - blockNum * blockSize;
  bytesLeft = ttlNumBytes;
  rwCount = blockSize - dataOffset;
}


//-----------------------------------------------------------------------------
// Generate a regex for basic validation of strings that define a parameter for
// a scalar value.
//-----------------------------------------------------------------------------
const regex& ParamInfo::scalarParamDefRegex()
{
  const string paramName    = "\\w+";

  const string whiteSpaces  = "\\s+";

  const string address      = "0x[0-9a-fA-F]+";
  const string asynType     = "(" + joinMapKeys(asynTypes, "|") + ")";
  const string ctlrFmt      = "(" + joinMapKeys(ctlrFmts,  "|") + ")";

  const string optionalPart = "(" + whiteSpaces + address
                                  + whiteSpaces + asynType
                                  + "(" + whiteSpaces + ctlrFmt + ")?"
                            + ")?";

  static const regex re(paramName + optionalPart);

  return re;
}

//-----------------------------------------------------------------------------
// Generate a regex for basic validation of strings that define a parameter for
// a PMEM (persistent memory) value.
//-----------------------------------------------------------------------------
const regex& ParamInfo::pmemParamDefRegex()
{
  const string paramName    = "\\w+";

  const string whiteSpaces  = "\\s+";

  const string address      = "0x[0-9a-fA-F]+";

  const string chipNum      = "[1-9]";
  const string blockSize    = "[1-9][0-9]+";

  const string eraseReq     = "[YN]";

  const string offset       = "0x[0-9a-fA-F]+";
  const string length       = "0x[0-9a-fA-F]+";

  const string rdStatusParamName = "\\w+";
  const string wrStatusParamName = "\\w+";


  const string pmemArrayRegExStr = paramName
                                 + whiteSpaces + address
                                 + whiteSpaces + chipNum
                                 + whiteSpaces + blockSize
                                 + whiteSpaces + eraseReq
                                 + whiteSpaces + offset
                                 + whiteSpaces + length
                                 + whiteSpaces + rdStatusParamName
                                 + whiteSpaces + wrStatusParamName;

  static const regex re(pmemArrayRegExStr);

  return re;
}

//-----------------------------------------------------------------------------
//  Return a string definition for a parameter
//-----------------------------------------------------------------------------
ostream& operator<<(ostream& os, const ParamInfo &param)
{
  os << param.name << " 0x" << hex << param.regAddr;

  if (param.blockSize)
    os << dec
       << " " << param.chipNum
       << " " << param.blockSize
       << " " << param.eraseReq
       << hex
       << " 0x" << param.offset
       << " 0x" << param.length
       << dec
       << " " << param.rdStatusParamName
       << " " << param.wrStatusParamName;
  else
    os << " " << ParamInfo::asynTypeToStr(param.asynType)
       << " " << ParamInfo::ctlrFmtToStr(param.ctlrFmt);

  return os;
}

//-----------------------------------------------------------------------------
void ParamInfo::newReadVal(uint32_t newVal)
{
  ctlrValRead = newVal;
  readState = ReadState::Pending;

  if (drvValue)  *drvValue = newVal;
}

//-----------------------------------------------------------------------------
int ParamInfo::getStatusParamID(void)
{
  if (activePMEMwrite())  return wrStatusParamID;
  if (activePMEMread())   return rdStatusParamID;
  return -1;
}

//-----------------------------------------------------------------------------
uint32_t ParamInfo::getArraySize(void)
{
  if ((setState == SetState::Pending) or (setState == SetState::Processing))
    return arrayValSet.size();

  if ((readState == ReadState::Update) or (readState == ReadState::Pending))
    return arrayValRead.size();

  return 0;
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
const string & ParamInfo::setStateToStr(void) const
{
  return setStates.at(setState);
}

//-----------------------------------------------------------------------------
const string & ParamInfo::readStateToStr(void) const
{
  return readStates.at(readState);
}

//-----------------------------------------------------------------------------
template <typename T>
std::pair<asynStatus,paramDefState> updateProp(T &curVal, const T &newVal, T notDefined, const paramDefState paramDefSt)
{
  paramDefState newparamDefSt = paramDefSt;

  //NOTE: defined followed by not defined is valid
  if (newVal == notDefined)  return std::make_pair(asynSuccess,newparamDefSt);

  // First "defined" value for this property?
  if (curVal == notDefined)  {
    curVal = newVal;
    newparamDefSt = paramDefState::Updated;
    return std::make_pair(asynSuccess,newparamDefSt);
  }

  // curVal and newVal both defined, so they must be equal
  if (newVal != curVal)  return std::make_pair(asynError,newparamDefSt);

  return std::make_pair(asynSuccess,newparamDefSt);

}

//-----------------------------------------------------------------------------
//  Update the properties for an existing parameter.
//
//  Checks for conflicts and updates any missing property values using ones
//  from the new set of properties.
//-----------------------------------------------------------------------------
paramDefState ParamInfo::updateParamDef(const string &context,
                                        const ParamInfo &newParam)
{
  if (name != newParam.name)
    throw invalid_argument("Invalid parameter definition: No param name "
                           "provided");

  asynStatus stat = asynSuccess;
  paramDefState paramDef = paramDefState::NotUpdated;
  bool conflict = false;

  std::tie(stat, paramDef) = updateProp(regAddr, newParam.regAddr,
                                        static_cast<uint32_t>(0), paramDef);
  conflict |= (stat != asynSuccess);

  std::tie(stat, paramDef) = updateProp(asynType, newParam.asynType,
                                        asynParamNotDefined, paramDef);
  conflict |= (stat != asynSuccess);

  std::tie(stat, paramDef) = updateProp(ctlrFmt, newParam.ctlrFmt,
                                        CtlrDataFmt::NotDefined, paramDef);
  conflict |= (stat != asynSuccess);

  m_readOnly = LCPUtil::readOnlyAddr(regAddr);

  if (conflict) {
    cout << endl << "*** " << context << ":" << name << ": "
         "Conflicting parameter definitions ***" << endl
         << "  cur: " << *this << endl
         << "  new: " << newParam << endl;
    throw invalid_argument("Invalid parameter definition");
  }

  return paramDef;

}

//----------------------------------------------------------------------------
//  Convert a value from the format used by the controller to a float
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
  }

  return dval;
}

//----------------------------------------------------------------------------
//  Convert a value from a floating point to the format used by the controller
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
  }

  return ctlrVal;
}

//----------------------------------------------------------------------------
//  Convert a value from an int32 to the format used by the controller
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
  }

  return ctlrVal;
}

//----------------------------------------------------------------------------

