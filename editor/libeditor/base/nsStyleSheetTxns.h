/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStylesheetTxns_h__
#define nsStylesheetTxns_h__

#include "EditTxn.h"                    // for EditTxn, NS_DECL_EDITTXN
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "mozilla/CSSStyleSheet.h"      // for mozilla::CSSStyleSheet
#include "nsCycleCollectionParticipant.h"
#include "nsID.h"                       // for REFNSIID
#include "nsISupportsImpl.h"            // for CSSStyleSheet::Release
#include "nscore.h"                     // for NS_IMETHOD

class nsIEditor;

class AddStyleSheetTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aSheet   the stylesheet to add
    */
  NS_IMETHOD Init(nsIEditor* aEditor,
                  mozilla::CSSStyleSheet* aSheet);

  AddStyleSheetTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AddStyleSheetTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

protected:

  nsIEditor* mEditor;                      // the editor that created this transaction
  nsRefPtr<mozilla::CSSStyleSheet> mSheet; // the style sheet to add

};


class RemoveStyleSheetTxn : public EditTxn
{
public:
  /** Initialize the transaction.
    * @param aEditor the object providing core editing operations
    * @param aSheet   the stylesheet to remove
    */
  NS_IMETHOD Init(nsIEditor* aEditor,
                  mozilla::CSSStyleSheet* aSheet);

  RemoveStyleSheetTxn();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(RemoveStyleSheetTxn, EditTxn)
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_DECL_EDITTXN

protected:

  nsIEditor* mEditor;                      // the editor that created this transaction
  nsRefPtr<mozilla::CSSStyleSheet> mSheet; // the style sheet to remove

};


#endif /* nsStylesheetTxns_h__ */
