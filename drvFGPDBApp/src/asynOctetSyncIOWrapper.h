#ifndef ASYNOCTETSYNCIOWRAPPER_H
#define ASYNOCTETSYNCIOWRAPPER_H

#include <asynOctetSyncIO.h>
#include "asynOctetSyncIOInterface.h"

// Wrap the C-style code from Asyn to facilitate mocking

class asynOctetSyncIOWrapper : public asynOctetSyncIOInterface {
public:
  asynStatus connect(const char *port, int addr, asynUser **ppasynUser,
                             const char *drvInfo) override;
  asynStatus disconnect(asynUser *pasynUser) override;
  asynStatus write(asynUser *pasynUser, writeData outData,
                   double timeout) override;
  asynStatus read(asynUser *pasynUser, readData inData, double timeout,
                  int *eomReason) override;
  asynStatus writeRead(asynUser *pasynUser, writeData outData, readData inData,
                       double timeout, int *eomReason) override;
  asynStatus flush(asynUser *pasynUser) override;
  asynStatus setInputEos(asynUser *pasynUser, const char *eos, int eoslen)
      override;
  asynStatus getInputEos(asynUser *pasynUser, char *eos, int eossize,
                                 int *eoslen) override;
  asynStatus setOutputEos(asynUser *pasynUser, const char *eos,
                                  int eoslen) override;
  asynStatus getOutputEos(asynUser *pasynUser, char *eos, int eossize,
                                  int *eoslen) override;
  asynStatus writeOnce(const char *port, int addr, writeData outData,
                       double timeout, const char *drvInfo)
      override;
  asynStatus readOnce(const char *port, int addr, readData inData,
                      double timeout, int *eomReason,
                      const char *drvInfo) override;
  asynStatus writeReadOnce(const char *port, int addr, writeData outData,
                           readData inData, double timeout, int *eomReason,
                           const char *drvInfo) override;
  asynStatus flushOnce(const char *port, int addr,const char *drvInfo) override;
  asynStatus setInputEosOnce(const char *port, int addr, const char *eos,
                             int eoslen, const char *drvInfo) override;
  asynStatus getInputEosOnce(const char *port, int addr, char *eos,
                             int eossize, int *eoslen, const char *drvInfo)
      override;
  asynStatus setOutputEosOnce(const char *port, int addr, const char *eos,
                              int eoslen, const char *drvInfo) override;
  asynStatus getOutputEosOnce(const char *port, int addr, char *eos,
                              int eossize, int *eoslen, const char *drvInfo)
      override;
};

#endif // ASYNOCTETSYNCIOWRAPPER_H