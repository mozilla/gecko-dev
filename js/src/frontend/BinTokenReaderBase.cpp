/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinTokenReaderBase.h"

#include "frontend/BinSource-macros.h"
#include "js/Result.h"

namespace js {
namespace frontend {

template <typename T>
using ErrorResult = mozilla::GenericErrorResult<T>;

// We use signalling NaN (which doesn't exist in the JS syntax)
// to represent a `null` number.
const uint64_t NULL_FLOAT_REPRESENTATION = 0x7FF0000000000001;

void BinTokenReaderBase::updateLatestKnownGood() {
  MOZ_ASSERT(current_ >= start_);
  const size_t update = current_ - start_;
  MOZ_ASSERT(update >= latestKnownGoodPos_);
  latestKnownGoodPos_ = update;
}

ErrorResult<JS::Error&> BinTokenReaderBase::raiseError(
    const char* description) {
  MOZ_ASSERT(!hasRaisedError());
  errorReporter_->reportErrorNoOffset(JSMSG_BINAST, description);
  return cx_->alreadyReportedError();
}

ErrorResult<JS::Error&> BinTokenReaderBase::raiseOOM() {
  ReportOutOfMemory(cx_);
  return cx_->alreadyReportedError();
}

ErrorResult<JS::Error&> BinTokenReaderBase::raiseInvalidNumberOfFields(
    const BinKind kind, const uint32_t expected, const uint32_t got) {
  Sprinter out(cx_);
  BINJS_TRY(out.init());
  BINJS_TRY(out.printf("In %s, invalid number of fields: expected %u, got %u",
                       describeBinKind(kind), expected, got));
  return raiseError(out.string());
}

ErrorResult<JS::Error&> BinTokenReaderBase::raiseInvalidField(
    const char* kind, const BinField field) {
  Sprinter out(cx_);
  BINJS_TRY(out.init());
  BINJS_TRY(
      out.printf("In %s, invalid field '%s'", kind, describeBinField(field)));
  return raiseError(out.string());
}

bool BinTokenReaderBase::hasRaisedError() const {
  if (cx_->helperThread()) {
    // When performing off-main-thread parsing, we don't set a pending
    // exception but instead add a pending compile error.
    return cx_->isCompileErrorPending();
  }

  return cx_->isExceptionPending();
}

size_t BinTokenReaderBase::offset() const { return current_ - start_; }

TokenPos BinTokenReaderBase::pos() { return pos(offset()); }

TokenPos BinTokenReaderBase::pos(size_t start) {
  TokenPos pos;
  pos.begin = start;
  pos.end = current_ - start_;
  MOZ_ASSERT(pos.end >= pos.begin);
  return pos;
}

void BinTokenReaderBase::seek(size_t offset) {
  MOZ_ASSERT(start_ + offset >= start_ && start_ + offset < stop_);
  current_ = start_ + offset;
}

JS::Result<Ok> BinTokenReaderBase::readBuf(uint8_t* bytes, uint32_t len) {
  MOZ_ASSERT(!hasRaisedError());
  MOZ_ASSERT(len > 0);

  if (stop_ < current_ + len) {
    return raiseError("Buffer exceeds length");
  }

  for (uint32_t i = 0; i < len; ++i) {
    *bytes++ = *current_++;
  }

  return Ok();
}

JS::Result<uint8_t> BinTokenReaderBase::readByte() {
  uint8_t byte;
  MOZ_TRY(readBuf(&byte, 1));
  return byte;
}

}  // namespace frontend
}  // namespace js
