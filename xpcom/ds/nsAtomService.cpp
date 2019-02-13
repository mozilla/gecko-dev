/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAtomService.h"
#include "nsIAtom.h"

NS_IMPL_ISUPPORTS(nsAtomService, nsIAtomService)

nsAtomService::nsAtomService()
{
}

nsresult
nsAtomService::GetAtom(const nsAString& aString, nsIAtom** aResult)
{
  *aResult = NS_NewAtom(aString).take();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
nsAtomService::GetPermanentAtom(const nsAString& aString, nsIAtom** aResult)
{
  *aResult = NS_NewPermanentAtom(aString);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAtomService::GetAtomUTF8(const char* aValue, nsIAtom** aResult)
{
  *aResult = NS_NewAtom(aValue).take();
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAtomService::GetPermanentAtomUTF8(const char* aValue, nsIAtom** aResult)
{
  *aResult = NS_NewPermanentAtom(NS_ConvertUTF8toUTF16(aValue));
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}
