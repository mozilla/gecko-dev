/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/SourceLocation.h"
#include "nsContentUtils.h"
#include "jsapi.h"

namespace mozilla {

SourceLocation::SourceLocation() = default;
SourceLocation::~SourceLocation() = default;

SourceLocation::SourceLocation(nsCString&& aResource, uint32_t aLine,
                               uint32_t aCol, nsCString&& aSourceLine)
    : mResource(std::move(aResource)),
      mLine(aLine),
      mColumn(aCol),
      mSourceLine(std::move(aSourceLine)) {}

SourceLocation::SourceLocation(nsCOMPtr<nsIURI>&& aResource, uint32_t aLine,
                               uint32_t aCol, nsCString&& aSourceLine)
    : mResource(std::move(aResource)),
      mLine(aLine),
      mColumn(aCol),
      mSourceLine(std::move(aSourceLine)) {}

JSCallingLocation JSCallingLocation::Get() {
  return Get(nsContentUtils::GetCurrentJSContext());
}

JSCallingLocation JSCallingLocation::Get(JSContext* aCx) {
  JSCallingLocation result;
  if (!aCx) {
    return result;
  }
  JS::AutoFilename filename;
  uint32_t line;
  JS::ColumnNumberOneOrigin column;
  if (!JS::DescribeScriptedCaller(aCx, &filename, &line, &column)) {
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
