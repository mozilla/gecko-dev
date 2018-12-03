/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeCompiler_h
#define frontend_BytecodeCompiler_h

#include "mozilla/Maybe.h"

#include "NamespaceImports.h"

#include "js/CompileOptions.h"
#include "js/SourceText.h"
#include "vm/Scope.h"
#include "vm/TraceLogging.h"

class JSLinearString;

namespace js {

class LazyScript;
class ModuleObject;
class ScriptSourceObject;

namespace frontend {

class ErrorReporter;
class FunctionBox;
class ParseNode;

#if defined(JS_BUILD_BINAST)

JSScript* CompileGlobalBinASTScript(
    JSContext* cx, LifoAlloc& alloc, const JS::ReadOnlyCompileOptions& options,
    const uint8_t* src, size_t len,
    ScriptSourceObject** sourceObjectOut = nullptr);

MOZ_MUST_USE bool CompileLazyBinASTFunction(JSContext* cx,
                                            Handle<LazyScript*> lazy,
                                            const uint8_t* buf, size_t length);

#endif  // JS_BUILD_BINAST

ModuleObject* CompileModule(JSContext* cx,
                            const JS::ReadOnlyCompileOptions& options,
                            JS::SourceText<char16_t>& srcBuf);

ModuleObject* CompileModule(JSContext* cx,
                            const JS::ReadOnlyCompileOptions& options,
                            JS::SourceText<char16_t>& srcBuf,
                            ScriptSourceObject** sourceObjectOut);

//
// Compile a single function. The source in srcBuf must match the ECMA-262
// FunctionExpression production.
//
// If nonzero, parameterListEnd is the offset within srcBuf where the parameter
// list is expected to end. During parsing, if we find that it ends anywhere
// else, it's a SyntaxError. This is used to implement the Function constructor;
// it's how we detect that these weird cases are SyntaxErrors:
//
//     Function("/*", "*/x) {")
//     Function("x){ if (3", "return x;}")
//
MOZ_MUST_USE bool CompileStandaloneFunction(
    JSContext* cx, MutableHandleFunction fun,
    const JS::ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const mozilla::Maybe<uint32_t>& parameterListEnd,
    HandleScope enclosingScope = nullptr);

MOZ_MUST_USE bool CompileStandaloneGenerator(
    JSContext* cx, MutableHandleFunction fun,
    const JS::ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const mozilla::Maybe<uint32_t>& parameterListEnd);

MOZ_MUST_USE bool CompileStandaloneAsyncFunction(
    JSContext* cx, MutableHandleFunction fun,
    const JS::ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const mozilla::Maybe<uint32_t>& parameterListEnd);

MOZ_MUST_USE bool CompileStandaloneAsyncGenerator(
    JSContext* cx, MutableHandleFunction fun,
    const JS::ReadOnlyCompileOptions& options, JS::SourceText<char16_t>& srcBuf,
    const mozilla::Maybe<uint32_t>& parameterListEnd);

ScriptSourceObject* CreateScriptSourceObject(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    const mozilla::Maybe<uint32_t>& parameterListEnd = mozilla::Nothing());

/*
 * True if str consists of an IdentifierStart character, followed by one or
 * more IdentifierPart characters, i.e. it matches the IdentifierName production
 * in the language spec.
 *
 * This returns true even if str is a keyword like "if".
 *
 * Defined in TokenStream.cpp.
 */
bool IsIdentifier(JSLinearString* str);

bool IsIdentifierNameOrPrivateName(JSLinearString* str);

/*
 * As above, but taking chars + length.
 */
bool IsIdentifier(const Latin1Char* chars, size_t length);
bool IsIdentifier(const char16_t* chars, size_t length);

bool IsIdentifierNameOrPrivateName(const Latin1Char* chars, size_t length);
bool IsIdentifierNameOrPrivateName(const char16_t* chars, size_t length);

/* True if str is a keyword. Defined in TokenStream.cpp. */
bool IsKeyword(JSLinearString* str);

/* Trace all GC things reachable from parser. Defined in Parser.cpp. */
void TraceParser(JSTracer* trc, JS::AutoGCRooter* parser);

#if defined(JS_BUILD_BINAST)

/* Trace all GC things reachable from binjs parser. Defined in
 * BinASTParserPerTokenizer.cpp. */
void TraceBinParser(JSTracer* trc, JS::AutoGCRooter* parser);

#endif  // defined(JS_BUILD_BINAST)

class MOZ_STACK_CLASS AutoFrontendTraceLog {
#ifdef JS_TRACE_LOGGING
  TraceLoggerThread* logger_;
  mozilla::Maybe<TraceLoggerEvent> frontendEvent_;
  mozilla::Maybe<AutoTraceLog> frontendLog_;
  mozilla::Maybe<AutoTraceLog> typeLog_;
#endif

 public:
  AutoFrontendTraceLog(JSContext* cx, const TraceLoggerTextId id,
                       const ErrorReporter& reporter);

  AutoFrontendTraceLog(JSContext* cx, const TraceLoggerTextId id,
                       const ErrorReporter& reporter, FunctionBox* funbox);

  AutoFrontendTraceLog(JSContext* cx, const TraceLoggerTextId id,
                       const ErrorReporter& reporter, ParseNode* pn);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeCompiler_h */
