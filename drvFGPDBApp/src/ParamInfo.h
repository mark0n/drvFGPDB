#ifndef PARAMINFO_H
#define PARAMINFO_H

/**
 * @file  ParamInfo.h
 * @brief Defines the LLRF Communication Protocol.
 */

#include <regex>
#include <map>
#include <iostream>

#include <asynPortDriver.h>

/**
 * @brief Data formats supported by the ctlr
 * @note  Be sure to update ParamInfo::ctlrFmts in ParamInfo.cpp
 */
enum class CtlrDataFmt {
  NotDefined, //!< Not defined
  S32,        //!< signed 32-bit int
  U32,        //!< unsigned 32-bit int
  F32,        //!< 32-bit float
  U16_16,     //!< = (uint) (value * 2.0^16)
};

/**
 * @brief Current write state of a ctlr/driver-only value
 * @note  Be sure to update ParamInfo::setStates map in ParamInfo.cpp
 */
enum class SetState {
  Undefined,  //!< no value written to the parameter yet
  Pending,    //!< new setting ready to be processed
  Processing, //!< in the middle of processing a write
  Sent,       //!< ack'd by ctlr or driver-only value updated
  Error       //!< failed while writing new value
};

/**
 * @brief Current read state of a ctlr/driver-only value
 * @note  Be sure to update ParamInfo::readStates map in ParamInfo.cpp
 */
enum class ReadState {
  Undefined,  //!< no value read from ctlr yet
  Pending,    //!< new reading ready to be posted
  Update,     //!< value needs to be/is being updated
  Current     //!< most recent value posted to asyn layer
};

/**
 * Information the driver keeps about each parameter.  This list is generated
 * during IOC startup from the data in the INP/OUT fields in the EPICS records
 * that are linked to these parameters.
 */
class ParamInfo {
  public:
    ParamInfo() :
      regAddr(0),
      asynType(asynParamNotDefined),
      ctlrFmt(CtlrDataFmt::NotDefined),
      readOnly(true),
      ctlrValSet(0),
      setState(SetState::Undefined),
      ctlrValRead(0),
      readState(ReadState::Undefined),
      drvValue(nullptr),
      chipNum(0),
      blockSize(0),
      eraseReq(false),
      offset(0),
      length(0),
      statusParamID(-1),
      rwOffset(0),
      blockNum(0),
      dataOffset(0),
      bytesLeft(0),
      rwCount(0)
    {};
    /**
     * @brief Constructs the Parameter object.
     *
     * @param[in] paramStr string that describes the parameter. Formats allowed are:
     *                     - name addr asynType ctlrFmt.
     *                     - name addr chipID blockSize eraseReq offset length statusName.
     * @param[in] portName Name of the port that owns the parameter.
     */
    ParamInfo(const std::string& paramStr, const std::string& portName);

    /**
     * @brief Method to set param attribute values related with the array read/write process.
     *
     * @param[in] ttlNumBytes Number of bytes left to r/w.
     */
    void initBlockRW(uint32_t ttlNumBytes);

    /**
     * @brief Updates the properties for an existing parameter.
     *        Checks for conflicts and updates any missing property values using ones
     *        from the new set of properties.
     *
     * @param[in] context  Name of the port that wants to update the parameter.
     * @param[in] newParam Parameter used to update the existing parameter.
     *
     * @return asynStatus of the update process.
     */
    asynStatus updateParamDef(const std::string &context,
                              const ParamInfo &newParam);

    /**
     * @brief Method that identifies the asyn interface format equivalent to a given string.
     *
     * @param[in] typeName string to identify the asyn format
     *
     * @return    Equivalent asyn format
     */
    static asynParamType strToAsynType(const std::string &typeName);

    /**
     * @brief Method that identifies the ctlr format equivalent to a given string.
     *
     * @param[in] fmtName string to identify the ctrl format
     *
     * @return    Equivalent ctrl format
     */
    static CtlrDataFmt strToCtlrFmt(const std::string &fmtName);

    /**
     * @brief Method that identifies the equivalent string of the given asyn format
     *
     * @param[in] asynType asyn format to identify the equivalent string
     *
     * @return    Equivalent string
     */
    static const std::string & asynTypeToStr(const asynParamType asynType);

    /**
     * @brief Method that identifies the equivalent string of the given ctrl format
     *
     * @param[in] ctlrFmt ctlr Format to identify the equivalent string
     *
     * @return    Equivalent string
     */
    static const std::string & ctlrFmtToStr(const CtlrDataFmt ctlrFmt);

    /**
     * @brief Method to know the state of ctlrValSet param attribute
     *        - Undefined, Pending, Processing, Sent and Error
     *
     * @return state of ctlrValSet
     */
    const std::string & setStateToStr(void) const;

    /**
     * @brief Method to know the state of ctlrValRead  param attribute
     *        - Undefined, Pending, Update and Current
     *
     * @return state of ctlrValRead
     */
    const std::string & readStateToStr(void) const;

    /**
     * @brief Method to convert a value from the format used by the ctlr to a double
     *
     * @param[in] ctlrVal  value to be converted
     * @param[in] ctlrFmt  ctrl data format
     *
     * @return Value converted to double
     */
    static double ctlrFmtToDouble(uint32_t ctlrVal, CtlrDataFmt ctlrFmt);

    /**
     * @brief Method to convert a value from double to the format used by the ctlr
     *
     * @param[in] dval      double value to be converted
     * @param[in] ctlrFmt   ctrl data format
     *
     * @return Value converted to ctlr data format
     */
    static uint32_t doubleToCtlrFmt(double dval, CtlrDataFmt ctlrFmt);

    /**
     * @brief CMethod to convert a value from int32 to the format used by the ctlr
     *
     * @param[in] ival      int32 value to be converted
     * @param[in] ctlrFmt   ctrl data format
     *
     * @return Value converted to ctlr data format
     */
    static uint32_t int32ToCtlrFmt(int32_t ival, CtlrDataFmt ctlrFmt);

    /**
     * @brief Operator to manage parameter definitions
     *
     * @param[in] os     String to store the definition of a parameter
     * @param[in] param  Parameter used to read its definition
     *
     * @return String with the definition of the parameter
     */
    friend std::ostream& operator<<(std::ostream& os, const ParamInfo &param);

    /**
     * @brief Method to know if the parameter is a scalar
     *
     * @return true/false
     */
    bool isScalarParam() const {
      return (asynType == asynParamInt32)
          or (asynType == asynParamUInt32Digital)
          or (asynType == asynParamFloat64);
    }

    /**
     * @brief Method to know if the parameter is an array
     *
     * @return true/false
     */
    bool isArrayParam() const {
      return (blockSize and length and
               ((asynType == asynParamInt8Array) or
                (asynType == asynParamInt16Array) or
                (asynType == asynParamInt32Array) or
                (asynType == asynParamFloat32Array) or
                (asynType == asynParamFloat64Array)) );
    }

    void newReadVal(uint32_t newVal);


    std::string    name;        //!< Name of the parameter

    uint           regAddr;     //!< LCP reg addr or driver param group

    asynParamType  asynType;    //!< Format used by asyn interface
    CtlrDataFmt    ctlrFmt;     //!< Format of run-time value in ctlr or driver

    bool           readOnly;    //!< Clients cannot write to the value

    epicsUInt32    ctlrValSet;  //!< Value to write to ctlr @note In ctlr fmt, host byte order!
    SetState       setState;    //!< State of ctlrValSet

    epicsUInt32    ctlrValRead; //!< Most recently read value from ctlr @note In ctlr fmt, host byte order!
    ReadState      readState;   //!< State of ctlrValRead

    uint32_t      *drvValue;    //!< Run-time variable for key freq-used scalar params

    // properties for pmem (array) parameters
    std::vector<uint8_t> arrayValSet;   //!< Array to write to ctlr
    std::vector<uint8_t> arrayValRead;  //!< Most recently read array from ctlr

    uint           chipNum;     //!< ID for persistent memory chip
    ulong          blockSize;   //!< Size to use in PMEM r/w cmds
    bool           eraseReq;    //!< Is erasing a block reqd before writing to it?
    ulong          offset;      //!< Offset from start of chips memory
    ulong          length;      //!< Number of bytes that make up logical entity

    std::string    statusParamName; //!< Name of the status param associated to an PMEM param
    int            statusParamID;   //!< ID of the status param

    // state data for in-progress read or write of an array value
    uint32_t       rwOffset;    //!< Offset in to arrayValSet/Read buffers
    uint32_t       blockNum;    //!< BlockNum used in PMEM r/w cmd
    uint32_t       dataOffset;  //!< Offset in to r/w cmd's block buffer
    uint32_t       bytesLeft;   //!< Number of bytes left to r/w
    uint           rwCount;     //!< Number of bytes req in PMEM r/w cmd

    std::vector<uint8_t> rwBuf;

#ifndef TEST_DRVFGPDB
  private:
#endif
    /**
     * @brief Generates a regex for basic validation of strings that define a parameter
     *        for a scalar value.
     *
     * @return scalar regex
     */
    static const std::regex& scalarParamDefRegex();

    /**
     * @brief Generates a regex for basic validation of strings that define a parameter for
     *        a PMEM (persistent memory) value.
     *
     * @return pmem regex
     */
    static const std::regex& pmemParamDefRegex();

    static const std::map<std::string, asynParamType> asynTypes;  //!< Map w/ the asyn data formats supported by the driver
    static const std::map<std::string, CtlrDataFmt> ctlrFmts;     //!< Map w/ the data formats supported by the ctlr
    static const std::map<SetState, std::string> setStates;       //!< Map w/ supported write states of a ctlr/driver-only value
    static const std::map<ReadState, std::string> readStates;     //!< Map w/ supported read states of a ctlr/driver-only value
};

/**
 * @brief Return a string with the set of key values from a map
 */
template <typename T>
std::string joinMapKeys(const std::map<std::string, T>& map,
                        const std::string& separator) {
  std::string joinedKeys;
  bool first = true;
  for(auto const& x : map) {
    if(!first)  joinedKeys += separator;
    first = false;
    joinedKeys += x.first;
  }
  return joinedKeys;
}

#endif // PARAMINFO_H
