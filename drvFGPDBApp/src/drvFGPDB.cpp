#include "drvFGPDB.h"

using namespace std;

const int maxAddr = 1;
const int paramTableSize = 1;
const int interfaceMask = interfaceMask | asynInt8ArrayMask | asynInt32Mask |
                          asynUInt32DigitalMask | asynFloat64Mask |
                          asynFloat64ArrayMask | asynOctetMask |
                          asynDrvUserMask;
const int interruptMask = interruptMask | asynInt8ArrayMask | asynInt32Mask |
                          asynUInt32DigitalMask | asynFloat64Mask |
                          asynFloat64ArrayMask | asynOctetMask;
const int asynFlags = 0;
const int autoConnect = 1;
const int priority = 0;
const int stackSize = 0;

drvFGPDB::drvFGPDB(const string drvPortName) :
    asynPortDriver(drvPortName.c_str(), maxAddr, paramTableSize, interfaceMask,
                   interruptMask, asynFlags, autoConnect, priority, stackSize)
{
}