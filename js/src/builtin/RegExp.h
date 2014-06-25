/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_RegExp_h
#define builtin_RegExp_h

#include "vm/RegExpObject.h"

JSObject *
js_InitRegExpClass(JSContext *cx, js::HandleObject obj);

/*
 * The following builtin natives are extern'd for pointer comparison in
 * other parts of the engine.
 */

namespace js {

// Whether RegExp statics should be updated with the input and results of a
// regular expression execution.
enum RegExpStaticsUpdate { UpdateRegExpStatics, DontUpdateRegExpStatics };

RegExpRunStatus
ExecuteRegExp(JSContext *cx, HandleObject regexp, HandleString string,
              MatchPairs &matches, RegExpStaticsUpdate staticsUpdate);

/*
 * Legacy behavior of ExecuteRegExp(), which is baked into the JSAPI.
 *
 * |res| may be nullptr if the RegExpStatics are not to be updated.
 * |input| may be nullptr if there is no JSString corresponding to
 * |chars| and |length|.
 */
bool
ExecuteRegExpLegacy(JSContext *cx, RegExpStatics *res, RegExpObject &reobj,
                    HandleLinearString input, size_t *lastIndex, bool test,
                    MutableHandleValue rval);

/* Translation from MatchPairs to a JS array in regexp_exec()'s output format. */
bool
CreateRegExpMatchResult(JSContext *cx, HandleString input, const MatchPairs &matches,
                        MutableHandleValue rval);

extern bool
regexp_exec_raw(JSContext *cx, HandleObject regexp, HandleString input, MutableHandleValue output);

extern bool
regexp_exec(JSContext *cx, unsigned argc, Value *vp);

bool
regexp_test_raw(JSContext *cx, HandleObject regexp, HandleString input, bool *result);

extern bool
regexp_test(JSContext *cx, unsigned argc, Value *vp);

/*
 * The following functions are for use by self-hosted code.
 */

/*
 * Behaves like regexp.exec(string), but doesn't set RegExp statics.
 *
 * Usage: match = regexp_exec_no_statics(regexp, string)
 */
extern bool
regexp_exec_no_statics(JSContext *cx, unsigned argc, Value *vp);

/*
 * Behaves like regexp.test(string), but doesn't set RegExp statics.
 *
 * Usage: does_match = regexp_test_no_statics(regexp, string)
 */
extern bool
regexp_test_no_statics(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

#endif /* builtin_RegExp_h */
