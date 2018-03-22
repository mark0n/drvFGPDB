#ifndef ASYNOCTETSYNCIOINTERFACE_H
#define ASYNOCTETSYNCIOINTERFACE_H

/**
 * @file  asynOctetSyncIOInterface.h
 * @brief Defines the C++ interface for accessing the functions in asynOctetSyncIO.
 */

/**
 * @brief struct with all read-related parameters used by asynOctetSyncIOInterface
 * @note  needed because max function arguments supported by google mock is 9
 */
struct readData {
  char *read_buffer;
  size_t read_buffer_len;
  size_t *nbytesIn;
};

/**
 * @brief struct with all write-related parameters used by asynOctetSyncIOInterface
 * @note  needed because max function arguments supported by google mock is 9
 */
struct writeData {
  const char *write_buffer;
  size_t write_buffer_len;
  size_t *nbytesOut;
};
/**
 * @brief C++ interface for accessing the functions in asynOctetSyncIO
 */
class asynOctetSyncIOInterface {
public:
  virtual asynStatus connect(const char *port, int addr, asynUser **ppasynUser,
                             const char *drvInfo) = 0;
  virtual asynStatus disconnect(asynUser *pasynUser) = 0;
  virtual asynStatus write(asynUser *pasynUser, writeData outData,
                           double timeout) = 0;
  virtual asynStatus read(asynUser *pasynUser, readData inData, double timeout,
                          int *eomReason) = 0;
  virtual asynStatus writeRead(asynUser *pasynUser, writeData outData,
                               readData inData, double timeout,
                               int *eomReason) = 0;
  virtual asynStatus flush(asynUser *pasynUser) = 0;
  virtual asynStatus setInputEos(asynUser *pasynUser, const char *eos,
                                 int eoslen) = 0;
  virtual asynStatus getInputEos(asynUser *pasynUser, char *eos, int eossize,
                                 int *eoslen) = 0;
  virtual asynStatus setOutputEos(asynUser *pasynUser, const char *eos,
                                  int eoslen) = 0;
  virtual asynStatus getOutputEos(asynUser *pasynUser, char *eos, int eossize,
                                  int *eoslen) = 0;
  virtual asynStatus writeOnce(const char *port, int addr, writeData outData,
                               double timeout, const char *drvInfo) = 0;
  virtual asynStatus readOnce(const char *port, int addr, readData inData,
                              double timeout, int *eomReason,
                              const char *drvInfo) = 0;
  virtual asynStatus writeReadOnce(const char *port, int addr,
                                   writeData outData, readData inData,
                                   double timeout, int *eomReason,
                                   const char *drvInfo) = 0;
  virtual asynStatus flushOnce(const char *port, int addr,
                               const char *drvInfo) = 0;
  virtual asynStatus setInputEosOnce(const char *port, int addr,
                                     const char *eos, int eoslen,
                                     const char *drvInfo) = 0;
  virtual asynStatus getInputEosOnce(const char *port, int addr, char *eos,
                                     int eossize, int *eoslen,
                                     const char *drvInfo) = 0;
  virtual asynStatus setOutputEosOnce(const char *port, int addr,
                                      const char *eos, int eoslen,
                                      const char *drvInfo) = 0;
  virtual asynStatus getOutputEosOnce(const char *port, int addr, char *eos,
                                      int eossize, int *eoslen,
                                      const char *drvInfo) = 0;
};

#endif // ASYNOCTETSYNCIOINTERFACE_H
