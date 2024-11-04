/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TEXTDIRECTIVEFINDER_H_
#define DOM_TEXTDIRECTIVEFINDER_H_
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

class nsRange;
struct TextDirective;
namespace mozilla::dom {

class Document;

/**
 * @brief Finds one or more `TextDirective`s in a `Document`.
 *
 * This class is designed to consume the `TextDirective`s.
 * Every `TextDirective` which is found is removed from the list of uninvoked
 * text directives, and is returned as an `nsRange`.
 *
 * Internally, finding a text directive in a document uses Gecko's find-in-page
 * implementation `nsFind`.
 */
class TextDirectiveFinder final {
 public:
  TextDirectiveFinder(Document& aDocument,
                      nsTArray<TextDirective>&& aTextDirectives);

  /**
   * @brief Attempts to convert all uninvoked text directives to ranges.
   *
   * This method is the main entry point of this class.
   */
  nsTArray<RefPtr<nsRange>> FindTextDirectivesInDocument();

  /**
   * Returns true if there are text directives left which were not yet found in
   * the document.
   */
  bool HasUninvokedDirectives() const;

  /**
   * Finds a range for _one_ text directive.
   */
  RefPtr<nsRange> FindRangeForTextDirective(
      const TextDirective& aTextDirective);

 private:
  Document& mDocument;
  nsTArray<TextDirective> mUninvokedTextDirectives;
};
}  // namespace mozilla::dom

#endif
