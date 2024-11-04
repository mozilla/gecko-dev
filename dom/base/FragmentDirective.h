/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FRAGMENTDIRECTIVE_H_
#define DOM_FRAGMENTDIRECTIVE_H_

#include "js/TypeDecls.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/fragmentdirectives_ffi_generated.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStringFwd.h"
#include "nsWrapperCache.h"

class nsINode;
class nsIURI;
class nsRange;
namespace mozilla::dom {
class Document;
class Text;
class TextDirectiveFinder;

/**
 * @brief The `FragmentDirective` class is the C++ representation of the
 * `Document.fragmentDirective` webidl property.
 *
 * This class also serves as the main interface to interact with the fragment
 * directive from the C++ side. It allows to find text fragment ranges from a
 * given list of `TextDirective`s using
 * `FragmentDirective::FindTextFragmentsInDocument()`.
 * To avoid Text Directives being applied multiple times, this class implements
 * the `uninvoked directive` mechanism, which in the spec is defined to be part
 * of the `Document` [0], by encapsuling the code in a lazily constructed
 * helper, which is destroyed when all text directives have been found.
 *
 * [0]
 * https://wicg.github.io/scroll-to-text-fragment/#document-uninvoked-directives
 */
class FragmentDirective final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FragmentDirective)

 public:
  explicit FragmentDirective(Document* aDocument);

 protected:
  ~FragmentDirective();

 public:
  Document* GetParentObject() const { return mDocument; };

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  /**
   * @brief Sets Text Directives as "uninvoked directive".
   */
  void SetTextDirectives(nsTArray<TextDirective>&& aTextDirectives);

  /** Returns true if there are Text Directives that have not been applied to
   * the `Document`.
   */
  bool HasUninvokedDirectives() const;

  /** Clears all uninvoked directives. */
  void ClearUninvokedDirectives();

  /** Inserts all text directive ranges into a `eTargetText` `Selection`. */
  MOZ_CAN_RUN_SCRIPT
  void HighlightTextDirectives(
      const nsTArray<RefPtr<nsRange>>& aTextDirectiveRanges);

  /** Searches for the current uninvoked text directives and creates a range for
   * each one that is found.
   *
   * When this method returns, the uninvoked directives for this document are
   * cleared.
   *
   * This method tries to follow the specification as close as possible in how
   * to find a matching range for a text directive. However, instead of using
   * collator-based search, the Gecko find-in-page algorithm is used (`nsFind`).
   */
  nsTArray<RefPtr<nsRange>> FindTextFragmentsInDocument();

  /** Utility function which parses the fragment directive and removes it from
   * the hash of the given URI. This operation happens in-place.
   *
   * If aTextDirectives is nullptr, the parsed fragment directive is discarded.
   */
  static void ParseAndRemoveFragmentDirectiveFromFragment(
      nsCOMPtr<nsIURI>& aURI,
      nsTArray<TextDirective>* aTextDirectives = nullptr);

  /** Parses the fragment directive and removes it from the hash, given as
   * string. This operation happens in-place.
   *
   * This function is called internally by
   * `ParseAndRemoveFragmentDirectiveFromFragment()`.
   *
   * This function returns true if it modified `aFragment`.
   *
   * Note: the parameter `aURI` is only used for logging purposes.
   */
  static bool ParseAndRemoveFragmentDirectiveFromFragmentString(
      nsCString& aFragment, nsTArray<TextDirective>* aTextDirectives = nullptr,
      nsIURI* aURI = nullptr);

  /** Utility function than returns a string for `aURI` ignoring all fragment
   * directives.
   */
  static nsresult GetSpecIgnoringFragmentDirective(
      nsCOMPtr<nsIURI>& aURI, nsACString& aSpecIgnoringFragmentDirective);

  /** Performs various checks to determine if a text directive is allowed to be
   * scrolled to.
   *
   * This follows the algorithm "check if a text directive can be scrolled" in
   * section 3.5.4 of the text fragment spec
   * (https://wicg.github.io/scroll-to-text-fragment/#restricting-the-text-fragment).
   */
  bool IsTextDirectiveAllowedToBeScrolledTo();

  /** Return an array of all current text directive ranges.
   *
   * This is exposed as a Chrome-Only API.
   */
  void GetTextDirectiveRanges(nsTArray<RefPtr<nsRange>>& aRanges) const;

  /** Removes all text directive ranges.
   *
   * Under the hood this method only calls `Selection::RemoveAllRanges()`.
   * This is exposed as a Chrome-Only API.
   */
  MOZ_CAN_RUN_SCRIPT void RemoveAllTextDirectives(ErrorResult& aRv);

 private:
  RefPtr<Document> mDocument;
  UniquePtr<TextDirectiveFinder> mFinder;
};

}  // namespace mozilla::dom

#endif  // DOM_FRAGMENTDIRECTIVE_H_
