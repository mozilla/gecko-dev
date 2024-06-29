/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2017 Mozilla Foundation
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

#include "wasm/WasmProcess.h"

#include "mozilla/Attributes.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/ScopeExit.h"

#include "gc/Memory.h"
#include "threading/ExclusiveData.h"
#include "vm/MutexIDs.h"
#include "vm/Runtime.h"
#include "wasm/WasmBuiltinModule.h"
#include "wasm/WasmBuiltins.h"
#include "wasm/WasmCode.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmModuleTypes.h"
#include "wasm/WasmStaticTypeDefs.h"

using namespace js;
using namespace wasm;

using mozilla::BinarySearchIf;

// Per-process map from values of program-counter (pc) to CodeBlocks.
//
// Whenever a new CodeBlock is ready to use, it has to be registered so that
// we can have fast lookups from pc to CodeBlocks in numerous places. Since
// wasm compilation may be tiered, and the second tier doesn't have access to
// any JSContext/JS::Compartment/etc lying around, we have to use a process-wide
// map instead.

using CodeBlockVector = Vector<const CodeBlock*, 0, SystemAllocPolicy>;

Atomic<bool> wasm::CodeExists(false);

// Because of profiling, the thread running wasm might need to know to which
// CodeBlock the current PC belongs, during a call to lookup(). A lookup
// is a read-only operation, and we don't want to take a lock then
// (otherwise, we could have a deadlock situation if an async lookup
// happened on a given thread that was holding mutatorsMutex_ while getting
// sampled). Since the writer could be modifying the data that is getting
// looked up, the writer functions use spin-locks to know if there are any
// observers (i.e. calls to lookup()) of the atomic data.

static Atomic<size_t> sNumActiveLookups(0);

class ThreadSafeCodeBlockMap {
  // Since writes (insertions or removals) can happen on any background
  // thread at the same time, we need a lock here.

  Mutex mutatorsMutex_ MOZ_UNANNOTATED;

  CodeBlockVector segments1_;
  CodeBlockVector segments2_;

  // Except during swapAndWait(), there are no lookup() observers of the
  // vector pointed to by mutableCodeBlocks_

  CodeBlockVector* mutableCodeBlocks_;
  Atomic<const CodeBlockVector*> readonlyCodeBlocks_;

  struct CodeBlockPC {
    const void* pc;
    explicit CodeBlockPC(const void* pc) : pc(pc) {}
    int operator()(const CodeBlock* cb) const {
      if (cb->containsCodePC(pc)) {
        return 0;
      }
      if (pc < cb->base()) {
        return -1;
      }
      return 1;
    }
  };

  void swapAndWait() {
    // Both vectors are consistent for lookup at this point although their
    // contents are different: there is no way for the looked up PC to be
    // in the code segment that is getting registered, because the code
    // segment is not even fully created yet.

    // If a lookup happens before this instruction, then the
    // soon-to-become-former read-only pointer is used during the lookup,
    // which is valid.

    mutableCodeBlocks_ = const_cast<CodeBlockVector*>(
        readonlyCodeBlocks_.exchange(mutableCodeBlocks_));

    // If a lookup happens after this instruction, then the updated vector
    // is used, which is valid:
    // - in case of insertion, it means the new vector contains more data,
    // but it's fine since the code segment is getting registered and thus
    // isn't even fully created yet, so the code can't be running.
    // - in case of removal, it means the new vector contains one less
    // entry, but it's fine since unregistering means the code segment
    // isn't used by any live instance anymore, thus PC can't be in the
    // to-be-removed code segment's range.

    // A lookup could have happened on any of the two vectors. Wait for
    // observers to be done using any vector before mutating.

    while (sNumActiveLookups > 0) {
    }
  }

 public:
  ThreadSafeCodeBlockMap()
      : mutatorsMutex_(mutexid::WasmCodeBlockMap),
        mutableCodeBlocks_(&segments1_),
        readonlyCodeBlocks_(&segments2_) {}

  ~ThreadSafeCodeBlockMap() {
    MOZ_RELEASE_ASSERT(sNumActiveLookups == 0);
    MOZ_ASSERT(segments1_.empty());
    MOZ_ASSERT(segments2_.empty());
    segments1_.clearAndFree();
    segments2_.clearAndFree();
  }

  bool insert(const CodeBlock* cs) {
    LockGuard<Mutex> lock(mutatorsMutex_);

    size_t index;
    MOZ_ALWAYS_FALSE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                    mutableCodeBlocks_->length(),
                                    CodeBlockPC(cs->base()), &index));

    if (!mutableCodeBlocks_->insert(mutableCodeBlocks_->begin() + index, cs)) {
      return false;
    }

    CodeExists = true;

    swapAndWait();

#ifdef DEBUG
    size_t otherIndex;
    MOZ_ALWAYS_FALSE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                    mutableCodeBlocks_->length(),
                                    CodeBlockPC(cs->base()), &otherIndex));
    MOZ_ASSERT(index == otherIndex);
#endif

    // Although we could simply revert the insertion in the read-only
    // vector, it is simpler to just crash and given that each CodeBlock
    // consumes multiple pages, it is unlikely this insert() would OOM in
    // practice
    AutoEnterOOMUnsafeRegion oom;
    if (!mutableCodeBlocks_->insert(mutableCodeBlocks_->begin() + index, cs)) {
      oom.crash("when inserting a CodeBlock in the process-wide map");
    }

    return true;
  }

  void remove(const CodeBlock* cs) {
    LockGuard<Mutex> lock(mutatorsMutex_);

    size_t index;
    MOZ_ALWAYS_TRUE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                   mutableCodeBlocks_->length(),
                                   CodeBlockPC(cs->base()), &index));

    mutableCodeBlocks_->erase(mutableCodeBlocks_->begin() + index);

    if (!mutableCodeBlocks_->length()) {
      CodeExists = false;
    }

    swapAndWait();

#ifdef DEBUG
    size_t otherIndex;
    MOZ_ALWAYS_TRUE(BinarySearchIf(*mutableCodeBlocks_, 0,
                                   mutableCodeBlocks_->length(),
                                   CodeBlockPC(cs->base()), &otherIndex));
    MOZ_ASSERT(index == otherIndex);
#endif

    mutableCodeBlocks_->erase(mutableCodeBlocks_->begin() + index);
  }

  const CodeBlock* lookup(const void* pc) {
    const CodeBlockVector* readonly = readonlyCodeBlocks_;

    size_t index;
    if (!BinarySearchIf(*readonly, 0, readonly->length(), CodeBlockPC(pc),
                        &index)) {
      return nullptr;
    }

    // It is fine returning a raw CodeBlock*, because we assume we are
    // looking up a live PC in code which is on the stack, keeping the
    // CodeBlock alive.

    return (*readonly)[index];
  }
};

// This field is only atomic to handle buggy scenarios where we crash during
// startup or shutdown and thus racily perform wasm::LookupCodeBlock() from
// the crashing thread.

static Atomic<ThreadSafeCodeBlockMap*> sThreadSafeCodeBlockMap(nullptr);

bool wasm::RegisterCodeBlock(const CodeBlock* cs) {
  MOZ_ASSERT(cs->code->initialized());

  // This function cannot race with startup/shutdown.
  ThreadSafeCodeBlockMap* map = sThreadSafeCodeBlockMap;
  MOZ_RELEASE_ASSERT(map);
  return map->insert(cs);
}

void wasm::UnregisterCodeBlock(const CodeBlock* cs) {
  // This function cannot race with startup/shutdown.
  ThreadSafeCodeBlockMap* map = sThreadSafeCodeBlockMap;
  MOZ_RELEASE_ASSERT(map);
  map->remove(cs);
}

const CodeBlock* wasm::LookupCodeBlock(
    const void* pc, const CodeRange** codeRange /*= nullptr */) {
  // Since wasm::LookupCodeBlock() can race with wasm::ShutDown(), we must
  // additionally keep sNumActiveLookups above zero for the duration we're
  // using the ThreadSafeCodeBlockMap. wasm::ShutDown() spin-waits on
  // sNumActiveLookups getting to zero.

  auto decObserver = mozilla::MakeScopeExit([&] {
    MOZ_ASSERT(sNumActiveLookups > 0);
    sNumActiveLookups--;
  });
  sNumActiveLookups++;

  ThreadSafeCodeBlockMap* map = sThreadSafeCodeBlockMap;
  if (!map) {
    return nullptr;
  }

  if (const CodeBlock* found = map->lookup(pc)) {
    if (codeRange) {
      *codeRange = found->lookupRange(pc);
    }
    return found;
  }

  if (codeRange) {
    *codeRange = nullptr;
  }

  return nullptr;
}

const Code* wasm::LookupCode(const void* pc,
                             const CodeRange** codeRange /* = nullptr */) {
  const CodeBlock* found = LookupCodeBlock(pc, codeRange);
  MOZ_ASSERT_IF(!found && codeRange, !*codeRange);
  return found ? found->code : nullptr;
}

bool wasm::InCompiledCode(void* pc) {
  if (LookupCodeBlock(pc)) {
    return true;
  }

  const CodeRange* codeRange;
  const uint8_t* codeBase;
  return LookupBuiltinThunk(pc, &codeRange, &codeBase);
}

/**
 * ReadLockFlag maintains a flag that can be mutated multiple times before it
 * is read, at which point it maintains the same value.
 */
class ReadLockFlag {
 private:
  bool enabled_;
  bool read_;

 public:
  ReadLockFlag() : enabled_(false), read_(false) {}

  bool get() {
    read_ = true;
    return enabled_;
  }

  bool set(bool enabled) {
    if (read_) {
      return false;
    }
    enabled_ = enabled;
    return true;
  }
};

#ifdef WASM_SUPPORTS_HUGE_MEMORY
/*
 * Some 64 bit systems greatly limit the range of available virtual memory. We
 * require about 6GiB for each wasm huge memory, which can exhaust the address
 * spaces of these systems quickly. In order to avoid this, we only enable huge
 * memory if we observe a large enough address space.
 *
 * This number is conservatively chosen to continue using huge memory on our
 * smallest address space system, Android on ARM64 (39 bits), along with a bit
 * for error in detecting the address space limit.
 */
static const size_t MinAddressBitsForHugeMemory = 38;

/*
 * In addition to the above, some systems impose an independent limit on the
 * amount of virtual memory that may be used.
 */
static const size_t MinVirtualMemoryLimitForHugeMemory =
    size_t(1) << MinAddressBitsForHugeMemory;
#endif

ExclusiveData<ReadLockFlag> sHugeMemoryEnabled32(
    mutexid::WasmHugeMemoryEnabled);
ExclusiveData<ReadLockFlag> sHugeMemoryEnabled64(
    mutexid::WasmHugeMemoryEnabled);

static MOZ_NEVER_INLINE bool IsHugeMemoryEnabledHelper32() {
  auto state = sHugeMemoryEnabled32.lock();
  return state->get();
}

static MOZ_NEVER_INLINE bool IsHugeMemoryEnabledHelper64() {
  auto state = sHugeMemoryEnabled64.lock();
  return state->get();
}

bool wasm::IsHugeMemoryEnabled(wasm::IndexType t) {
  if (t == IndexType::I32) {
    static bool enabled32 = IsHugeMemoryEnabledHelper32();
    return enabled32;
  }
  static bool enabled64 = IsHugeMemoryEnabledHelper64();
  return enabled64;
}

bool wasm::DisableHugeMemory() {
  bool ok = true;
  {
    auto state = sHugeMemoryEnabled64.lock();
    ok = ok && state->set(false);
  }
  {
    auto state = sHugeMemoryEnabled32.lock();
    ok = ok && state->set(false);
  }
  return ok;
}

void ConfigureHugeMemory() {
#ifdef WASM_SUPPORTS_HUGE_MEMORY
  bool ok = true;

  {
    // Currently no huge memory for IndexType::I64, so always set to false.
    auto state = sHugeMemoryEnabled64.lock();
    ok = ok && state->set(false);
  }

  if (gc::SystemAddressBits() < MinAddressBitsForHugeMemory) {
    return;
  }

  if (gc::VirtualMemoryLimit() != size_t(-1) &&
      gc::VirtualMemoryLimit() < MinVirtualMemoryLimitForHugeMemory) {
    return;
  }

  {
    auto state = sHugeMemoryEnabled32.lock();
    ok = ok && state->set(true);
  }

  MOZ_RELEASE_ASSERT(ok);
#endif
}

const TagType* wasm::sWrappedJSValueTagType = nullptr;

static bool InitTagForJSValue() {
  MutableTagType type = js_new<TagType>();
  if (!type) {
    return false;
  }

  ValTypeVector args;
  if (!args.append(ValType(RefType::extern_()))) {
    return false;
  }

  if (!type->initialize(std::move(args))) {
    return false;
  }
  MOZ_ASSERT(WrappedJSValueTagType_ValueOffset == type->argOffsets()[0]);

  type.forget(&sWrappedJSValueTagType);

  return true;
}

bool wasm::Init() {
  MOZ_RELEASE_ASSERT(!sThreadSafeCodeBlockMap);

  // Assert invariants that should universally hold true, but cannot be checked
  // at compile time.
  uintptr_t pageSize = gc::SystemPageSize();
  MOZ_RELEASE_ASSERT(wasm::NullPtrGuardSize <= pageSize);
  MOZ_RELEASE_ASSERT(intptr_t(nullptr) == AnyRef::NullRefValue);

  ConfigureHugeMemory();

  AutoEnterOOMUnsafeRegion oomUnsafe;
  ThreadSafeCodeBlockMap* map = js_new<ThreadSafeCodeBlockMap>();
  if (!map) {
    oomUnsafe.crash("js::wasm::Init");
  }

  if (!StaticTypeDefs::init()) {
    oomUnsafe.crash("js::wasm::Init");
  }

  // This uses StaticTypeDefs
  if (!BuiltinModuleFuncs::init()) {
    oomUnsafe.crash("js::wasm::Init");
  }

  sThreadSafeCodeBlockMap = map;

  if (!InitTagForJSValue()) {
    oomUnsafe.crash("js::wasm::Init");
  }

  return true;
}

void wasm::ShutDown() {
  // If there are live runtimes then we are already pretty much leaking the
  // world, so to avoid spurious assertions (which are valid and valuable when
  // there are not live JSRuntimes), don't bother releasing anything here.
  if (JSRuntime::hasLiveRuntimes()) {
    return;
  }

  BuiltinModuleFuncs::destroy();
  StaticTypeDefs::destroy();
  PurgeCanonicalTypes();

  if (sWrappedJSValueTagType) {
    sWrappedJSValueTagType->Release();
    sWrappedJSValueTagType = nullptr;
  }

  // After signalling shutdown by clearing sThreadSafeCodeBlockMap, wait for
  // concurrent wasm::LookupCodeBlock()s to finish.
  ThreadSafeCodeBlockMap* map = sThreadSafeCodeBlockMap;
  MOZ_RELEASE_ASSERT(map);
  sThreadSafeCodeBlockMap = nullptr;
  while (sNumActiveLookups > 0) {
  }

  ReleaseBuiltinThunks();
  js_delete(map);
}
