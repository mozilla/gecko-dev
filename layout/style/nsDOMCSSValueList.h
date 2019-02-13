/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing lists of values in DOM computed style */

#ifndef nsDOMCSSValueList_h___
#define nsDOMCSSValueList_h___

#include "nsIDOMCSSValueList.h"
#include "CSSValue.h"
#include "nsTArray.h"

class nsDOMCSSValueList final : public mozilla::dom::CSSValue,
                                public nsIDOMCSSValueList
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsDOMCSSValueList, mozilla::dom::CSSValue)

  // nsIDOMCSSValue
  NS_DECL_NSIDOMCSSVALUE

  // nsDOMCSSValueList
  nsDOMCSSValueList(bool aCommaDelimited, bool aReadonly);

  /**
   * Adds a value to this list.
   */
  void AppendCSSValue(CSSValue* aValue);

  virtual void GetCssText(nsString& aText, mozilla::ErrorResult& aRv)
    override final;
  virtual void SetCssText(const nsAString& aText,
                          mozilla::ErrorResult& aRv) override final;
  virtual uint16_t CssValueType() const override final;

  CSSValue* IndexedGetter(uint32_t aIdx, bool& aFound) const
  {
    aFound = aIdx <= Length();
    return Item(aIdx);
  }

  CSSValue* Item(uint32_t aIndex) const
  {
    return mCSSValues.SafeElementAt(aIndex);
  }

  uint32_t Length() const
  {
    return mCSSValues.Length();
  }

  nsISupports* GetParentObject()
  {
    return nullptr;
  }

  virtual JSObject *WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

private:
  ~nsDOMCSSValueList();

  bool                        mCommaDelimited;  // some value lists use a comma
                                                // as the delimiter, some just use
                                                // spaces.

  bool                        mReadonly;    // Are we read-only?

  InfallibleTArray<nsRefPtr<CSSValue> > mCSSValues;
};

#endif /* nsDOMCSSValueList_h___ */
