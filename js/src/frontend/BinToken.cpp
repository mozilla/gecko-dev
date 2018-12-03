/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinToken.h"

#include "mozilla/Maybe.h"

#include <sys/types.h>

#include "frontend/BinSourceRuntimeSupport.h"
#include "frontend/TokenStream.h"
#include "js/Result.h"
#include "vm/JSContext.h"

namespace js {
namespace frontend {

const BinaryASTSupport::CharSlice BINKIND_DESCRIPTIONS[] = {
#define WITH_VARIANT(_, SPEC_NAME) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_KIND(WITH_VARIANT)
#undef WITH_VARIANT
};

const BinaryASTSupport::CharSlice BINFIELD_DESCRIPTIONS[] = {
#define WITH_VARIANT(_, SPEC_NAME) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_FIELD(WITH_VARIANT)
#undef WITH_VARIANT
};

const BinaryASTSupport::CharSlice BINVARIANT_DESCRIPTIONS[] = {
#define WITH_VARIANT(_, SPEC_NAME) \
  BinaryASTSupport::CharSlice(SPEC_NAME, sizeof(SPEC_NAME) - 1),
    FOR_EACH_BIN_VARIANT(WITH_VARIANT)
#undef WITH_VARIANT
};

const BinaryASTSupport::CharSlice& getBinKind(const BinKind& variant) {
  return BINKIND_DESCRIPTIONS[static_cast<size_t>(variant)];
}

const BinaryASTSupport::CharSlice& getBinVariant(const BinVariant& variant) {
  return BINVARIANT_DESCRIPTIONS[static_cast<size_t>(variant)];
}

const BinaryASTSupport::CharSlice& getBinField(const BinField& variant) {
  return BINFIELD_DESCRIPTIONS[static_cast<size_t>(variant)];
}

const char* describeBinKind(const BinKind& variant) {
  return getBinKind(variant).begin();
}

const char* describeBinField(const BinField& variant) {
  return getBinField(variant).begin();
}

const char* describeBinVariant(const BinVariant& variant) {
  return getBinVariant(variant).begin();
}

}  // namespace frontend

BinaryASTSupport::BinaryASTSupport()
    : binKindMap_(frontend::BINKIND_LIMIT),
      binFieldMap_(frontend::BINFIELD_LIMIT),
      binVariantMap_(frontend::BINVARIANT_LIMIT) {}

/**
 * It is expected that all bin tables are initialized on the main thread, and
 * that any helper threads will find the read-only tables properly initialized,
 * so that they can do their accesses safely without taking any locks.
 */
bool BinaryASTSupport::ensureBinTablesInitialized(JSContext* cx) {
  return ensureBinKindsInitialized(cx) && ensureBinVariantsInitialized(cx);
}

bool BinaryASTSupport::ensureBinKindsInitialized(JSContext* cx) {
  MOZ_ASSERT(!cx->helperThread());
  if (binKindMap_.empty()) {
    for (size_t i = 0; i < frontend::BINKIND_LIMIT; ++i) {
      const BinKind variant = static_cast<BinKind>(i);
      const CharSlice& key = getBinKind(variant);
      auto ptr = binKindMap_.lookupForAdd(key);
      MOZ_ASSERT(!ptr);
      if (!binKindMap_.add(ptr, key, variant)) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
  }

  return true;
}

bool BinaryASTSupport::ensureBinVariantsInitialized(JSContext* cx) {
  MOZ_ASSERT(!cx->helperThread());
  if (binVariantMap_.empty()) {
    for (size_t i = 0; i < frontend::BINVARIANT_LIMIT; ++i) {
      const BinVariant variant = static_cast<BinVariant>(i);
      const CharSlice& key = getBinVariant(variant);
      auto ptr = binVariantMap_.lookupForAdd(key);
      MOZ_ASSERT(!ptr);
      if (!binVariantMap_.add(ptr, key, variant)) {
        ReportOutOfMemory(cx);
        return false;
      }
    }
  }
  return true;
}

JS::Result<const js::frontend::BinKind*> BinaryASTSupport::binKind(
    JSContext* cx, const CharSlice key) {
  MOZ_ASSERT_IF(cx->helperThread(), !binKindMap_.empty());
  if (!cx->helperThread()) {
    // Initialize Lazily if on main thread.
    if (!ensureBinKindsInitialized(cx)) {
      return cx->alreadyReportedError();
    }
  }

  auto ptr = binKindMap_.readonlyThreadsafeLookup(key);
  if (!ptr) {
    return nullptr;
  }

  return &ptr->value();
}

JS::Result<const js::frontend::BinVariant*> BinaryASTSupport::binVariant(
    JSContext* cx, const CharSlice key) {
  MOZ_ASSERT_IF(cx->helperThread(), !binVariantMap_.empty());
  if (!cx->helperThread()) {
    // Initialize lazily if on main thread.
    if (!ensureBinVariantsInitialized(cx)) {
      return cx->alreadyReportedError();
    }
  }

  auto ptr = binVariantMap_.readonlyThreadsafeLookup(key);
  if (!ptr) {
    return nullptr;
  }

  return &ptr->value();
}

}  // namespace js
