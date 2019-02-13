/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsatom.h"

#include "jsapi-tests/tests.h"
#include "vm/StringBuffer.h"

BEGIN_TEST(testStringBuffer_finishString)
{
    JSString* str = JS_NewStringCopyZ(cx, "foopy");
    CHECK(str);

    JS::Rooted<JSAtom*> atom(cx, js::AtomizeString(cx, str));
    CHECK(atom);

    js::StringBuffer buffer(cx);
    CHECK(buffer.append("foopy"));

    JS::Rooted<JSAtom*> finishedAtom(cx, buffer.finishAtom());
    CHECK(finishedAtom);
    CHECK_EQUAL(atom, finishedAtom);
    return true;
}
END_TEST(testStringBuffer_finishString)
