/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The Gecko Profiler is an always-on profiler that takes fast and low overhead
// samples of the program execution using only userspace functionality for
// portability. The goal of this module is to provide performance data in a
// generic cross-platform way without requiring custom tools or kernel support.
//
// Samples are collected to form a timeline with optional timeline event
// (markers) used for filtering. The samples include both native stacks and
// platform-independent "label stack" frames.

#ifndef GeckoProfiler_h
#define GeckoProfiler_h

// everything in here is also safe to include unconditionally, and only defines
// empty macros if MOZ_GECKO_PROFILER is unset
#include "mozilla/ProfilerCounts.h"

#ifndef MOZ_GECKO_PROFILER

// This file can be #included unconditionally. However, everything within this
// file must be guarded by a #ifdef MOZ_GECKO_PROFILER, *except* for the
// following macros, which encapsulate the most common operations and thus
// avoid the need for many #ifdefs.

#define AUTO_PROFILER_INIT

#define PROFILER_REGISTER_THREAD(name)
#define PROFILER_UNREGISTER_THREAD()
#define AUTO_PROFILER_REGISTER_THREAD(name)

#define AUTO_PROFILER_THREAD_SLEEP
#define AUTO_PROFILER_THREAD_WAKE

#define PROFILER_JS_INTERRUPT_CALLBACK()

#define PROFILER_SET_JS_CONTEXT(cx)
#define PROFILER_CLEAR_JS_CONTEXT()

#define AUTO_PROFILER_LABEL(label, category)
#define AUTO_PROFILER_LABEL_DYNAMIC_CSTR(label, category, cStr)
#define AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(label, category, nsCStr)
#define AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(label, category, nsStr)
#define AUTO_PROFILER_LABEL_FAST(label, category, ctx)
#define AUTO_PROFILER_LABEL_DYNAMIC_FAST(label, dynamicString, category, ctx, flags)

#define PROFILER_ADD_MARKER(markerName)
#define PROFILER_ADD_NETWORK_MARKER(uri, pri, channel, type, start, end, count, cache, timings, redirect)

#define DECLARE_DOCSHELL_AND_HISTORY_ID(docShell)
#define PROFILER_TRACING(category, markerName, kind)
#define PROFILER_TRACING_DOCSHELL(category, markerName, kind, docshell)
#define AUTO_PROFILER_TRACING(category, markerName)
#define AUTO_PROFILER_TRACING_DOCSHELL(category, markerName, docShell)

#else // !MOZ_GECKO_PROFILER

#include <functional>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Atomics.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Maybe.h"
#include "mozilla/Sprintf.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "js/ProfilingStack.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "nscore.h"
#include "nsID.h"
#include "nsString.h"

// Make sure that we can use std::min here without the Windows headers messing
// with us.
#ifdef min
# undef min
#endif

class ProfilerBacktrace;
class ProfilerMarkerPayload;
class SpliceableJSONWriter;
namespace mozilla {
namespace net {
struct TimingStruct;
enum CacheDisposition : uint8_t;
}
}
class nsIURI;

namespace mozilla {
class MallocAllocPolicy;
template <class T, size_t MinInlineCapacity, class AllocPolicy> class Vector;
class TimeStamp;
} // namespace mozilla

// Macros used by the AUTO_PROFILER_* macros below.
#define PROFILER_RAII_PASTE(id, line) id ## line
#define PROFILER_RAII_EXPAND(id, line) PROFILER_RAII_PASTE(id, line)
#define PROFILER_RAII PROFILER_RAII_EXPAND(raiiObject, __LINE__)

//---------------------------------------------------------------------------
// Profiler features
//---------------------------------------------------------------------------

// Higher-order macro containing all the feature info in one place. Define
// |MACRO| appropriately to extract the relevant parts. Note that the number
// values are used internally only and so can be changed without consequence.
// Any changes to this list should also be applied to the feature list in
// browser/components/extensions/schemas/geckoProfiler.json.
#define PROFILER_FOR_EACH_FEATURE(MACRO) \
  /* Profile Java code (Android only). */ \
  MACRO(0, "java", Java) \
  \
  /* Get the JS engine to expose the JS stack to the profiler */ \
  MACRO(1, "js", JS) \
  \
  /* Include the C++ leaf node if not stackwalking. */ \
  /* The DevTools profiler doesn't want the native addresses. */ \
  MACRO(2, "leaf", Leaf) \
  \
  /* Add main thread I/O to the profile. */ \
  MACRO(3, "mainthreadio", MainThreadIO) \
  \
  /* Add memory measurements (e.g. RSS). */ \
  MACRO(4, "memory", Memory) \
  \
  /* Do not include user-identifiable information. */ \
  MACRO(5, "privacy", Privacy) \
  \
  /* Collect thread responsiveness information. */ \
  MACRO(6, "responsiveness", Responsiveness) \
  \
  /* Take a snapshot of the window on every composition. */ \
  MACRO(7, "screenshots", Screenshots) \
  \
  /* Disable parallel traversal in styling. */ \
  MACRO(8, "seqstyle", SequentialStyle) \
  \
  /* Walk the C++ stack. Not available on all platforms. */ \
  MACRO(9, "stackwalk", StackWalk) \
  \
  /* Start profiling with feature TaskTracer. */ \
  MACRO(10, "tasktracer", TaskTracer) \
  \
  /* Profile the registered secondary threads. */ \
  MACRO(11, "threads", Threads) \
  \
  /* Have the JavaScript engine track JIT optimizations. */ \
  MACRO(12, "trackopts", TrackOptimizations)

struct ProfilerFeature
{
  #define DECLARE(n_, str_, Name_) \
    static const uint32_t Name_ = (1u << n_); \
    static bool Has##Name_(uint32_t aFeatures) { return aFeatures & Name_; } \
    static void Set##Name_(uint32_t& aFeatures) { aFeatures |= Name_; } \
    static void Clear##Name_(uint32_t& aFeatures) { aFeatures &= ~Name_; }

  // Define a bitfield constant, a getter, and two setters for each feature.
  PROFILER_FOR_EACH_FEATURE(DECLARE)

  #undef DECLARE
};

namespace mozilla {
namespace profiler {
namespace detail {

// RacyFeatures is only defined in this header file so that its methods can
// be inlined into profiler_is_active(). Please do not use anything from the
// detail namespace outside the profiler.

// Within the profiler's code, the preferred way to check profiler activeness
// and features is via ActivePS(). However, that requires locking gPSMutex.
// There are some hot operations where absolute precision isn't required, so we
// duplicate the activeness/feature state in a lock-free manner in this class.
class RacyFeatures
{
public:
  static void SetActive(uint32_t aFeatures)
  {
    sActiveAndFeatures = Active | aFeatures;
  }

  static void SetInactive() { sActiveAndFeatures = 0; }

  static bool IsActive() { return uint32_t(sActiveAndFeatures) & Active; }

  static bool IsActiveWithFeature(uint32_t aFeature)
  {
    uint32_t af = sActiveAndFeatures;  // copy it first
    return (af & Active) && (af & aFeature);
  }

  static bool IsActiveWithoutPrivacy()
  {
    uint32_t af = sActiveAndFeatures;  // copy it first
    return (af & Active) && !(af & ProfilerFeature::Privacy);
  }

private:
  static const uint32_t Active = 1u << 31;

  // Ensure Active doesn't overlap with any of the feature bits.
  #define NO_OVERLAP(n_, str_, Name_) \
    static_assert(ProfilerFeature::Name_ != Active, "bad Active value");

  PROFILER_FOR_EACH_FEATURE(NO_OVERLAP);

  #undef NO_OVERLAP

  // We combine the active bit with the feature bits so they can be read or
  // written in a single atomic operation. Accesses to this atomic are not
  // recorded by web replay as they may occur at non-deterministic points.
  static mozilla::Atomic<uint32_t, mozilla::MemoryOrdering::Relaxed,
                         recordreplay::Behavior::DontPreserve> sActiveAndFeatures;
};

bool IsThreadBeingProfiled();

} // namespace detail
} // namespace profiler
} // namespace mozilla

//---------------------------------------------------------------------------
// Start and stop the profiler
//---------------------------------------------------------------------------

#if !defined(ARCH_ARMV6)
# define PROFILER_DEFAULT_ENTRIES 1000000
#else
# define PROFILER_DEFAULT_ENTRIES 100000
#endif

#define PROFILER_DEFAULT_DURATION 20
#define PROFILER_DEFAULT_INTERVAL 1

// Initialize the profiler. If MOZ_PROFILER_STARTUP is set the profiler will
// also be started. This call must happen before any other profiler calls
// (except profiler_start(), which will call profiler_init() if it hasn't
// already run).
void profiler_init(void* stackTop);

#define AUTO_PROFILER_INIT \
  mozilla::AutoProfilerInit PROFILER_RAII

// Clean up the profiler module, stopping it if required. This function may
// also save a shutdown profile if requested. No profiler calls should happen
// after this point and all profiling stack labels should have been popped.
void profiler_shutdown();

// Start the profiler -- initializing it first if necessary -- with the
// selected options. Stops and restarts the profiler if it is already active.
// After starting the profiler is "active". The samples will be recorded in a
// circular buffer.
//   "aCapacity" is the maximum number of entries in the profiler's circular buffer.
//   "aInterval" the sampling interval, measured in millseconds.
//   "aFeatures" is the feature set. Features unsupported by this
//               platform/configuration are ignored.
//   "aFilters" is the list of thread filters. Threads that do not match any
//              of the filters are not profiled. A filter matches a thread if
//              (a) the thread name contains the filter as a case-insensitive
//                  substring, or
//              (b) the filter is of the form "pid:<n>" where n is the process
//                  id of the process that the thread is running in.
//   "aDuration" is the duration of entries in the profiler's circular buffer.
void profiler_start(uint32_t aCapacity, double aInterval, uint32_t aFeatures,
                    const char** aFilters, uint32_t aFilterCount,
                    const mozilla::Maybe<double>& aDuration = mozilla::Nothing());

// Stop the profiler and discard the profile without saving it. A no-op if the
// profiler is inactive. After stopping the profiler is "inactive".
void profiler_stop();

// If the profiler is inactive, start it. If it's already active, restart it if
// the requested settings differ from the current settings. Both the check and
// the state change are performed while the profiler state is locked.
// The only difference to profiler_start is that the current buffer contents are
// not discarded if the profiler is already running with the requested settings.
void profiler_ensure_started(uint32_t aCapacity, double aInterval,
                             uint32_t aFeatures, const char** aFilters,
                             uint32_t aFilterCount,
                             const mozilla::Maybe<double>& aDuration = mozilla::Nothing());

//---------------------------------------------------------------------------
// Control the profiler
//---------------------------------------------------------------------------

// Register/unregister threads with the profiler. Both functions operate the
// same whether the profiler is active or inactive.
#define PROFILER_REGISTER_THREAD(name) \
  do { char stackTop; profiler_register_thread(name, &stackTop); } while (0)
#define PROFILER_UNREGISTER_THREAD() \
  profiler_unregister_thread()
ProfilingStack* profiler_register_thread(const char* name, void* guessStackTop);
void profiler_unregister_thread();

// Register pages with the profiler.
//
// The `page` means every new history entry for docShells.
// DocShellId + HistoryID is a unique pair to identify these pages.
// We also keep these pairs inside markers to associate with the pages.
// That allows us to see which markers belong to a specific page and filter the
// markers by a page.
// We register pages in these cases:
// - If there is a navigation through a link or URL bar.
// - If there is a navigation through `location.replace` or `history.pushState`.
// We do not register pages in these cases:
// - If there is a history navigation through the back and forward buttons.
// - If there is a navigation through `history.replaceState` or anchor scrolls.
//
//   "aDocShellId" is the ID of the docShell that page belongs to.
//   "aHistoryId"  is the ID of the history entry on the given docShell.
//   "aUrl"        is the URL of the page.
//   "aIsSubFrame" is true if the page is a sub frame.
void profiler_register_page(const nsID& aDocShellId,
                            uint32_t aHistoryId,
                            const nsCString& aUrl,
                            bool aIsSubFrame);
// Unregister pages with the profiler.
//
// Take a docShellId and unregister all the page entries that have the given ID.
void profiler_unregister_pages(const nsID& aRegisteredDocShellId);

class BaseProfilerCount;
void profiler_add_sampled_counter(BaseProfilerCount* aCounter);
void profiler_remove_sampled_counter(BaseProfilerCount* aCounter);

// Register and unregister a thread within a scope.
#define AUTO_PROFILER_REGISTER_THREAD(name) \
  mozilla::AutoProfilerRegisterThread PROFILER_RAII(name)

// Pause and resume the profiler. No-ops if the profiler is inactive. While
// paused the profile will not take any samples and will not record any data
// into its buffers. The profiler remains fully initialized in this state.
// Timeline markers will still be stored. This feature will keep JavaScript
// profiling enabled, thus allowing toggling the profiler without invalidating
// the JIT.
void profiler_pause();
void profiler_resume();

// These functions tell the profiler that a thread went to sleep so that we can
// avoid sampling it while it's sleeping. Calling profiler_thread_sleep()
// twice without an intervening profiler_thread_wake() is an error. All three
// functions operate the same whether the profiler is active or inactive.
void profiler_thread_sleep();
void profiler_thread_wake();

// Mark a thread as asleep/awake within a scope.
#define AUTO_PROFILER_THREAD_SLEEP \
  mozilla::AutoProfilerThreadSleep PROFILER_RAII
#define AUTO_PROFILER_THREAD_WAKE \
  mozilla::AutoProfilerThreadWake PROFILER_RAII

// Called by the JSRuntime's operation callback. This is used to start profiling
// on auxiliary threads. Operates the same whether the profiler is active or
// not.
#define PROFILER_JS_INTERRUPT_CALLBACK() profiler_js_interrupt_callback()
void profiler_js_interrupt_callback();

// Set and clear the current thread's JSContext.
#define PROFILER_SET_JS_CONTEXT(cx) profiler_set_js_context(cx)
#define PROFILER_CLEAR_JS_CONTEXT() profiler_clear_js_context()
void profiler_set_js_context(JSContext* aCx);
void profiler_clear_js_context();

//---------------------------------------------------------------------------
// Get information from the profiler
//---------------------------------------------------------------------------

// Is the profiler active? Note: the return value of this function can become
// immediately out-of-date. E.g. the profile might be active but then
// profiler_stop() is called immediately afterward. One common and reasonable
// pattern of usage is the following:
//
//   if (profiler_is_active()) {
//     ExpensiveData expensiveData = CreateExpensiveData();
//     PROFILER_OPERATION(expensiveData);
//   }
//
// where PROFILER_OPERATION is a no-op if the profiler is inactive. In this
// case the profiler_is_active() check is just an optimization -- it prevents
// us calling CreateExpensiveData() unnecessarily in most cases, but the
// expensive data will end up being created but not used if another thread
// stops the profiler between the CreateExpensiveData() and PROFILER_OPERATION
// calls.
inline bool profiler_is_active()
{
  return mozilla::profiler::detail::RacyFeatures::IsActive();
}

// Is the profiler active, and is the current thread being profiled?
// (Same caveats and recommented usage as profiler_is_active().)
inline bool profiler_thread_is_being_profiled()
{
  return profiler_is_active() &&
         mozilla::profiler::detail::IsThreadBeingProfiled();
}

// Is the profiler active and paused? Returns false if the profiler is inactive.
bool profiler_is_paused();

// Is the current thread sleeping?
bool profiler_thread_is_sleeping();

// Get all the features supported by the profiler that are accepted by
// profiler_start(). The result is the same whether the profiler is active or
// not.
uint32_t profiler_get_available_features();

// Check if a profiler feature (specified via the ProfilerFeature type) is
// active. Returns false if the profiler is inactive. Note: the return value
// can become immediately out-of-date, much like the return value of
// profiler_is_active().
bool profiler_feature_active(uint32_t aFeature);

// Get the params used to start the profiler. Returns 0 and an empty vector
// (via outparams) if the profile is inactive. It's possible that the features
// returned may be slightly different to those requested due to required
// adjustments.
void profiler_get_start_params(int* aEntrySize, mozilla::Maybe<double>* aDuration,
                               double* aInterval, uint32_t* aFeatures,
                               mozilla::Vector<const char*, 0,
                                               mozilla::MallocAllocPolicy>*
                                 aFilters);

// The number of milliseconds since the process started. Operates the same
// whether the profiler is active or inactive.
double profiler_time();

// Get the current thread's ID.
int profiler_current_thread_id();

// An object of this class is passed to profiler_suspend_and_sample_thread().
// For each stack frame, one of the Collect methods will be called.
class ProfilerStackCollector
{
public:
  // Some collectors need to worry about possibly overwriting previous
  // generations of data. If that's not an issue, this can return Nothing,
  // which is the default behaviour.
  virtual mozilla::Maybe<uint64_t> SamplePositionInBuffer() { return mozilla::Nothing(); }
  virtual mozilla::Maybe<uint64_t> BufferRangeStart() { return mozilla::Nothing(); }

  // This method will be called once if the thread being suspended is the main
  // thread. Default behaviour is to do nothing.
  virtual void SetIsMainThread() {}

  // WARNING: The target thread is suspended when the Collect methods are
  // called. Do not try to allocate or acquire any locks, or you could
  // deadlock. The target thread will have resumed by the time this function
  // returns.

  virtual void CollectNativeLeafAddr(void* aAddr) = 0;

  virtual void CollectJitReturnAddr(void* aAddr) = 0;

  virtual void CollectWasmFrame(const char* aLabel) = 0;

  virtual void CollectProfilingStackFrame(const js::ProfilingStackFrame& aFrame) = 0;
};

// This method suspends the thread identified by aThreadId, samples its
// profiling stack, JS stack, and (optionally) native stack, passing the collected
// frames into aCollector. aFeatures dictates which compiler features are used.
// |Privacy| and |Leaf| are the only relevant ones.
void profiler_suspend_and_sample_thread(int aThreadId, uint32_t aFeatures,
                                        ProfilerStackCollector& aCollector,
                                        bool aSampleNative = true);

struct ProfilerBacktraceDestructor
{
  void operator()(ProfilerBacktrace*);
};

using UniqueProfilerBacktrace =
  mozilla::UniquePtr<ProfilerBacktrace, ProfilerBacktraceDestructor>;

// Immediately capture the current thread's call stack and return it. A no-op
// if the profiler is inactive or in privacy mode.
UniqueProfilerBacktrace profiler_get_backtrace();

struct ProfilerBufferInfo
{
  uint64_t mRangeStart;
  uint64_t mRangeEnd;
  uint32_t mEntryCount;
};

// Get information about the current buffer status.
// Returns Nothing() if the profiler is inactive.
//
// This information may be useful to a user-interface displaying the current
// status of the profiler, allowing the user to get a sense for how fast the
// buffer is being written to, and how much data is visible.
mozilla::Maybe<ProfilerBufferInfo> profiler_get_buffer_info();

//---------------------------------------------------------------------------
// Put profiling data into the profiler (labels and markers)
//---------------------------------------------------------------------------

// Insert an RAII object in this scope to enter a label stack frame. Any
// samples collected in this scope will contain this label in their stack.
// The label argument must be a static C string. It is usually of the
// form "ClassName::FunctionName". (Ideally we'd use the compiler to provide
// that for us, but __func__ gives us the function name without the class
// name.) If the label applies to only part of a function, you can qualify it
// like this: "ClassName::FunctionName:PartName".
//
// Use AUTO_PROFILER_LABEL_DYNAMIC_* if you want to add additional / dynamic
// information to the label stack frame.
#define AUTO_PROFILER_LABEL(label, category) \
  mozilla::AutoProfilerLabel PROFILER_RAII(label, nullptr, \
                                           js::ProfilingStackFrame::Category::category)

// Similar to AUTO_PROFILER_LABEL, but with an additional string. The inserted
// RAII object stores the cStr pointer in a field; it does not copy the string.
//
// WARNING: This means that the string you pass to this macro needs to live at
// least until the end of the current scope. Be careful using this macro with
// ns[C]String; the other AUTO_PROFILER_LABEL_DYNAMIC_* macros below are
// preferred because they avoid this problem.
//
// If the profiler samples the current thread and walks the label stack while
// this RAII object is on the stack, it will copy the supplied string into the
// profile buffer. So there's one string copy operation, and it happens at
// sample time.
//
// Compare this to the plain AUTO_PROFILER_LABEL macro, which only accepts
// literal strings: When the label stack frames generated by
// AUTO_PROFILER_LABEL are sampled, no string copy needs to be made because the
// profile buffer can just store the raw pointers to the literal strings.
// Consequently, AUTO_PROFILER_LABEL frames take up considerably less space in
// the profile buffer than AUTO_PROFILER_LABEL_DYNAMIC_* frames.
#define AUTO_PROFILER_LABEL_DYNAMIC_CSTR(label, category, cStr) \
  mozilla::AutoProfilerLabel \
    PROFILER_RAII(label, cStr, js::ProfilingStackFrame::Category::category)

// Similar to AUTO_PROFILER_LABEL_DYNAMIC_CSTR, but takes an nsACString.
//
// Note: The use of the Maybe<>s ensures the scopes for the dynamic string and
// the AutoProfilerLabel are appropriate, while also not incurring the runtime
// cost of the string assignment unless the profiler is active. Therefore,
// unlike AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC_CSTR, this macro
// doesn't push/pop a label when the profiler is inactive.
#define AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(label, category, nsCStr) \
  mozilla::Maybe<nsAutoCString> autoCStr; \
  mozilla::Maybe<AutoProfilerLabel> raiiObjectNsCString; \
  if (profiler_is_active()) { \
    autoCStr.emplace(nsCStr); \
    raiiObjectNsCString.emplace(label, autoCStr->get(), \
                                js::ProfilingStackFrame::Category::category); \
  }

// Similar to AUTO_PROFILER_LABEL_DYNAMIC_CSTR, but takes an nsString that is
// is lossily converted to an ASCII string.
//
// Note: The use of the Maybe<>s ensures the scopes for the converted dynamic
// string and the AutoProfilerLabel are appropriate, while also not incurring
// the runtime cost of the string conversion unless the profiler is active.
// Therefore, unlike AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC_CSTR,
// this macro doesn't push/pop a label when the profiler is inactive.
#define AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(label, category, nsStr) \
  mozilla::Maybe<NS_LossyConvertUTF16toASCII> asciiStr; \
  mozilla::Maybe<AutoProfilerLabel> raiiObjectLossyNsString; \
  if (profiler_is_active()) { \
    asciiStr.emplace(nsStr); \
    raiiObjectLossyNsString.emplace(label, asciiStr->get(), \
                                    js::ProfilingStackFrame::Category::category); \
  }

// Similar to AUTO_PROFILER_LABEL, but accepting a JSContext* parameter, and a
// no-op if the profiler is disabled.
// Used to annotate functions for which overhead in the range of nanoseconds is
// noticeable. It avoids overhead from the TLS lookup because it can get the
// ProfilingStack from the JS context, and avoids almost all overhead in the case
// where the profiler is disabled.
#define AUTO_PROFILER_LABEL_FAST(label, category, ctx) \
  mozilla::AutoProfilerLabel PROFILER_RAII(ctx, label, nullptr, \
                                           js::ProfilingStackFrame::Category::category)

// Similar to AUTO_PROFILER_LABEL_FAST, but also takes an extra string and an
// additional set of flags. The flags parameter should carry values from the
// js::ProfilingStackFrame::Flags enum.
#define AUTO_PROFILER_LABEL_DYNAMIC_FAST(label, dynamicString, category, ctx, flags) \
  mozilla::AutoProfilerLabel PROFILER_RAII(ctx, label, dynamicString, \
                                           js::ProfilingStackFrame::Category::category, \
                                           flags)

// Insert a marker in the profile timeline. This is useful to delimit something
// important happening such as the first paint. Unlike labels, which are only
// recorded in the profile buffer if a sample is collected while the label is
// on the label stack, markers will always be recorded in the profile buffer.
// aMarkerName is copied, so the caller does not need to ensure it lives for a
// certain length of time. A no-op if the profiler is inactive or in privacy
// mode.
#define PROFILER_ADD_MARKER(markerName) \
  profiler_add_marker(markerName)
void profiler_add_marker(const char* aMarkerName);
void profiler_add_marker(const char* aMarkerName,
                         mozilla::UniquePtr<ProfilerMarkerPayload> aPayload);

// Insert a marker in the profile timeline for a specified thread.
void profiler_add_marker_for_thread(int aThreadId,
                                    const char* aMarkerName,
                                    mozilla::UniquePtr<ProfilerMarkerPayload> aPayload);

enum class NetworkLoadType {
  LOAD_START,
  LOAD_STOP,
  LOAD_REDIRECT
};

#define PROFILER_ADD_NETWORK_MARKER(uri, pri, channel, type, start, end, count, cache, timings, redirect) \
  profiler_add_network_marker(uri, pri, channel, type, start, end, count, cache, timings, redirect)

void profiler_add_network_marker(nsIURI* aURI,
                                 int32_t aPriority,
                                 uint64_t aChannelId,
                                 NetworkLoadType aType,
                                 mozilla::TimeStamp aStart,
                                 mozilla::TimeStamp aEnd,
                                 int64_t aCount,
                                 mozilla::net::CacheDisposition aCacheDisposition,
                                 const mozilla::net::TimingStruct* aTimings = nullptr,
                                 nsIURI* aRedirectURI = nullptr);


enum TracingKind {
  TRACING_EVENT,
  TRACING_INTERVAL_START,
  TRACING_INTERVAL_END,
};

// Helper macro to retrieve DocShellId and DocShellHistoryId from docShell
#define DECLARE_DOCSHELL_AND_HISTORY_ID(docShell)                              \
  mozilla::Maybe<nsID> docShellId;                                             \
  mozilla::Maybe<uint32_t> docShellHistoryId;                                  \
  if (docShell) {                                                              \
    docShellId = Some(docShell->HistoryID());                                  \
    uint32_t id;                                                               \
    nsresult rv = docShell->GetOSHEId(&id);                                    \
    if (NS_SUCCEEDED(rv)) {                                                    \
      docShellHistoryId = Some(id);                                            \
    } else {                                                                   \
      docShellHistoryId = Nothing();                                           \
    }                                                                          \
  } else {                                                                     \
    docShellId = Nothing();                                                    \
    docShellHistoryId = Nothing();                                             \
  }

// Adds a tracing marker to the profile. A no-op if the profiler is inactive or
// in privacy mode.
#define PROFILER_TRACING(category, markerName, kind)                           \
  profiler_tracing(category, markerName, kind)
#define PROFILER_TRACING_DOCSHELL(category, markerName, kind, docShell)        \
  DECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                                   \
  profiler_tracing(category, markerName, kind, docShellId, docShellHistoryId)
void
profiler_tracing(
  const char* aCategory,
  const char* aMarkerName,
  TracingKind aKind,
  const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
  const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing());
void
profiler_tracing(
  const char* aCategory,
  const char* aMarkerName,
  TracingKind aKind,
  UniqueProfilerBacktrace aCause,
  const mozilla::Maybe<nsID>& aDocShellId = mozilla::Nothing(),
  const mozilla::Maybe<uint32_t>& aDocShellHistoryId = mozilla::Nothing());

// Adds a START/END pair of tracing markers.
#define AUTO_PROFILER_TRACING(category, markerName)                            \
  mozilla::AutoProfilerTracing PROFILER_RAII(                                  \
    category, markerName, mozilla::Nothing(), mozilla::Nothing())
#define AUTO_PROFILER_TRACING_DOCSHELL(category, markerName, docShell)         \
  DECLARE_DOCSHELL_AND_HISTORY_ID(docShell);                                   \
  mozilla::AutoProfilerTracing PROFILER_RAII(                                  \
    category, markerName, docShellId, docShellHistoryId)

//---------------------------------------------------------------------------
// Output profiles
//---------------------------------------------------------------------------

// Get the profile encoded as a JSON string. A no-op (returning nullptr) if the
// profiler is inactive.
// If aIsShuttingDown is true, the current time is included as the process
// shutdown time in the JSON's "meta" object.
mozilla::UniquePtr<char[]> profiler_get_profile(double aSinceTime = 0,
                                                bool aIsShuttingDown = false);

// Write the profile for this process (excluding subprocesses) into aWriter.
// Returns false if the profiler is inactive.
bool profiler_stream_json_for_this_process(SpliceableJSONWriter& aWriter,
                                           double aSinceTime = 0,
                                           bool aIsShuttingDown = false);

// Get the profile and write it into a file. A no-op if the profile is
// inactive.
//
// This function is 'extern "C"' so that it is easily callable from a debugger
// in a build without debugging information (a workaround for
// http://llvm.org/bugs/show_bug.cgi?id=22211).
extern "C" {
void profiler_save_profile_to_file(const char* aFilename);
}

//---------------------------------------------------------------------------
// RAII classes
//---------------------------------------------------------------------------

namespace mozilla {

class MOZ_RAII AutoProfilerInit
{
public:
  explicit AutoProfilerInit(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_init(this);
  }

  ~AutoProfilerInit() { profiler_shutdown(); }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Convenience class to register and unregister a thread with the profiler.
// Needs to be the first object on the stack of the thread.
class MOZ_RAII AutoProfilerRegisterThread final
{
public:
  explicit AutoProfilerRegisterThread(const char* aName
                                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_register_thread(aName, this);
  }

  ~AutoProfilerRegisterThread() { profiler_unregister_thread(); }

private:
  AutoProfilerRegisterThread(const AutoProfilerRegisterThread&) = delete;
  AutoProfilerRegisterThread& operator=(const AutoProfilerRegisterThread&) =
    delete;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class MOZ_RAII AutoProfilerThreadSleep
{
public:
  explicit AutoProfilerThreadSleep(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_thread_sleep();
  }

  ~AutoProfilerThreadSleep() { profiler_thread_wake(); }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

// Temporarily wake up the profiling of a thread while servicing events such as
// Asynchronous Procedure Calls (APCs).
class MOZ_RAII AutoProfilerThreadWake
{
public:
  explicit AutoProfilerThreadWake(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
    : mIssuedWake(profiler_thread_is_sleeping())
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    if (mIssuedWake) {
      profiler_thread_wake();
    }
  }

  ~AutoProfilerThreadWake()
  {
    if (mIssuedWake) {
      MOZ_ASSERT(!profiler_thread_is_sleeping());
      profiler_thread_sleep();
    }
  }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  bool mIssuedWake;
};

// This class creates a non-owning ProfilingStack reference. Objects of this class
// are stack-allocated, and so exist within a thread, and are thus bounded by
// the lifetime of the thread, which ensures that the references held can't be
// used after the ProfilingStack is destroyed.
class MOZ_RAII AutoProfilerLabel
{
public:
  // This is the AUTO_PROFILER_LABEL and AUTO_PROFILER_LABEL_DYNAMIC variant.
  AutoProfilerLabel(const char* aLabel, const char* aDynamicString,
                    js::ProfilingStackFrame::Category aCategory,
                    uint32_t aFlags = 0
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    // Get the ProfilingStack from TLS.
    Push(sProfilingStack.get(), aLabel, aDynamicString, aCategory, aFlags);
  }

  // This is the AUTO_PROFILER_LABEL_FAST variant. It retrieves the ProfilingStack
  // from the JSContext and does nothing if the profiler is inactive.
  AutoProfilerLabel(JSContext* aJSContext,
                    const char* aLabel, const char* aDynamicString,
                    js::ProfilingStackFrame::Category aCategory,
                    uint32_t aFlags
                    MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    Push(js::GetContextProfilingStackIfEnabled(aJSContext),
         aLabel, aDynamicString, aCategory, aFlags);
  }

  void Push(ProfilingStack* aProfilingStack,
            const char* aLabel, const char* aDynamicString,
            js::ProfilingStackFrame::Category aCategory,
            uint32_t aFlags = 0)
  {
    // This function runs both on and off the main thread.

    mProfilingStack = aProfilingStack;
    if (mProfilingStack) {
      mProfilingStack->pushLabelFrame(aLabel, aDynamicString, this, aCategory,
                                      aFlags);
    }
  }

  ~AutoProfilerLabel()
  {
    // This function runs both on and off the main thread.

    if (mProfilingStack) {
      mProfilingStack->pop();
    }
  }

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  // We save a ProfilingStack pointer in the ctor so we don't have to redo the TLS
  // lookup in the dtor.
  ProfilingStack* mProfilingStack;

public:
  // See the comment on the definition in platform.cpp for details about this.
  static MOZ_THREAD_LOCAL(ProfilingStack*) sProfilingStack;
};

class MOZ_RAII AutoProfilerTracing
{
public:
  AutoProfilerTracing(const char* aCategory,
                      const char* aMarkerName,
                      const mozilla::Maybe<nsID>& aDocShellId,
                      const mozilla::Maybe<uint32_t>& aDocShellHistoryId
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mMarkerName(aMarkerName)
    , mDocShellId(aDocShellId)
    , mDocShellHistoryId(aDocShellHistoryId)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory,
                     mMarkerName,
                     TRACING_INTERVAL_START,
                     mDocShellId,
                     mDocShellHistoryId);
  }

  AutoProfilerTracing(const char* aCategory,
                      const char* aMarkerName,
                      UniqueProfilerBacktrace aBacktrace,
                      const mozilla::Maybe<nsID>& aDocShellId,
                      const mozilla::Maybe<uint32_t>& aDocShellHistoryId
                      MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mCategory(aCategory)
    , mMarkerName(aMarkerName)
    , mDocShellId(aDocShellId)
    , mDocShellHistoryId(aDocShellHistoryId)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    profiler_tracing(mCategory,
                     mMarkerName,
                     TRACING_INTERVAL_START,
                     std::move(aBacktrace),
                     mDocShellId,
                     mDocShellHistoryId);
  }

  ~AutoProfilerTracing()
  {
    profiler_tracing(mCategory,
                     mMarkerName,
                     TRACING_INTERVAL_END,
                     mDocShellId,
                     mDocShellHistoryId);
  }

protected:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  const char* mCategory;
  const char* mMarkerName;
  const mozilla::Maybe<nsID> mDocShellId;
  const mozilla::Maybe<uint32_t> mDocShellHistoryId;
};

// Set MOZ_PROFILER_STARTUP* environment variables that will be inherited into
// a child process that is about to be launched, in order to make that child
// process start with the same profiler settings as in the current process.
class MOZ_RAII AutoSetProfilerEnvVarsForChildProcess
{
public:
  explicit AutoSetProfilerEnvVarsForChildProcess(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  ~AutoSetProfilerEnvVarsForChildProcess();

private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  char mSetCapacity[64];
  char mSetInterval[64];
  char mSetFeaturesBitfield[64];
  char mSetFilters[1024];
};

} // namespace mozilla

#endif // !MOZ_GECKO_PROFILER

#endif  // GeckoProfiler_h
