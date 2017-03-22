#include <asynOctetSyncIO.h>

#include "asynOctetSyncIOWrapper.h"

asynStatus asynOctetSyncIOWrapper::connect(const char* port, int addr,
                                           asynUser** ppasynUser,
                                           const char* drvInfo)
{
  return pasynOctetSyncIO->connect(port, addr, ppasynUser, drvInfo);
}

asynStatus asynOctetSyncIOWrapper::disconnect(asynUser* pasynUser)
{
  return pasynOctetSyncIO->disconnect(pasynUser);
}

asynStatus asynOctetSyncIOWrapper::write(asynUser* pasynUser, writeData outData,
                                         double timeout)
{
  return pasynOctetSyncIO->write(pasynUser, outData.write_buffer,
                                 outData.write_buffer_len, timeout,
                                 outData.nbytesOut);
}

asynStatus asynOctetSyncIOWrapper::read(asynUser* pasynUser, readData inData,
                                        double timeout, int* eomReason)
{
  return pasynOctetSyncIO->read(pasynUser, inData.read_buffer,
                                inData.read_buffer_len, timeout,
                                inData.nbytesIn, eomReason);
}

asynStatus asynOctetSyncIOWrapper::writeRead(asynUser* pasynUser,
                                             writeData outData, readData inData,
                                             double timeout, int* eomReason)
{
  return pasynOctetSyncIO->writeRead(pasynUser, outData.write_buffer,
                                     outData.write_buffer_len,
                                     inData.read_buffer, inData.read_buffer_len,
                                     timeout, outData.nbytesOut,
                                     inData.nbytesIn, eomReason);
}

asynStatus asynOctetSyncIOWrapper::flush(asynUser* pasynUser)
{
  return pasynOctetSyncIO->flush(pasynUser);
}

asynStatus asynOctetSyncIOWrapper::setInputEos(asynUser* pasynUser,
                                               const char* eos, int eoslen)
{
  return pasynOctetSyncIO->setInputEos(pasynUser, eos, eoslen);
}

asynStatus asynOctetSyncIOWrapper::getInputEos(asynUser* pasynUser, char* eos,
                                               int eossize, int* eoslen)
{
  return pasynOctetSyncIO->getInputEos(pasynUser, eos, eossize, eoslen);
}

asynStatus asynOctetSyncIOWrapper::setOutputEos(asynUser* pasynUser,
                                                const char* eos, int eoslen)
{
  return pasynOctetSyncIO->setOutputEos(pasynUser, eos, eoslen);
}

asynStatus asynOctetSyncIOWrapper::getOutputEos(asynUser* pasynUser, char* eos,
                                                int eossize, int* eoslen)
{
  return pasynOctetSyncIO->getOutputEos(pasynUser, eos, eossize, eoslen);
}

asynStatus asynOctetSyncIOWrapper::writeOnce(const char* port, int addr,
                                             writeData outData, double timeout,
                                             const char* drvInfo)
{
  return pasynOctetSyncIO->writeOnce(port, addr, outData.write_buffer,
                                     outData.write_buffer_len, timeout,
                                     outData.nbytesOut, drvInfo);
}

asynStatus asynOctetSyncIOWrapper::readOnce(const char* port, int addr,
                                            readData inData, double timeout,
                                            int* eomReason, const char* drvInfo)
{
  return pasynOctetSyncIO->readOnce(port, addr, inData.read_buffer,
                                    inData.read_buffer_len, timeout,
                                    inData.nbytesIn, eomReason, drvInfo);
}

asynStatus asynOctetSyncIOWrapper::writeReadOnce(const char* port, int addr,
                                                 writeData outData,
                                                 readData inData,
                                                 double timeout, int* eomReason,
                                                 const char* drvInfo)
{
  return pasynOctetSyncIO->writeReadOnce(port, addr, outData.write_buffer,
                                         outData.write_buffer_len,
                                         inData.read_buffer,
                                         inData.read_buffer_len, timeout,
                                         outData.nbytesOut, inData.nbytesIn,
                                         eomReason, drvInfo);
}

asynStatus asynOctetSyncIOWrapper::flushOnce(const char* port, int addr,
                                             const char* drvInfo)
{
  return pasynOctetSyncIO->flushOnce(port, addr, drvInfo);
}

asynStatus asynOctetSyncIOWrapper::setInputEosOnce(const char *port, int addr,
                                                   const char *eos, int eoslen,
                                                   const char *drvInfo)
{
  return pasynOctetSyncIO->setInputEosOnce(port, addr, eos, eoslen, drvInfo);
}

asynStatus asynOctetSyncIOWrapper::getInputEosOnce(const char* port, int addr,
                                                   char* eos, int eossize,
                                                   int* eoslen,
                                                   const char* drvInfo)
{
  return pasynOctetSyncIO->getInputEosOnce(port, addr, eos, eossize, eoslen,
                                           drvInfo);
}

asynStatus asynOctetSyncIOWrapper::setOutputEosOnce(const char* port, int addr,
                                                    const char* eos, int eoslen,
                                                    const char* drvInfo)
{
  return pasynOctetSyncIO->setOutputEosOnce(port, addr, eos, eoslen, drvInfo);
}

asynStatus asynOctetSyncIOWrapper::getOutputEosOnce(const char* port, int addr,
                                                    char* eos, int eossize,
                                                    int* eoslen,
                                                    const char* drvInfo)
{
  return pasynOctetSyncIO->getOutputEosOnce(port, addr, eos, eossize, eoslen,
                                            drvInfo);
}
