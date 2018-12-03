/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmCompile.h"

#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"

#include "jit/ProcessExecutableMemory.h"
#include "util/Text.h"
#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmIonCompile.h"
#include "wasm/WasmOpIter.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

uint32_t wasm::ObservedCPUFeatures() {
  enum Arch {
    X86 = 0x1,
    X64 = 0x2,
    ARM = 0x3,
    MIPS = 0x4,
    MIPS64 = 0x5,
    ARM64 = 0x6,
    ARCH_BITS = 3
  };

#if defined(JS_CODEGEN_X86)
  MOZ_ASSERT(uint32_t(jit::CPUInfo::GetSSEVersion()) <=
             (UINT32_MAX >> ARCH_BITS));
  return X86 | (uint32_t(jit::CPUInfo::GetSSEVersion()) << ARCH_BITS);
#elif defined(JS_CODEGEN_X64)
  MOZ_ASSERT(uint32_t(jit::CPUInfo::GetSSEVersion()) <=
             (UINT32_MAX >> ARCH_BITS));
  return X64 | (uint32_t(jit::CPUInfo::GetSSEVersion()) << ARCH_BITS);
#elif defined(JS_CODEGEN_ARM)
  MOZ_ASSERT(jit::GetARMFlags() <= (UINT32_MAX >> ARCH_BITS));
  return ARM | (jit::GetARMFlags() << ARCH_BITS);
#elif defined(JS_CODEGEN_ARM64)
  MOZ_ASSERT(jit::GetARM64Flags() <= (UINT32_MAX >> ARCH_BITS));
  return ARM64 | (jit::GetARM64Flags() << ARCH_BITS);
#elif defined(JS_CODEGEN_MIPS32)
  MOZ_ASSERT(jit::GetMIPSFlags() <= (UINT32_MAX >> ARCH_BITS));
  return MIPS | (jit::GetMIPSFlags() << ARCH_BITS);
#elif defined(JS_CODEGEN_MIPS64)
  MOZ_ASSERT(jit::GetMIPSFlags() <= (UINT32_MAX >> ARCH_BITS));
  return MIPS64 | (jit::GetMIPSFlags() << ARCH_BITS);
#elif defined(JS_CODEGEN_NONE)
  return 0;
#else
#error "unknown architecture"
#endif
}

CompileArgs::CompileArgs(JSContext* cx, ScriptedCaller&& scriptedCaller)
    : scriptedCaller(std::move(scriptedCaller)) {
#ifdef ENABLE_WASM_GC
  bool gcEnabled = cx->options().wasmGc();
#else
  bool gcEnabled = false;
#endif

  baselineEnabled = cx->options().wasmBaseline();
  ionEnabled = cx->options().wasmIon();
#ifdef ENABLE_WASM_CRANELIFT
  forceCranelift = cx->options().wasmForceCranelift();
#else
  forceCranelift = false;
#endif
  sharedMemoryEnabled =
      cx->realm()->creationOptions().getSharedMemoryAndAtomicsEnabled();
  gcTypesConfigured = gcEnabled ? HasGcTypes::True : HasGcTypes::False;
  testTiering = cx->options().testWasmAwaitTier2() || JitOptions.wasmDelayTier2;

  // Debug information such as source view or debug traps will require
  // additional memory and permanently stay in baseline code, so we try to
  // only enable it when a developer actually cares: when the debugger tab
  // is open.
  debugEnabled = cx->realm()->debuggerObservesAsmJS();
}

// Classify the current system as one of a set of recognizable classes.  This
// really needs to get our tier-1 systems right.
//
// TODO: We don't yet have a good measure of how fast a system is.  We
// distinguish between mobile and desktop because these are very different kinds
// of systems, but we could further distinguish between low / medium / high end
// within those major classes.  If we do so, then constants below would be
// provided for each (class, architecture, system-tier) combination, not just
// (class, architecture) as now.
//
// CPU clock speed is not by itself a good predictor of system performance, as
// there are high-performance systems with slow clocks (recent Intel) and
// low-performance systems with fast clocks (older AMD).  We can also use
// physical memory, core configuration, OS details, CPU class and family, and
// CPU manufacturer to disambiguate.

enum class SystemClass {
  DesktopX86,
  DesktopX64,
  DesktopUnknown32,
  DesktopUnknown64,
  MobileX86,
  MobileArm32,
  MobileArm64,
  MobileUnknown32,
  MobileUnknown64
};

static SystemClass ClassifySystem() {
  bool isDesktop;

#if defined(ANDROID) || defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_ARM64)
  isDesktop = false;
#else
  isDesktop = true;
#endif

  if (isDesktop) {
#if defined(JS_CODEGEN_X64)
    return SystemClass::DesktopX64;
#elif defined(JS_CODEGEN_X86)
    return SystemClass::DesktopX86;
#elif defined(JS_64BIT)
    return SystemClass::DesktopUnknown64;
#else
    return SystemClass::DesktopUnknown32;
#endif
  } else {
#if defined(JS_CODEGEN_X86)
    return SystemClass::MobileX86;
#elif defined(JS_CODEGEN_ARM)
    return SystemClass::MobileArm32;
#elif defined(JS_CODEGEN_ARM64)
    return SystemClass::MobileArm64;
#elif defined(JS_64BIT)
    return SystemClass::MobileUnknown64;
#else
    return SystemClass::MobileUnknown32;
#endif
  }
}

// Code sizes in machine code bytes per bytecode byte, again empirical except
// where marked as "Guess".

static const double x64Tox86Inflation = 1.25;

static const double x64IonBytesPerBytecode = 2.45;
static const double x86IonBytesPerBytecode =
    x64IonBytesPerBytecode * x64Tox86Inflation;
static const double arm32IonBytesPerBytecode = 3.3;
static const double arm64IonBytesPerBytecode = 3.0;  // Guess

static const double x64BaselineBytesPerBytecode = x64IonBytesPerBytecode * 1.43;
static const double x86BaselineBytesPerBytecode =
    x64BaselineBytesPerBytecode * x64Tox86Inflation;
static const double arm32BaselineBytesPerBytecode =
    arm32IonBytesPerBytecode * 1.39;
static const double arm64BaselineBytesPerBytecode =
    arm64IonBytesPerBytecode * 1.39;  // Guess

static double OptimizedBytesPerBytecode(SystemClass cls) {
  switch (cls) {
    case SystemClass::DesktopX86:
    case SystemClass::MobileX86:
    case SystemClass::DesktopUnknown32:
      return x86IonBytesPerBytecode;
    case SystemClass::DesktopX64:
    case SystemClass::DesktopUnknown64:
      return x64IonBytesPerBytecode;
    case SystemClass::MobileArm32:
    case SystemClass::MobileUnknown32:
      return arm32IonBytesPerBytecode;
    case SystemClass::MobileArm64:
    case SystemClass::MobileUnknown64:
      return arm64IonBytesPerBytecode;
    default:
      MOZ_CRASH();
  }
}

static double BaselineBytesPerBytecode(SystemClass cls) {
  switch (cls) {
    case SystemClass::DesktopX86:
    case SystemClass::MobileX86:
    case SystemClass::DesktopUnknown32:
      return x86BaselineBytesPerBytecode;
    case SystemClass::DesktopX64:
    case SystemClass::DesktopUnknown64:
      return x64BaselineBytesPerBytecode;
    case SystemClass::MobileArm32:
    case SystemClass::MobileUnknown32:
      return arm32BaselineBytesPerBytecode;
    case SystemClass::MobileArm64:
    case SystemClass::MobileUnknown64:
      return arm64BaselineBytesPerBytecode;
    default:
      MOZ_CRASH();
  }
}

double wasm::EstimateCompiledCodeSize(Tier tier, size_t bytecodeSize) {
  SystemClass cls = ClassifySystem();
  switch (tier) {
    case Tier::Baseline:
      return double(bytecodeSize) * BaselineBytesPerBytecode(cls);
    case Tier::Optimized:
      return double(bytecodeSize) * OptimizedBytesPerBytecode(cls);
  }
  MOZ_CRASH("bad tier");
}

// If parallel Ion compilation is going to take longer than this, we should
// tier.

static const double tierCutoffMs = 250;

// Compilation rate values are empirical except when noted, the reference
// systems are:
//
// Late-2013 MacBook Pro (2.6GHz quad hyperthreaded Haswell)
// Late-2015 Nexus 5X (1.4GHz quad Cortex-A53 + 1.8GHz dual Cortex-A57)

static const double x64BytecodesPerMs = 2100;
static const double x86BytecodesPerMs = 1500;
static const double arm32BytecodesPerMs = 450;
static const double arm64BytecodesPerMs = 650;  // Guess

// Tiering cutoff values: if code section sizes are below these values (when
// divided by the effective number of cores) we do not tier, because we guess
// that parallel Ion compilation will be fast enough.

static const double x64DesktopTierCutoff = x64BytecodesPerMs * tierCutoffMs;
static const double x86DesktopTierCutoff = x86BytecodesPerMs * tierCutoffMs;
static const double x86MobileTierCutoff = x86DesktopTierCutoff / 2;  // Guess
static const double arm32MobileTierCutoff = arm32BytecodesPerMs * tierCutoffMs;
static const double arm64MobileTierCutoff = arm64BytecodesPerMs * tierCutoffMs;

static double CodesizeCutoff(SystemClass cls, uint32_t codeSize) {
  switch (cls) {
    case SystemClass::DesktopX86:
    case SystemClass::DesktopUnknown32:
      return x86DesktopTierCutoff;
    case SystemClass::DesktopX64:
    case SystemClass::DesktopUnknown64:
      return x64DesktopTierCutoff;
    case SystemClass::MobileX86:
      return x86MobileTierCutoff;
    case SystemClass::MobileArm32:
    case SystemClass::MobileUnknown32:
      return arm32MobileTierCutoff;
    case SystemClass::MobileArm64:
    case SystemClass::MobileUnknown64:
      return arm64MobileTierCutoff;
    default:
      MOZ_CRASH();
  }
}

// As the number of cores grows the effectiveness of each core dwindles (on the
// systems we care about for SpiderMonkey).
//
// The data are empirical, computed from the observed compilation time of the
// Tanks demo code on a variable number of cores.
//
// The heuristic may fail on NUMA systems where the core count is high but the
// performance increase is nil or negative once the program moves beyond one
// socket.  However, few browser users have such systems.

static double EffectiveCores(SystemClass cls, uint32_t cores) {
  if (cores <= 3) {
    return pow(cores, 0.9);
  }
  return pow(cores, 0.75);
}

#ifndef JS_64BIT
// Don't tier if tiering will fill code memory to more to more than this
// fraction.

static const double spaceCutoffPct = 0.9;
#endif

// Figure out whether we should use tiered compilation or not.
static bool TieringBeneficial(uint32_t codeSize) {
  uint32_t cpuCount = HelperThreadState().cpuCount;
  MOZ_ASSERT(cpuCount > 0);

  // It's mostly sensible not to background compile when there's only one
  // hardware thread as we want foreground computation to have access to that.
  // However, if wasm background compilation helper threads can be given lower
  // priority then background compilation on single-core systems still makes
  // some kind of sense.  That said, this is a non-issue: as of September 2017
  // 1-core was down to 3.5% of our population and falling.

  if (cpuCount == 1) {
    return false;
  }

  MOZ_ASSERT(HelperThreadState().threadCount >= cpuCount);

  // Compute the max number of threads available to do actual background
  // compilation work.

  uint32_t workers = HelperThreadState().maxWasmCompilationThreads();

  // The number of cores we will use is bounded both by the CPU count and the
  // worker count.

  uint32_t cores = Min(cpuCount, workers);

  SystemClass cls = ClassifySystem();

  // Ion compilation on available cores must take long enough to be worth the
  // bother.

  double cutoffSize = CodesizeCutoff(cls, codeSize);
  double effectiveCores = EffectiveCores(cls, cores);

  if ((codeSize / effectiveCores) < cutoffSize) {
    return false;
  }

  // Do not implement a size cutoff for 64-bit systems since the code size
  // budget for 64 bit is so large that it will hardly ever be an issue.
  // (Also the cutoff percentage might be different on 64-bit.)

#ifndef JS_64BIT
  // If the amount of executable code for baseline compilation jeopardizes the
  // availability of executable memory for ion code then do not tier, for now.
  //
  // TODO: For now we consider this module in isolation.  We should really
  // worry about what else is going on in this process and might be filling up
  // the code memory.  It's like we need some kind of code memory reservation
  // system or JIT compilation for large modules.

  double ionRatio = OptimizedBytesPerBytecode(cls);
  double baselineRatio = BaselineBytesPerBytecode(cls);
  double needMemory = codeSize * (ionRatio + baselineRatio);
  double availMemory = LikelyAvailableExecutableMemory();
  double cutoff = spaceCutoffPct * MaxCodeBytesPerProcess;

  // If the sum of baseline and ion code makes us exceeds some set percentage
  // of the executable memory then disable tiering.

  if ((MaxCodeBytesPerProcess - availMemory) + needMemory > cutoff) {
    return false;
  }
#endif

  return true;
}

CompilerEnvironment::CompilerEnvironment(const CompileArgs& args)
    : state_(InitialWithArgs), args_(&args) {}

CompilerEnvironment::CompilerEnvironment(CompileMode mode, Tier tier,
                                         OptimizedBackend optimizedBackend,
                                         DebugEnabled debugEnabled,
                                         HasGcTypes gcTypesConfigured)
    : state_(InitialWithModeTierDebug),
      mode_(mode),
      tier_(tier),
      optimizedBackend_(optimizedBackend),
      debug_(debugEnabled),
      gcTypes_(gcTypesConfigured) {}

void CompilerEnvironment::computeParameters(HasGcTypes gcFeatureOptIn) {
  MOZ_ASSERT(state_ == InitialWithModeTierDebug);

  if (gcTypes_ == HasGcTypes::True) {
    gcTypes_ = gcFeatureOptIn;
  }
  state_ = Computed;
}

void CompilerEnvironment::computeParameters(Decoder& d,
                                            HasGcTypes gcFeatureOptIn) {
  MOZ_ASSERT(!isComputed());

  if (state_ == InitialWithModeTierDebug) {
    computeParameters(gcFeatureOptIn);
    return;
  }

  bool gcEnabled = args_->gcTypesConfigured == HasGcTypes::True &&
                   gcFeatureOptIn == HasGcTypes::True;
  bool argBaselineEnabled = args_->baselineEnabled || gcEnabled;
  bool argIonEnabled = args_->ionEnabled && !gcEnabled;
  bool argTestTiering = args_->testTiering && !gcEnabled;
  bool argDebugEnabled = args_->debugEnabled;

  uint32_t codeSectionSize = 0;

  SectionRange range;
  if (StartsCodeSection(d.begin(), d.end(), &range)) {
    codeSectionSize = range.size;
  }

  // Attempt to default to ion if baseline is disabled.
  bool baselineEnabled =
      BaselineCanCompile() && (argBaselineEnabled || argTestTiering);
  bool debugEnabled = BaselineCanCompile() && argDebugEnabled;
  bool ionEnabled =
      IonCanCompile() && (argIonEnabled || !baselineEnabled || argTestTiering);
#ifdef ENABLE_WASM_CRANELIFT
  bool forceCranelift = args_->forceCranelift;
#endif

  // HasCompilerSupport() should prevent failure here
  MOZ_RELEASE_ASSERT(baselineEnabled || ionEnabled);

  if (baselineEnabled && ionEnabled && !debugEnabled && CanUseExtraThreads() &&
      (TieringBeneficial(codeSectionSize) || argTestTiering)) {
    mode_ = CompileMode::Tier1;
    tier_ = Tier::Baseline;
  } else {
    mode_ = CompileMode::Once;
    tier_ = debugEnabled || !ionEnabled ? Tier::Baseline : Tier::Optimized;
  }

  optimizedBackend_ = OptimizedBackend::Ion;
#ifdef ENABLE_WASM_CRANELIFT
  if (forceCranelift) {
    optimizedBackend_ = OptimizedBackend::Cranelift;
  }
#endif

  debug_ = debugEnabled ? DebugEnabled::True : DebugEnabled::False;
  gcTypes_ = gcEnabled ? HasGcTypes::True : HasGcTypes::False;
  state_ = Computed;
}

template <class DecoderT>
static bool DecodeFunctionBody(DecoderT& d, ModuleGenerator& mg,
                               uint32_t funcIndex) {
  uint32_t bodySize;
  if (!d.readVarU32(&bodySize)) {
    return d.fail("expected number of function body bytes");
  }

  if (bodySize > MaxFunctionBytes) {
    return d.fail("function body too big");
  }

  const size_t offsetInModule = d.currentOffset();

  // Skip over the function body; it will be validated by the compilation
  // thread.
  const uint8_t* bodyBegin;
  if (!d.readBytes(bodySize, &bodyBegin)) {
    return d.fail("function body length too big");
  }

  return mg.compileFuncDef(funcIndex, offsetInModule, bodyBegin,
                           bodyBegin + bodySize);
}

template <class DecoderT>
static bool DecodeCodeSection(const ModuleEnvironment& env, DecoderT& d,
                              ModuleGenerator& mg) {
  if (!env.codeSection) {
    if (env.numFuncDefs() != 0) {
      return d.fail("expected code section");
    }

    return mg.finishFuncDefs();
  }

  uint32_t numFuncDefs;
  if (!d.readVarU32(&numFuncDefs)) {
    return d.fail("expected function body count");
  }

  if (numFuncDefs != env.numFuncDefs()) {
    return d.fail(
        "function body count does not match function signature count");
  }

  for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs; funcDefIndex++) {
    if (!DecodeFunctionBody(d, mg, env.numFuncImports() + funcDefIndex)) {
      return false;
    }
  }

  if (!d.finishSection(*env.codeSection, "code")) {
    return false;
  }

  return mg.finishFuncDefs();
}

SharedModule wasm::CompileBuffer(const CompileArgs& args,
                                 const ShareableBytes& bytecode,
                                 UniqueChars* error,
                                 UniqueCharsVector* warnings,
                                 UniqueLinkData* maybeLinkData) {
  Decoder d(bytecode.bytes, 0, error, warnings);

  CompilerEnvironment compilerEnv(args);
  ModuleEnvironment env(
      args.gcTypesConfigured, &compilerEnv,
      args.sharedMemoryEnabled ? Shareable::True : Shareable::False);
  if (!DecodeModuleEnvironment(d, &env)) {
    return nullptr;
  }

  ModuleGenerator mg(args, &env, nullptr, error);
  if (!mg.init()) {
    return nullptr;
  }

  if (!DecodeCodeSection(env, d, mg)) {
    return nullptr;
  }

  if (!DecodeModuleTail(d, &env, mg.deferredValidationState())) {
    return nullptr;
  }

  return mg.finishModule(bytecode, nullptr, maybeLinkData);
}

void wasm::CompileTier2(const CompileArgs& args, const Bytes& bytecode,
                        const Module& module, Atomic<bool>* cancelled) {
  UniqueChars error;
  Decoder d(bytecode, 0, &error);

  HasGcTypes gcTypesConfigured =
      HasGcTypes::False;  // No optimized backend support yet
  OptimizedBackend optimizedBackend =
      args.forceCranelift ? OptimizedBackend::Cranelift : OptimizedBackend::Ion;

  CompilerEnvironment compilerEnv(CompileMode::Tier2, Tier::Optimized,
                                  optimizedBackend, DebugEnabled::False,
                                  gcTypesConfigured);

  ModuleEnvironment env(
      gcTypesConfigured, &compilerEnv,
      args.sharedMemoryEnabled ? Shareable::True : Shareable::False);
  if (!DecodeModuleEnvironment(d, &env)) {
    return;
  }

  MOZ_ASSERT(env.gcTypesEnabled() == HasGcTypes::False,
             "can't ion-compile with gc types yet");

  ModuleGenerator mg(args, &env, cancelled, &error);
  if (!mg.init()) {
    return;
  }

  if (!DecodeCodeSection(env, d, mg)) {
    return;
  }

  if (!DecodeModuleTail(d, &env, mg.deferredValidationState())) {
    return;
  }

  if (!mg.finishTier2(module)) {
    return;
  }

  // The caller doesn't care about success or failure; only that compilation
  // is inactive, so there is no success to return here.
}

class StreamingDecoder {
  Decoder d_;
  const ExclusiveBytesPtr& codeBytesEnd_;
  const Atomic<bool>& cancelled_;

 public:
  StreamingDecoder(const ModuleEnvironment& env, const Bytes& begin,
                   const ExclusiveBytesPtr& codeBytesEnd,
                   const Atomic<bool>& cancelled, UniqueChars* error,
                   UniqueCharsVector* warnings)
      : d_(begin, env.codeSection->start, error, warnings),
        codeBytesEnd_(codeBytesEnd),
        cancelled_(cancelled) {}

  bool fail(const char* msg) { return d_.fail(msg); }

  bool done() const { return d_.done(); }

  size_t currentOffset() const { return d_.currentOffset(); }

  bool waitForBytes(size_t numBytes) {
    numBytes = Min(numBytes, d_.bytesRemain());
    const uint8_t* requiredEnd = d_.currentPosition() + numBytes;
    auto codeBytesEnd = codeBytesEnd_.lock();
    while (codeBytesEnd < requiredEnd) {
      if (cancelled_) {
        return false;
      }
      codeBytesEnd.wait();
    }
    return true;
  }

  bool readVarU32(uint32_t* u32) {
    return waitForBytes(MaxVarU32DecodedBytes) && d_.readVarU32(u32);
  }

  bool readBytes(size_t size, const uint8_t** begin) {
    return waitForBytes(size) && d_.readBytes(size, begin);
  }

  bool finishSection(const SectionRange& range, const char* name) {
    return d_.finishSection(range, name);
  }
};

static SharedBytes CreateBytecode(const Bytes& env, const Bytes& code,
                                  const Bytes& tail, UniqueChars* error) {
  size_t size = env.length() + code.length() + tail.length();
  if (size > MaxModuleBytes) {
    *error = DuplicateString("module too big");
    return nullptr;
  }

  MutableBytes bytecode = js_new<ShareableBytes>();
  if (!bytecode || !bytecode->bytes.resize(size)) {
    return nullptr;
  }

  uint8_t* p = bytecode->bytes.begin();

  memcpy(p, env.begin(), env.length());
  p += env.length();

  memcpy(p, code.begin(), code.length());
  p += code.length();

  memcpy(p, tail.begin(), tail.length());
  p += tail.length();

  MOZ_ASSERT(p == bytecode->end());

  return bytecode;
}

SharedModule wasm::CompileStreaming(
    const CompileArgs& args, const Bytes& envBytes, const Bytes& codeBytes,
    const ExclusiveBytesPtr& codeBytesEnd,
    const ExclusiveStreamEndData& exclusiveStreamEnd,
    const Atomic<bool>& cancelled, UniqueChars* error,
    UniqueCharsVector* warnings) {
  CompilerEnvironment compilerEnv(args);
  Maybe<ModuleEnvironment> env;

  {
    Decoder d(envBytes, 0, error, warnings);

    env.emplace(args.gcTypesConfigured, &compilerEnv,
                args.sharedMemoryEnabled ? Shareable::True : Shareable::False);
    if (!DecodeModuleEnvironment(d, env.ptr())) {
      return nullptr;
    }

    MOZ_ASSERT(d.done());
  }

  ModuleGenerator mg(args, env.ptr(), &cancelled, error);
  if (!mg.init()) {
    return nullptr;
  }

  {
    MOZ_ASSERT(env->codeSection->size == codeBytes.length());
    StreamingDecoder d(*env, codeBytes, codeBytesEnd, cancelled, error,
                       warnings);

    if (!DecodeCodeSection(*env, d, mg)) {
      return nullptr;
    }

    MOZ_ASSERT(d.done());
  }

  {
    auto streamEnd = exclusiveStreamEnd.lock();
    while (!streamEnd->reached) {
      if (cancelled) {
        return nullptr;
      }
      streamEnd.wait();
    }
  }

  const StreamEndData& streamEnd = exclusiveStreamEnd.lock();
  const Bytes& tailBytes = *streamEnd.tailBytes;

  {
    Decoder d(tailBytes, env->codeSection->end(), error, warnings);

    if (!DecodeModuleTail(d, env.ptr(), mg.deferredValidationState())) {
      return nullptr;
    }

    MOZ_ASSERT(d.done());
  }

  SharedBytes bytecode = CreateBytecode(envBytes, codeBytes, tailBytes, error);
  if (!bytecode) {
    return nullptr;
  }

  return mg.finishModule(*bytecode, streamEnd.tier2Listener);
}
