
/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#ifndef SkDisplayMath_DEFINED
#define SkDisplayMath_DEFINED

#include "SkDisplayable.h"
#include "SkMemberInfo.h"
#include "SkRandom.h"

class SkDisplayMath : public SkDisplayable {
    DECLARE_DISPLAY_MEMBER_INFO(Math);
    void executeFunction(SkDisplayable* , int index,
        SkTDArray<SkScriptValue>& parameters, SkDisplayTypes type,
        SkScriptValue* ) override;
    const SkFunctionParamType* getFunctionsParameters() override;
    bool getProperty(int index, SkScriptValue* value) const override;
private:
    mutable SkRandom fRandom;
    static const SkScalar gConstants[];
    static const SkFunctionParamType fFunctionParameters[];

};

#endif // SkDisplayMath_DEFINED
