#ifndef ASYNOCTETSYNCIOWRAPPER_H
#define ASYNOCTETSYNCIOWRAPPER_H

/**
 * @file  asynOctetSyncIOWrapper.h
 * @brief Wraps the C-style code from Asyn to facilitate mocking
 */

#include "asynOctetSyncIOInterface.h"

/**
 * @brief Wraps the C-style code from Asyn to facilitate mocking
 */
class asynOctetSyncIOWrapper : public asynOctetSyncIOInterface {
public:
  asynStatus connect(const char *port, int addr, asynUser **ppasynUser,
                             const char *drvInfo) override;
  asynStatus disconnect(asynUser *pasynUser) override;
  asynStatus write(asynUser *pasynUser, writeData outData, size_t *nbytesOut,
                   double timeout) override;
  asynStatus read(asynUser *pasynUser, readData inData, size_t *nbytesIn,
                  double timeout, int *eomReason) override;
  asynStatus writeRead(asynUser *pasynUser, writeData outData,
                       size_t *nbytesOut, readData inData, size_t *nbytesIn,
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
                       size_t *nbytesOut, double timeout, const char *drvInfo)
      override;
  asynStatus readOnce(const char *port, int addr, readData inData,
                      size_t *nbytesIn, double timeout, int *eomReason,
                      const char *drvInfo) override;
  asynStatus writeReadOnce(const char *port, int addr, writeData outData,
                           size_t *nbytesOut, readData inData, size_t *nbytesIn,
                           double timeout, int *eomReason, const char *drvInfo)
      override;
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
