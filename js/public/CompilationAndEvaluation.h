/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Functions for compiling and evaluating scripts. */

#ifndef js_CompilationAndEvaluation_h
#define js_CompilationAndEvaluation_h

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <stddef.h>  // size_t
#include <stdio.h>   // FILE

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/CompileOptions.h"  // JS::CompileOptions, JS::ReadOnlyCompileOptions
#include "js/RootingAPI.h"      // JS::Handle, JS::MutableHandle
#include "js/Value.h"  // JS::Value and specializations of JS::*Handle-related types

struct JSContext;
class JSFunction;
class JSObject;
class JSScript;

namespace JS {

template <typename T>
class AutoVector;

template <typename UnitT>
class SourceText;

}  // namespace JS

/**
 * Given a buffer, return false if the buffer might become a valid JavaScript
 * script with the addition of more lines, or true if the validity of such a
 * script is conclusively known (because it's the prefix of a valid script --
 * and possibly the entirety of such a script).
 *
 * The intent of this function is to enable interactive compilation: accumulate
 * lines in a buffer until JS_Utf8BufferIsCompilableUnit is true, then pass it
 * to the compiler.
 *
 * The provided buffer is interpreted as UTF-8 data.  An error is reported if
 * a UTF-8 encoding error is encountered.
 */
extern JS_PUBLIC_API bool JS_Utf8BufferIsCompilableUnit(
    JSContext* cx, JS::Handle<JSObject*> obj, const char* utf8, size_t length);

/*
 * NB: JS_ExecuteScript and the JS::Evaluate APIs come in two flavors: either
 * they use the global as the scope, or they take an AutoObjectVector of objects
 * to use as the scope chain.  In the former case, the global is also used as
 * the "this" keyword value and the variables object (ECMA parlance for where
 * 'var' and 'function' bind names) of the execution context for script.  In the
 * latter case, the first object in the provided list is used, unless the list
 * is empty, in which case the global is used.
 *
 * Why a runtime option?  The alternative is to add APIs duplicating those
 * for the other value of flags, and that doesn't seem worth the code bloat
 * cost.  Such new entry points would probably have less obvious names, too, so
 * would not tend to be used.  The ContextOptionsRef adjustment, OTOH, can be
 * more easily hacked into existing code that does not depend on the bug; such
 * code can continue to use the familiar JS::Evaluate, etc., entry points.
 */

/**
 * Evaluate a script in the scope of the current global of cx.
 */
extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::Handle<JSScript*> script,
                                           JS::MutableHandle<JS::Value> rval);

extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::Handle<JSScript*> script);

/**
 * As above, but providing an explicit scope chain.  envChain must not include
 * the global object on it; that's implicit.  It needs to contain the other
 * objects that should end up on the script's scope chain.
 */
extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::AutoVector<JSObject*>& envChain,
                                           JS::Handle<JSScript*> script,
                                           JS::MutableHandle<JS::Value> rval);

extern JS_PUBLIC_API bool JS_ExecuteScript(JSContext* cx,
                                           JS::AutoVector<JSObject*>& envChain,
                                           JS::Handle<JSScript*> script);

namespace JS {

/**
 * Like the above, but handles a cross-compartment script. If the script is
 * cross-compartment, it is cloned into the current compartment before
 * executing.
 */
extern JS_PUBLIC_API bool CloneAndExecuteScript(JSContext* cx,
                                                Handle<JSScript*> script,
                                                MutableHandle<Value> rval);

/**
 * Like CloneAndExecuteScript above, but allows executing under a non-syntactic
 * environment chain.
 */
extern JS_PUBLIC_API bool CloneAndExecuteScript(JSContext* cx,
                                                AutoVector<JSObject*>& envChain,
                                                Handle<JSScript*> script,
                                                MutableHandle<Value> rval);

/**
 * Evaluate the given source buffer in the scope of the current global of cx.
 */
extern JS_PUBLIC_API bool Evaluate(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   SourceText<char16_t>& srcBuf,
                                   MutableHandle<Value> rval);

/**
 * As above, but providing an explicit scope chain.  envChain must not include
 * the global object on it; that's implicit.  It needs to contain the other
 * objects that should end up on the script's scope chain.
 */
extern JS_PUBLIC_API bool Evaluate(JSContext* cx,
                                   AutoVector<JSObject*>& envChain,
                                   const ReadOnlyCompileOptions& options,
                                   SourceText<char16_t>& srcBuf,
                                   MutableHandle<Value> rval);

/**
 * Evaluate the provided UTF-8 data in the scope of the current global of |cx|,
 * and return the completion value in |rval|.  If the data contains invalid
 * UTF-8, an error is reported.
 */
extern JS_PUBLIC_API bool EvaluateUtf8(JSContext* cx,
                                       const ReadOnlyCompileOptions& options,
                                       const char* bytes, size_t length,
                                       MutableHandle<Value> rval);

/**
 * Evaluate the provided Latin-1 data (i.e. each byte directly corresponds to
 * the same Unicode code point) in the scope of the current global of |cx|, and
 * return the completion value in |rval|.
 *
 * This function may eventually be removed, such that *only* bytes containing
 * UTF-8 source text may be directly compiled.  Avoid using it if you can.
 */
extern JS_PUBLIC_API bool EvaluateLatin1(JSContext* cx,
                                         const ReadOnlyCompileOptions& options,
                                         const char* bytes, size_t length,
                                         MutableHandle<Value> rval);

/**
 * Evaluate the UTF-8 contents of the file at the given path, and return the
 * completion value in |rval|.  (The path itself is in the system encoding, not
 * [necessarily] UTF-8.)  If the contents contain any malformed UTF-8, an error
 * is reported.
 */
extern JS_PUBLIC_API bool EvaluateUtf8Path(
    JSContext* cx, const ReadOnlyCompileOptions& options, const char* filename,
    MutableHandle<Value> rval);

/**
 * |script| will always be set. On failure, it will be set to nullptr.
 */
extern JS_PUBLIC_API bool Compile(JSContext* cx,
                                  const ReadOnlyCompileOptions& options,
                                  SourceText<char16_t>& srcBuf,
                                  MutableHandle<JSScript*> script);

/**
 * Identical to |JS::Compile|, but compiles UTF-8.
 *
 * The "DontInflate" suffix is temporary while bugs in UTF-8 compilation are
 * ironed out.  In the long term this function and |JS::Compile| will follow
 * the same naming scheme.
 *
 * NOTE: This function DOES NOT INFLATE the UTF-8 bytes to UTF-16 before
 *       compiling them.  UTF-8 compilation is currently experimental and has
 *       known bugs.  Use only if you're willing to tolerate unspecified bugs!
 */
extern JS_PUBLIC_API bool CompileDontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<mozilla::Utf8Unit>& srcBuf, MutableHandle<JSScript*> script);

/**
 * Compile the provided UTF-8 data into a script.  If the data contains invalid
 * UTF-8, an error is reported.
 *
 * |script| is always set to the compiled script or to null in case of error.
 */
extern JS_PUBLIC_API bool CompileUtf8(JSContext* cx,
                                      const ReadOnlyCompileOptions& options,
                                      const char* bytes, size_t length,
                                      MutableHandle<JSScript*> script);

/**
 * Compile the provided UTF-8 data into a script.  If the data contains invalid
 * UTF-8, an error is reported.
 *
 * |script| is always set to the compiled script or to null in case of error.
 *
 * NOTE: This function DOES NOT INFLATE the UTF-8 bytes to UTF-16 before
 *       compiling them.  UTF-8 compilation is currently experimental and has
 *       known bugs.  Use only if you're willing to tolerate unspecified bugs!
 */
extern JS_PUBLIC_API bool CompileUtf8DontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options, const char* bytes,
    size_t length, MutableHandle<JSScript*> script);

/**
 * Compile the provided Latin-1 data (i.e. each byte directly corresponds to
 * the same Unicode code point) into a script.
 *
 * This function may eventually be removed, such that *only* bytes containing
 * UTF-8 source text may be directly compiled.  Avoid using it if you can.
 *
 * |script| is always set to the compiled script or to null in case of error.
 */
extern JS_PUBLIC_API bool CompileLatin1(JSContext* cx,
                                        const ReadOnlyCompileOptions& options,
                                        const char* bytes, size_t length,
                                        MutableHandle<JSScript*> script);

/**
 * Compile the UTF-8 contents of the given file into a script.  If the contents
 * contain any malformed UTF-8, an error is reported.
 *
 * |script| is always set to the compiled script or to null in case of error.
 */
extern JS_PUBLIC_API bool CompileUtf8File(JSContext* cx,
                                          const ReadOnlyCompileOptions& options,
                                          FILE* file,
                                          MutableHandle<JSScript*> script);

/**
 * Compile the UTF-8 contents of the given file into a script.  If the contents
 * contain any malformed UTF-8, an error is reported.
 *
 * |script| is always set to the compiled script or to null in case of error.
 *
 * NOTE: This function DOES NOT INFLATE the UTF-8 bytes to UTF-16 before
 *       compiling them.  UTF-8 compilation is currently experimental and has
 *       known bugs.  Use only if you're willing to tolerate unspecified bugs!
 */
extern JS_PUBLIC_API bool CompileUtf8FileDontInflate(
    JSContext* cx, const ReadOnlyCompileOptions& options, FILE* file,
    MutableHandle<JSScript*> script);

/**
 * Compile the UTF-8 contents of the file at the given path into a script.
 * (The path itself is in the system encoding, not [necessarily] UTF-8.)  If
 * the contents contain any malformed UTF-8, an error is reported.
 *
 * |script| is always set to the compiled script or to null in case of error.
 */
extern JS_PUBLIC_API bool CompileUtf8Path(JSContext* cx,
                                          const ReadOnlyCompileOptions& options,
                                          const char* filename,
                                          MutableHandle<JSScript*> script);

extern JS_PUBLIC_API bool CompileForNonSyntacticScope(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<char16_t>& srcBuf, MutableHandle<JSScript*> script);

/**
 * Compile the given Latin-1 data for non-syntactic scope.
 *
 * There is no way to compile UTF-8 data for non-syntactic scope because no
 * user currently needs it.  Such way could be added in the future if it's ever
 * needed.
 */
extern JS_PUBLIC_API bool CompileLatin1ForNonSyntacticScope(
    JSContext* cx, const ReadOnlyCompileOptions& options, const char* bytes,
    size_t length, MutableHandle<JSScript*> script);

/**
 * Compile a function with envChain plus the global as its scope chain.
 * envChain must contain objects in the current compartment of cx.  The actual
 * scope chain used for the function will consist of With wrappers for those
 * objects, followed by the current global of the compartment cx is in.  This
 * global must not be explicitly included in the scope chain.
 */
extern JS_PUBLIC_API bool CompileFunction(JSContext* cx,
                                          AutoVector<JSObject*>& envChain,
                                          const ReadOnlyCompileOptions& options,
                                          const char* name, unsigned nargs,
                                          const char* const* argnames,
                                          SourceText<char16_t>& srcBuf,
                                          MutableHandle<JSFunction*> fun);

/**
 * Same as above, but taking UTF-8 encoded const char* for the function body.
 */
extern JS_PUBLIC_API bool CompileFunctionUtf8(
    JSContext* cx, AutoVector<JSObject*>& envChain,
    const ReadOnlyCompileOptions& options, const char* name, unsigned nargs,
    const char* const* argnames, const char* utf8, size_t length,
    MutableHandle<JSFunction*> fun);

/*
 * Associate an element wrapper and attribute name with a previously compiled
 * script, for debugging purposes. Calling this function is optional, but should
 * be done before script execution if it is required.
 */
extern JS_PUBLIC_API bool InitScriptSourceElement(
    JSContext* cx, Handle<JSScript*> script, Handle<JSObject*> element,
    Handle<JSString*> elementAttrName = nullptr);

/*
 * For a script compiled with the hideScriptFromDebugger option, expose the
 * script to the debugger by calling the debugger's onNewScript hook.
 */
extern JS_PUBLIC_API void ExposeScriptToDebugger(JSContext* cx,
                                                 Handle<JSScript*> script);

} /* namespace JS */

#endif /* js_CompilationAndEvaluation_h */
