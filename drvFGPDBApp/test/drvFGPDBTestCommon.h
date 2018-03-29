#ifndef DRVFGPDBTESTCOMMON_H
#define DRVFGPDBTESTCOMMON_H

/**
 * @file  drvFGPDBTestCommon.h
 * @brief Creates common resources to perform Unit and Integration tests.
 */

#include <memory>

using namespace testing;
using namespace std;


static int testNum = 0; //!< number of the test being executed

const string UDPPortName("FGPDB_com"); //!< name of the UDP port

const uint32_t startupDiagFlags = TestMode_; //!< test mode used in the tests

/**
 * @brief creates the UDP communication port
 *
 * @note  asyn does not provide a way to destroy a drvAsynIPPort, yet. For now we
 *        only create it once and reuse it for all our tests. As soon as Asyn
 *        supports destroying it we should make our tests 100% independent again.
 *
 * @return status of the UDP port created
 */
int createPortUDP(void)
{
  static int stat = drvAsynIPPortConfigure(UDPPortName.c_str(), "127.0.0.1:2005 udp", 0, 0, 1);

  return stat;
}

/**
 * Class that creates all common resources needed by unit and integration tests
 * such us the asynUser, the UDP port, the asynOctetSyncIOInterface or the driver's name
 */
class AnFGPDBDriver: public ::testing::Test
{
public:
  /**
   * @brief Constructor
   *
   * @param[in] syncIOIn interface to perform "synchronous" I/O operations
   */

  AnFGPDBDriver(shared_ptr<asynOctetSyncIOInterface> syncIOIn) :
    pasynUser(pasynManager->createAsynUser(nullptr, nullptr)),
    stat(asynError),
    syncIO(syncIOIn),
    drvName("testDriver" + std::to_string(++testNum)),
    udpPortStat(createPortUDP()),  // Must be created before drvFGPDB object
    id(-1),
    testParamID_RO(-1),
    maxParamID_RO(-1),
    testParamID_WA(-1),
    lastRegID_WA(-1),
    arrayWriteStatusID(-1),
    testArrayID(-1),
    numDrvParams(0),
    RO_groupSize(0),
    WA_groupSize(0),
    WO_groupSize(0)
  {};

  /**
   * @brief Destructor
   */
  ~AnFGPDBDriver() { pasynManager->freeAsynUser(pasynUser); };

  /**
   *  @brief Method to create and register new parameters in the driver
   *         and in the asynDriver layer (assigns pasynUser->reason)
   *
   *  @param paramStr string with the description of the new param
   *
   *  @return param's ID
   */
  int addParam(string paramStr) {
    pasynUser->reason = -1;
    stat = testDrv->drvUserCreate(pasynUser, paramStr.c_str(), nullptr, nullptr);
    return (stat == asynSuccess) ? pasynUser->reason : -1;
  }

  /**
   * @brief Method to add new predefined parameters for testing purposes
   */
  void addParams()  {
    // No params for addrs 0x10000 - 0x10001
    id = addParam("lcpRegRO_1 0x10002 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams));  testParamID_RO = id;
    // No params for addr 0x10003
    id = addParam("lcpRegRO_4 0x10004 Float64 F32");
    ASSERT_THAT(id, Eq(numDrvParams+1));  maxParamID_RO = id;

    // Set addr for required RO regs already added by driver
    addParam("upSecs 0x10005 Int32 U32");
    addParam("upMs 0x10006 Int32 U32");
    addParam("writerIP 0x10007 Int32 U32");
    addParam("writerPort 0x10008 Int32 U32");
    RO_groupSize = 0x0009u;

    id = addParam("lcpRegWA_1 0x20000 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+2));
    //No param defined for addr 0x20001
    id = addParam("lcpRegWA_2 0x20002 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+3));  testParamID_WA = id;
    id = addParam("lcpRegWA_3 0x20003 Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+4));
    id = addParam("lcpRegWA_4 0x20004 Float64 F32");
    ASSERT_THAT(id, Eq(numDrvParams+5));  lastRegID_WA = id;
    WA_groupSize = 0x0005u;

    id = addParam("sessionID  0x3000E Int32 U32");
    id = addParam("lcpRegWO_2 0x300FF Int32 U32");
    ASSERT_THAT(id, Eq(numDrvParams+6));
    WO_groupSize = 0x0100u;

    id = addParam("pmemWriteStatus 0x1 Int32");
    ASSERT_THAT(id, Eq(numDrvParams+7));  arrayWriteStatusID = id;
    id = addParam("pmemWriteTest 0x2 1 256 Y 0x00800000 0x410000 pmemWriteStatus");
    ASSERT_THAT(id, Eq(numDrvParams+8));  testArrayID = id;
  }

  //---------------------------------------------
  asynUser  *pasynUser;          //!< structure that encodes the reason and address
  asynStatus  stat;              //!< asynStatus
  shared_ptr<asynOctetSyncIOInterface>  syncIO; //!< interface to perform "synchronous" I/O operations
  std::string  drvName;          //!< name of the driver instance
  int  udpPortStat;              //!< status of the UDP port creation
  unique_ptr<drvFGPDB> testDrv;  //!< driver instance
  int id;                        //!< param's id
  int testParamID_RO;            //!< read-only param
  int maxParamID_RO;             //!< MAX number of read-only params
  int testParamID_WA;            //!< write-anytime param
  int lastRegID_WA;              //!< last write-anytime registered param
  int arrayWriteStatusID;        //!< ID of the status param assigned to pmemWriteTest param
  int testArrayID;               //!< ID of the pmemWriteTest param
  int numDrvParams;              //!< number of registered params
  uint RO_groupSize;             //!< # of params in Read-Only group
  uint WA_groupSize;             //!< # of params in Write-Anytime group
  uint WO_groupSize;             //!< # of params in Write-Once group
  ParamInfo  param;              //!< copy of a param
};


#endif // DRVFGPDBTESTCOMMON_H