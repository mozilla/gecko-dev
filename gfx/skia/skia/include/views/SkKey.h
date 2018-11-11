
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkKey_DEFINED
#define SkKey_DEFINED

#include "SkTypes.h"

enum SkKey {
    //reordering these to match android.app.KeyEvent
    kNONE_SkKey,    //corresponds to android's UNKNOWN

    kLeftSoftKey_SkKey,
    kRightSoftKey_SkKey,

    kHome_SkKey,    //!< the home key - added to match android
    kBack_SkKey,    //!< (CLR)
    kSend_SkKey,    //!< the green (talk) key
    kEnd_SkKey,     //!< the red key

    k0_SkKey,
    k1_SkKey,
    k2_SkKey,
    k3_SkKey,
    k4_SkKey,
    k5_SkKey,
    k6_SkKey,
    k7_SkKey,
    k8_SkKey,
    k9_SkKey,
    kStar_SkKey,    //!< the * key
    kHash_SkKey,    //!< the # key

    kUp_SkKey,
    kDown_SkKey,
    kLeft_SkKey,
    kRight_SkKey,

    kOK_SkKey,      //!< the center key

    kVolUp_SkKey,   //!< volume up - match android
    kVolDown_SkKey, //!< volume down - same
    kPower_SkKey,   //!< power button - same
    kCamera_SkKey,  //!< camera         - same

    kSkKeyCount
};

enum SkModifierKeys {
    kShift_SkModifierKey    = 1 << 0,
    kControl_SkModifierKey  = 1 << 1,
    kOption_SkModifierKey   = 1 << 2,   // same as ALT
    kCommand_SkModifierKey  = 1 << 3,
};

#endif
