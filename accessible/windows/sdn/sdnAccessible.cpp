/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdnAccessible-inl.h"
#include "ISimpleDOM_i.c"

#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;

sdnAccessible::~sdnAccessible() = default;

STDMETHODIMP
sdnAccessible::QueryInterface(REFIID aREFIID, void** aInstancePtr) {
  if (!aInstancePtr) return E_FAIL;
  *aInstancePtr = nullptr;

  if (aREFIID == IID_IClientSecurity) {
    // Some code might QI(IID_IClientSecurity) to detect whether or not we are
    // a proxy. Right now that can potentially happen off the main thread, so we
    // look for this condition immediately so that we don't trigger other code
    // that might not be thread-safe.
    return E_NOINTERFACE;
  }

  if (aREFIID == IID_ISimpleDOMNode) {
    *aInstancePtr = static_cast<ISimpleDOMNode*>(this);
    AddRef();
    return S_OK;
  }

  MsaaAccessible* accessible = GetMsaa();
  if (accessible) return accessible->QueryInterface(aREFIID, aInstancePtr);

  // IUnknown* is the canonical one if and only if this accessible doesn't have
  // an accessible.
  if (aREFIID == IID_IUnknown) {
    *aInstancePtr = static_cast<ISimpleDOMNode*>(this);
    AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

STDMETHODIMP
sdnAccessible::get_nodeInfo(BSTR __RPC_FAR* aNodeName,
                            short __RPC_FAR* aNameSpaceID,
                            BSTR __RPC_FAR* aNodeValue,
                            unsigned int __RPC_FAR* aNumChildren,
                            unsigned int __RPC_FAR* aUniqueID,
                            unsigned short __RPC_FAR* aNodeType) {
  if (!aNodeName || !aNameSpaceID || !aNodeValue || !aNumChildren ||
      !aUniqueID || !aNodeType)
    return E_INVALIDARG;

  *aNodeName = nullptr;
  *aNameSpaceID = 0;
  *aNodeValue = nullptr;
  *aNumChildren = 0;
  *aUniqueID = 0;
  *aNodeType = 0;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  // This is a unique ID for every content node. The 3rd party accessibility
  // application can compare this to the childID we return for events such as
  // focus events, to correlate back to data nodes in their internal object
  // model.
  Accessible* acc = GetMsaa()->Acc();
  *aUniqueID = MsaaAccessible::GetChildIDFor(acc);
  if (acc->IsText()) {
    *aNodeType = nsINode::TEXT_NODE;
  } else if (acc->IsDoc()) {
    *aNodeType = nsINode::DOCUMENT_NODE;
  } else {
    *aNodeType = nsINode::ELEMENT_NODE;
  }
  if (nsAtom* tag = acc->TagName()) {
    nsAutoString nodeName;
    tag->ToString(nodeName);
    *aNodeName = ::SysAllocString(nodeName.get());
  }
  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_attributes(unsigned short aMaxAttribs,
                              BSTR __RPC_FAR* aAttribNames,
                              short __RPC_FAR* aNameSpaceIDs,
                              BSTR __RPC_FAR* aAttribValues,
                              unsigned short __RPC_FAR* aNumAttribs) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_attributesForNames(unsigned short aMaxAttribs,
                                      BSTR __RPC_FAR* aAttribNames,
                                      short __RPC_FAR* aNameSpaceID,
                                      BSTR __RPC_FAR* aAttribValues) {
  if (!aAttribNames || !aNameSpaceID || !aAttribValues) return E_INVALIDARG;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;
  // NVDA expects this to succeed for MathML and won't call innerHTML if this
  // fails. Therefore, return S_FALSE here instead of E_NOTIMPL, indicating
  // that the attributes aren't present.
  return S_FALSE;
}

STDMETHODIMP
sdnAccessible::get_computedStyle(
    unsigned short aMaxStyleProperties, boolean aUseAlternateView,
    BSTR __RPC_FAR* aStyleProperties, BSTR __RPC_FAR* aStyleValues,
    unsigned short __RPC_FAR* aNumStyleProperties) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_computedStyleForProperties(
    unsigned short aNumStyleProperties, boolean aUseAlternateView,
    BSTR __RPC_FAR* aStyleProperties, BSTR __RPC_FAR* aStyleValues) {
  return E_NOTIMPL;
}

// XXX Use MOZ_CAN_RUN_SCRIPT_BOUNDARY for now due to bug 1543294.
MOZ_CAN_RUN_SCRIPT_BOUNDARY STDMETHODIMP
sdnAccessible::scrollTo(boolean aScrollTopLeft) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_parentNode(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_firstChild(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_lastChild(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_previousSibling(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_nextSibling(ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_childAt(unsigned aChildIndex,
                           ISimpleDOMNode __RPC_FAR* __RPC_FAR* aNode) {
  return E_NOTIMPL;
}

STDMETHODIMP
sdnAccessible::get_innerHTML(BSTR __RPC_FAR* aInnerHTML) {
  if (!aInnerHTML) return E_INVALIDARG;
  *aInnerHTML = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsAutoString innerHTML;
  if (!mNode) {
    RemoteAccessible* remoteAcc = mMsaa->Acc()->AsRemote();
    MOZ_ASSERT(remoteAcc);
    if (RequestDomainsIfInactive(CacheDomain::InnerHTML)) {
      return S_FALSE;
    }
    if (!remoteAcc->mCachedFields) {
      return S_FALSE;
    }
    remoteAcc->mCachedFields->GetAttribute(CacheKey::InnerHTML, innerHTML);
  } else {
    if (!mNode->IsElement()) {
      return S_FALSE;
    }
    mNode->AsElement()->GetInnerHTML(innerHTML, IgnoreErrors());
  }

  if (innerHTML.IsEmpty()) return S_FALSE;

  *aInnerHTML = ::SysAllocStringLen(innerHTML.get(), innerHTML.Length());
  if (!*aInnerHTML) return E_OUTOFMEMORY;

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_localInterface(void __RPC_FAR* __RPC_FAR* aLocalInterface) {
  if (!aLocalInterface) return E_INVALIDARG;
  *aLocalInterface = nullptr;

  if (IsDefunct()) return CO_E_OBJNOTCONNECTED;

  *aLocalInterface = this;
  AddRef();

  return S_OK;
}

STDMETHODIMP
sdnAccessible::get_language(BSTR __RPC_FAR* aLanguage) { return E_NOTIMPL; }
