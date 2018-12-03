/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_SelfHosting_h_
#define vm_SelfHosting_h_

#include "jsapi.h"
#include "NamespaceImports.h"

#include "vm/Stack.h"

namespace js {

/*
 * Check whether the given JSFunction is a self-hosted function whose
 * self-hosted name is the given name.
 */
bool IsSelfHostedFunctionWithName(JSFunction* fun, JSAtom* name);

JSAtom* GetSelfHostedFunctionName(JSFunction* fun);

bool IsCallSelfHostedNonGenericMethod(NativeImpl impl);

bool ReportIncompatibleSelfHostedMethod(JSContext* cx, const CallArgs& args);

/* Get the compile options used when compiling self hosted code. */
void FillSelfHostingCompileOptions(JS::CompileOptions& options);

#ifdef DEBUG
/*
 * Calls a self-hosted function by name.
 *
 * This function is only available in debug mode, because it always atomizes
 * its |name| parameter. Use the alternative function below in non-debug code.
 */
bool CallSelfHostedFunction(JSContext* cx, char const* name, HandleValue thisv,
                            const AnyInvokeArgs& args, MutableHandleValue rval);
#endif

/*
 * Calls a self-hosted function by name.
 */
bool CallSelfHostedFunction(JSContext* cx, HandlePropertyName name,
                            HandleValue thisv, const AnyInvokeArgs& args,
                            MutableHandleValue rval);

bool intrinsic_StringSplitString(JSContext* cx, unsigned argc, JS::Value* vp);

bool intrinsic_NewArrayIterator(JSContext* cx, unsigned argc, JS::Value* vp);

bool intrinsic_NewStringIterator(JSContext* cx, unsigned argc, JS::Value* vp);

bool intrinsic_IsSuspendedGenerator(JSContext* cx, unsigned argc,
                                    JS::Value* vp);

} /* namespace js */

#endif /* vm_SelfHosting_h_ */
