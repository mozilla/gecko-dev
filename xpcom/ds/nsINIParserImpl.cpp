/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsINIParserImpl.h"

#include "nsINIParser.h"
#include "nsStringEnumerator.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

class nsINIParserImpl final
  : public nsIINIParser
{
  ~nsINIParserImpl() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINIPARSER

  nsresult Init(nsIFile* aINIFile) { return mParser.Init(aINIFile); }

private:
  nsINIParser mParser;
};

NS_IMPL_ISUPPORTS(nsINIParserFactory,
                  nsIINIParserFactory,
                  nsIFactory)

NS_IMETHODIMP
nsINIParserFactory::CreateINIParser(nsIFile* aINIFile,
                                    nsIINIParser** aResult)
{
  *aResult = nullptr;

  RefPtr<nsINIParserImpl> p(new nsINIParserImpl());
  if (!p) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = p->Init(aINIFile);

  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(*aResult = p);
  }

  return rv;
}

NS_IMETHODIMP
nsINIParserFactory::CreateInstance(nsISupports* aOuter,
                                   REFNSIID aIID,
                                   void** aResult)
{
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }

  // We are our own singleton.
  return QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsINIParserFactory::LockFactory(bool aLock)
{
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsINIParserImpl,
                  nsIINIParser)

static bool
SectionCB(const char* aSection, void* aClosure)
{
  nsTArray<nsCString>* strings = static_cast<nsTArray<nsCString>*>(aClosure);
  strings->AppendElement()->Assign(aSection);
  return true;
}

NS_IMETHODIMP
nsINIParserImpl::GetSections(nsIUTF8StringEnumerator** aResult)
{
  nsTArray<nsCString>* strings = new nsTArray<nsCString>;
  if (!strings) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = mParser.GetSections(SectionCB, strings);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewAdoptingUTF8StringEnumerator(aResult, strings);
  }

  if (NS_FAILED(rv)) {
    delete strings;
  }

  return rv;
}

static bool
KeyCB(const char* aKey, const char* aValue, void* aClosure)
{
  nsTArray<nsCString>* strings = static_cast<nsTArray<nsCString>*>(aClosure);
  strings->AppendElement()->Assign(aKey);
  return true;
}

NS_IMETHODIMP
nsINIParserImpl::GetKeys(const nsACString& aSection,
                         nsIUTF8StringEnumerator** aResult)
{
  nsTArray<nsCString>* strings = new nsTArray<nsCString>;
  if (!strings) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = mParser.GetStrings(PromiseFlatCString(aSection).get(),
                                   KeyCB, strings);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewAdoptingUTF8StringEnumerator(aResult, strings);
  }

  if (NS_FAILED(rv)) {
    delete strings;
  }

  return rv;

}

NS_IMETHODIMP
nsINIParserImpl::GetString(const nsACString& aSection,
                           const nsACString& aKey,
                           nsACString& aResult)
{
  return mParser.GetString(PromiseFlatCString(aSection).get(),
                           PromiseFlatCString(aKey).get(),
                           aResult);
}
