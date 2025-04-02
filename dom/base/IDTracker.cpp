/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDTracker.h"

#include "mozilla/Encoding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/SVGUseElement.h"
#include "nsAtom.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsIReferrerInfo.h"
#include "nsEscape.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStringFwd.h"

namespace mozilla::dom {

static Element* LookupElement(DocumentOrShadowRoot& aDocOrShadow,
                              const nsAString& aRef, bool aReferenceImage) {
  if (aReferenceImage) {
    return aDocOrShadow.LookupImageElement(aRef);
  }
  return aDocOrShadow.GetElementById(aRef);
}

static DocumentOrShadowRoot* FindTreeToWatch(nsIContent& aContent,
                                             const nsAString& aID,
                                             bool aReferenceImage) {
  ShadowRoot* shadow = aContent.GetContainingShadow();

  // We allow looking outside an <svg:use> shadow tree for backwards compat.
  while (shadow && shadow->Host()->IsSVGElement(nsGkAtoms::use)) {
    // <svg:use> shadow trees are immutable, so we can just early-out if we find
    // our relevant element instead of having to support watching multiple
    // trees.
    if (LookupElement(*shadow, aID, aReferenceImage)) {
      return shadow;
    }
    shadow = shadow->Host()->GetContainingShadow();
  }

  if (shadow) {
    return shadow;
  }

  return aContent.OwnerDoc();
}

IDTracker::IDTracker() = default;

IDTracker::~IDTracker() { Unlink(); }

void IDTracker::ResetToURIWithFragmentID(Element& aFrom, nsIURI* aURI,
                                         nsIReferrerInfo* aReferrerInfo,
                                         bool aReferenceImage) {
  Unlink();

  if (!aURI) {
    return;
  }

  nsAutoCString refPart;
  aURI->GetRef(refPart);
  // Unescape %-escapes in the reference. The result will be in the
  // document charset, hopefully...
  NS_UnescapeURL(refPart);

  // Get the thing to observe changes to.
  Document* doc = aFrom.OwnerDoc();
  auto encoding = doc->GetDocumentCharacterSet();

  nsAutoString ref;
  nsresult rv = encoding->DecodeWithoutBOMHandling(refPart, ref);
  if (NS_FAILED(rv) || ref.IsEmpty()) {
    return;
  }

  if (aFrom.IsInNativeAnonymousSubtree()) {
    // This happens, for example, if aFromContent is part of the content
    // inserted by a call to Document::InsertAnonymousContent, which we
    // also want to handle.  (It also happens for other native anonymous content
    // etc.)
    Element* anonRoot = doc->GetAnonRootIfInAnonymousContentContainer(&aFrom);
    if (anonRoot) {
      mElement = nsContentUtils::MatchElementId(anonRoot, ref);
      // We don't have watching working yet for anonymous content, so bail out
      // here.
      return;
    }
  }

  bool isEqualExceptRef;
  rv = aURI->EqualsExceptRef(doc->GetDocumentURI(), &isEqualExceptRef);
  if (NS_FAILED(rv) || !isEqualExceptRef) {
    return ResetToExternalResource(aURI, aReferrerInfo, ref, aFrom,
                                   aReferenceImage);
  }

  RefPtr<nsAtom> id = NS_Atomize(ref);
  ResetToID(aFrom, id, aReferenceImage);
}

void IDTracker::ResetToExternalResource(nsIURI* aURI,
                                        nsIReferrerInfo* aReferrerInfo,
                                        const nsAString& aRef, Element& aFrom,
                                        bool aReferenceImage) {
  Unlink();

  RefPtr<Document::ExternalResourceLoad> load;
  Document* resourceDoc = aFrom.OwnerDoc()->RequestExternalResource(
      aURI, aReferrerInfo, &aFrom, getter_AddRefs(load));
  if (!resourceDoc) {
    if (!load) {
      // Nothing will ever happen here
      return;
    }
    auto* observer = new DocumentLoadNotification(this, aRef);
    mPendingNotification = observer;
    load->AddObserver(observer);
  }

  mWatchID = NS_Atomize(aRef);
  mReferencingImage = aReferenceImage;
  HaveNewDocumentOrShadowRoot(resourceDoc, /* aWatch = */ true, aRef);
}

static nsIURI* GetExternalResourceURIIfNeeded(nsIURI* aBaseURI,
                                              Element& aFrom) {
  if (!aBaseURI) {
    // We don't know where this URI came from.
    return nullptr;
  }
  SVGUseElement* use = aFrom.GetContainingSVGUseShadowHost();
  if (!use) {
    return nullptr;
  }
  Document* doc = use->GetSourceDocument();
  if (!doc || doc == aFrom.OwnerDoc()) {
    return nullptr;
  }
  nsIURI* originalURI = doc->GetDocumentURI();
  if (!originalURI) {
    return nullptr;
  }
  // Content is in a shadow tree of an external resource.  If this URL was
  // specified in the subtree referenced by the <use> element, then we want the
  // fragment-only URL to resolve to an element from the resource document.
  // Otherwise, the URL was specified somewhere in the document with the <use>
  // element, and we want the fragment-only URL to resolve to an element in that
  // document.
  bool equals = false;
  if (NS_FAILED(aBaseURI->EqualsExceptRef(originalURI, &equals)) || !equals) {
    return nullptr;
  }
  return originalURI;
}

void IDTracker::ResetToLocalFragmentID(Element& aFrom,
                                       const nsAString& aLocalRef,
                                       nsIURI* aBaseURI,
                                       nsIReferrerInfo* aReferrerInfo,
                                       bool aReferenceImage) {
  MOZ_ASSERT(nsContentUtils::IsLocalRefURL(aLocalRef));

  auto ref = Substring(aLocalRef, 1);
  if (ref.IsEmpty()) {
    Unlink();
    return;
  }

  nsAutoCString utf8Ref;
  if (!AppendUTF16toUTF8(ref, utf8Ref, mozilla::fallible)) {
    Unlink();
    return;
  }

  // Only unescape ASCII characters; if we were to unescape arbitrary bytes,
  // we'd potentially end up with invalid UTF-8.
  nsAutoCString unescaped;
  bool appended;
  if (NS_FAILED(NS_UnescapeURL(utf8Ref.BeginReading(), utf8Ref.Length(),
                               esc_OnlyASCII | esc_AlwaysCopy, unescaped,
                               appended, mozilla::fallible))) {
    Unlink();
    return;
  }

  if (nsIURI* resourceUri = GetExternalResourceURIIfNeeded(aBaseURI, aFrom)) {
    NS_ConvertUTF8toUTF16 utf16ref(unescaped);
    return ResetToExternalResource(resourceUri, aReferrerInfo, utf16ref, aFrom,
                                   aReferenceImage);
  }

  RefPtr<nsAtom> idAtom = NS_Atomize(unescaped);
  ResetToID(aFrom, idAtom, aReferenceImage);
}

void IDTracker::ResetToID(Element& aFrom, nsAtom* aID, bool aReferenceImage) {
  MOZ_ASSERT(aID);

  Unlink();

  if (aID->IsEmpty()) {
    return;
  }

  mWatchID = aID;
  mReferencingImage = aReferenceImage;

  nsDependentAtomString str(aID);
  DocumentOrShadowRoot* docOrShadow =
      FindTreeToWatch(aFrom, str, aReferenceImage);
  HaveNewDocumentOrShadowRoot(docOrShadow, /*aWatch*/ true, str);
}

void IDTracker::HaveNewDocumentOrShadowRoot(DocumentOrShadowRoot* aDocOrShadow,
                                            bool aWatch,
                                            const nsAString& aRef) {
  if (aWatch) {
    mWatchDocumentOrShadowRoot = nullptr;
    if (aDocOrShadow) {
      mWatchDocumentOrShadowRoot = &aDocOrShadow->AsNode();
      mElement = aDocOrShadow->AddIDTargetObserver(mWatchID, Observe, this,
                                                   mReferencingImage);
    }
    return;
  }

  if (!aDocOrShadow) {
    return;
  }

  if (Element* e = LookupElement(*aDocOrShadow, aRef, mReferencingImage)) {
    mElement = e;
  }
}

void IDTracker::Traverse(nsCycleCollectionTraversalCallback* aCB) {
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mWatchDocumentOrShadowRoot");
  aCB->NoteXPCOMChild(mWatchDocumentOrShadowRoot);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mElement");
  aCB->NoteXPCOMChild(mElement);
}

void IDTracker::Unlink() {
  if (mWatchID) {
    if (DocumentOrShadowRoot* docOrShadow = GetWatchDocOrShadowRoot()) {
      docOrShadow->RemoveIDTargetObserver(mWatchID, Observe, this,
                                          mReferencingImage);
    }
  }
  if (mPendingNotification) {
    mPendingNotification->Clear();
    mPendingNotification = nullptr;
  }
  mWatchDocumentOrShadowRoot = nullptr;
  mWatchID = nullptr;
  mElement = nullptr;
  mReferencingImage = false;
}

void IDTracker::ElementChanged(Element* aFrom, Element* aTo) { mElement = aTo; }

bool IDTracker::Observe(Element* aOldElement, Element* aNewElement,
                        void* aData) {
  IDTracker* p = static_cast<IDTracker*>(aData);
  if (p->mPendingNotification) {
    p->mPendingNotification->SetTo(aNewElement);
  } else {
    NS_ASSERTION(aOldElement == p->mElement, "Failed to track content!");
    ChangeNotification* watcher =
        new ChangeNotification(p, aOldElement, aNewElement);
    p->mPendingNotification = watcher;
    nsContentUtils::AddScriptRunner(watcher);
  }
  bool keepTracking = p->IsPersistent();
  if (!keepTracking) {
    p->mWatchDocumentOrShadowRoot = nullptr;
    p->mWatchID = nullptr;
  }
  return keepTracking;
}

IDTracker::ChangeNotification::ChangeNotification(IDTracker* aTarget,
                                                  Element* aFrom, Element* aTo)
    : mozilla::Runnable("IDTracker::ChangeNotification"),
      Notification(aTarget),
      mFrom(aFrom),
      mTo(aTo) {}

IDTracker::ChangeNotification::~ChangeNotification() = default;

void IDTracker::ChangeNotification::SetTo(Element* aTo) { mTo = aTo; }

void IDTracker::ChangeNotification::Clear() {
  Notification::Clear();
  mFrom = nullptr;
  mTo = nullptr;
}

NS_IMPL_ISUPPORTS_INHERITED0(IDTracker::ChangeNotification, mozilla::Runnable)
NS_IMPL_ISUPPORTS(IDTracker::DocumentLoadNotification, nsIObserver)

NS_IMETHODIMP
IDTracker::DocumentLoadNotification::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* aData) {
  NS_ASSERTION(!strcmp(aTopic, "external-resource-document-created"),
               "Unexpected topic");
  if (mTarget) {
    nsCOMPtr<Document> doc = do_QueryInterface(aSubject);
    mTarget->mPendingNotification = nullptr;
    NS_ASSERTION(!mTarget->mElement, "Why do we have content here?");
    // Keep watching if IsPersistent().
    mTarget->HaveNewDocumentOrShadowRoot(doc, mTarget->IsPersistent(), mRef);
    mTarget->ElementChanged(nullptr, mTarget->mElement);
  }
  return NS_OK;
}

DocumentOrShadowRoot* IDTracker::GetWatchDocOrShadowRoot() const {
  if (!mWatchDocumentOrShadowRoot) {
    return nullptr;
  }
  MOZ_ASSERT(mWatchDocumentOrShadowRoot->IsDocument() ||
             mWatchDocumentOrShadowRoot->IsShadowRoot());
  if (ShadowRoot* shadow = ShadowRoot::FromNode(*mWatchDocumentOrShadowRoot)) {
    return shadow;
  }
  return mWatchDocumentOrShadowRoot->AsDocument();
}

}  // namespace mozilla::dom
