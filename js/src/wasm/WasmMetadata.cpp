/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "wasm/WasmMetadata.h"

#include "mozilla/CheckedInt.h"

using mozilla::CheckedInt;

using namespace js;
using namespace js::wasm;

// CodeMetadata helpers -- adding functions.

bool CodeMetadata::addDefinedFunc(
    /*MOD*/ ModuleMetadata* moduleMeta, ValTypeVector&& params,
    ValTypeVector&& results, bool declareForRef,
    Maybe<CacheableName>&& optionalExportedName) {
  uint32_t typeIndex = types->length();
  FuncType funcType(std::move(params), std::move(results));
  if (!types->addType(std::move(funcType))) {
    return false;
  }

  FuncDesc funcDesc = FuncDesc(&(*types)[typeIndex].funcType(), typeIndex);
  uint32_t funcIndex = funcs.length();
  if (!funcs.append(funcDesc)) {
    return false;
  }
  if (declareForRef) {
    declareFuncExported(funcIndex, true, true);
  }
  if (optionalExportedName.isSome()) {
    if (!moduleMeta->exports.emplaceBack(std::move(optionalExportedName.ref()),
                                         funcIndex, DefinitionKind::Function)) {
      return false;
    }
  }
  return true;
}

bool CodeMetadata::addImportedFunc(/*MOD*/ ModuleMetadata* moduleMeta,
                                   ValTypeVector&& params,
                                   ValTypeVector&& results,
                                   CacheableName&& importModName,
                                   CacheableName&& importFieldName) {
  MOZ_ASSERT(numFuncImports == funcs.length());
  if (!addDefinedFunc(moduleMeta, std::move(params), std::move(results), false,
                      mozilla::Nothing())) {
    return false;
  }
  numFuncImports++;
  return moduleMeta->imports.emplaceBack(std::move(importModName),
                                         std::move(importFieldName),
                                         DefinitionKind::Function);
}

// CodeMetadata helpers -- computing the Instance layout.

// This is the highest offset into Instance::globalArea that will not overflow
// a signed 32-bit integer.
static const uint32_t MaxInstanceDataOffset =
    INT32_MAX - Instance::offsetOfData();

[[nodiscard]] static bool AllocateInstanceDataBytes(
    uint32_t bytes, uint32_t align,
    /*MOD*/ uint32_t* instanceDataLength,
    /*OUT*/ uint32_t* assignedOffset) {
  // Assert that this offset hasn't already been computed.
  MOZ_ASSERT(*assignedOffset == UINT32_MAX);

  CheckedInt<uint32_t> newInstanceDataLength(*instanceDataLength);

  // Adjust the current global data length so that it's aligned to `align`
  newInstanceDataLength +=
      ComputeByteAlignment(newInstanceDataLength.value(), align);
  if (!newInstanceDataLength.isValid()) {
    return false;
  }

  // The allocated data is given by the aligned length
  *assignedOffset = newInstanceDataLength.value();

  // Advance the length for `bytes` being allocated
  newInstanceDataLength += bytes;
  if (!newInstanceDataLength.isValid()) {
    return false;
  }

  // Check that the highest offset into this allocated space would not overflow
  // a signed 32-bit integer.
  if (newInstanceDataLength.value() > MaxInstanceDataOffset + 1) {
    return false;
  }

  *instanceDataLength = newInstanceDataLength.value();
  return true;
}

[[nodiscard]] static bool AllocateInstanceDataBytesN(
    uint32_t bytes, uint32_t align, uint32_t count,
    /*MOD*/ uint32_t* instanceDataLength,
    /*OUT*/ uint32_t* assignedOffset) {
  // The size of each allocation should be a multiple of alignment so that a
  // contiguous array of allocations will be aligned
  MOZ_ASSERT(bytes % align == 0);

  // Compute the total bytes being allocated
  CheckedInt<uint32_t> totalBytes = bytes;
  totalBytes *= count;
  if (!totalBytes.isValid()) {
    return false;
  }

  // Allocate the bytes
  return AllocateInstanceDataBytes(totalBytes.value(), align,
                                   instanceDataLength, assignedOffset);
}

Maybe<uint32_t> CodeMetadata::doInstanceLayout() {
  uint32_t instanceDataLength = 0;

  // Allocate space for type definitions
  if (!AllocateInstanceDataBytesN(sizeof(TypeDefInstanceData),
                                  alignof(TypeDefInstanceData), types->length(),
                                  &instanceDataLength, &typeDefsOffsetStart)) {
    return Nothing();
  }

  // Allocate space for every function import
  if (!AllocateInstanceDataBytesN(
          sizeof(FuncImportInstanceData), alignof(FuncImportInstanceData),
          numFuncImports, &instanceDataLength, &funcImportsOffsetStart)) {
    return Nothing();
  }

  // Allocate space for every memory
  if (!AllocateInstanceDataBytesN(
          sizeof(MemoryInstanceData), alignof(MemoryInstanceData),
          memories.length(), &instanceDataLength, &memoriesOffsetStart)) {
    return Nothing();
  }

  // Allocate space for every table
  if (!AllocateInstanceDataBytesN(sizeof(TableInstanceData),
                                  alignof(TableInstanceData), tables.length(),
                                  &instanceDataLength, &tablesOffsetStart)) {
    return Nothing();
  }

  // Allocate space for every tag
  if (!AllocateInstanceDataBytesN(sizeof(TagInstanceData),
                                  alignof(TagInstanceData), tags.length(),
                                  &instanceDataLength, &tagsOffsetStart)) {
    return Nothing();
  }

  // Allocate space for every global that requires it
  for (GlobalDesc& global : globals) {
    if (global.isConstant()) {
      continue;
    }

    uint32_t width = global.isIndirect() ? sizeof(void*) : global.type().size();

    uint32_t assignedOffset = UINT32_MAX;
    if (!AllocateInstanceDataBytes(width, width, &instanceDataLength,
                                   &assignedOffset)) {
      return Nothing();
    }

    global.setOffset(assignedOffset);
  }

  return Some(instanceDataLength);
}

// CodeMetadata helpers -- memory accounting.

size_t CodeMetadata::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  // FIXME: do other fields need to be considered?  How can we know/check?
  return types->sizeOfExcludingThis(mallocSizeOf) +
         globals.sizeOfExcludingThis(mallocSizeOf) +
         tables.sizeOfExcludingThis(mallocSizeOf) +
         tags.sizeOfExcludingThis(mallocSizeOf) +
         funcNames.sizeOfExcludingThis(mallocSizeOf) +
         filename.sizeOfExcludingThis(mallocSizeOf) +
         sourceMapURL.sizeOfExcludingThis(mallocSizeOf);
}
