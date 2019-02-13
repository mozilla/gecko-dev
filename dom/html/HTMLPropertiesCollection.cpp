/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLPropertiesCollection.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"
#include "nsVariant.h"
#include "nsDOMSettableTokenList.h"
#include "nsAttrValue.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/HTMLPropertiesCollectionBinding.h"
#include "jsapi.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLPropertiesCollection)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HTMLPropertiesCollection)
  // SetDocument(nullptr) ensures that we remove ourselves as a mutation observer
  tmp->SetDocument(nullptr);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNames)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNamedItemEntries)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mProperties)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HTMLPropertiesCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNames)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNamedItemEntries);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProperties)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(HTMLPropertiesCollection)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

HTMLPropertiesCollection::HTMLPropertiesCollection(nsGenericHTMLElement* aRoot)
  : mRoot(aRoot)
  , mDoc(aRoot->GetUncomposedDoc())
  , mIsDirty(true)
{
  mNames = new PropertyStringList(this);
  if (mDoc) {
    mDoc->AddMutationObserver(this);
  }
}

HTMLPropertiesCollection::~HTMLPropertiesCollection()
{
  if (mDoc) {
    mDoc->RemoveMutationObserver(this);
  }
}

NS_INTERFACE_TABLE_HEAD(HTMLPropertiesCollection)
    NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
    NS_INTERFACE_TABLE(HTMLPropertiesCollection,
                       nsIDOMHTMLCollection,
                       nsIHTMLCollection,
                       nsIMutationObserver)
    NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(HTMLPropertiesCollection)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(HTMLPropertiesCollection)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HTMLPropertiesCollection)


static PLDHashOperator
SetPropertyListDocument(const nsAString& aKey, PropertyNodeList* aEntry, void* aData)
{
  aEntry->SetDocument(static_cast<nsIDocument*>(aData));
  return PL_DHASH_NEXT;
}

void
HTMLPropertiesCollection::SetDocument(nsIDocument* aDocument) {
  if (mDoc) {
    mDoc->RemoveMutationObserver(this);
  }
  mDoc = aDocument;
  if (mDoc) {
    mDoc->AddMutationObserver(this);
  }
  mNamedItemEntries.EnumerateRead(SetPropertyListDocument, aDocument);
  mIsDirty = true;
}

JSObject*
HTMLPropertiesCollection::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLPropertiesCollectionBinding::Wrap(cx, this, aGivenProto);
}

NS_IMETHODIMP
HTMLPropertiesCollection::GetLength(uint32_t* aLength)
{
  EnsureFresh();
  *aLength = mProperties.Length();
  return NS_OK;
}

NS_IMETHODIMP
HTMLPropertiesCollection::Item(uint32_t aIndex, nsIDOMNode** aResult)
{
  nsINode* result = nsIHTMLCollection::Item(aIndex);
  if (result) {
    NS_ADDREF(*aResult = result->AsDOMNode());
  } else {
    *aResult = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLPropertiesCollection::NamedItem(const nsAString& aName,
                                    nsIDOMNode** aResult)
{
  *aResult = nullptr;
  return NS_OK;
}

Element*
HTMLPropertiesCollection::GetElementAt(uint32_t aIndex)
{
  EnsureFresh();
  return mProperties.SafeElementAt(aIndex);
}

nsINode*
HTMLPropertiesCollection::GetParentObject()
{
  return mRoot;
}

PropertyNodeList*
HTMLPropertiesCollection::NamedItem(const nsAString& aName)
{
  EnsureFresh();

  PropertyNodeList* propertyList = mNamedItemEntries.GetWeak(aName);
  if (!propertyList) {
    nsRefPtr<PropertyNodeList> newPropertyList =
      new PropertyNodeList(this, mRoot, aName);
    mNamedItemEntries.Put(aName, newPropertyList);
    propertyList = newPropertyList;
  }
  return propertyList;
}

void
HTMLPropertiesCollection::AttributeChanged(nsIDocument *aDocument, Element* aElement,
                                           int32_t aNameSpaceID, nsIAtom* aAttribute,
                                           int32_t aModType)
{
  mIsDirty = true;
}

void
HTMLPropertiesCollection::ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                                          nsIContent* aFirstNewContent,
                                          int32_t aNewIndexInContainer)
{
  mIsDirty = true;
}

void
HTMLPropertiesCollection::ContentInserted(nsIDocument *aDocument,
                                          nsIContent* aContainer,
                                          nsIContent* aChild,
                                          int32_t aIndexInContainer)
{
  mIsDirty = true;
}

void
HTMLPropertiesCollection::ContentRemoved(nsIDocument *aDocument,
                                         nsIContent* aContainer,
                                         nsIContent* aChild,
                                         int32_t aIndexInContainer,
                                         nsIContent* aPreviousSibling)
{
  mIsDirty = true;
}

static PLDHashOperator
MarkDirty(const nsAString& aKey, PropertyNodeList* aEntry, void* aData)
{
  aEntry->SetDirty();
  return PL_DHASH_NEXT;
}

void
HTMLPropertiesCollection::EnsureFresh()
{
  if (mDoc && !mIsDirty) {
    return;
  }
  mIsDirty = false;

  mProperties.Clear();
  mNames->Clear();
  // We don't clear NamedItemEntries because the PropertyNodeLists must be live.
  mNamedItemEntries.EnumerateRead(MarkDirty, nullptr);
  if (!mRoot->HasAttr(kNameSpaceID_None, nsGkAtoms::itemscope)) {
    return;
  }

  CrawlProperties();
  TreeOrderComparator comparator;
  mProperties.Sort(comparator);

  // Create the names DOMStringList
  uint32_t count = mProperties.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const nsAttrValue* attr = mProperties.ElementAt(i)->GetParsedAttr(nsGkAtoms::itemprop); 
    for (uint32_t i = 0; i < attr->GetAtomCount(); i++) {
      nsDependentAtomString propName(attr->AtomAt(i));
      bool contains = mNames->ContainsInternal(propName);
      if (!contains) {
        mNames->Add(propName);
      }
    }
  }
}

static Element*
GetElementByIdForConnectedSubtree(nsIContent* aContent, const nsIAtom* aId)
{
  aContent = static_cast<nsIContent*>(aContent->SubtreeRoot());
  do {
    if (aContent->GetID() == aId) {
      return aContent->AsElement();
    }
    aContent = aContent->GetNextNode();
  } while(aContent);

  return nullptr;
}

void
HTMLPropertiesCollection::CrawlProperties()
{
  nsIDocument* doc = mRoot->GetUncomposedDoc();

  const nsAttrValue* attr = mRoot->GetParsedAttr(nsGkAtoms::itemref);
  if (attr) {
    for (uint32_t i = 0; i < attr->GetAtomCount(); i++) {
      nsIAtom* ref = attr->AtomAt(i);
      Element* element;
      if (doc) {
        element = doc->GetElementById(nsDependentAtomString(ref));
      } else {
        element = GetElementByIdForConnectedSubtree(mRoot, ref);
      }
      if (element && element != mRoot) {
        CrawlSubtree(element);
      }
    }
  }

  CrawlSubtree(mRoot);
}

void
HTMLPropertiesCollection::CrawlSubtree(Element* aElement)
{
  nsIContent* aContent = aElement;
  while (aContent) {
    // We must check aContent against mRoot because 
    // an element must not be its own property
    if (aContent == mRoot || !aContent->IsHTMLElement()) {
      // Move on to the next node in the tree
      aContent = aContent->GetNextNode(aElement);
    } else {
      MOZ_ASSERT(aContent->IsElement(), "IsHTMLElement() returned true!");
      Element* element = aContent->AsElement();
      if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::itemprop) &&
          !mProperties.Contains(element)) {
        mProperties.AppendElement(static_cast<nsGenericHTMLElement*>(element));
      }

      if (element->HasAttr(kNameSpaceID_None, nsGkAtoms::itemscope)) {
        aContent = element->GetNextNonChildNode(aElement);
      } else {
        aContent = element->GetNextNode(aElement);
      }
    }
  }
}

void
HTMLPropertiesCollection::GetSupportedNames(unsigned, nsTArray<nsString>& aNames)
{
  EnsureFresh();
  mNames->CopyList(aNames);
}

PropertyNodeList::PropertyNodeList(HTMLPropertiesCollection* aCollection,
                                   nsIContent* aParent, const nsAString& aName)
  : mName(aName),
    mDoc(aParent->GetUncomposedDoc()),
    mCollection(aCollection),
    mParent(aParent),
    mIsDirty(true)
{
  if (mDoc) {
    mDoc->AddMutationObserver(this);
  }
}

PropertyNodeList::~PropertyNodeList()
{
  if (mDoc) {
    mDoc->RemoveMutationObserver(this);
  }
}

void
PropertyNodeList::SetDocument(nsIDocument* aDoc)
{
  if (mDoc) {
    mDoc->RemoveMutationObserver(this);
  }
  mDoc = aDoc;
  if (mDoc) {
    mDoc->AddMutationObserver(this);
  }
  mIsDirty = true;
}

NS_IMETHODIMP
PropertyNodeList::GetLength(uint32_t* aLength)
{
  EnsureFresh();
  *aLength = mElements.Length();
  return NS_OK;
}

NS_IMETHODIMP
PropertyNodeList::Item(uint32_t aIndex, nsIDOMNode** aReturn)
{
  EnsureFresh();
  nsINode* element = mElements.SafeElementAt(aIndex);
  if (!element) {
    *aReturn = nullptr;
    return NS_OK;
  }
  return CallQueryInterface(element, aReturn);
}

nsIContent*
PropertyNodeList::Item(uint32_t aIndex)
{
  EnsureFresh();
  return mElements.SafeElementAt(aIndex);
}

int32_t
PropertyNodeList::IndexOf(nsIContent* aContent)
{
  EnsureFresh();
  return mElements.IndexOf(aContent);
}

nsINode*
PropertyNodeList::GetParentObject()
{
  return mParent;
}

JSObject*
PropertyNodeList::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return PropertyNodeListBinding::Wrap(cx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PropertyNodeList)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PropertyNodeList)
  // SetDocument(nullptr) ensures that we remove ourselves as a mutation observer
  tmp->SetDocument(nullptr);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCollection)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PropertyNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDoc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCollection)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(PropertyNodeList)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PropertyNodeList)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PropertyNodeList)

NS_INTERFACE_TABLE_HEAD(PropertyNodeList)
    NS_WRAPPERCACHE_INTERFACE_TABLE_ENTRY
    NS_INTERFACE_TABLE(PropertyNodeList,
                       nsIDOMNodeList,
                       nsINodeList,
                       nsIMutationObserver)
    NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(PropertyNodeList)
NS_INTERFACE_MAP_END

void
PropertyNodeList::GetValues(JSContext* aCx, nsTArray<JS::Value >& aResult,
                            ErrorResult& aError)
{
  EnsureFresh();

  JS::Rooted<JSObject*> wrapper(aCx, GetWrapper());
  JSAutoCompartment ac(aCx, wrapper);
  uint32_t length = mElements.Length();
  for (uint32_t i = 0; i < length; ++i) {
    JS::Rooted<JS::Value> v(aCx);
    mElements.ElementAt(i)->GetItemValue(aCx, wrapper, &v, aError);
    if (aError.Failed()) {
      return;
    }
    aResult.AppendElement(v);
  }
}

void
PropertyNodeList::AttributeChanged(nsIDocument* aDocument, Element* aElement,
                                   int32_t aNameSpaceID, nsIAtom* aAttribute,
                                   int32_t aModType)
{
  mIsDirty = true;
}

void
PropertyNodeList::ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                                  nsIContent* aFirstNewContent,
                                  int32_t aNewIndexInContainer)
{
  mIsDirty = true;
}

void
PropertyNodeList::ContentInserted(nsIDocument* aDocument,
                                  nsIContent* aContainer,
                                  nsIContent* aChild,
                                  int32_t aIndexInContainer)
{
  mIsDirty = true;
}

void
PropertyNodeList::ContentRemoved(nsIDocument* aDocument,
                                 nsIContent* aContainer,
                                 nsIContent* aChild,
                                 int32_t aIndexInContainer,
                                 nsIContent* aPreviousSibling)
{
  mIsDirty = true;
}

void
PropertyNodeList::EnsureFresh()
{
  if (mDoc && !mIsDirty) {
    return;
  }
  mIsDirty = false;

  mCollection->EnsureFresh();
  Clear();

  uint32_t count = mCollection->mProperties.Length();
  for (uint32_t i = 0; i < count; ++i) {
    nsGenericHTMLElement* element = mCollection->mProperties.ElementAt(i);
    const nsAttrValue* attr = element->GetParsedAttr(nsGkAtoms::itemprop);
    if (attr->Contains(mName)) {
      AppendElement(element);
    }
  }
}

PropertyStringList::PropertyStringList(HTMLPropertiesCollection* aCollection)
  : DOMStringList()
  , mCollection(aCollection)
{ }

PropertyStringList::~PropertyStringList()
{ }

NS_IMPL_CYCLE_COLLECTION_INHERITED(PropertyStringList, DOMStringList,
                                   mCollection)

NS_IMPL_ADDREF_INHERITED(PropertyStringList, DOMStringList)
NS_IMPL_RELEASE_INHERITED(PropertyStringList, DOMStringList)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PropertyStringList)
NS_INTERFACE_MAP_END_INHERITING(DOMStringList)

void
PropertyStringList::EnsureFresh()
{
  MOZ_ASSERT(NS_IsMainThread());

  mCollection->EnsureFresh();
}

bool
PropertyStringList::ContainsInternal(const nsAString& aString)
{
  // This method should not call EnsureFresh, otherwise we may become stuck in an infinite loop.
  return mNames.Contains(aString);
}

} // namespace dom
} // namespace mozilla
