/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentOrShadowRoot_h__
#define mozilla_dom_DocumentOrShadowRoot_h__

#include "mozilla/dom/NameSpaceConstants.h"
#include "nsClassHashtable.h"
#include "nsContentListDeclarations.h"
#include "nsTArray.h"
#include "nsIdentifierMapEntry.h"

class nsContentList;
class nsCycleCollectionTraversalCallback;
class nsIDocument;
class nsINode;
class nsIRadioVisitor;
class nsWindowSizes;

namespace mozilla {
class StyleSheet;

namespace dom {

class Element;
class DocumentOrShadowRoot;
class HTMLInputElement;
struct nsRadioGroupStruct;
class StyleSheetList;
class ShadowRoot;

/**
 * A class meant to be shared by ShadowRoot and Document, that holds a list of
 * stylesheets.
 *
 * TODO(emilio, bug 1418159): In the future this should hold most of the
 * relevant style state, this should allow us to fix bug 548397.
 */
class DocumentOrShadowRoot {
  enum class Kind {
    Document,
    ShadowRoot,
  };

 public:
  explicit DocumentOrShadowRoot(nsIDocument&);
  explicit DocumentOrShadowRoot(mozilla::dom::ShadowRoot&);

  // Unusual argument naming is because of cycle collection macros.
  static void Traverse(DocumentOrShadowRoot* tmp,
                       nsCycleCollectionTraversalCallback& cb);
  static void Unlink(DocumentOrShadowRoot* tmp);

  nsINode& AsNode() { return mAsNode; }

  const nsINode& AsNode() const { return mAsNode; }

  StyleSheet* SheetAt(size_t aIndex) const {
    return mStyleSheets.SafeElementAt(aIndex);
  }

  size_t SheetCount() const { return mStyleSheets.Length(); }

  int32_t IndexOfSheet(const StyleSheet& aSheet) const {
    return mStyleSheets.IndexOf(&aSheet);
  }

  StyleSheetList& EnsureDOMStyleSheets();

  Element* GetElementById(const nsAString& aElementId);

  /**
   * This method returns _all_ the elements in this scope which have id
   * aElementId, if there are any.  Otherwise it returns null.
   *
   * This is useful for stuff like QuerySelector optimization and such.
   */
  inline const nsTArray<Element*>* GetAllElementsForId(
      const nsAString& aElementId) const;

  already_AddRefed<nsContentList> GetElementsByTagName(
      const nsAString& aTagName) {
    return NS_GetContentList(&AsNode(), kNameSpaceID_Unknown, aTagName);
  }

  already_AddRefed<nsContentList> GetElementsByTagNameNS(
      const nsAString& aNamespaceURI, const nsAString& aLocalName);

  already_AddRefed<nsContentList> GetElementsByTagNameNS(
      const nsAString& aNamespaceURI, const nsAString& aLocalName,
      mozilla::ErrorResult&);

  already_AddRefed<nsContentList> GetElementsByClassName(
      const nsAString& aClasses);

  ~DocumentOrShadowRoot();

  Element* GetPointerLockElement();
  Element* GetFullscreenElement();

  Element* ElementFromPoint(float aX, float aY);
  void ElementsFromPoint(float aX, float aY,
                         nsTArray<RefPtr<mozilla::dom::Element>>& aElements);

  /**
   * Helper for elementFromPoint implementation that allows
   * ignoring the scroll frame and/or avoiding layout flushes.
   *
   * @see nsIDOMWindowUtils::elementFromPoint
   */
  Element* ElementFromPointHelper(float aX, float aY,
                                  bool aIgnoreRootScrollFrame,
                                  bool aFlushLayout);
  enum ElementsFromPointFlags {
    IGNORE_ROOT_SCROLL_FRAME = 1,
    FLUSH_LAYOUT = 2,
    IS_ELEMENT_FROM_POINT = 4
  };

  void ElementsFromPointHelper(
      float aX, float aY, uint32_t aFlags,
      nsTArray<RefPtr<mozilla::dom::Element>>& aElements);

  /**
   * This gets fired when the element that an id refers to changes.
   * This fires at difficult times. It is generally not safe to do anything
   * which could modify the DOM in any way. Use
   * nsContentUtils::AddScriptRunner.
   * @return true to keep the callback in the callback set, false
   * to remove it.
   */
  typedef bool (*IDTargetObserver)(Element* aOldElement, Element* aNewelement,
                                   void* aData);

  /**
   * Add an IDTargetObserver for a specific ID. The IDTargetObserver
   * will be fired whenever the content associated with the ID changes
   * in the future. If aForImage is true, mozSetImageElement can override
   * what content is associated with the ID. In that case the IDTargetObserver
   * will be notified at those times when the result of LookupImageElement
   * changes.
   * At most one (aObserver, aData, aForImage) triple can be
   * registered for each ID.
   * @return the content currently associated with the ID.
   */
  Element* AddIDTargetObserver(nsAtom* aID, IDTargetObserver aObserver,
                               void* aData, bool aForImage);

  /**
   * Remove the (aObserver, aData, aForImage) triple for a specific ID, if
   * registered.
   */
  void RemoveIDTargetObserver(nsAtom* aID, IDTargetObserver aObserver,
                              void* aData, bool aForImage);

  /**
   * Lookup an image element using its associated ID, which is usually provided
   * by |-moz-element()|. Similar to GetElementById, with the difference that
   * elements set using mozSetImageElement have higher priority.
   * @param aId the ID associated the element we want to lookup
   * @return the element associated with |aId|
   */
  Element* LookupImageElement(const nsAString& aElementId);

  /**
   * Check that aId is not empty and log a message to the console
   * service if it is.
   * @returns true if aId looks correct, false otherwise.
   */
  inline bool CheckGetElementByIdArg(const nsAString& aId) {
    if (aId.IsEmpty()) {
      ReportEmptyGetElementByIdArg();
      return false;
    }
    return true;
  }

  void ReportEmptyGetElementByIdArg();

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor,
                            bool aFlushContent);
  void SetCurrentRadioButton(const nsAString& aName, HTMLInputElement* aRadio);
  HTMLInputElement* GetCurrentRadioButton(const nsAString& aName);
  nsresult GetNextRadioButton(const nsAString& aName, const bool aPrevious,
                              HTMLInputElement* aFocusedRadio,
                              HTMLInputElement** aRadioOut);
  void AddToRadioGroup(const nsAString& aName, HTMLInputElement* aRadio);
  void RemoveFromRadioGroup(const nsAString& aName, HTMLInputElement* aRadio);
  uint32_t GetRequiredRadioCount(const nsAString& aName) const;
  void RadioRequiredWillChange(const nsAString& aName, bool aRequiredAdded);
  bool GetValueMissingState(const nsAString& aName) const;
  void SetValueMissingState(const nsAString& aName, bool aValue);

  // for radio group
  nsRadioGroupStruct* GetRadioGroup(const nsAString& aName) const;
  nsRadioGroupStruct* GetOrCreateRadioGroup(const nsAString& aName);

 protected:
  // Returns the reference to the sheet, if found in mStyleSheets.
  already_AddRefed<StyleSheet> RemoveSheet(StyleSheet& aSheet);
  void InsertSheetAt(size_t aIndex, StyleSheet& aSheet);

  void AddSizeOfExcludingThis(nsWindowSizes&) const;
  void AddSizeOfOwnedSheetArrayExcludingThis(
      nsWindowSizes&, const nsTArray<RefPtr<StyleSheet>>&) const;

  nsIContent* Retarget(nsIContent* aContent) const;

  /**
   * If focused element's subtree root is this document or shadow root, return
   * focused element, otherwise, get the shadow host recursively until the
   * shadow host's subtree root is this document or shadow root.
   */
  Element* GetRetargetedFocusedElement();

  nsTArray<RefPtr<mozilla::StyleSheet>> mStyleSheets;
  RefPtr<mozilla::dom::StyleSheetList> mDOMStyleSheets;

  /*
   * mIdentifierMap works as follows for IDs:
   * 1) Attribute changes affect the table immediately (removing and adding
   *    entries as needed).
   * 2) Removals from the DOM affect the table immediately
   * 3) Additions to the DOM always update existing entries for names, and add
   *    new ones for IDs.
   */
  nsTHashtable<nsIdentifierMapEntry> mIdentifierMap;

  nsClassHashtable<nsStringHashKey, nsRadioGroupStruct> mRadioGroups;

  nsINode& mAsNode;
  const Kind mKind;
};

inline const nsTArray<Element*>* DocumentOrShadowRoot::GetAllElementsForId(
    const nsAString& aElementId) const {
  if (aElementId.IsEmpty()) {
    return nullptr;
  }

  nsIdentifierMapEntry* entry = mIdentifierMap.GetEntry(aElementId);
  return entry ? &entry->GetIdElements() : nullptr;
}

}  // namespace dom

}  // namespace mozilla

#endif
