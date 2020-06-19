/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JSControl.h"

#include "mozilla/Base64.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/JSON.h"
#include "js/PropertySpec.h"
#include "ChildInternal.h"
#include "ParentInternal.h"
#include "nsImportModule.h"
#include "rrIModule.h"
#include "xpcprivate.h"

#include "nsMediaFeatures.h"

using namespace JS;

namespace mozilla {
namespace recordreplay {
namespace js {

// Callback for filling CharBuffers when converting objects to JSON.
static bool FillCharBufferCallback(const char16_t* buf, uint32_t len,
                                   void* data) {
  CharBuffer* buffer = (CharBuffer*)data;
  MOZ_RELEASE_ASSERT(buffer->length() == 0);
  buffer->append(buf, len);
  return true;
}

static JSObject* RequireObject(JSContext* aCx, HandleValue aValue) {
  if (!aValue.isObject()) {
    JS_ReportErrorASCII(aCx, "Expected object");
    return nullptr;
  }
  return &aValue.toObject();
}

static bool RequireNumber(JSContext* aCx, HandleValue aValue, size_t* aNumber) {
  if (!aValue.isNumber()) {
    JS_ReportErrorASCII(aCx, "Expected number");
    return false;
  }
  *aNumber = aValue.toNumber();
  return true;
}

static void InitializeScriptHits();

static nsCString gModuleText;

void ReadReplayJS(const char* aFile) {
  FileHandle file = DirectOpenFile(aFile, /* aWriting */ false);
  size_t size = DirectFileSize(file);
  gModuleText.SetLength(size);
  DirectRead(file, gModuleText.BeginWriting(), size);
  DirectCloseFile(file);
}

// URL of the root module script.
#define ModuleURL "resource://devtools/server/actors/replay/module.js"

static StaticRefPtr<rrIModule> gModule;
static PersistentRootedObject* gModuleObject;

bool IsInitialized() {
  return !!gModule;
}

static void EnsureInitialized() {
  if (IsInitialized()) {
    return;
  }

  // Initialization so we can repaint at the first checkpoint without having
  // an unhandled recording divergence.
  nsMediaFeatures::InitSystemMetrics();

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  nsCOMPtr<rrIModule> module = do_ImportModule(ModuleURL);
  gModule = module.forget();
  ClearOnShutdown(&gModule);

  if (IsReplaying()) {
    MOZ_RELEASE_ASSERT(gModuleText.Length());
  }

  RootedValue value(cx);
  if (NS_FAILED(gModule->Initialize(gModuleText, &value))) {
    MOZ_CRASH("EnsureInitialized: Initialize failed");
  }
  MOZ_RELEASE_ASSERT(value.isObject());

  gModuleObject = new PersistentRootedObject(cx);
  *gModuleObject = &value.toObject();

  if (IsRecordingOrReplaying()) {
    InitializeScriptHits();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Devtools Sandbox
///////////////////////////////////////////////////////////////////////////////

extern "C" {

MOZ_EXPORT bool RecordReplayInterface_ShouldUpdateProgressCounter(
    const char* aURL) {
  // Progress counters are only updated for scripts which are exposed to the
  // debugger. The devtools timeline is based on progress values and we don't
  // want gaps on the timeline which users can't seek to.
  return aURL && strncmp(aURL, "resource:", 9) && strncmp(aURL, "chrome:", 7);
}

}  // extern "C"

#undef ReplayScriptURL

void ManifestStart(const CharBuffer& aContents) {
  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  RootedValue value(cx);
  if (!JS_ParseJSON(cx, aContents.begin(), aContents.length(), &value)) {
    MOZ_CRASH("ManifestStart: ParseJSON failed");
  }

  RootedValue rv(cx);
  HandleValueArray args(value);
  if (!JS_CallFunctionName(cx, *gModuleObject, "ManifestStart", args, &rv)) {
    MOZ_CRASH("ManifestStart: Handler failed");
  }

  // Processing the manifest may have called into MaybeDivergeFromRecording.
  // If it did so, we should already have finished any processing that required
  // diverging from the recording. Don't tolerate future events that
  // would otherwise cause us to rewind to the last checkpoint.
  DisallowUnhandledDivergeFromRecording();
}

void HitCheckpoint(size_t aCheckpoint, TimeDuration aTime) {
  EnsureInitialized();

  if (IsRecording()) {
    return;
  }

  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  RootedValue rv(cx);
  JS::AutoValueArray<2> args(cx);
  args[0].setInt32(aCheckpoint);
  args[1].setInt32(aTime.ToMilliseconds());
  if (!JS_CallFunctionName(cx, *gModuleObject, "HitCheckpoint", args, &rv)) {
    MOZ_CRASH("HitCheckpoint");
  }
}

bool CanCreateCheckpoint() {
  if (!IsInitialized()) {
    return true;
  }

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "CanCreateCheckpoint",
                           JS::HandleValueArray::empty(), &rv)) {
    MOZ_CRASH("CanCreateCheckpoint");
  }

  return ToBoolean(rv);
}

static ProgressCounter gProgressCounter;

static inline void SetProgressCounter(ProgressCounter aValue) {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  gProgressCounter = aValue;
}

extern "C" {

MOZ_EXPORT ProgressCounter* RecordReplayInterface_ExecutionProgressCounter() {
  return &gProgressCounter;
}

MOZ_EXPORT void RecordReplayInterface_AdvanceExecutionProgressCounter() {
  SetProgressCounter(gProgressCounter + 1);
}

MOZ_EXPORT ProgressCounter RecordReplayInterface_NewTimeWarpTarget() {
  if (AreThreadEventsDisallowed()) {
    return 0;
  }

  // NewTimeWarpTarget() must be called at consistent points between recording
  // and replaying.
  RecordReplayAssert("NewTimeWarpTarget");

  if (!IsInitialized() || IsRecording()) {
    return 0;
  }

  AutoDisallowThreadEvents disallow;
  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "NewTimeWarpTarget", HandleValueArray::empty(), &rv)) {
    MOZ_CRASH("NewTimeWarpTarget");
  }

  MOZ_RELEASE_ASSERT(rv.isNumber());
  return rv.toNumber();
}

}  // extern "C"

void PaintComplete(size_t aCheckpoint) {
  if (IsRecording()) {
    return;
  }

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  JS::AutoValueArray<1> args(cx);
  args[0].setInt32(aCheckpoint);

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "PaintComplete", args, &rv)) {
    MOZ_CRASH("PaintComplete");
  }
}

void OnMouseEvent(const TimeDuration& aTime, const char* aType, int32_t aX, int32_t aY) {
  if (!IsInitialized()) {
    return;
  }

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  JSString* str = JS_AtomizeString(cx, aType);
  if (!str) {
    MOZ_CRASH("OnMouseEvent");
  }

  JS::AutoValueArray<4> args(cx);
  args[0].setInt32(aTime.ToMilliseconds());
  args[1].setString(str);
  args[2].setInt32(aX);
  args[3].setInt32(aY);

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "OnMouseEvent", args, &rv)) {
    MOZ_CRASH("OnMouseEvent");
  }
}

void SendRecordingData(size_t aOffset, const uint8_t* aData, size_t aLength,
                       const Maybe<size_t>& aTotalLength,
                       const Maybe<TimeDuration>& aRecordingDuration) {
  MOZ_RELEASE_ASSERT(IsInitialized());

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  JS::Rooted<JSObject*> bufferObject(cx);
  bufferObject = JS::NewArrayBufferWithUserOwnedContents(cx, aLength, (void*)aData);
  MOZ_RELEASE_ASSERT(bufferObject);

  JS::AutoValueArray<6> args(cx);
  args[0].setNumber((double)child::MiddlemanProcessId());
  args[1].setNumber((double)aOffset);
  args[2].setNumber((double)aLength);
  args[3].setObject(*bufferObject);
  if (aTotalLength.isSome()) {
    args[4].setNumber((double)aTotalLength.ref());
  }
  if (aRecordingDuration.isSome()) {
    args[5].setNumber(aRecordingDuration.ref().ToSeconds());
  }

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "SendRecordingData", args, &rv)) {
    MOZ_CRASH("SendRecordingData");
  }

  MOZ_ALWAYS_TRUE(JS::DetachArrayBuffer(cx, bufferObject));
}

void OnTestCommand(const char* aString) {
  EnsureInitialized();

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  JSString* str = JS_NewStringCopyZ(cx, aString);
  MOZ_RELEASE_ASSERT(str);

  JS::AutoValueArray<1> args(cx);
  args[0].setString(str);

  RootedValue rv(cx);
  if (!JS_CallFunctionName(cx, *gModuleObject, "OnTestCommand", args, &rv)) {
    MOZ_CRASH("OnTestCommand");
  }
}

///////////////////////////////////////////////////////////////////////////////
// Replaying process content
///////////////////////////////////////////////////////////////////////////////

struct ContentInfo {
  const void* mToken;
  char* mURL;
  char* mContentType;
  InfallibleVector<char> mContent8;
  InfallibleVector<char16_t> mContent16;

  ContentInfo(const void* aToken, const char* aURL, const char* aContentType)
      : mToken(aToken),
        mURL(strdup(aURL)),
        mContentType(strdup(aContentType)) {}

  ContentInfo(ContentInfo&& aOther)
      : mToken(aOther.mToken),
        mURL(aOther.mURL),
        mContentType(aOther.mContentType),
        mContent8(std::move(aOther.mContent8)),
        mContent16(std::move(aOther.mContent16)) {
    aOther.mURL = nullptr;
    aOther.mContentType = nullptr;
  }

  ~ContentInfo() {
    free(mURL);
    free(mContentType);
  }

  size_t Length() {
    MOZ_RELEASE_ASSERT(!mContent8.length() || !mContent16.length());
    return mContent8.length() ? mContent8.length() : mContent16.length();
  }
};

// All content that has been parsed so far. Protected by child::gMonitor.
static StaticInfallibleVector<ContentInfo> gContent;

void DumpContent() {
  // Don't use a lock, this is for debugging.
  for (const auto& content : gContent) {
    nsAutoCString str;
    if (content.mContent8.length()) {
      str = nsCString(content.mContent8.begin(), content.mContent8.length());
    } else if (content.mContent16.length()) {
      nsString str16(content.mContent16.begin(), content.mContent16.length());
      str = NS_ConvertUTF16toUTF8(str16);
    }
    Print("Content %s %s:\n", content.mURL, content.mContentType);
    DirectPrint(str.get());
    Print("\nContentEnd %s %s\n", content.mURL, content.mContentType);
  }
}

extern "C" {

MOZ_EXPORT void RecordReplayInterface_BeginContentParse(
    const void* aToken, const char* aURL, const char* aContentType) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    MOZ_RELEASE_ASSERT(info.mToken != aToken);
  }
  gContent.emplaceBack(aToken, aURL, aContentType);
}

MOZ_EXPORT void RecordReplayInterface_AddContentParseData8(
    const void* aToken, const Utf8Unit* aUtf8Buffer, size_t aLength) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    if (info.mToken == aToken) {
      info.mContent8.append(reinterpret_cast<const char*>(aUtf8Buffer),
                            aLength);
      return;
    }
  }
  MOZ_CRASH("Unknown content parse token");
}

MOZ_EXPORT void RecordReplayInterface_AddContentParseData16(
    const void* aToken, const char16_t* aBuffer, size_t aLength) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    if (info.mToken == aToken) {
      info.mContent16.append(aBuffer, aLength);
      return;
    }
  }
  MOZ_CRASH("Unknown content parse token");
}

MOZ_EXPORT void RecordReplayInterface_EndContentParse(const void* aToken) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  MOZ_RELEASE_ASSERT(aToken);

  MonitorAutoLock lock(*child::gMonitor);
  for (ContentInfo& info : gContent) {
    if (info.mToken == aToken) {
      info.mToken = nullptr;
      return;
    }
  }
  MOZ_CRASH("Unknown content parse token");
}

}  // extern "C"

static bool FetchContent(JSContext* aCx, HandleString aURL,
                         MutableHandleString aContentType,
                         MutableHandleString aContent) {
  MonitorAutoLock lock(*child::gMonitor);

  // Find the longest content parse data with this URL. This is to handle inline
  // script elements in HTML pages, where we will see content parses for both
  // the HTML itself and for each inline script.
  ContentInfo* best = nullptr;
  for (ContentInfo& info : gContent) {
    if (JS_LinearStringEqualsAscii(JS_ASSERT_STRING_IS_LINEAR(aURL),
                                   info.mURL)) {
      if (!best || info.Length() > best->Length()) {
        best = &info;
      }
    }
  }

  if (!best) {
    JS_ReportErrorASCII(aCx, "Could not find record/replay content");
    return false;
  }

  aContentType.set(JS_NewStringCopyZ(aCx, best->mContentType));

  MOZ_ASSERT(best->mContent8.length() == 0 || best->mContent16.length() == 0,
             "should have content data of only one type");

  aContent.set(best->mContent8.length() > 0
                   ? JS_NewStringCopyUTF8N(
                         aCx, JS::UTF8Chars(best->mContent8.begin(),
                                            best->mContent8.length()))
                   : JS_NewUCStringCopyN(aCx, best->mContent16.begin(),
                                         best->mContent16.length()));

  return aContentType && aContent;
}

///////////////////////////////////////////////////////////////////////////////
// Recording/Replaying Methods
///////////////////////////////////////////////////////////////////////////////

static bool RecordReplay_Fork(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Expected numeric argument");
    return false;
  }

  size_t id = args.get(0).toNumber();
  child::PerformFork(id);

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_ChildId(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  args.rval().setInt32(child::GetId());
  return true;
}

static bool RecordReplay_ForkId(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  args.rval().setInt32(child::GetForkId());
  return true;
}

static bool RecordReplay_MiddlemanPid(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  args.rval().setInt32(child::MiddlemanProcessId());
  return true;
}

static bool RecordReplay_AreThreadEventsDisallowed(JSContext* aCx,
                                                   unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setBoolean(AreThreadEventsDisallowed());
  return true;
}

static bool RecordReplay_DivergeFromRecording(JSContext* aCx, unsigned aArgc,
                                              Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  DivergeFromRecording();
  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_ProgressCounter(JSContext* aCx, unsigned aArgc,
                                         Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setNumber((double)gProgressCounter);
  return true;
}

static bool RecordReplay_SetProgressCounter(JSContext* aCx, unsigned aArgc,
                                            Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Expected numeric argument");
    return false;
  }

  SetProgressCounter(args.get(0).toNumber());

  args.rval().setUndefined();
  return true;
}

JSString* ConvertStringToJSString(JSContext* aCx, const nsAString& aString) {
  JSString* rv = JS_NewUCStringCopyN(aCx, aString.BeginReading(), aString.Length());
  MOZ_RELEASE_ASSERT(rv);
  return rv;
}

void ConvertJSStringToCString(JSContext* aCx, JSString* aString,
                              nsAutoCString& aResult) {
  size_t len = JS_GetStringLength(aString);

  nsAutoString chars;
  chars.SetLength(len);
  if (!JS_CopyStringChars(aCx, Range<char16_t>(chars.BeginWriting(), len),
                          aString)) {
    MOZ_CRASH("ConvertJSStringToCString");
  }

  NS_ConvertUTF16toUTF8 utf8(chars);
  aResult = utf8;
}

static bool RecordReplay_ShouldUpdateProgressCounter(JSContext* aCx,
                                                     unsigned aArgc,
                                                     Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (args.get(0).isNull()) {
    args.rval().setBoolean(ShouldUpdateProgressCounter(nullptr));
  } else {
    if (!args.get(0).isString()) {
      JS_ReportErrorASCII(aCx, "Expected string or null as first argument");
      return false;
    }

    nsAutoCString str;
    ConvertJSStringToCString(aCx, args.get(0).toString(), str);
    args.rval().setBoolean(ShouldUpdateProgressCounter(str.get()));
  }

  return true;
}

static bool RecordReplay_ManifestFinished(JSContext* aCx, unsigned aArgc,
                                          Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  CharBuffer responseBuffer;
  if (args.hasDefined(0)) {
    RootedObject responseObject(aCx, RequireObject(aCx, args.get(0)));
    if (!responseObject) {
      return false;
    }

    if (!ToJSONMaybeSafely(aCx, responseObject, FillCharBufferCallback,
                           &responseBuffer)) {
      return false;
    }
  }

  child::ManifestFinished(responseBuffer);

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_ResumeExecution(JSContext* aCx, unsigned aArgc,
                                         Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  ResumeExecution();

  args.rval().setUndefined();
  return true;
}

// The total amount of time this process has spent idling.
static double gIdleTimeTotal;

// When recording and we are idle, the time when we became idle.
static double gIdleTimeStart;

void BeginIdleTime() {
  if (IsRecording() && Thread::CurrentIsMainThread()) {
    MOZ_RELEASE_ASSERT(!gIdleTimeStart);
    gIdleTimeStart = CurrentTime();
  }
}

void EndIdleTime() {
  if (IsRecording() && Thread::CurrentIsMainThread()) {
    MOZ_RELEASE_ASSERT(!!gIdleTimeStart);
    gIdleTimeTotal += CurrentTime() - gIdleTimeStart;
    gIdleTimeStart = 0;
  }
}

double TotalIdleTime() {
  return gIdleTimeTotal;
}

static bool RecordReplay_CurrentExecutionTime(JSContext* aCx, unsigned aArgc,
                                              Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  // Get the elapsed time in milliseconds since the process started.
  args.rval().setInt32(ElapsedTime() * 1000.0);
  return true;
}

static bool RecordReplay_FlushExternalCalls(JSContext* aCx, unsigned aArgc,
                                            Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  FlushExternalCalls();

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_GetRecordingSummary(JSContext* aCx, unsigned aArgc,
                                             Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  // Load any recording data accumulated off thread.
  child::AddPendingRecordingData(/* aRequireMore */ false);

  InfallibleVector<ProgressCounter> progressCounters;
  InfallibleVector<size_t> elapsed;
  InfallibleVector<size_t> times;
  GetRecordingSummary(progressCounters, elapsed, times);

  RootedValueVector values(aCx);

  for (size_t i = 0; i < progressCounters.length(); i++) {
    if (!values.append(NumberValue(progressCounters[i])) ||
        !values.append(NumberValue(elapsed[i])) ||
        !values.append(NumberValue(times[i]))) {
      return false;
    }
  }

  JSObject* array = NewArrayObject(aCx, values);
  if (!array) {
    return false;
  }

  args.rval().setObject(*array);
  return true;
}

static bool RecordReplay_GetContent(JSContext* aCx, unsigned aArgc,
                                    Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  RootedString url(aCx, ToString(aCx, args.get(0)));

  RootedString contentType(aCx), content(aCx);
  if (!FetchContent(aCx, url, &contentType, &content)) {
    return false;
  }

  RootedObject obj(aCx, JS_NewObject(aCx, nullptr));
  if (!obj ||
      !JS_DefineProperty(aCx, obj, "contentType", contentType,
                         JSPROP_ENUMERATE) ||
      !JS_DefineProperty(aCx, obj, "content", content, JSPROP_ENUMERATE)) {
    return false;
  }

  args.rval().setObject(*obj);
  return true;
}

static bool RecordReplay_GetGraphics(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(1).isString() || !args.get(2).isString()) {
    JS_ReportErrorASCII(aCx, "Expected string arguments");
    return false;
  }

  bool repaint = ToBoolean(args.get(0));

  nsAutoCString mimeType, encodeOptions;
  ConvertJSStringToCString(aCx, args.get(1).toString(), mimeType);
  ConvertJSStringToCString(aCx, args.get(2).toString(), encodeOptions);

  nsCString data;
  if (!child::GetGraphics(repaint, mimeType, encodeOptions, data)) {
    args.rval().setNull();
    return true;
  }

  JSString* str = JS_NewStringCopyN(aCx, data.BeginReading(), data.Length());
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
}

static bool RecordReplay_HadUnhandledExternalCall(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  args.rval().setBoolean(HadUnhandledExternalCall());
  return true;
}

static bool RecordReplay_GetEnv(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  args.rval().setUndefined();

  if (!args.get(0).isString()) {
    JS_ReportErrorASCII(aCx, "Expected string argument");
    return false;
  }

  if (ReplayingInCloud()) {
    AutoEnsurePassThroughThreadEvents pt;

    nsAutoCString env;
    ConvertJSStringToCString(aCx, args.get(0).toString(), env);

    const char* value = getenv(env.get());
    if (value) {
      JSString* str = JS_NewStringCopyZ(aCx, value);
      if (!str) {
        return false;
      }

      args.rval().setString(str);
    }
  }

  return true;
}

static bool RecordReplay_SetUnhandledDivergenceAllowed(JSContext* aCx,
                                                       unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  child::SetUnhandledDivergenceAllowed(ToBoolean(args.get(0)));

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_SetCrashNote(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString()) {
    JS_ReportErrorASCII(aCx, "Expected string argument");
    return false;
  }

  nsAutoCString str;
  ConvertJSStringToCString(aCx, args.get(0).toString(), str);
  child::SetCrashNote(str.get());

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_Dump(JSContext* aCx, unsigned aArgc, Value* aVp) {
  // This method is an alternative to dump() that can be used in places where
  // thread events are disallowed.
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  for (size_t i = 0; i < args.length(); i++) {
    RootedString str(aCx, ToString(aCx, args[i]));
    if (!str) {
      return false;
    }
    JS::UniqueChars cstr = JS_EncodeStringToLatin1(aCx, str);
    if (!cstr) {
      return false;
    }
    DirectPrint(cstr.get());
  }

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_Crash(JSContext* aCx, unsigned aArgc, Value* aVp) {
  Print("Intentionally crashing...\n");
  MOZ_CRASH("Intentional Crash");
}

static bool RecordReplay_MemoryUsage(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  uint64_t nbytes = child::GetMemoryUsage();

  args.rval().setNumber((double)nbytes);
  return true;
}

static bool RecordReplay_SetSharedKey(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString() || !args.get(1).isString()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  nsAutoCString key, value;
  ConvertJSStringToCString(aCx, args.get(0).toString(), key);
  ConvertJSStringToCString(aCx, args.get(1).toString(), value);

  child::SetSharedKey(key, value);

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_GetSharedKey(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString()) {
    JS_ReportErrorASCII(aCx, "Bad parameter");
    return false;
  }

  nsAutoCString key;
  ConvertJSStringToCString(aCx, args.get(0).toString(), key);

  nsAutoCString value;
  child::GetSharedKey(key, value);

  args.rval().setString(ConvertStringToJSString(aCx, NS_ConvertUTF8toUTF16(value)));
  return true;
}

static bool RecordReplay_DumpToFile(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString() || !args.get(1).isString()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  nsAutoCString file, contents;
  ConvertJSStringToCString(aCx, args.get(0).toString(), file);
  ConvertJSStringToCString(aCx, args.get(1).toString(), contents);

  FileHandle fd = DirectOpenFile(file.get(), true);
  DirectWrite(fd, contents.get(), contents.Length());
  DirectCloseFile(fd);

  args.rval().setUndefined();
  return true;
}

enum class LogJSAPILevel {
  NoLogging = 0,
  TopLevelEnterExit = 1,
  AllEnterExit = 2,
};
static LogJSAPILevel gLogJSAPI;

static bool RecordReplay_LogJSAPI(JSContext* aCx, unsigned aArgc, Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad parameter");
    return false;
  }

  gLogJSAPI = (LogJSAPILevel) args.get(0).toNumber();

  args.rval().setUndefined();
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Recording/Replaying Script Hit Methods
///////////////////////////////////////////////////////////////////////////////

enum ChangeFrameKind {
  ChangeFrameEnter,
  ChangeFrameExit,
  ChangeFrameResume,
  ChangeFrameCall,
  NumChangeFrameKinds
};

// Information about a location where a script offset has been hit.
struct ScriptHit {
  uint32_t mFrameIndex : 16;
  ProgressCounter mProgress : 48;

  ScriptHit(uint32_t aFrameIndex, ProgressCounter aProgress)
      : mFrameIndex(aFrameIndex), mProgress(aProgress) {
    static_assert(sizeof(ScriptHit) == 8, "Unexpected size");
    MOZ_RELEASE_ASSERT(aFrameIndex < 1 << 16);
    MOZ_RELEASE_ASSERT(aProgress < uint64_t(1) << 48);
  }
};

typedef InfallibleVector<ScriptHit> ScriptHitVector;

struct ScriptHitKey {
  uint32_t mScript;
  uint32_t mOffset;

  ScriptHitKey(uint32_t aScript, uint32_t aOffset)
      : mScript(aScript), mOffset(aOffset) {
    static_assert(sizeof(ScriptHitKey) == 8);
  }

  typedef ScriptHitKey Lookup;

  static HashNumber hash(const ScriptHitKey& aKey) {
    return HashGeneric(aKey.mScript, aKey.mOffset);
  }

  static bool match(const ScriptHitKey& aFirst, const ScriptHitKey& aSecond) {
    return aFirst.mScript == aSecond.mScript &&
           aFirst.mOffset == aSecond.mOffset;
  }
};

typedef HashMap<ScriptHitKey, ScriptHitVector*, ScriptHitKey> ScriptHitMap;

struct AnyScriptHit {
  uint32_t mScript;
  uint32_t mOffset;
  uint32_t mFrameIndex : 16;
  ProgressCounter mProgress : 48;

  AnyScriptHit() {}

  AnyScriptHit(uint32_t aScript, uint32_t aOffset, uint32_t aFrameIndex,
               ProgressCounter aProgress)
      : mScript(aScript), mOffset(aOffset), mFrameIndex(aFrameIndex),
        mProgress(aProgress) {
    static_assert(sizeof(AnyScriptHit) == 16);
  }
};

typedef InfallibleVector<AnyScriptHit, 128> AnyScriptHitVector;

// All information about script execution in some region of the recording.
struct ScriptHitRegion {
  ScriptHitMap mTable;
  AnyScriptHitVector mChangeFrames[NumChangeFrameKinds];

  void WriteContents(BufferStream& aStream) {
    aStream.WriteScalar32(mTable.count());
    for (auto iter = mTable.iter(); !iter.done(); iter.next()) {
      aStream.WriteBytes(&iter.get().key(), sizeof(ScriptHitKey));

      ScriptHitVector* hits = iter.get().value();
      aStream.WriteScalar32(hits->length());
      aStream.WriteBytes(hits->begin(), hits->length() * sizeof(ScriptHit));
    }

    for (const auto& vector : mChangeFrames) {
      aStream.WriteScalar32(vector.length());
      aStream.WriteBytes(vector.begin(), vector.length() * sizeof(AnyScriptHit));
    }
  }

  void ReadContents(BufferStream& aStream) {
    MOZ_RELEASE_ASSERT(mTable.empty());
    size_t count = aStream.ReadScalar32();
    for (size_t i = 0; i < count; i++) {
      ScriptHitKey key(0, 0);
      aStream.ReadBytes(&key, sizeof(ScriptHitKey));

      size_t numHits = aStream.ReadScalar32();
      ScriptHitVector* hits = new ScriptHitVector();
      hits->appendN(ScriptHit(0, 0), numHits);
      aStream.ReadBytes(hits->begin(), hits->length() * sizeof(ScriptHit));

      ScriptHitMap::AddPtr p = mTable.lookupForAdd(key);
      MOZ_RELEASE_ASSERT(!p);
      if (!mTable.add(p, key, hits)) {
        MOZ_CRASH("ReadContents");
      }
    }

    for (auto& vector : mChangeFrames) {
      MOZ_RELEASE_ASSERT(vector.empty());
      size_t numChangeFrames = aStream.ReadScalar32();
      vector.appendN(AnyScriptHit(), numChangeFrames);
      aStream.ReadBytes(vector.begin(), vector.length() * sizeof(AnyScriptHit));
    }
  }

  ScriptHitVector* FindHits(uint32_t aScript, uint32_t aOffset) {
    ScriptHitKey key(aScript, aOffset);
    ScriptHitMap::Ptr p = mTable.lookup(key);
    return p ? p->value() : nullptr;
  }

  AnyScriptHitVector* FindChangeFrames(uint32_t aWhich) {
    MOZ_RELEASE_ASSERT(aWhich < NumChangeFrameKinds);
    return &mChangeFrames[aWhich];
  }
};

typedef InfallibleVector<ScriptHitRegion*> ScriptHitRegionVector;

// Granularity for subdividing regions according to the progress values of
// their contents. A lower number will improve certain times of lookups, while
// a higher number will (slightly) hurt others and reduce memory usage.
static const size_t RegionGranularity = 10000;

static size_t GetProgressIndex(ProgressCounter aProgress) {
  return 1 + (aProgress / RegionGranularity);
}

// All information about execution between one checkpoint and the next.
struct ScriptHitCheckpoint {
  // Progress index of the first region, zero if not set.
  size_t mBaseProgressIndex = 0;

  ScriptHitRegionVector mRegions;
  InfallibleVector<char> mPaintData;

  ScriptHitCheckpoint() {}

  ScriptHitRegion* GetRegion(ProgressCounter aProgress) {
    size_t progressIndex = GetProgressIndex(aProgress);
    if (!mBaseProgressIndex) {
      mBaseProgressIndex = progressIndex;
    }
    MOZ_RELEASE_ASSERT(progressIndex >= mBaseProgressIndex);
    size_t index = progressIndex - mBaseProgressIndex;
    while (index >= mRegions.length()) {
      mRegions.append(new ScriptHitRegion());
    }
    return mRegions[index];
  }

  size_t GetRegionIndex(ProgressCounter aProgress) {
    MOZ_RELEASE_ASSERT(mBaseProgressIndex);
    MOZ_RELEASE_ASSERT(mRegions.length());

    size_t progressIndex = GetProgressIndex(aProgress);
    if (progressIndex < mBaseProgressIndex) {
      return 0;
    }
    size_t index = progressIndex - mBaseProgressIndex;
    return std::min<size_t>(index, mRegions.length() - 1);
  }

  void WriteContents(BufferStream& aStream) {
    aStream.WriteScalar(mBaseProgressIndex);
    aStream.WriteScalar(mRegions.length());
    for (auto region : mRegions) {
      region->WriteContents(aStream);
    }

    aStream.WriteScalar32(mPaintData.length());
    aStream.WriteBytes(mPaintData.begin(), mPaintData.length());
  }

  void ReadContents(BufferStream& aStream) {
    size_t baseProgressIndex = aStream.ReadScalar();
    if (baseProgressIndex) {
      MOZ_RELEASE_ASSERT(!mBaseProgressIndex);
      mBaseProgressIndex = baseProgressIndex;
    }

    size_t numRegions = aStream.ReadScalar();
    MOZ_RELEASE_ASSERT(!numRegions || mRegions.length() == 0);
    for (size_t i = 0; i < numRegions; i++) {
      mRegions.append(new ScriptHitRegion());
      mRegions[i]->ReadContents(aStream);
    }

    size_t paintDataLength = aStream.ReadScalar32();
    MOZ_RELEASE_ASSERT(!paintDataLength || mPaintData.empty());
    if (paintDataLength) {
      mPaintData.appendN(0, paintDataLength);
      aStream.ReadBytes(mPaintData.begin(), paintDataLength);
    }
  }
};

struct AllScriptHits {
  // Information about each checkpoint, indexed by the checkpoint ID.
  InfallibleVector<ScriptHitCheckpoint*, 1024> mCheckpoints;

  // When scanning the recording, this has the last breakpoint hit on a script
  // at each frame depth.
  InfallibleVector<AnyScriptHit, 256> mLastHits;

  // Get the information about the given checkpoint, creating it if necessary.
  ScriptHitCheckpoint* GetCheckpoint(uint32_t aCheckpoint) {
    while (aCheckpoint >= mCheckpoints.length()) {
      mCheckpoints.append(nullptr);
    }
    if (!mCheckpoints[aCheckpoint]) {
      mCheckpoints[aCheckpoint] = new ScriptHitCheckpoint();
    }
    return mCheckpoints[aCheckpoint];
  }

  // Get the region for the given checkpoint/progress, creating it if necessary.
  ScriptHitRegion* GetRegion(uint32_t aCheckpoint, ProgressCounter aProgress) {
    return GetCheckpoint(aCheckpoint)->GetRegion(aProgress);
  }

  void FindRegions(uint32_t aCheckpoint,
                   const Maybe<size_t>& aMinProgress,
                   const Maybe<size_t>& aMaxProgress,
                   ScriptHitRegionVector& aRegions) {
    ScriptHitCheckpoint* info = GetCheckpoint(aCheckpoint);
    if (info->mRegions.empty()) {
      return;
    }

    size_t minIndex = aMinProgress.isSome() ? info->GetRegionIndex(*aMinProgress) : 0;
    size_t maxIndex = aMaxProgress.isSome()
        ? info->GetRegionIndex(*aMaxProgress)
        : info->mRegions.length() - 1;
    for (size_t i = minIndex; i <= maxIndex; i++) {
      aRegions.append(info->mRegions[i]);
    }
  }

  void AddHit(uint32_t aCheckpoint, uint32_t aScript, uint32_t aOffset,
              uint32_t aFrameIndex, ProgressCounter aProgress) {
    ScriptHitRegion* region = GetRegion(aCheckpoint, aProgress);

    ScriptHitKey key(aScript, aOffset);
    ScriptHitMap::AddPtr p = region->mTable.lookupForAdd(key);
    if (!p && !region->mTable.add(p, key, new ScriptHitVector())) {
      MOZ_CRASH("AllScriptHits::AddHit");
    }

    ScriptHitVector* hits = p->value();
    hits->append(ScriptHit(aFrameIndex, aProgress));

    while (aFrameIndex >= mLastHits.length()) {
      mLastHits.emplaceBack();
    }
    AnyScriptHit& lastHit = mLastHits[aFrameIndex];
    lastHit.mScript = aScript;
    lastHit.mOffset = aOffset;
    lastHit.mFrameIndex = aFrameIndex;
    lastHit.mProgress = aProgress;
  }

  const AnyScriptHit& LastHit(uint32_t aFrameIndex) {
    MOZ_RELEASE_ASSERT(aFrameIndex < mLastHits.length());
    return mLastHits[aFrameIndex];
  }

  void AddChangeFrame(uint32_t aCheckpoint, uint32_t aWhich, uint32_t aScript,
                      uint32_t aOffset, uint32_t aFrameIndex,
                      ProgressCounter aProgress) {
    ScriptHitRegion* region = GetRegion(aCheckpoint, aProgress);
    MOZ_RELEASE_ASSERT(aWhich < NumChangeFrameKinds);
    region->mChangeFrames[aWhich].emplaceBack(aScript, aOffset, aFrameIndex, aProgress);
  }

  InfallibleVector<char>& GetPaintData(uint32_t aCheckpoint) {
    return GetCheckpoint(aCheckpoint)->mPaintData;
  }

  void WriteContents(InfallibleVector<char>& aData) {
    BufferStream stream(&aData);

    for (size_t i = 0; i < mCheckpoints.length(); i++) {
      ScriptHitCheckpoint* info = mCheckpoints[i];
      if (info) {
        stream.WriteScalar32(i);
        info->WriteContents(stream);
      }
    }
  }

  void ReadContents(const char* aData, size_t aSize) {
    BufferStream stream(aData, aSize);

    while (!stream.IsEmpty()) {
      size_t checkpoint = stream.ReadScalar32();
      ScriptHitCheckpoint* info = GetCheckpoint(checkpoint);
      info->ReadContents(stream);
    }
  }
};

static AllScriptHits* gScriptHits;

// Interned atoms for the various instrumented operations.
static JSString* gMainAtom;
static JSString* gEntryAtom;
static JSString* gBreakpointAtom;
static JSString* gExitAtom;

// Messages containing scan data which should be incorporated into this procdess.
// This is accessed off thread and protected by gMonitor.
static StaticInfallibleVector<Message::UniquePtr> gPendingScanDataMessages;

static void InitializeScriptHits() {
  gScriptHits = new AllScriptHits();

  AutoSafeJSContext cx;
  JSAutoRealm ar(cx, xpc::PrivilegedJunkScope());

  gMainAtom = JS_AtomizeAndPinString(cx, "main");
  gEntryAtom = JS_AtomizeAndPinString(cx, "entry");
  gBreakpointAtom = JS_AtomizeAndPinString(cx, "breakpoint");
  gExitAtom = JS_AtomizeAndPinString(cx, "exit");

  MOZ_RELEASE_ASSERT(gMainAtom && gEntryAtom && gBreakpointAtom && gExitAtom);
}

void AddScanDataMessage(Message::UniquePtr aMsg) {
  MonitorAutoLock lock(*child::gMonitor);
  gPendingScanDataMessages.append(std::move(aMsg));
}

static void MaybeIncorporateScanData() {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MonitorAutoLock lock(*child::gMonitor);
  for (const auto& msg : gPendingScanDataMessages) {
    MOZ_RELEASE_ASSERT(msg->mType == MessageType::ScanData);
    const auto& nmsg = static_cast<const ScanDataMessage&>(*msg);
    gScriptHits->ReadContents(nmsg.BinaryData(), nmsg.BinaryDataSize());
  }
  gPendingScanDataMessages.clear();
}

static bool gScanningScripts;
static uint32_t gFrameDepth;

// Any point we will stop at while scanning. When this is set we don't update
// the scan information, but still track the frame depth so we know when we
// are at the target point.
size_t gScanBreakpointProgress;
size_t gScanBreakpointScript;
size_t gScanBreakpointOffset;
size_t gScanBreakpointFrameIndex;
size_t gScanBreakpointIsOnPop;

static bool RecordReplay_IsScanningScripts(JSContext* aCx, unsigned aArgc,
                                           Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  args.rval().setBoolean(gScanningScripts);
  return true;
}

static bool RecordReplay_SetScanningScripts(JSContext* aCx, unsigned aArgc,
                                            Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  gScanningScripts = ToBoolean(args.get(0));

  if (gScanningScripts) {
    size_t depth;
    if (!RequireNumber(aCx, args.get(1), &depth)) {
      return false;
    }
    gFrameDepth = depth;
  } else {
    gFrameDepth = 0;
    gScanBreakpointProgress = 0;
    gScanBreakpointScript = 0;
    gScanBreakpointOffset = 0;
    gScanBreakpointFrameIndex = 0;
  }

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_SetScanBreakpoint(JSContext* aCx, unsigned aArgc,
                                           Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  MOZ_RELEASE_ASSERT(gScanningScripts);

  if (!RequireNumber(aCx, args.get(0), &gScanBreakpointProgress) ||
      !RequireNumber(aCx, args.get(1), &gScanBreakpointScript) ||
      !RequireNumber(aCx, args.get(2), &gScanBreakpointOffset) ||
      !RequireNumber(aCx, args.get(3), &gScanBreakpointFrameIndex) ||
      !RequireNumber(aCx, args.get(4), &gScanBreakpointIsOnPop)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static ProgressCounter gEnterJSAPIProgress;

static inline void SetFrameDepth(uint32_t aDepth, uint32_t aScript) {
  if ((size_t)gLogJSAPI) {
    switch (gLogJSAPI) {
      case LogJSAPILevel::TopLevelEnterExit:
        if (!gFrameDepth && aDepth) {
          child::PrintLog("EnterJSAPI");
          gEnterJSAPIProgress = gProgressCounter;
        } else if (gFrameDepth && !aDepth) {
          child::PrintLog("ExitJSAPI %llu", gProgressCounter - gEnterJSAPIProgress);
          gEnterJSAPIProgress = 0;
        }
        break;
      case LogJSAPILevel::AllEnterExit:
        child::PrintLog("JSAPI Depth %u Script %u", aDepth, aScript);
        break;
      default:
        break;
    }
  }

  gFrameDepth = aDepth;
}

static bool RecordReplay_SetFrameDepth(JSContext* aCx, unsigned aArgc,
                                       Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  MOZ_RELEASE_ASSERT(gScanningScripts);

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad parameter");
    return false;
  }

  SetFrameDepth(args.get(0).toNumber(), 0);

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_OnScriptHit(JSContext* aCx, unsigned aArgc,
                                     Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  MOZ_RELEASE_ASSERT(gScanningScripts);

  if (!args.get(1).isNumber() || !args.get(2).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  uint32_t script = args.get(1).toNumber();
  uint32_t offset = args.get(2).toNumber();
  uint32_t frameIndex = gFrameDepth - 1;

  if (!script) {
    // This script is not being tracked.
    args.rval().setUndefined();
    return true;
  }

  if (gScanBreakpointProgress) {
    if (!gScanBreakpointIsOnPop &&
        gScanBreakpointProgress == gProgressCounter &&
        gScanBreakpointScript == script &&
        gScanBreakpointOffset == offset &&
        gScanBreakpointFrameIndex == frameIndex) {
      JSAutoRealm ar(aCx, xpc::PrivilegedJunkScope());

      RootedValue rv(aCx);
      HandleValueArray resumeArgs(args.get(1));
      if (!JS_CallFunctionName(aCx, *gModuleObject, "ScanBreakpointHit",
                               HandleValueArray::empty(), &rv)) {
        MOZ_CRASH("RecordReplay_OnScriptHit");
      }
    }

    args.rval().setUndefined();
    return true;
  }

  gScriptHits->AddHit(GetLastCheckpoint(), script, offset, frameIndex,
                      gProgressCounter);
  args.rval().setUndefined();
  return true;
}

template <ChangeFrameKind Kind>
static bool RecordReplay_OnChangeFrame(JSContext* aCx, unsigned aArgc,
                                       Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  MOZ_RELEASE_ASSERT(gScanningScripts);

  if (!args.get(1).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  uint32_t script = args.get(1).toNumber();
  if (!script) {
    // This script is not being tracked and doesn't update the frame depth.
    args.rval().setUndefined();
    return true;
  }

  if (Kind == ChangeFrameEnter || Kind == ChangeFrameResume) {
    SetFrameDepth(gFrameDepth + 1, script);
  }

  uint32_t frameIndex = gFrameDepth - 1;

  if (gScanBreakpointProgress) {
    if (gScanBreakpointIsOnPop &&
        Kind == ChangeFrameExit &&
        gScanBreakpointProgress == gProgressCounter &&
        gScanBreakpointScript == script &&
        gScanBreakpointFrameIndex == frameIndex) {
      JSAutoRealm ar(aCx, xpc::PrivilegedJunkScope());

      RootedValue rv(aCx);
      HandleValueArray resumeArgs(args.get(1));
      if (!JS_CallFunctionName(aCx, *gModuleObject, "ScanBreakpointHit",
                               HandleValueArray::empty(), &rv)) {
        MOZ_CRASH("RecordReplay_OnScriptHit");
      }
    }
  } else {
    if (Kind == ChangeFrameEnter && frameIndex) {
      // Find the last breakpoint hit in the calling frame.
      const AnyScriptHit& lastHit = gScriptHits->LastHit(frameIndex - 1);
      gScriptHits->AddChangeFrame(GetLastCheckpoint(), ChangeFrameCall,
                                  lastHit.mScript, lastHit.mOffset,
                                  lastHit.mFrameIndex, lastHit.mProgress);
    }

    gScriptHits->AddChangeFrame(GetLastCheckpoint(), Kind, script, 0, frameIndex,
                                gProgressCounter);
  }

  if (Kind == ChangeFrameExit) {
    SetFrameDepth(gFrameDepth - 1, script);
  }

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_InstrumentationCallback(JSContext* aCx, unsigned aArgc,
                                                 Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isString()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  // The kind string should be an atom which we have captured already.
  JSString* kind = args.get(0).toString();

  if (kind == gBreakpointAtom) {
    return RecordReplay_OnScriptHit(aCx, aArgc, aVp);
  }

  if (kind == gMainAtom) {
    return RecordReplay_OnChangeFrame<ChangeFrameEnter>(aCx, aArgc, aVp);
  }

  if (kind == gExitAtom) {
    return RecordReplay_OnChangeFrame<ChangeFrameExit>(aCx, aArgc, aVp);
  }

  if (kind == gEntryAtom) {
    JSAutoRealm ar(aCx, xpc::PrivilegedJunkScope());

    RootedValue rv(aCx);
    HandleValueArray resumeArgs(args.get(1));
    if (!JS_CallFunctionName(aCx, *gModuleObject, "ScriptResumeFrame", resumeArgs, &rv)) {
      MOZ_CRASH("RecordReplay_InstrumentationCallback");
    }

    args.rval().setUndefined();
    return true;
  }

  JS_ReportErrorASCII(aCx, "Unexpected kind");
  return false;
}

static bool RecordReplay_SetScannedPaintData(JSContext* aCx, unsigned aArgc,
                                             Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  if (!args.get(0).isNumber() || !args.get(1).isString()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  uint32_t checkpoint = args.get(0).toNumber();

  nsAutoCString paintData;
  ConvertJSStringToCString(aCx, args.get(1).toString(), paintData);

  InfallibleVector<char>& data = gScriptHits->GetPaintData(checkpoint);
  MOZ_RELEASE_ASSERT(data.length() == 0);
  data.append(paintData.BeginReading(), paintData.Length());

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_CopyScanDataToRoot(JSContext* aCx, unsigned aArgc,
                                            Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);

  InfallibleVector<char> data;
  gScriptHits->WriteContents(data);

  child::SendScanDataToRoot(data.begin(), data.length());

  args.rval().setUndefined();
  return true;
}

static bool RecordReplay_GetScannedPaintData(JSContext* aCx, unsigned aArgc,
                                             Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  MaybeIncorporateScanData();

  if (!args.get(0).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  uint32_t checkpoint = args.get(0).toNumber();

  InfallibleVector<char>& data = gScriptHits->GetPaintData(checkpoint);
  if (data.length()) {
    JSString* str = JS_NewStringCopyN(aCx, data.begin(), data.length());
    if (!str) {
      return false;
    }
    args.rval().setString(str);
  } else {
    args.rval().setNull();
  }

  return true;
}

static bool MaybeGetNumberProperty(JSContext* aCx, HandleObject aObject,
                                   const char* aName, Maybe<size_t>* aResult) {
  RootedValue v(aCx);
  if (!JS_GetProperty(aCx, aObject, aName, &v)) {
    return false;
  }

  if (v.isNumber()) {
    aResult->emplace(v.toNumber());
  }

  return true;
}

struct SearchFilter {
  Maybe<size_t> mScript;
  Maybe<size_t> mFrameIndex;
  Maybe<size_t> mMinProgress;
  Maybe<size_t> mMaxProgress;

  bool Parse(JSContext* aCx, HandleValue aFilter) {
    if (!aFilter.isObject()) {
      if (!aFilter.isUndefined()) {
        JS_ReportErrorASCII(aCx, "Expected undefined or object filter");
        return false;
      }
      return true;
    }

    RootedObject filter(aCx, &aFilter.toObject());
    if (!MaybeGetNumberProperty(aCx, filter, "script", &mScript) ||
        !MaybeGetNumberProperty(aCx, filter, "frameIndex", &mFrameIndex) ||
        !MaybeGetNumberProperty(aCx, filter, "minProgress", &mMinProgress) ||
        !MaybeGetNumberProperty(aCx, filter, "maxProgress", &mMaxProgress)) {
      return false;
    }

    return true;
  }

  bool Exclude(size_t aScript, size_t aFrameIndex, size_t aProgress) {
    return (mScript.isSome() && aScript != *mScript) ||
           (mFrameIndex.isSome() && aFrameIndex != *mFrameIndex) ||
           (mMinProgress.isSome() && aProgress < *mMinProgress) ||
           (mMaxProgress.isSome() && aProgress > *mMaxProgress);
  }
};

static bool RecordReplay_FindScriptHits(JSContext* aCx, unsigned aArgc,
                                        Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  MaybeIncorporateScanData();

  if (!args.get(0).isNumber() || !args.get(1).isNumber() ||
      !args.get(2).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  uint32_t checkpoint = args.get(0).toNumber();
  uint32_t script = args.get(1).toNumber();
  uint32_t offset = args.get(2).toNumber();

  SearchFilter filter;
  if (!filter.Parse(aCx, args.get(3))) {
    return false;
  }

  RootedValueVector values(aCx);

  ScriptHitRegionVector regions;
  if (gScriptHits) {
    gScriptHits->FindRegions(checkpoint, filter.mMinProgress, filter.mMaxProgress,
                             regions);
  }

  for (auto region : regions) {
    ScriptHitVector* hits = region->FindHits(script, offset);
    if (!hits) {
      continue;
    }
    for (const auto& hit : *hits) {
      if (filter.Exclude(script, hit.mFrameIndex, hit.mProgress)) {
        continue;
      }
      RootedObject hitObject(aCx, JS_NewObject(aCx, nullptr));
      if (!hitObject ||
          !JS_DefineProperty(aCx, hitObject, "progress",
                             (double)hit.mProgress, JSPROP_ENUMERATE) ||
          !JS_DefineProperty(aCx, hitObject, "frameIndex", hit.mFrameIndex,
                             JSPROP_ENUMERATE) ||
          !values.append(ObjectValue(*hitObject))) {
        return false;
      }
    }
  }

  JSObject* array = NewArrayObject(aCx, values);
  if (!array) {
    return false;
  }

  args.rval().setObject(*array);
  return true;
}

static bool RecordReplay_FindChangeFrames(JSContext* aCx, unsigned aArgc,
                                          Value* aVp) {
  CallArgs args = CallArgsFromVp(aArgc, aVp);
  MaybeIncorporateScanData();

  if (!args.get(0).isNumber() || !args.get(1).isNumber()) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  uint32_t checkpoint = args.get(0).toNumber();
  uint32_t which = args.get(1).toNumber();

  if (which >= NumChangeFrameKinds) {
    JS_ReportErrorASCII(aCx, "Bad parameters");
    return false;
  }

  SearchFilter filter;
  if (!filter.Parse(aCx, args.get(2))) {
    return false;
  }

  RootedValueVector values(aCx);

  ScriptHitRegionVector regions;
  if (gScriptHits) {
    gScriptHits->FindRegions(checkpoint, filter.mMinProgress, filter.mMaxProgress,
                             regions);
  }

  for (auto region : regions) {
    AnyScriptHitVector* hits = region->FindChangeFrames(which);
    if (!hits) {
      continue;
    }
    for (const AnyScriptHit& hit : *hits) {
      if (filter.Exclude(hit.mScript, hit.mFrameIndex, hit.mProgress)) {
        continue;
      }
      RootedObject hitObject(aCx, JS_NewObject(aCx, nullptr));
      if (!hitObject ||
          !JS_DefineProperty(aCx, hitObject, "script", hit.mScript,
                             JSPROP_ENUMERATE) ||
          !JS_DefineProperty(aCx, hitObject, "progress", (double)hit.mProgress,
                             JSPROP_ENUMERATE) ||
          !JS_DefineProperty(aCx, hitObject, "frameIndex", hit.mFrameIndex,
                             JSPROP_ENUMERATE) ||
          !JS_DefineProperty(aCx, hitObject, "offset", hit.mOffset,
                             JSPROP_ENUMERATE) ||
          !values.append(ObjectValue(*hitObject))) {
        return false;
      }
    }
  }

  JSObject* array = NewArrayObject(aCx, values);
  if (!array) {
    return false;
  }

  args.rval().setObject(*array);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Plumbing
///////////////////////////////////////////////////////////////////////////////

static const JSFunctionSpec gRecordReplayMethods[] = {
    JS_FN("fork", RecordReplay_Fork, 1, 0),
    JS_FN("childId", RecordReplay_ChildId, 0, 0),
    JS_FN("forkId", RecordReplay_ForkId, 0, 0),
    JS_FN("middlemanPid", RecordReplay_MiddlemanPid, 0, 0),
    JS_FN("areThreadEventsDisallowed", RecordReplay_AreThreadEventsDisallowed,
          0, 0),
    JS_FN("divergeFromRecording", RecordReplay_DivergeFromRecording, 0, 0),
    JS_FN("progressCounter", RecordReplay_ProgressCounter, 0, 0),
    JS_FN("setProgressCounter", RecordReplay_SetProgressCounter, 1, 0),
    JS_FN("shouldUpdateProgressCounter",
          RecordReplay_ShouldUpdateProgressCounter, 1, 0),
    JS_FN("manifestFinished", RecordReplay_ManifestFinished, 1, 0),
    JS_FN("resumeExecution", RecordReplay_ResumeExecution, 0, 0),
    JS_FN("currentExecutionTime", RecordReplay_CurrentExecutionTime, 0, 0),
    JS_FN("flushExternalCalls", RecordReplay_FlushExternalCalls, 0, 0),
    JS_FN("getRecordingSummary", RecordReplay_GetRecordingSummary, 0, 0),
    JS_FN("getContent", RecordReplay_GetContent, 1, 0),
    JS_FN("getGraphics", RecordReplay_GetGraphics, 3, 0),
    JS_FN("hadUnhandledExternalCall", RecordReplay_HadUnhandledExternalCall, 0, 0),
    JS_FN("isScanningScripts", RecordReplay_IsScanningScripts, 0, 0),
    JS_FN("setScanningScripts", RecordReplay_SetScanningScripts, 2, 0),
    JS_FN("setScanBreakpoint", RecordReplay_SetScanBreakpoint, 5, 0),
    JS_FN("setFrameDepth", RecordReplay_SetFrameDepth, 1, 0),
    JS_FN("onScriptHit", RecordReplay_OnScriptHit, 3, 0),
    JS_FN("onEnterFrame", RecordReplay_OnChangeFrame<ChangeFrameEnter>, 2, 0),
    JS_FN("onExitFrame", RecordReplay_OnChangeFrame<ChangeFrameExit>, 2, 0),
    JS_FN("onResumeFrame", RecordReplay_OnChangeFrame<ChangeFrameResume>, 2, 0),
    JS_FN("instrumentationCallback", RecordReplay_InstrumentationCallback, 3,
          0),
    JS_FN("setScannedPaintData", RecordReplay_SetScannedPaintData, 2, 0),
    JS_FN("copyScanDataToRoot", RecordReplay_CopyScanDataToRoot, 0, 0),
    JS_FN("getScannedPaintData", RecordReplay_GetScannedPaintData, 1, 0),
    JS_FN("findScriptHits", RecordReplay_FindScriptHits, 4, 0),
    JS_FN("findChangeFrames", RecordReplay_FindChangeFrames, 3, 0),
    JS_FN("getenv", RecordReplay_GetEnv, 1, 0),
    JS_FN("setUnhandledDivergenceAllowed", RecordReplay_SetUnhandledDivergenceAllowed, 1, 0),
    JS_FN("setCrashNote", RecordReplay_SetCrashNote, 1, 0),
    JS_FN("dump", RecordReplay_Dump, 1, 0),
    JS_FN("crash", RecordReplay_Crash, 0, 0),
    JS_FN("memoryUsage", RecordReplay_MemoryUsage, 0, 0),
    JS_FN("setSharedKey", RecordReplay_SetSharedKey, 2, 0),
    JS_FN("getSharedKey", RecordReplay_GetSharedKey, 1, 0),
    JS_FN("dumpToFile", RecordReplay_DumpToFile, 2, 0),
    JS_FN("logJSAPI", RecordReplay_LogJSAPI, 1, 0),
    JS_FS_END};

extern "C" {

MOZ_EXPORT bool RecordReplayInterface_DefineRecordReplayControlObject(
    void* aCxVoid, void* aObjectArg) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());

  JSContext* aCx = static_cast<JSContext*>(aCxVoid);
  RootedObject object(aCx, static_cast<JSObject*>(aObjectArg));

  RootedObject staticObject(aCx, JS_NewObject(aCx, nullptr));
  if (!staticObject ||
      !JS_DefineProperty(aCx, object, "RecordReplayControl", staticObject, 0)) {
    return false;
  }

  if (gModuleObject) {
    // RecordReplayControl objects created while setting up the module itself
    // don't get references to the module.
    RootedObject obj(aCx, *gModuleObject);
    if (!JS_WrapObject(aCx, &obj) ||
        !JS_DefineProperty(aCx, staticObject, "module", obj, 0)) {
      return false;
    }
  }

  if (!JS_DefineFunctions(aCx, staticObject, gRecordReplayMethods)) {
    return false;
  }

  return true;
}

}  // extern "C"

}  // namespace js
}  // namespace recordreplay
}  // namespace mozilla
