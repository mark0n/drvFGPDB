EPICS Support Module for FRIB General Purpose Digital Board
===========================================================
This support module connects to an FRIB General Purpose Digital Board over Ethernet. It has the following features:
* read/write registers
* read arrays (waveforms)
* read/write EEPROM data (e.g. firmware)

Dependencies
------------
* Asyn >= 4.31
* CMake >=3
* CMake4EPICS
* EPICS Base
* Google Test and Google Mock

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
cmake -DCMAKE_INSTALL_PREFIX=.. -DEPICS_MODULE_PATH=/home/marko/support/asyn ..
```

### Code Coverage
To extract useful code coverage information the tool needs to be build without optimization:
```
mkdir build
cd build/
cmake -DCMAKE_BUILD_TYPE=Debug ..
make drvCoverageMerge
```
Coverage data can be found in `build/*Coverage/index.html`.

### Documentation
Auto generated, using Doxygen, under `build/docs`
Output formats: HTML and Latex.
  - HTML: open `build/docs/html/index.html`
  - Latex:
      1) Navigate to `build/docs/latex`
      2) make pdf
      3) open refman.pdf

Building a Debian Package
-------------------------
A Debian package can be build using git build package:
```
gbp buildpackage -uc -us --git-debian-branch=debian
```
Note: This automatically builds the package.