/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMDocumentType node.
 */

#ifndef DocumentType_h
#define DocumentType_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIDOMDocumentType.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

// XXX DocumentType is currently implemented by inheriting the generic
// CharacterData object, even though DocumentType is not character
// data. This is done simply for convenience and should be changed if
// this restricts what should be done for character data.

class DocumentTypeForward : public nsGenericDOMDataNode,
                            public nsIDOMDocumentType
{
public:
  explicit DocumentTypeForward(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
    : nsGenericDOMDataNode(aNodeInfo)
  {
  }

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE
};

class DocumentType final : public DocumentTypeForward
{
public:
  DocumentType(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
               const nsAString& aPublicId,
               const nsAString& aSystemId,
               const nsAString& aInternalSubset);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  // Forwarded by base class

  // nsIDOMDocumentType
  NS_DECL_NSIDOMDOCUMENTTYPE

  // nsINode
  virtual bool IsNodeOfType(uint32_t aFlags) const override;
  virtual void GetNodeValueInternal(nsAString& aNodeValue) override
  {
    SetDOMStringToNull(aNodeValue);
  }
  virtual void SetNodeValueInternal(const nsAString& aNodeValue,
                                    mozilla::ErrorResult& aError) override
  {
  }

  // nsIContent overrides
  virtual const nsTextFragment* GetText() override;

  virtual nsGenericDOMDataNode* CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo,
                                              bool aCloneText) const override;

  virtual nsIDOMNode* AsDOMNode() override { return this; }

protected:
  virtual ~DocumentType();

  virtual JSObject* WrapNode(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  nsString mPublicId;
  nsString mSystemId;
  nsString mInternalSubset;
};

} // namespace dom
} // namespace mozilla

already_AddRefed<mozilla::dom::DocumentType>
NS_NewDOMDocumentType(nsNodeInfoManager* aNodeInfoManager,
                      nsIAtom *aName,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset,
                      mozilla::ErrorResult& rv);

nsresult
NS_NewDOMDocumentType(nsIDOMDocumentType** aDocType,
                      nsNodeInfoManager* aNodeInfoManager,
                      nsIAtom *aName,
                      const nsAString& aPublicId,
                      const nsAString& aSystemId,
                      const nsAString& aInternalSubset);

#endif // DocumentType_h
