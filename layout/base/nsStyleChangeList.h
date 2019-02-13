/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * a list of the recomputation that needs to be done in response to a
 * style change
 */

#ifndef nsStyleChangeList_h___
#define nsStyleChangeList_h___

#include "mozilla/Attributes.h"

#include "nsError.h"
#include "nsChangeHint.h"

class nsIFrame;
class nsIContent;

// XXX would all platforms support putting this inside the list?
struct nsStyleChangeData {
  nsIFrame*   mFrame;
  nsIContent* mContent;
  nsChangeHint mHint;
};

static const uint32_t kStyleChangeBufferSize = 10;

// Note:  nsStyleChangeList owns a reference to
//  nsIContent pointers in its list.
class nsStyleChangeList {
public:
  nsStyleChangeList();
  ~nsStyleChangeList();

  int32_t Count(void) const {
    return mCount;
  }

  /**
   * Fills in pointers without reference counting.  
   */
  nsresult ChangeAt(int32_t aIndex, nsIFrame*& aFrame, nsIContent*& aContent,
                    nsChangeHint& aHint) const;

  /**
   * Fills in a pointer to the list entry storage (no reference counting
   * involved).
   */
  nsresult ChangeAt(int32_t aIndex, const nsStyleChangeData** aChangeData) const;

  nsresult AppendChange(nsIFrame* aFrame, nsIContent* aContent, nsChangeHint aHint);

  void Clear(void);

protected:
  nsStyleChangeList&  operator=(const nsStyleChangeList& aCopy);
  bool                operator==(const nsStyleChangeList& aOther) const;

  nsStyleChangeData*  mArray;
  int32_t             mArraySize;
  int32_t             mCount;
  nsStyleChangeData   mBuffer[kStyleChangeBufferSize];

private:
  nsStyleChangeList(const nsStyleChangeList&) = delete;
};


#endif /* nsStyleChangeList_h___ */
