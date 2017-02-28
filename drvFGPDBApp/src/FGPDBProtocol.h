#ifndef FGPDBPROTOCOL_H
#define FGPDBPROTOCOL_H

// Definition of the protocol for FGPDB devices.

enum class CtlrDataFmt {
  NotDefined,
  S32,       // signed 32-bit int
  U32,       // unsigned 32-bit int
  F32,       // 32-bit float
  U16_16,    // = (uint) (value * 2.0^16)
  PHASE      // = (int) (degrees * (2.0^32) / 360.0)
};

enum class ParamGroup {
  Invalid,
  LCP_RO,  // Read-Only LCP register
  LCP_WA,  // Write-Anytime LCP register
  LCP_WO,  // Write-Once LCP register
  DRV_RO,  // Read-Only Driver parameter
  DRV_RW   // Read-Write Driver parameter
};


/* For probable future use
//===== values for StaticRegInfo.syncMode =====
//
//  These values are used to specify what should be done for each writable
//  value when the IOC or the Ctlr has been restarted.
//
//  SM_DN:
//        Don't do anything special to the set value when resyncing the IOC and ctlr states.
//
//  SM_EQ:
//        These are values that, once the IOC has successfully sent the value
//        to the ctlr, should always be the same on the IOC and the ctlr. This
//        means that, if one of them is restarted after they were in sync, the
//        values can be restored from the one that did not restart.
//
//  SM_CM:
//        These are values that must be "reset" whenever the ctlr is restarted.
//        In this case, "reset" means changing the IOC's value to match the
//        ctlrs.  This is needed for values that control the things like
//        on/off, enabled/disabled, etc. where we do NOT want the previous
//        state to be reasserted automatically without human intervention when
//        a ctlr is restarted.
//
//  SM_IM:
//        These are values for which the readback from the controller may not
//        match the last setting.  For example, the IOC sends a new value for a
//        setpoint and the readback value indicates the currently "applied"
//        setpoint as it ramps toward the specified setpoint.  For these
//        values, the IOC copy cannot be (reliably) restored from the ctlr, so
//        a restarted IOC must restore the most recently saved value from the
//        persistent data files.
//
//  Of course, there is no way to guarantee that last saved value is equal to
//  the most recent user-supplied setpoint, but updating the saved copy
//  frequently will minimize the possiblity for a mismatch.
//----------------------------------------------
enum class SyncMode {
  NotDefined,
  SM_DN,   // Do Nothing (just leave default startup values)
  SM_EQ,   // IOC and ctlr values should be EQual
  SM_CM,   // Ctlr Master:  If ctlr restarted, use ctlr value
  SM_IM,   // IOC Master:  If IOC restarted, restore from file
};
*/

#endif // FGPDBPROTOCOL_H