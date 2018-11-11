
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkDrawSaveLayer_DEFINED
#define SkDrawSaveLayer_DEFINED

#include "SkDrawGroup.h"
#include "SkMemberInfo.h"

class SkDrawPaint;
class SkDrawRect;

class SkSaveLayer : public SkGroup {
    DECLARE_MEMBER_INFO(SaveLayer);
    SkSaveLayer();
    virtual ~SkSaveLayer();
    bool draw(SkAnimateMaker& ) override;
#ifdef SK_DUMP_ENABLED
    void dump(SkAnimateMaker* ) override;
#endif
    void onEndElement(SkAnimateMaker& ) override;
protected:
    SkDrawPaint* paint;
    SkDrawRect* bounds;
private:
    typedef SkGroup INHERITED;

};

#endif //SkDrawSaveLayer_DEFINED
