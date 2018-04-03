//               ============================================
//                 Summary of how the drvFGPDB driver works
//               ============================================
//
//                           Mark Davis, NSCL/FRIB
//
//  NOTE:  This is done in text format to make it easy to do a diff on it to
//         see what changed between versions.  The reason for making all lines
//         "comment" lines is so that editors that support some simple
//         formatting on comment blocks (such as the one I use: SlickEdit) can
//         be used to edit it without having to worry about changes to the line
//         breaks, indentation, etc.
//
//
//  Acronyms:
//     ANL:  Argonne National Laboratory
//     FRIB:  Facility for Rare Isotope Beams
//     IOC:  Input/Output Controller
//     NSCL:  National Superconducting Cyclotron Laboratory
//     FGPDB:  FRIB General Purpose Digital Board
//     LLRF:  Low Level Radio Frequency
//     LCP: LLRF Control Protocol
//
//  The drvFGPDB is designed to be as generic as possible and work with devices
//  that support LCP.  While the name came from the first use of this protocol,
//  the protocol itself is also very generic and makes very few assumptions
//  about the devices that support it.
//
//
//  The drvFGPDB class is the heart of the driver and is derived from the
//  asynPortDriver class that is part of the popular asyn package from ANL.
//  Unlike the orignal version of the driver for LCP devices, it is very
//  generic and can be used without changes or additions for many different
//  devices.
//
//
//  Summary of LCP:
//
//     LCP is designed to support the reading and writing of 32-bit scalar
//     values (referred to as registers) and arrays of 8-bit values.  The
//     register values are grouped in to 3 categories: Read-Only (RO),
//     Write-Anytime (WA), and Write-Once (WO).
//
//     One or more values can be read from the controller at any time, each
//     command reading up to the # of register values in each group.  LCP also
//     supports streaming connections, where the device can send packets
//     containing register values on a periodic basis and/or when values have
//     changed.
//
//     The current value of one or more registers can be read at any time,
//     although each read command is currently limited to one or more registers
//     within a group.
//
//     Write-Anytime registers are generally used for values that represent
//     level-triggered logic:  If a value written to one of these registers is
//     the same as the last value written, the state of the controller will not
//     change (e.g. a setpoint).
//
//     Write-Once registers are generally used for values that represent
//     edge-triggered logic:  Even if the value written to one of these
//     registers is the same as the last value written, it may result in a
//     change (possibly just temporary) in the state of the controller (e.g.
//     increment a count, recalibrate, perform some processing).
//
//     NOTE:  At present, the driver assumes that the Write-Anytime registers
//     represents the set of values that we want to be saved and restored
//     whenever the controller or the IOC is restarted.  This means that, for
//     certain key values, even though they fit the general description for
//     write-anytime values, they often need to be part of the write-once group
//     instead.  The most common example is the on/off state.  While having all
//     the most recent settings for a controller automatically restored after
//     it is reset can be very helpful, allowing the control system to
//     automatically turn it back on is generally NOT what we want to happen.
//
//     Of course, such decisions are entirely dependent on the controller.  For
//     example, if the controller asserts an interlock following each restart,
//     and it ignores commands to turn on until the interlock is cleared, then
//     you could safely include the on/off command in the write-anytime
//     registers and the reset command in the write-once registers.
//
//

