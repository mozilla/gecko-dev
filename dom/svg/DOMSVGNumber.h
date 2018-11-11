/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOMSVGNUMBER_H__
#define MOZILLA_DOMSVGNUMBER_H__

#include "DOMSVGNumberList.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsWrapperCache.h"

class nsSVGElement;

#define MOZ_SVG_LIST_INDEX_BIT_COUNT 27 // supports > 134 million list items

namespace mozilla {

/**
 * Class DOMSVGNumber
 *
 * This class creates the DOM objects that wrap internal SVGNumber objects that
 * are in an SVGNumberList. It is also used to create the objects returned by
 * SVGSVGElement.createSVGNumber().
 *
 * For the DOM wrapper classes for non-list SVGNumber, see nsSVGNumber2.h.
 *
 * See the architecture comment in DOMSVGAnimatedNumberList.h.
 *
 * See the comment in DOMSVGLength.h (yes, LENGTH), which applies here too.
 */
class DOMSVGNumber final : public nsISupports
                         , public nsWrapperCache
{
  friend class AutoChangeNumberNotifier;

  ~DOMSVGNumber() {
    // Our mList's weak ref to us must be nulled out when we die. If GC has
    // unlinked us using the cycle collector code, then that has already
    // happened, and mList is null.
    if (mList) {
      mList->mItems[mListIndex] = nullptr;
    }
  }

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGNumber)

  /**
   * Generic ctor for DOMSVGNumber objects that are created for an attribute.
   */
  DOMSVGNumber(DOMSVGNumberList *aList,
               uint8_t aAttrEnum,
               uint32_t aListIndex,
               bool aIsAnimValItem);

  /**
   * Ctor for creating the objects returned by SVGSVGElement.createSVGNumber(),
   * which do not initially belong to an attribute.
   */
  explicit DOMSVGNumber(nsISupports* aParent);

  /**
   * Create an unowned copy. The caller is responsible for the first AddRef().
   */
  DOMSVGNumber* Clone() {
    DOMSVGNumber *clone = new DOMSVGNumber(mParent);
    clone->mValue = ToSVGNumber();
    return clone;
  }

  bool IsInList() const {
    return !!mList;
  }

  /**
   * In future, if this class is used for non-list numbers, this will be
   * different to IsInList().
   */
  bool HasOwner() const {
    return !!mList;
  }

  /**
   * This method is called to notify this DOM object that it is being inserted
   * into a list, and give it the information it needs as a result.
   *
   * This object MUST NOT already belong to a list when this method is called.
   * That's not to say that script can't move these DOM objects between
   * lists - it can - it's just that the logic to handle that (and send out
   * the necessary notifications) is located elsewhere (in DOMSVGNumberList).)
   */
  void InsertingIntoList(DOMSVGNumberList *aList,
                         uint8_t aAttrEnum,
                         uint32_t aListIndex,
                         bool aIsAnimValItem);

  static uint32_t MaxListIndex() {
    return (1U << MOZ_SVG_LIST_INDEX_BIT_COUNT) - 1;
  }

  /// This method is called to notify this object that its list index changed.
  void UpdateListIndex(uint32_t aListIndex) {
    mListIndex = aListIndex;
  }

  /**
   * This method is called to notify this DOM object that it is about to be
   * removed from its current DOM list so that it can first make a copy of its
   * internal counterpart's value. (If it didn't do this, then it would
   * "lose" its value on being removed.)
   */
  void RemovingFromList();

  float ToSVGNumber();

  nsISupports* GetParentObject()
  {
    return mParent;
  }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<DOMSVGNumber>
  Constructor(const dom::GlobalObject& aGlobal, ErrorResult& aRv);

  static already_AddRefed<DOMSVGNumber>
  Constructor(const dom::GlobalObject& aGlobal, float aValue, ErrorResult& aRv);

  float Value();

  void SetValue(float aValue, ErrorResult& aRv);

private:

  nsSVGElement* Element() {
    return mList->Element();
  }

  uint8_t AttrEnum() const {
    return mAttrEnum;
  }

  /**
   * Get a reference to the internal SVGNumber list item that this DOM wrapper
   * object currently wraps.
   *
   * To simplify the code we just have this one method for obtaining both
   * baseVal and animVal internal items. This means that animVal items don't
   * get const protection, but then our setter methods guard against changing
   * animVal items.
   */
  float& InternalItem();

#ifdef DEBUG
  bool IndexIsValid();
#endif

  RefPtr<DOMSVGNumberList> mList;
  nsCOMPtr<nsISupports> mParent;

  // Bounds for the following are checked in the ctor, so be sure to update
  // that if you change the capacity of any of the following.

  uint32_t mListIndex:MOZ_SVG_LIST_INDEX_BIT_COUNT;
  uint32_t mAttrEnum:4; // supports up to 16 attributes
  uint32_t mIsAnimValItem:1;

  // The following member is only used when we're not in a list:
  float mValue;
};

} // namespace mozilla

#undef MOZ_SVG_LIST_INDEX_BIT_COUNT

#endif // MOZILLA_DOMSVGNUMBER_H__
