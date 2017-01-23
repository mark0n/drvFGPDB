#include <string>
#include <asynPortDriver.h>

class drvFGPDB : public asynPortDriver {
public:
  drvFGPDB(std::string drvPortName);
};