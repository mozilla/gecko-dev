
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkDrawPaint_DEFINED
#define SkDrawPaint_DEFINED

#include "SkADrawable.h"
#include "SkIntArray.h"
#include "SkMemberInfo.h"
#include "SkPaint.h"
#include "SkXfermode.h"

class SkDrawMaskFilter;
class SkDrawPathEffect;
class SkDrawShader;
class SkTransferMode;
class SkDrawTypeface;

class SkDrawPaint : public SkADrawable {
    DECLARE_DRAW_MEMBER_INFO(Paint);
    SkDrawPaint();
    virtual ~SkDrawPaint();
    virtual bool add(SkAnimateMaker* , SkDisplayable* child);
    SkDisplayable* deepCopy(SkAnimateMaker* ) override;
    bool draw(SkAnimateMaker& ) override;
#ifdef SK_DUMP_ENABLED
    void dump(SkAnimateMaker* ) override;
#endif
    void executeFunction(SkDisplayable* target, int index,
        SkTDArray<SkScriptValue>& parameters, SkDisplayTypes type,
        SkScriptValue* ) override;
    const SkFunctionParamType* getFunctionsParameters() override;
    bool getProperty(int index, SkScriptValue* value) const override;
    bool resolveIDs(SkAnimateMaker& maker, SkDisplayable* original, SkApply* apply) override;
protected:
    static const SkFunctionParamType fFunctionParameters[];
    void setupPaint(SkPaint* paint) const;
public:
    SkBool antiAlias;
    SkDrawColor* color;
    SkBool fakeBold;
    SkBool filterBitmap;
    SkBool linearText;
    SkDrawMaskFilter* maskFilter;
    SkDrawPathEffect* pathEffect;
    SkDrawShader* shader;
    SkBool strikeThru;
    SkBool stroke;
    int /*SkPaint::Cap*/ strokeCap;
    int /*SkPaint::Join */ strokeJoin;
    SkScalar strokeMiter;
    SkScalar strokeWidth;
    int /* SkPaint::Style */ style;
    int /* SkPaint::Align */ textAlign;
    SkScalar textScaleX;
    SkScalar textSize;
    SkScalar textSkewX;
    SkDrawTypeface* typeface;
    SkBool underline;
    int /*SkXfermode::Modes*/ xfermode;
    SkBool8 fOwnsColor;
    SkBool8 fOwnsMaskFilter;
    SkBool8 fOwnsPathEffect;
    SkBool8 fOwnsShader;
    SkBool8 fOwnsTransferMode;
    SkBool8 fOwnsTypeface;
private:
    typedef SkADrawable INHERITED;
    friend class SkTextToPath;
    friend class SkSaveLayer;
};

#endif // SkDrawPaint_DEFINED
