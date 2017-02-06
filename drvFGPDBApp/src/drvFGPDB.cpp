
#include "drvFGPDB.h"

#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <string>
#include <vector>


using namespace std;
using namespace boost;


//-----------------------------------------------------------------------------
drvFGPDB::drvFGPDB(const string &drvPortName) :
    asynPortDriver(drvPortName.c_str(), MaxAddr, MaxParams, InterfaceMask,
                   InterruptMask, AsynFlags, AutoConnect, Priority, StackSize),
    numParams(0)
{

}


//-----------------------------------------------------------------------------
//  Search the driver's list of parameters for an entry with the given name.
//  Note that, unlike asynPortDriver::findParam(), this function will work
//  during IOC startup, before the actual asyn parameters are generated from
//  the driver's list.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::findParamByName(const string &name, int *index)
{
  for (int i=0; i<numParams; ++i)
    if (paramList[i].name == name)  { *index = i;  return (asynSuccess); }

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
//  Return the ParamGroup value associated with a string
//-----------------------------------------------------------------------------
ParamGroup drvFGPDB::strToParamGroup(const string &groupName)
{
  if (groupName == "LCP_RO")  return ParamGroup::LCP_RO;
  if (groupName == "LCP_WA")  return ParamGroup::LCP_WA;
  if (groupName == "LCP_WO")  return ParamGroup::LCP_WO;
  if (groupName == "DRV_RO")  return ParamGroup::DRV_RO;
  if (groupName == "DRV_RW")  return ParamGroup::DRV_RW;

  return ParamGroup::NotDefined;
}

//-----------------------------------------------------------------------------
//  Return the asynParamType associated with a string
//-----------------------------------------------------------------------------
asynParamType drvFGPDB::strToAsynType(const string &typeName)
{
  if (typeName == "Int32")          return asynParamInt32;
  if (typeName == "UInt32Digital")  return asynParamUInt32Digital;
  if (typeName == "Float64")        return asynParamFloat64;
  if (typeName == "Octet")          return asynParamOctet;
  if (typeName == "Int8Array")      return asynParamInt8Array;
  if (typeName == "Int16Array")     return asynParamInt16Array;
  if (typeName == "Int32Array")     return asynParamInt32Array;
  if (typeName == "Float32Array")   return asynParamFloat32Array;
  if (typeName == "Float64Array")   return asynParamFloat64Array;

  return asynParamNotDefined;
}

//-----------------------------------------------------------------------------
//  Return the CtlrDataFmt associated with a string
//-----------------------------------------------------------------------------
CtlrDataFmt drvFGPDB::strToCtlrFmt(const string &fmtName)
{
  if (fmtName == "S32")     return CtlrDataFmt::S32;
  if (fmtName == "U32")     return CtlrDataFmt::U32;
  if (fmtName == "F32")     return CtlrDataFmt::F32;
  if (fmtName == "U16_16")  return CtlrDataFmt::U16_16;
  if (fmtName == "PHASE")   return CtlrDataFmt::PHASE;

  return CtlrDataFmt::NotDefined;
}

//-----------------------------------------------------------------------------
//  Return the SyncMode associated with a string
//-----------------------------------------------------------------------------
SyncMode drvFGPDB::strToSyncMode(const string &modeName)
{
  if (modeName == "SM_DN")  return SyncMode::SM_DN;
  if (modeName == "SM_EQ")  return SyncMode::SM_EQ;
  if (modeName == "SM_CM")  return SyncMode::SM_CM;
  if (modeName == "SM_IM")  return SyncMode::SM_IM;

  return SyncMode::NotDefined;
}

//-----------------------------------------------------------------------------
//  Initialize a ParamInfo object with whatever property information is
//  provided in the properties agrument.
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::extractProperties(vector <string> &properties,
                                       ParamInfo &param)
{
  ParamGroup  group;
  uint  regNum;
  asynParamType asynType;
  CtlrDataFmt ctlrFmt;
  SyncMode  syncMode;

  int numFields = properties.size();

  // Must have at least the name
  if (numFields < 1)  return asynError;  //msg

  param.name = properties[0];
  if (numFields < 2)  return asynSuccess;

  // Anything more than the name currently requires all the related properties
  group = strToParamGroup(properties[1]);
  if (group == ParamGroup::NotDefined)  return asynError;  //msg

  param.group = group;


  // Parameter for LCP Read-Only register value
  // format:  name LCP_RO 0x##### asynType ctlrFmt syncMode
  if (group == ParamGroup::LCP_RO)  {
    if (numFields < 6)  return asynError;  //msg

    regNum = stoul(properties[2], nullptr, 16);
    if ((regNum < 0x10000) or (regNum > 0x1FFFF))  return asynError;  //msg

    asynType = strToAsynType(properties[3]);
    if (asynType == asynParamNotDefined)  return asynError;  //msg

    ctlrFmt = strToCtlrFmt(properties[4]);
    if (ctlrFmt == CtlrDataFmt::NotDefined)  return asynError;  //msg

    syncMode = strToSyncMode(properties[5]);
    if (syncMode == SyncMode::NotDefined)  return asynError;  //msg
  } else


  // Parameter for LCP Write-Anytime register value
  // format:  name LCP_WA 0x##### asynType ctlrFmt syncMode
  if (group == ParamGroup::LCP_WA)  {

  } //...


  param.lcpRegNum = regNum;
  param.asynType = asynType;
  param.ctlrFmt = ctlrFmt;
  param.syncMode = syncMode;

  return asynSuccess;
}

//-----------------------------------------------------------------------------
//  Compare a new parameter definition with an existing one.
//
//  Returns true if the new definition contains property values that conflict
//  with the existing one, false if it can safely replace the existing one.
//-----------------------------------------------------------------------------
bool drvFGPDB::propertyConflicts(const ParamInfo &curParam,
                                 const ParamInfo &newParam)
{

  return true;
}


//-----------------------------------------------------------------------------
// Called by clients to get an index in to the list of parameters for a given
// "port" (device) and "addr" (sub-device).
//
// To allow the list of parameters to be determined at run time (rather than
// compiled in to the code), this driver uses these calls to construct a list
// of parameters and their properties during IOC startup. The assumption is
// that the drvInfo string comes from an EPICS record's INP or OUT string
// (everything after the "@asyn(port, addr, timeout)" prefix required for
// records linked to asyn parameter values) and will include the name for the
// parameter and (*optionally*) the properties for the parameter.
//
// Because multiple records can refer to the same parameter, this function may
// be called multiple times during IOC initialization for the same parameter.
// And because we want to avoid redundancy (and the errors that often result),
// only ONE of the records is expected to include all the property values for
// the paramter.  NOTE that it is NOT an error for multiple records to include
// some or all the property values, so long as the values are the same for all
// calls for the same parameter.
//
// We also don't want to require records be loaded in a specific order, so this
// function does not actually create the asyn parameters.  That is done by the
// driver during a later phase of the IOC initialization, when we can be sure
// that no additional calls to this function for new parameters will occur.
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
//     accomodated if necessary),
//-----------------------------------------------------------------------------
asynStatus drvFGPDB::drvUserCreate(asynUser *pasynUser, const char *drvInfo,
                                   const char **pptypeName, size_t *psize)
{

//  cout << "portName: [" << portName << "]" << endl;

  vector <string> properties;
  string s = string(drvInfo);
  ParamInfo  param;
  asynStatus  stat;


  split(properties, s, is_any_of(" "), token_compress_on);

//  for (size_t n=0; n<properties.size(); n++)
//    cout << "[" << properties[n] << "] ";
//  cout << endl;

  // Process the property values provided in this call
  stat = extractProperties(properties, param);
  if (stat != asynSuccess)  return stat;

  // If the parameter is not already in the list, then add it
  int  index;
  if (findParamByName(properties[0], &index) != asynSuccess)  {
    // If we already reached the max # params, return an error
    if (numParams >= MaxParams)  return asynError;
    paramList[numParams] = param;
    pasynUser->reason = numParams;  ++numParams;  return asynSuccess;
  }

  // If just the name, then simply return the index
  if (properties.size() < 2) { pasynUser->reason = index;  return asynSuccess; }

  // Check for conflicting property values
  if (propertyConflicts(paramList[index], param))  return asynError;

  // Replace existing info with new one in case the old one was incomplete
  paramList[index] = param;

  return asynSuccess;
}

//-----------------------------------------------------------------------------
