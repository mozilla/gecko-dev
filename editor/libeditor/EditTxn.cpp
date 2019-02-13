/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EditTxn.h"
#include "nsError.h"
#include "nsISupportsBase.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(EditTxn)

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(EditTxn)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EditTxn)
  // We don't have anything to traverse, but some of our subclasses do.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EditTxn)
  NS_INTERFACE_MAP_ENTRY(nsITransaction)
  NS_INTERFACE_MAP_ENTRY(nsPIEditorTransaction)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITransaction)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(EditTxn)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(EditTxn, LastRelease())

EditTxn::~EditTxn()
{
}

NS_IMETHODIMP
EditTxn::RedoTransaction(void)
{
  return DoTransaction();
}

NS_IMETHODIMP
EditTxn::GetIsTransient(bool *aIsTransient)
{
  *aIsTransient = false;

  return NS_OK;
}

NS_IMETHODIMP
EditTxn::Merge(nsITransaction *aTransaction, bool *aDidMerge)
{
  *aDidMerge = false;

  return NS_OK;
}
