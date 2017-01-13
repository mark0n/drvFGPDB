TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += drvFGPDB

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install drvFGPDB.dbd into <top>/dbd
DBD += drvFGPDB.dbd

# specify all source files to be compiled and added to the library
#drvFGPDB_SRCS += xxx

drvFGPDB_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

