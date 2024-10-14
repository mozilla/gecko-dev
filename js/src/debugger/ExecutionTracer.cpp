/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "debugger/ExecutionTracer.h"

#include "debugger/Frame.h"       // DebuggerFrameType
#include "vm/ObjectOperations.h"  // DefineDataElement
#include "vm/Time.h"

#include "debugger/Debugger-inl.h"
#include "vm/Stack-inl.h"

using namespace js;

enum class OutOfLineEntryType : uint8_t {
  ScriptURL,
  Atom,
};

enum class InlineEntryType : uint8_t {
  StackFunctionEnter,
  StackFunctionLeave,
  LabelEnter,
  LabelLeave,
};

static ExecutionTracer::ImplementationType GetImplementation(
    AbstractFramePtr frame) {
  if (frame.isBaselineFrame()) {
    return ExecutionTracer::ImplementationType::Baseline;
  }

  if (frame.isRematerializedFrame()) {
    return ExecutionTracer::ImplementationType::Ion;
  }

  if (frame.isWasmDebugFrame()) {
    return ExecutionTracer::ImplementationType::Wasm;
  }

  return ExecutionTracer::ImplementationType::Interpreter;
}

static DebuggerFrameType GetFrameType(AbstractFramePtr frame) {
  // Indirect eval frames are both isGlobalFrame() and isEvalFrame(), so the
  // order of checks here is significant.
  if (frame.isEvalFrame()) {
    return DebuggerFrameType::Eval;
  }

  if (frame.isGlobalFrame()) {
    return DebuggerFrameType::Global;
  }

  if (frame.isFunctionFrame()) {
    return DebuggerFrameType::Call;
  }

  if (frame.isModuleFrame()) {
    return DebuggerFrameType::Module;
  }

  if (frame.isWasmDebugFrame()) {
    return DebuggerFrameType::WasmCall;
  }

  MOZ_CRASH("Unknown frame type");
}

[[nodiscard]] static bool GetFunctionName(JSContext* cx,
                                          JS::Handle<JSFunction*> fun,
                                          JS::MutableHandle<JSAtom*> result) {
  if (!fun->getDisplayAtom(cx, result)) {
    return false;
  }

  if (result) {
    cx->markAtom(result);
  }
  return true;
}

void ExecutionTracer::writeScriptUrl(ScriptSource* scriptSource) {
  outOfLineData_.beginWritingEntry();
  outOfLineData_.write(uint8_t(OutOfLineEntryType::ScriptURL));
  outOfLineData_.write(scriptSource->id());

  if (scriptSource->hasDisplayURL()) {
    outOfLineData_.writeCString<char16_t, TracerStringEncoding::TwoByte>(
        scriptSource->displayURL());
  } else {
    const char* filename =
        scriptSource->filename() ? scriptSource->filename() : "";
    outOfLineData_.writeCString<char, TracerStringEncoding::UTF8>(filename);
  }
  outOfLineData_.finishWritingEntry();
}

bool ExecutionTracer::writeAtom(JSContext* cx, JS::Handle<JSAtom*> atom,
                                uint32_t id) {
  outOfLineData_.beginWritingEntry();
  outOfLineData_.write(uint8_t(OutOfLineEntryType::Atom));
  outOfLineData_.write(id);

  if (!atom) {
    outOfLineData_.writeEmptyString();
  } else {
    if (!outOfLineData_.writeString(cx, atom)) {
      return false;
    }
  }
  outOfLineData_.finishWritingEntry();
  return true;
}

bool ExecutionTracer::writeFunctionFrame(JSContext* cx,
                                         AbstractFramePtr frame) {
  JS::Rooted<JSFunction*> fn(cx, frame.callee());
  TracingCaches& caches = cx->caches().tracingCaches;
  if (fn->baseScript()) {
    uint32_t scriptSourceId = fn->baseScript()->scriptSource()->id();
    TracingCaches::GetOrPutResult scriptSourceRes =
        caches.putScriptSourceIfMissing(scriptSourceId);
    if (scriptSourceRes == TracingCaches::GetOrPutResult::OOM) {
      ReportOutOfMemory(cx);
      return false;
    }
    if (scriptSourceRes == TracingCaches::GetOrPutResult::NewlyAdded) {
      writeScriptUrl(fn->baseScript()->scriptSource());
    }
    inlineData_.write(fn->baseScript()->lineno());
    inlineData_.write(fn->baseScript()->column().oneOriginValue());
    inlineData_.write(scriptSourceId);
  } else {
    // In the case of no baseScript, we just fill it out with 0s. 0 is an
    // invalid script source ID, so it is distinguishable from a real one
    inlineData_.write(uint32_t(0));  // line number
    inlineData_.write(uint32_t(0));  // column
    inlineData_.write(uint32_t(0));  // script source id
  }

  JS::Rooted<JSAtom*> functionName(cx);
  if (!GetFunctionName(cx, fn, &functionName)) {
    return false;
  }
  uint32_t functionNameId = 0;
  TracingCaches::GetOrPutResult fnNameRes =
      caches.getOrPutAtom(functionName, &functionNameId);
  if (fnNameRes == TracingCaches::GetOrPutResult::OOM) {
    ReportOutOfMemory(cx);
    return false;
  }
  if (fnNameRes == TracingCaches::GetOrPutResult::NewlyAdded) {
    if (!writeAtom(cx, functionName, functionNameId)) {
      // It's worth noting here that this will leave the caches out of sync
      // with what has actually been written into the out of line data.
      // This is a normal and allowed situation for the tracer, so we have
      // no special handling here for it. However, if we ever want to make
      // a stronger guarantee in the future, we need to revisit this.
      return false;
    }
  }

  inlineData_.write(functionNameId);
  inlineData_.write(uint8_t(GetImplementation(frame)));
  inlineData_.write(PRMJ_Now());
  return true;
}

bool ExecutionTracer::onEnterFrame(JSContext* cx, AbstractFramePtr frame) {
  DebuggerFrameType type = GetFrameType(frame);
  if (type == DebuggerFrameType::Call) {
    if (frame.isFunctionFrame()) {
      inlineData_.beginWritingEntry();
      inlineData_.write(uint8_t(InlineEntryType::StackFunctionEnter));
      if (!writeFunctionFrame(cx, frame)) {
        return false;
      }

      inlineData_.finishWritingEntry();
    }
  }
  return true;
}

bool ExecutionTracer::onLeaveFrame(JSContext* cx, AbstractFramePtr frame) {
  DebuggerFrameType type = GetFrameType(frame);
  if (type == DebuggerFrameType::Call) {
    if (frame.isFunctionFrame()) {
      inlineData_.beginWritingEntry();
      inlineData_.write(uint8_t(InlineEntryType::StackFunctionLeave));
      if (!writeFunctionFrame(cx, frame)) {
        return false;
      }
      inlineData_.finishWritingEntry();
    }
  }
  return true;
}

template <typename CharType, TracerStringEncoding Encoding>
void ExecutionTracer::onEnterLabel(const CharType* eventType) {
  inlineData_.beginWritingEntry();
  inlineData_.write(uint8_t(InlineEntryType::LabelEnter));
  inlineData_.writeCString<CharType, Encoding>(eventType);
  inlineData_.write(PRMJ_Now());
  inlineData_.finishWritingEntry();
}

template <typename CharType, TracerStringEncoding Encoding>
void ExecutionTracer::onLeaveLabel(const CharType* eventType) {
  inlineData_.beginWritingEntry();
  inlineData_.write(uint8_t(InlineEntryType::LabelLeave));
  inlineData_.writeCString<CharType, Encoding>(eventType);
  inlineData_.write(PRMJ_Now());
  inlineData_.finishWritingEntry();
}

static bool ThrowTracingReadFailed(JSContext* cx) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_NATIVE_TRACING_BUFFER_MALFORMED);
  return false;
}

bool ExecutionTracer::readFunctionFrame(JSContext* cx,
                                        JS::Handle<JSObject*> result,
                                        EventKind kind) {
  MOZ_ASSERT(kind == EventKind::FunctionEnter ||
             kind == EventKind::FunctionLeave);

  uint32_t lineno;
  uint32_t column;
  uint32_t url;
  uint32_t functionName;
  uint8_t implementation;
  uint64_t time;
  inlineData_.read(&lineno);
  inlineData_.read(&column);
  inlineData_.read(&url);
  inlineData_.read(&functionName);
  inlineData_.read(&implementation);
  inlineData_.read(&time);

  if (!NewbornArrayPush(cx, result, Int32Value(int32_t(kind)))) {
    return false;
  }
  if (!NewbornArrayPush(cx, result, Int32Value(lineno))) {
    return false;
  }
  if (!NewbornArrayPush(cx, result, Int32Value(column))) {
    return false;
  }
  if (!NewbornArrayPush(cx, result, Int32Value(url))) {
    return false;
  }
  if (!NewbornArrayPush(cx, result, Int32Value(functionName))) {
    return false;
  }
  if (!NewbornArrayPush(cx, result, Int32Value(implementation))) {
    return false;
  }

  double timeDouble = time / double(PRMJ_USEC_PER_MSEC);
  if (!NewbornArrayPush(cx, result, DoubleValue(timeDouble))) {
    return false;
  }

  return true;
}

bool ExecutionTracer::readStackFunctionEnter(JSContext* cx,
                                             JS::Handle<JSObject*> events) {
  JS::Rooted<JSObject*> obj(cx, NewDenseEmptyArray(cx));
  if (!obj) {
    return false;
  }

  if (!readFunctionFrame(cx, obj, EventKind::FunctionEnter)) {
    return false;
  }

  JS::Rooted<JS::Value> objVal(cx, ObjectValue(*obj));
  if (!NewbornArrayPush(cx, events, objVal)) {
    return false;
  }

  return true;
}

bool ExecutionTracer::readStackFunctionLeave(JSContext* cx,
                                             JS::Handle<JSObject*> events) {
  JS::Rooted<JSObject*> obj(cx, NewDenseEmptyArray(cx));
  if (!obj) {
    return false;
  }

  if (!readFunctionFrame(cx, obj, EventKind::FunctionLeave)) {
    return false;
  }
  JS::Rooted<JS::Value> objVal(cx, ObjectValue(*obj));
  if (!NewbornArrayPush(cx, events, objVal)) {
    return false;
  }

  return true;
}

bool ExecutionTracer::readScriptURLEntry(JSContext* cx,
                                         JS::Handle<JSObject*> scriptUrls) {
  uint32_t id;
  outOfLineData_.read(&id);

  JS::Rooted<JSString*> url(cx);
  if (!outOfLineData_.readString(cx, &url)) {
    return false;
  }

  JS::Rooted<JS::Value> urlVal(cx, StringValue(url));
  if (!DefineDataElement(cx, scriptUrls, id, urlVal, JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}

bool ExecutionTracer::readAtomEntry(JSContext* cx,
                                    JS::Handle<JSObject*> atoms) {
  uint32_t id;
  outOfLineData_.read(&id);
  JS::Rooted<JSString*> url(cx);
  if (!outOfLineData_.readString(cx, &url)) {
    return false;
  }

  JS::Rooted<JS::Value> atom(cx, StringValue(url));
  if (!DefineDataElement(cx, atoms, id, atom, JSPROP_ENUMERATE)) {
    return false;
  }

  return true;
}

bool ExecutionTracer::readLabel(JSContext* cx, JS::Handle<JSObject*> events,
                                EventKind kind) {
  MOZ_ASSERT(kind == EventKind::LabelEnter || kind == EventKind::LabelLeave);

  JS::Rooted<JSObject*> obj(cx, NewDenseEmptyArray(cx));
  if (!obj) {
    return false;
  }

  if (!NewbornArrayPush(cx, obj, Int32Value(int32_t(kind)))) {
    return false;
  }

  JS::Rooted<JSString*> eventType(cx);
  if (!inlineData_.readString(cx, &eventType)) {
    return false;
  }
  if (!NewbornArrayPush(cx, obj, StringValue(eventType))) {
    return false;
  }

  uint64_t time;
  inlineData_.read(&time);

  double timeDouble = time / double(PRMJ_USEC_PER_MSEC);
  if (!NewbornArrayPush(cx, obj, DoubleValue(timeDouble))) {
    return false;
  }

  JS::Rooted<JS::Value> objVal(cx, ObjectValue(*obj));
  if (!NewbornArrayPush(cx, events, objVal)) {
    return false;
  }

  return true;
}

bool ExecutionTracer::readInlineEntry(JSContext* cx,
                                      JS::Handle<JSObject*> events) {
  uint8_t entryType;
  inlineData_.read(&entryType);

  switch (InlineEntryType(entryType)) {
    case InlineEntryType::StackFunctionEnter:
      return readStackFunctionEnter(cx, events);
    case InlineEntryType::StackFunctionLeave:
      return readStackFunctionLeave(cx, events);
    case InlineEntryType::LabelEnter:
      return readLabel(cx, events, EventKind::LabelEnter);
    case InlineEntryType::LabelLeave:
      return readLabel(cx, events, EventKind::LabelLeave);
    default:
      return ThrowTracingReadFailed(cx);
  }
}

bool ExecutionTracer::readOutOfLineEntry(JSContext* cx,
                                         JS::Handle<JSObject*> scriptUrls,
                                         JS::Handle<JSObject*> atoms) {
  uint8_t entryType;
  outOfLineData_.read(&entryType);

  switch (OutOfLineEntryType(entryType)) {
    case OutOfLineEntryType::ScriptURL:
      return readScriptURLEntry(cx, scriptUrls);
    case OutOfLineEntryType::Atom:
      return readAtomEntry(cx, atoms);
    default:
      return ThrowTracingReadFailed(cx);
  }
}

bool ExecutionTracer::readInlineEntries(JSContext* cx,
                                        JS::Handle<JSObject*> events) {
  while (inlineData_.readable()) {
    inlineData_.beginReadingEntry();
    if (!readInlineEntry(cx, events)) {
      inlineData_.skipEntry();
      return false;
    }
    inlineData_.finishReadingEntry();
  }
  return true;
}

bool ExecutionTracer::readOutOfLineEntries(JSContext* cx,
                                           JS::Handle<JSObject*> scriptUrls,
                                           JS::Handle<JSObject*> atoms) {
  while (outOfLineData_.readable()) {
    outOfLineData_.beginReadingEntry();
    if (!readOutOfLineEntry(cx, scriptUrls, atoms)) {
      outOfLineData_.skipEntry();
      return false;
    }
    outOfLineData_.finishReadingEntry();
  }
  return true;
}

bool ExecutionTracer::getTrace(JSContext* cx, JS::Handle<JSObject*> result) {
  // TODO: the long term goal for traces is to be able to collect this data
  // live, while the tracer is still capturing, as well as all at once, which
  // this method covers. Bug 1910182 tracks the next step for the live tracing
  // case, which may in the end involve a similar method to this being called
  // from a separate process than the process containing the traced JSContext.
  // If we go down that route, the buffer would be shared via a shmem.
  JS::Rooted<JSObject*> scriptUrls(cx, NewPlainObject(cx));
  if (!scriptUrls) {
    return false;
  }
  JS::Rooted<JS::Value> scriptUrlsVal(cx, ObjectValue(*scriptUrls));
  if (!JS_DefineProperty(cx, result, "scriptURLs", scriptUrlsVal,
                         JSPROP_ENUMERATE)) {
    return false;
  }

  JS::Rooted<JSObject*> atoms(cx, NewPlainObject(cx));
  if (!atoms) {
    return false;
  }
  JS::Rooted<JS::Value> atomsVal(cx, ObjectValue(*atoms));
  if (!JS_DefineProperty(cx, result, "atoms", atomsVal, JSPROP_ENUMERATE)) {
    return false;
  }

  JS::Rooted<JSObject*> events(cx, NewDenseEmptyArray(cx));
  if (!events) {
    return false;
  }
  JS::Rooted<JS::Value> eventsVal(cx, ObjectValue(*events));
  if (!JS_DefineProperty(cx, result, "events", eventsVal, JSPROP_ENUMERATE)) {
    return false;
  }

  if (!readOutOfLineEntries(cx, scriptUrls, atoms)) {
    return false;
  }

  if (!readInlineEntries(cx, events)) {
    return false;
  }

  return true;
}

void JS_TracerEnterLabelTwoByte(JSContext* cx, const char16_t* label) {
  if (cx->hasExecutionTracer()) {
    cx->getExecutionTracer()
        .onEnterLabel<char16_t, TracerStringEncoding::TwoByte>(label);
  }
}

void JS_TracerEnterLabelLatin1(JSContext* cx, const char* label) {
  if (cx->hasExecutionTracer()) {
    cx->getExecutionTracer().onEnterLabel<char, TracerStringEncoding::Latin1>(
        label);
  }
}

void JS_TracerLeaveLabelTwoByte(JSContext* cx, const char16_t* label) {
  if (cx->hasExecutionTracer()) {
    cx->getExecutionTracer()
        .onLeaveLabel<char16_t, TracerStringEncoding::TwoByte>(label);
  }
}

void JS_TracerLeaveLabelLatin1(JSContext* cx, const char* label) {
  if (cx->hasExecutionTracer()) {
    cx->getExecutionTracer().onLeaveLabel<char, TracerStringEncoding::Latin1>(
        label);
  }
}
