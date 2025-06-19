/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_URLPattern_h
#define mozilla_dom_URLPattern_h

#include "mozilla/dom/URLPatternBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/net/URLPatternGlue.h"

namespace mozilla::dom {

class URLPattern final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(URLPattern)

  explicit URLPattern(nsISupports* aParent, UrlpPattern aPattern,
                      bool aIgnoreCase)
      : mParent(aParent),
        mPattern(std::move(aPattern)),
        mIgnoreCase(aIgnoreCase) {}

  nsISupports* GetParentObject() const { return mParent; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  bool Test(const UTF8StringOrURLPatternInit& aInput,
            const Optional<nsACString>& aBase, ErrorResult& rv);

  void Exec(const UTF8StringOrURLPatternInit& aInput,
            const Optional<nsACString>& aBaseUrl,
            Nullable<URLPatternResult>& aResult, ErrorResult& rv);

  static already_AddRefed<URLPattern> Constructor(
      const GlobalObject& aGlobal, const UTF8StringOrURLPatternInit& aInput,
      const URLPatternOptions& aOptions, ErrorResult& rv);

  static already_AddRefed<URLPattern> Constructor(
      const GlobalObject& aGlobal, const UTF8StringOrURLPatternInit& aInput,
      const nsACString& aBase, const URLPatternOptions& aOptions,
      ErrorResult& rv);

  void GetProtocol(nsACString& aProtocol) const;
  void GetUsername(nsACString& aUsername) const;
  void GetPassword(nsACString& aPassword) const;
  void GetHostname(nsACString& aHostname) const;
  void GetPort(nsACString& aPort) const;
  void GetPathname(nsACString& aPathname) const;
  void GetSearch(nsACString& aSearch) const;
  void GetHash(nsACString& aHash) const;
  bool HasRegExpGroups() const;

 private:
  ~URLPattern();
  nsCOMPtr<nsISupports> mParent;
  UrlpPattern mPattern;
  bool mIgnoreCase;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_URLPattern_h
