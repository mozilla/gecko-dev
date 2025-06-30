/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/LocationBase.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptContext.h"
#include "nsIClassifiedChannel.h"
#include "nsDocShellLoadState.h"
#include "nsIWebNavigation.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WindowContext.h"

namespace mozilla::dom {

void LocationBase::SetURI(nsIURI* aURI, nsIPrincipal& aSubjectPrincipal,
                          ErrorResult& aRv, bool aReplace) {
  RefPtr<BrowsingContext> bc = GetBrowsingContext();
  if (!bc || bc->IsDiscarded()) {
    return;
  }

  bc->Navigate(aURI, aSubjectPrincipal, aRv,
               aReplace ? NavigationHistoryBehavior::Replace
                        : NavigationHistoryBehavior::Push);
}

void LocationBase::SetHref(const nsACString& aHref,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  DoSetHref(aHref, aSubjectPrincipal, false, aRv);
}

void LocationBase::DoSetHref(const nsACString& aHref,
                             nsIPrincipal& aSubjectPrincipal, bool aReplace,
                             ErrorResult& aRv) {
  // Get the source of the caller
  nsCOMPtr<nsIURI> base = GetSourceBaseURL();
  SetHrefWithBase(aHref, base, aSubjectPrincipal, aReplace, aRv);
}

void LocationBase::SetHrefWithBase(const nsACString& aHref, nsIURI* aBase,
                                   nsIPrincipal& aSubjectPrincipal,
                                   bool aReplace, ErrorResult& aRv) {
  nsresult result;
  nsCOMPtr<nsIURI> newUri;

  if (Document* doc = GetEntryDocument()) {
    result = NS_NewURI(getter_AddRefs(newUri), aHref,
                       doc->GetDocumentCharacterSet(), aBase);
  } else {
    result = NS_NewURI(getter_AddRefs(newUri), aHref, nullptr, aBase);
  }

  if (NS_FAILED(result) || !newUri) {
    aRv.ThrowSyntaxError("'"_ns + aHref + "' is not a valid URL."_ns);
    return;
  }

  /* Check with the scriptContext if it is currently processing a script tag.
   * If so, this must be a <script> tag with a location.href in it.
   * we want to do a replace load, in such a situation.
   * In other cases, for example if a event handler or a JS timer
   * had a location.href in it, we want to do a normal load,
   * so that the new url will be appended to Session History.
   * This solution is tricky. Hopefully it isn't going to bite
   * anywhere else. This is part of solution for bug # 39938, 72197
   */
  bool inScriptTag = false;
  nsIScriptContext* scriptContext = nullptr;
  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(GetEntryGlobal());
  if (win) {
    scriptContext = nsGlobalWindowInner::Cast(win)->GetContextInternal();
  }

  if (scriptContext) {
    if (scriptContext->GetProcessingScriptTag()) {
      // Now check to make sure that the script is running in our window,
      // since we only want to replace if the location is set by a
      // <script> tag in the same window.  See bug 178729.
      nsCOMPtr<nsIDocShell> docShell(GetDocShell());
      nsCOMPtr<nsIScriptGlobalObject> ourGlobal =
          docShell ? docShell->GetScriptGlobalObject() : nullptr;
      inScriptTag = (ourGlobal == scriptContext->GetGlobalObject());
    }
  }

  SetURI(newUri, aSubjectPrincipal, aRv, aReplace || inScriptTag);
}

void LocationBase::Replace(const nsACString& aUrl,
                           nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  DoSetHref(aUrl, aSubjectPrincipal, true, aRv);
}

nsIURI* LocationBase::GetSourceBaseURL() {
  Document* doc = GetEntryDocument();

  // If there's no entry document, we either have no Script Entry Point or one
  // that isn't a DOM Window.  This doesn't generally happen with the DOM, but
  // can sometimes happen with extension code in certain IPC configurations.  If
  // this happens, try falling back on the current document associated with the
  // docshell. If that fails, just return null and hope that the caller passed
  // an absolute URI.
  if (!doc) {
    if (nsCOMPtr<nsIDocShell> docShell = GetDocShell()) {
      nsCOMPtr<nsPIDOMWindowOuter> docShellWin =
          do_QueryInterface(docShell->GetScriptGlobalObject());
      if (docShellWin) {
        doc = docShellWin->GetDoc();
      }
    }
  }
  return doc ? doc->GetBaseURI() : nullptr;
}

}  // namespace mozilla::dom
