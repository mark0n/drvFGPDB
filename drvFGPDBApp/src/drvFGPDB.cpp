
#include "drvFGPDB.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>


using namespace std;


//-----------------------------------------------------------------------------
const std::unordered_map<std::string, asynParamType> ParamInfo::asynTypes = {
  { "Int32",         asynParamInt32         },
  { "UInt32Digital", asynParamUInt32Digital },
  { "Float64",       asynParamFloat64       },
  { "Octet",         asynParamOctet         }
};

const std::unordered_map<std::string, CtlrDataFmt> ParamInfo::ctlrFmts = {
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
ParamInfo::ParamInfo(const string& paramStr) : ParamInfo()
{
  if (!regex_match(paramStr, generateParamStrRegex()))  return;

  stringstream paramStream(paramStr);
  string asynTypeName, ctlrFmtName;

  paramStream >> this->name
              >> hex >> this->regAddr
              >> asynTypeName
              >> ctlrFmtName;

  this->asynType = strToAsynType(asynTypeName);
  this->ctlrFmt = strToCtlrFmt(ctlrFmtName);
}

//-----------------------------------------------------------------------------
// Return a string with the set of key values from an unordered_map
//-----------------------------------------------------------------------------
template <typename T>
string joinMapKeys(const unordered_map<string, T>& map,
                              const string& separator)
{
  string joinedKeys;
  bool first = true;
  for (auto const& x : map)  {
    if (!first) joinedKeys += separator;
    first = false;
    joinedKeys += x.first;
  }
  return joinedKeys;
}

//-----------------------------------------------------------------------------
// Generate a regex for validating strings that define a parameter
//-----------------------------------------------------------------------------
regex & ParamInfo::generateParamStrRegex()
{
  static std::regex  paramStrRegex("");

  if (paramStrRegex.mark_count() < 2)  {
    string asynTypeNames = joinMapKeys(asynTypes, "|");
    string ctlrFmtNames  = joinMapKeys(ctlrFmts,  "|");
    paramStrRegex = regex(
          "\\w+("                        // param name
          "\\s+0x[0-9a-fA-F]+"           // addr or param group
          "\\s+(" + asynTypeNames + ")"  // asyn data type
          "\\s+(" + ctlrFmtNames + ")"   // ctlr data format
          ")*" );
  }

  return paramStrRegex;
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


//=============================================================================
drvFGPDB::drvFGPDB(const string &drvPortName) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, MaxParams, InterfaceMask,
                   InterruptMask, AsynFlags, AutoConnect, Priority, StackSize),
    numParams(0)
{
  initHookRegister(drvFGPDB_initHookFunc);

  drvList.push_back(this);
}

//-----------------------------------------------------------------------------
//  Search the driver's list of parameters for an entry with the given name.
//  Note that, unlike asynPortDriver::findParam(), this function works during
//  IOC startup, before the actual asyn parameters are generated from the
//  driver's list.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::findParamByName(const string &name, int *paramID)
{
  for (int i=0; i<numParams; ++i)
    if (paramList[i].name == name)  { *paramID = i;  return (asynSuccess); }

  return asynError;
}

//-----------------------------------------------------------------------------
//  Return a copy of the ParamInfo struct for a parameter
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::getParamInfo(int paramID, ParamInfo &paramInfo)
{
  if ((uint)paramID >= (uint)numParams)  return asynError;

  paramInfo = paramList[paramID];  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Update the properties for an existing parameter.
//
//  Checks for conflicts and updates any missing property values using ones
//  from the new set of properties.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::updateParam(int paramID, const ParamInfo &newParam)
{
//  cout << "updateParam(" << paramID << ", ...)" << endl;

  if ((uint)paramID >= (uint)MaxParams)  return asynError;  //msg

  ParamInfo  &curParam = paramList[paramID];

  if (curParam.name != newParam.name)  return asynError;

#define UpdateProp(prop, NotDef)  \
  if (curParam.prop == (NotDef))  \
    curParam.prop = newParam.prop;  \
  else  \
    if ((newParam.prop != (NotDef)) and (curParam.prop != newParam.prop))  \
      return asynError;  //msg

  UpdateProp(regAddr, 0);
  UpdateProp(asynType, asynParamNotDefined);
  UpdateProp(ctlrFmt, CtlrDataFmt::NotDefined);
//UpdateProp(syncMode, SyncMode::NotDefined);

  return asynSuccess;
}


//-----------------------------------------------------------------------------
// Called by clients to get the ID for a parameter for a given "port" (device)
// and "addr" (sub-device).
//
// To allow the list of parameters to be determined at run time (rather than
// compiled in to the code), this driver uses these calls to construct a list
// of parameters and their properties during IOC startup. The assumption is
// that the drvInfo string comes from an EPICS record's INP or OUT string
// (everything after the "@asyn(port, addr, timeout)" prefix required for
// records linked to asyn parameter values) and will include the name for the
// parameter and (in at least one of the records that references a parameter)
// the properties for the parameter.
//
// Because multiple records can refer to the same parameter, this function may
// be called multiple times during IOC initialization for the same parameter.
// And because we want to avoid redundancy (and the errors that often result),
// only ONE of the records is expected to include all the property values for
// the paramter.  NOTE that, although it is discouraged, it is NOT an error if
// multiple records include the property values, so long as the values are the
// same for all calls for the same parameter.
//
// We also don't want to require that records be loaded in a specific order, so
// this function does not actually create the asyn parameters.  That is done by
// the driver during a later phase of the IOC initialization, when we can be
// sure that no additional calls to this function for new parameters will
// occur.
//
// NOTE that this flexibility assumes and requires the following:
//
//   - Any attempt to access a parameter before the driver actually creates the
//     parameters must be limited to functions implemented in this driver. Any
//     attempt to do otherwise will result in an error.  If this requirement
//     can not be met at some time in the future, then this function will have
//     to be changed to require at least the asynType property for the first
//     call for a new parameter and to create the asyn parameter during the
//     call.
//
//   - When the driver creates the asyn parameters, the index used by the asyn
//     layer for each parameter is the same as its position in the driver's
//     list.  The driver checks for this and will fail and report an error if
//     they are not the same (although a consistent offset could also be easily
//     accomodated if necessary).  NOTE that the value of such an offset would
//     have to be known before the first call to this function returns, so the
//     value returned in pasynUser->reason is correct.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                   const char **pptypeName, size_t *psize)
{
  string paramCfgStr = string(drvInfo);
  ParamInfo  param(paramCfgStr);

  if (param.name.empty())  return asynError;

  // If the parameter is not already in the list, then add it
  int  paramID;
  if (findParamByName(param.name, &paramID) != asynSuccess)  {
    // If we already reached the max # params, return an error
    if (numParams >= MaxParams)  return asynError;
    paramList[numParams] = param;
    pasynUser->reason = numParams;  ++numParams;  return asynSuccess;
  }

  pasynUser->reason = paramID;

  // Update the existing entry (in case the old one was incomplete)
  return updateParam(paramID, param);
}

//-----------------------------------------------------------------------------
// Create the asyn params for the driver
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::createAsynParams(void)
{
  cout << "create asyn params for: [" << portName << "]" << endl;  //tdebug

  for (int i=0; i<numParams; ++i)  {
    ParamInfo *param = paramList + i;
    cout << "  [" << param->name << "]" << endl;  //tdebug
  }

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Callback function for EPICS IOC initialization steps
//-----------------------------------------------------------------------------
void drvFGPDB_initHookFunc(initHookState state)
{
  if (state != initHookAfterInitDatabase)  return;

  auto it = drvList.begin();
  while (it != drvList.end())  { (*it)->createAsynParams();  it++; }
}

//-----------------------------------------------------------------------------
