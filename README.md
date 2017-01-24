EPICS Support Module for FRIB General Purpose Digital Board
===========================================================
This support module connects to an FRIB General Purpose Digital Board over Ethernet. It has the following features:
* read/write registers
* read arrays (waveforms)
* read/write EEPROM data (e.g. firmware)

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

Building a Debian Package
-------------------------
A Debian package can be build using git build package:
```
gbp buildpackage -uc -us --git-debian-branch=debian
```
Note: This automatically builds the package.