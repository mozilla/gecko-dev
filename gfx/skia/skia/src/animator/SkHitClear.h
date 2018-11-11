
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkHitClear_DEFINED
#define SkHitClear_DEFINED

#include "SkDisplayable.h"
#include "SkMemberInfo.h"
#include "SkTypedArray.h"

class SkHitClear : public SkDisplayable {
    DECLARE_MEMBER_INFO(HitClear);
    bool enable(SkAnimateMaker& ) override;
    bool hasEnable() const override;
private:
    SkTDDisplayableArray targets;
};

#endif // SkHitClear_DEFINED
