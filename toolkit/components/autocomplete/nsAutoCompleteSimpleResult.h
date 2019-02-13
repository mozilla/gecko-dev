/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAutoCompleteSimpleResult__
#define __nsAutoCompleteSimpleResult__

#include "nsIAutoCompleteResult.h"
#include "nsIAutoCompleteSimpleResult.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

class nsAutoCompleteSimpleResult final : public nsIAutoCompleteSimpleResult
{
public:
  nsAutoCompleteSimpleResult();
  inline void CheckInvariants() {
    NS_ASSERTION(mValues.Length() == mComments.Length(), "Arrays out of sync");
    NS_ASSERTION(mValues.Length() == mImages.Length(),   "Arrays out of sync");
    NS_ASSERTION(mValues.Length() == mStyles.Length(),   "Arrays out of sync");
    NS_ASSERTION(mValues.Length() == mFinalCompleteValues.Length(), "Arrays out of sync");
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUTOCOMPLETERESULT
  NS_DECL_NSIAUTOCOMPLETESIMPLERESULT

private:
  ~nsAutoCompleteSimpleResult() {}

protected:

  // What we really want is an array of structs with value/comment/image/style contents.
  // But then we'd either have to use COM or manage object lifetimes ourselves.
  // Having four arrays of string simplifies this, but is stupid.
  nsTArray<nsString> mValues;
  nsTArray<nsString> mComments;
  nsTArray<nsString> mImages;
  nsTArray<nsString> mStyles;
  nsTArray<nsString> mFinalCompleteValues;

  nsString mSearchString;
  nsString mErrorDescription;
  int32_t mDefaultIndex;
  uint32_t mSearchResult;

  bool mTypeAheadResult;

  nsCOMPtr<nsIAutoCompleteSimpleResultListener> mListener;
};

#endif // __nsAutoCompleteSimpleResult__
