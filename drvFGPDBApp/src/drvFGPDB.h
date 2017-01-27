#include <string>
#include <asynPortDriver.h>

class drvFGPDB : public asynPortDriver {
private:
  static const int maxAddr = 1;
  static const int paramTableSize = 1;
  static const int interfaceMask = asynInt8ArrayMask | asynInt32Mask |
                                   asynUInt32DigitalMask | asynFloat64Mask |
                                   asynFloat64ArrayMask | asynOctetMask |
                                   asynDrvUserMask;
  static const int interruptMask = asynInt8ArrayMask | asynInt32Mask |
                                   asynUInt32DigitalMask | asynFloat64Mask |
                                   asynFloat64ArrayMask | asynOctetMask;
  static const int asynFlags = 0;
  static const int autoConnect = 1;
  static const int priority = 0;
  static const int stackSize = 0;
public:
  drvFGPDB(std::string drvPortName);
};