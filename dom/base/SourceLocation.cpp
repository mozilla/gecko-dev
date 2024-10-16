/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SourceLocation.h"
#include "mozilla/ThreadLocal.h"
#include "nsContentUtils.h"
#include "jsapi.h"

namespace mozilla {

SourceLocation::SourceLocation() = default;
SourceLocation::~SourceLocation() = default;

SourceLocation::SourceLocation(nsCString&& aResource, uint32_t aLine,
                               uint32_t aCol)
    : mResource(std::move(aResource)), mLine(aLine), mColumn(aCol) {}

SourceLocation::SourceLocation(nsCOMPtr<nsIURI>&& aResource, uint32_t aLine,
                               uint32_t aCol)
    : mResource(std::move(aResource)), mLine(aLine), mColumn(aCol) {}

static MOZ_THREAD_LOCAL(const JSCallingLocation*) tlsFallback;
const JSCallingLocation* JSCallingLocation::GetFallback() {
  if (!tlsFallback.initialized()) {
    return nullptr;
  }
  return tlsFallback.get();
}

void JSCallingLocation::SetFallback(const JSCallingLocation* aFallback) {
  if (!tlsFallback.init()) {
    return;
  }
  tlsFallback.set(aFallback);
}

JSCallingLocation JSCallingLocation::Get() {
  return Get(nsContentUtils::GetCurrentJSContext());
}

JSCallingLocation JSCallingLocation::Get(JSContext* aCx) {
  JSCallingLocation result;
  if (const JSCallingLocation* loc = GetFallback()) {
    result = *loc;
  }
  if (!aCx) {
    return result;
  }
  JS::AutoFilename filename;
  uint32_t line;
  JS::ColumnNumberOneOrigin column;
  if (!JS::DescribeScriptedCaller(&filename, aCx, &line, &column)) {
    return result;
  }
  nsCString file;
  if (!file.Assign(filename.get(), fallible)) {
    return result;
  }
  result.mResource = AsVariant(std::move(file));
  result.mLine = line;
  result.mColumn = column.oneOriginValue();
  return result;
}

}  // namespace mozilla
