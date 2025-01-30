/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Location_h
#define mozilla_dom_Location_h

#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/LinkedList.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/LocationBase.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsWrapperCache.h"

class nsIDocShell;
class nsIPrincipal;
class nsIURI;
class nsPIDOMWindowInner;

namespace mozilla::dom {

//*****************************************************************************
// Location: Script "location" object
//*****************************************************************************

class Location final : public nsISupports,
                       public LocationBase,
                       public nsWrapperCache,
                       public LinkedListElement<Location> {
 public:
  typedef BrowsingContext::LocationProxy RemoteProxy;

  explicit Location(nsPIDOMWindowInner* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Location)

  // WebIDL API:
  void Assign(const nsACString& aUrl, nsIPrincipal& aSubjectPrincipal,
              ErrorResult& aError);

  void Reload(bool aForceget, nsIPrincipal& aSubjectPrincipal,
              ErrorResult& aError);

  void GetHref(nsACString& aHref, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError) {
    if (!CallerSubsumes(&aSubjectPrincipal)) {
      aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return;
    }

    aError = GetHref(aHref);
  }

  void GetOrigin(nsACString& aOrigin, nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);

  void GetProtocol(nsACString& aProtocol, nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void SetProtocol(const nsACString& aProtocol, nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void GetHost(nsACString& aHost, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void SetHost(const nsACString& aHost, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void GetHostname(nsACString& aHostname, nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void SetHostname(const nsACString& aHostname, nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void GetPort(nsACString& aPort, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void SetPort(const nsACString& aPort, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void GetPathname(nsACString& aPathname, nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void SetPathname(const nsACString& aPathname, nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aError);

  void GetSearch(nsACString& aSeach, nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);

  void SetSearch(const nsACString& aSeach, nsIPrincipal& aSubjectPrincipal,
                 ErrorResult& aError);

  void GetHash(nsACString& aHash, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  void SetHash(const nsACString& aHash, nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aError);

  nsPIDOMWindowInner* GetParentObject() const { return mInnerWindow; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Non WebIDL methods:

  nsresult GetHref(nsACString& aHref);

  void ClearCachedValues();

 protected:
  virtual ~Location();

  BrowsingContext* GetBrowsingContext() override;
  nsIDocShell* GetDocShell() override;

  // In the case of jar: uris, we sometimes want the place the jar was
  // fetched from as the URI instead of the jar: uri itself.  Pass in
  // true for aGetInnermostURI when that's the case.
  // Note, this method can return NS_OK with a null value for aURL. This happens
  // if the docShell is null.
  nsresult GetURI(nsIURI** aURL, bool aGetInnermostURI = false);

  bool CallerSubsumes(nsIPrincipal* aSubjectPrincipal);

  nsCString mCachedHash;
  nsCOMPtr<nsPIDOMWindowInner> mInnerWindow;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Location_h
