EPICS Support Module for FRIB General Purpose Digital Board
===========================================================
This support module connects to an FRIB General Purpose Digital Board over Ethernet. It has the following features:
* read/write registers
* read arrays (waveforms)
* read/write EEPROM data (e.g. firmware)

Getting the Code
----------------
This project uses Git submodules. Please clone the project using the following commands:
```
git clone ssh://git@stash.frib.msu.edu:7999/rf/drvfgpdb.git
cd drvfgpdb/
git submodule update --init
```
The following command is a shortcut for the two commands above:
```
git clone --recursive ssh://git@stash.frib.msu.edu:7999/rf/drvfgpdb.git
```

Compiling
---------
Run the following commands to build this module:
```
mkdir build
cd build/
cmake -DCMAKE_INSTALL_PREFIX=.. ..
make
make install
```
You can run the tests by issuing the following command
```
./drvFGPDBApp/src/drvFGPDBTests
```

### Debug Build
Use the following command to build with debugging symbols:
```
cmake -DCMAKE_INSTALL_PREFIX=.. -DCMAKE_BUILD_TYPE=Debug ..
```

### Build Against a Specific Asyn Version
You can use a specific Asyn version by pointing CMake to it:
```
cmake -DCMAKE_INSTALL_PREFIX=.. -DEPICS_MODULE_PATH=/home/marko/work/RF/LLRF/support/asyn-1 ..
```

### Code Coverage
To extract useful code coverage information the tool needs to be build without optimization:
```
mkdir build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make drvFGPDB_coverage
```
Coverage data can be found in `build/coverage/index.html`.

Building a Debian Package
-------------------------
A Debian package can be build using git build package:
```
gbp buildpackage -uc -us --git-debian-branch=debian
```
Note: This automatically builds the package.