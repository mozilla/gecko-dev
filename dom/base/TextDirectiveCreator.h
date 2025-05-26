/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TEXTDIRECTIVECREATOR_H_
#define DOM_TEXTDIRECTIVECREATOR_H_

#include <tuple>
#include "RangeBoundary.h"
#include "mozilla/dom/fragmentdirectives_ffi_generated.h"
#include "TextDirectiveUtil.h"
#include "nsStringFwd.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"

class nsRange;

namespace mozilla {
class ErrorResult;
}

namespace mozilla::dom {
class Document;
/**
 * @brief Helper class to create a text directive string from a given `nsRange`.
 *
 * The class provides a public static creator function which encapsulates all
 * necessary logic. The class itself stores necessary state throughout the
 * steps.
 */
class TextDirectiveCreator final {
 public:
  /**
   * @brief Static creator function. Takes an `nsRange` and creates a text
   *        directive string, if possible.
   *
   * @param aDocument   The document in which `aInputRange` lives.
   * @param aInputRange The input range. This range will not be modified.
   *
   * @return Returns a percent-encoded text directive string on success, an
   *         empty string if it's not possible to create a text fragment for the
   *         given range, or an error code.
   */
  static Result<nsCString, ErrorResult> CreateTextDirectiveFromRange(
      Document& aDocument, nsRange* aInputRange);

  virtual ~TextDirectiveCreator() = default;

 protected:
  TextDirectiveCreator(Document& aDocument, AbstractRange* aRange);


  Document& mDocument;
  RefPtr<AbstractRange> mRange;

  /**
   * The watchdog ensures that the algorithm exits after a defined time
   * duration, to ensure that the main thread is not blocked for too long.
   *
   * The duration is defined by the pref
   * `dom.text_fragments.create_text_fragment.timeout`.
   */
  TimeoutWatchdog mWatchdog;
};
}  // namespace mozilla::dom
#endif
