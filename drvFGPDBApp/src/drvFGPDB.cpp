
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
drvFGPDB::drvFGPDB(const string drvPortName) :
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
    if (paramInfo[i].name == name)  { *index = i;  return (asynSuccess); }

  return (asynError);
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
//  printf("\ndrvUserCreate: [%.256s]\n", drvInfo);

  vector <string> fields;
  string s = string(drvInfo);

  split(fields, s, is_any_of(" "), token_compress_on);

//  for (size_t n=0; n<fields.size(); n++)  cout << "[" << fields[n] << "] ";
//  cout << endl;


  // If parameter already in the list, return its index
  int  index;
  if (findParamByName(fields[0], &index) == asynSuccess)  {
    pasynUser->reason = index;  return (asynSuccess); }

  // If we already reached the max # params, return an error
  if (numParams >= MaxParams)  return asynError;

  // Add the new parameter to the list
  ParamInfo *ppi = paramInfo + numParams;
  ppi->name = fields[0];
  pasynUser->reason = numParams;
  ++numParams;

  return (asynSuccess);
}

//-----------------------------------------------------------------------------
