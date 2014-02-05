/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef json_h
#define json_h

#include "NamespaceImports.h"

#include "js/RootingAPI.h"

namespace js {
class StringBuffer;
}

extern JSObject *
js_InitJSONClass(JSContext *cx, js::HandleObject obj);

extern bool
js_Stringify(JSContext *cx, js::MutableHandleValue vp, JSObject *replacer,
             js::Value space, js::StringBuffer &sb);

namespace js {

extern bool
ParseJSONWithReviver(JSContext *cx, JS::ConstTwoByteChars chars, size_t length,
                     HandleValue reviver, MutableHandleValue vp);

} // namespace js

#endif /* json_h */
