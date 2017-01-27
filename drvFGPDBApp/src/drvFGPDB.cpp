#include "drvFGPDB.h"

using namespace std;

drvFGPDB::drvFGPDB(const string drvPortName) :
    asynPortDriver(drvPortName.c_str(), maxAddr, paramTableSize, interfaceMask,
                   interruptMask, asynFlags, autoConnect, priority, stackSize)
{
}