/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This contains things related to the Gecko profiler, for use in third_party
// code. It is very minimal and is designed to be used by patching over
// upstream code.
// Only use the C ABI and guard C++ code with #ifdefs, don't pull anything from
// Gecko, it must be possible to include the header file into any C++ codebase.

#ifndef MICRO_GECKO_PROFILER
#define MICRO_GECKO_PROFILER

#ifdef __cplusplus
extern "C" {
#endif

#include <mozilla/Types.h>
#include <stdio.h>

#ifdef _WIN32
#  include <libloaderapi.h>
#else
#  include <dlfcn.h>
#endif

#include "ProfilerNativeStack.h"

#if !defined(CallerPC)
#  define CallerPC() __builtin_extract_return_addr(__builtin_return_address(0))
#endif  // !defined(CallerPC)

extern MOZ_EXPORT void uprofiler_register_thread(const char* aName,
                                                 void* aGuessStackTop);

extern MOZ_EXPORT void uprofiler_unregister_thread();

extern MOZ_EXPORT void uprofiler_simple_event_marker(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values);

extern MOZ_EXPORT void uprofiler_simple_event_marker_capture_stack(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values);

extern MOZ_EXPORT void uprofiler_simple_event_marker_with_stack(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values, void* provided_stack);

extern MOZ_EXPORT bool uprofiler_backtrace_into_buffer(
    struct NativeStack* stack, void* aBuffer);

extern MOZ_EXPORT void uprofiler_native_backtrace(const void* top,
                                                  struct NativeStack* stack);

extern MOZ_EXPORT bool uprofiler_is_active();

extern MOZ_EXPORT bool uprofiler_feature_active(int32_t feature);

extern MOZ_EXPORT bool uprofiler_get(struct UprofilerFuncPtrs* aFuncPtrs);

/* NOLINT because we want to stick to C here */
// NOLINTBEGIN(modernize-use-using)
typedef bool (*uprofiler_getter)(struct UprofilerFuncPtrs* aFuncPtrs);
// NOLINTEND(modernize-use-using)
#ifdef __cplusplus
}

struct AutoRegisterProfiler {
  AutoRegisterProfiler(const char* name, char* stacktop) {
    if (getenv("MOZ_UPROFILER_LOG_THREAD_CREATION")) {
      printf("### UProfiler: new thread: '%s'\n", name);
    }
    uprofiler_register_thread(name, stacktop);
  }
  ~AutoRegisterProfiler() { uprofiler_unregister_thread(); }
};
#endif  // __cplusplus

void uprofiler_simple_event_marker(const char* name, const char category,
                                   char phase, int num_args,
                                   const char** arg_names,
                                   const unsigned char* arg_types,
                                   const unsigned long long* arg_values);

struct UprofilerFuncPtrs {
  void (*register_thread)(const char* aName, void* aGuessStackTop);
  void (*unregister_thread)();
  void (*simple_event_marker)(const char* name, const char category, char phase,
                              int num_args, const char** arg_names,
                              const unsigned char* arg_types,
                              const unsigned long long* arg_values);
  void (*simple_event_marker_capture_stack)(
      const char* name, const char category, char phase, int num_args,
      const char** arg_names, const unsigned char* arg_types,
      const unsigned long long* arg_values);
  void (*simple_event_marker_with_stack)(const char* name, const char category,
                                         char phase, int num_args,
                                         const char** arg_names,
                                         const unsigned char* arg_types,
                                         const unsigned long long* arg_values,
                                         void* provided_stack);
  bool (*backtrace_into_buffer)(struct NativeStack* stack, void* aBuffer);
  void (*native_backtrace)(const void* top, struct NativeStack* stack);
  bool (*is_active)();
  bool (*feature_active)(int32_t feature);
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

static void register_thread_noop(const char* aName, void* aGuessStackTop) {
  /* no-op */
}
static void unregister_thread_noop() { /* no-op */ }
static void simple_event_marker_noop(const char* name, const char category,
                                     char phase, int num_args,
                                     const char** arg_names,
                                     const unsigned char* arg_types,
                                     const unsigned long long* arg_values) {
  /* no-op */
}

static void simple_event_marker_capture_stack_noop(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values) {
  /* no-op */
}

static void simple_event_marker_with_stack_noop(
    const char* name, const char category, char phase, int num_args,
    const char** arg_names, const unsigned char* arg_types,
    const unsigned long long* arg_values, void* provided_stack) {
  /* no-op */
}

static bool backtrace_into_buffer_noop(struct NativeStack* stack,
                                       void* aBuffer) { /* no-op */
  return false;
}

static void native_backtrace_noop(const void* top,
                                  struct NativeStack* stack) { /* no-op */ }

static bool is_active_noop() { /* no-op */ return false; }

static bool feature_active_noop(int32_t feature) { /* no-op */ return false; }

#pragma GCC diagnostic pop

#if defined(_WIN32)
#  define UPROFILER_OPENLIB() GetModuleHandle(NULL)
#else
#  define UPROFILER_OPENLIB() dlopen(NULL, RTLD_NOW)
#endif

#if defined(_WIN32)
#  define UPROFILER_GET_SYM(handle, sym) GetProcAddress(handle, sym)
#else
#  define UPROFILER_GET_SYM(handle, sym) (typeof(sym)*)(dlsym(handle, #sym))
#endif

#if defined(_WIN32)
#  define UPROFILER_PRINT_ERROR(func) fprintf(stderr, "%s error\n", #func);
#else
#  define UPROFILER_PRINT_ERROR(func) \
    fprintf(stderr, "%s error: %s\n", #func, dlerror());
#endif

#define FETCH(func)                                             \
  uprofiler.func = UPROFILER_GET_SYM(handle, uprofiler_##func); \
  if (!uprofiler.func) {                                        \
    UPROFILER_PRINT_ERROR(uprofiler_##func);                    \
    uprofiler.func = func##_noop;                               \
  }

#define UPROFILER_VISIT()                  \
  FETCH(register_thread)                   \
  FETCH(unregister_thread)                 \
  FETCH(simple_event_marker)               \
  FETCH(simple_event_marker_capture_stack) \
  FETCH(simple_event_marker_with_stack)    \
  FETCH(backtrace_into_buffer)             \
  FETCH(native_backtrace)                  \
  FETCH(is_active)                         \
  FETCH(feature_active)

// Assumes that a variable of type UprofilerFuncPtrs, named uprofiler
// is accessible in the scope
#define UPROFILER_GET_FUNCTIONS()                                 \
  void* handle = UPROFILER_OPENLIB();                             \
  if (!handle) {                                                  \
    UPROFILER_PRINT_ERROR(UPROFILER_OPENLIB);                     \
    uprofiler.register_thread = register_thread_noop;             \
    uprofiler.unregister_thread = unregister_thread_noop;         \
    uprofiler.simple_event_marker = simple_event_marker_noop;     \
    uprofiler.simple_event_marker_capture_stack =                 \
        simple_event_marker_capture_stack_noop;                   \
    uprofiler.simple_event_marker_with_stack =                    \
        simple_event_marker_with_stack_noop;                      \
    uprofiler.backtrace_into_buffer = backtrace_into_buffer_noop; \
    uprofiler.native_backtrace = native_backtrace_noop;           \
    uprofiler.is_active = is_active_noop;                         \
    uprofiler.feature_active = feature_active_noop;               \
  }                                                               \
  UPROFILER_VISIT()

#define UPROFILER_GET(var)                            \
  uprofiler_getter var = nullptr;                     \
  void* handle = UPROFILER_OPENLIB();                 \
  if (!handle) {                                      \
    UPROFILER_PRINT_ERROR(UPROFILER_OPENLIB);         \
  } else {                                            \
    (var) = UPROFILER_GET_SYM(handle, uprofiler_get); \
    if (!(var)) {                                     \
      UPROFILER_PRINT_ERROR(uprofiler_get);           \
    }                                                 \
  }

#endif  // MICRO_GECKO_PROFILER
