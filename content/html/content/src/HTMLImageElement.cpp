/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLImageElementBinding.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsSize.h"
#include "nsIDocument.h"
#include "nsIDOMMutationEvent.h"
#include "nsIScriptContext.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsContainerFrame.h"
#include "nsNodeInfoManager.h"
#include "mozilla/MouseEvents.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsFocusManager.h"
#include "mozilla/dom/HTMLFormElement.h"

// Responsive images!
#include "mozilla/dom/HTMLSourceElement.h"
#include "mozilla/dom/ResponsiveImageSelector.h"

#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgINotificationObserver.h"
#include "imgRequestProxy.h"

#include "nsILoadGroup.h"

#include "nsRuleData.h"

#include "nsIDOMHTMLMapElement.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"

#include "nsLayoutUtils.h"

#include "mozilla/Preferences.h"
static const char *kPrefSrcsetEnabled = "dom.image.srcset.enabled";

NS_IMPL_NS_NEW_HTML_ELEMENT(Image)

// Is aSubject a previous sibling of aNode.
static bool IsPreviousSibling(nsINode *aSubject, nsINode *aNode)
{
  if (aSubject == aNode) {
    return false;
  }

  nsINode *parent = aSubject->GetParentNode();
  if (parent && parent == aNode->GetParentNode()) {
    return parent->IndexOf(aSubject) < parent->IndexOf(aNode);
  }

  return false;
}

namespace mozilla {
namespace dom {

HTMLImageElement::HTMLImageElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
  , mForm(nullptr)
{
  // We start out broken
  AddStatesSilently(NS_EVENT_STATE_BROKEN);
}

HTMLImageElement::~HTMLImageElement()
{
  DestroyImageLoadingContent();
}


NS_IMPL_ADDREF_INHERITED(HTMLImageElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLImageElement, Element)


// QueryInterface implementation for HTMLImageElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLImageElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLImageElement,
                               nsIDOMHTMLImageElement,
                               nsIImageLoadingContent,
                               imgIOnloadBlocker,
                               imgINotificationObserver)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)


NS_IMPL_ELEMENT_CLONE(HTMLImageElement)


NS_IMPL_STRING_ATTR(HTMLImageElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLImageElement, Align, align)
NS_IMPL_STRING_ATTR(HTMLImageElement, Alt, alt)
NS_IMPL_STRING_ATTR(HTMLImageElement, Border, border)
NS_IMPL_INT_ATTR(HTMLImageElement, Hspace, hspace)
NS_IMPL_BOOL_ATTR(HTMLImageElement, IsMap, ismap)
NS_IMPL_URI_ATTR(HTMLImageElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(HTMLImageElement, Sizes, sizes)
NS_IMPL_STRING_ATTR(HTMLImageElement, Lowsrc, lowsrc)
NS_IMPL_URI_ATTR(HTMLImageElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLImageElement, Srcset, srcset)
NS_IMPL_STRING_ATTR(HTMLImageElement, UseMap, usemap)
NS_IMPL_INT_ATTR(HTMLImageElement, Vspace, vspace)

bool
HTMLImageElement::IsSrcsetEnabled()
{
  return Preferences::GetBool(kPrefSrcsetEnabled, false);
}

nsresult
HTMLImageElement::GetCurrentSrc(nsAString& aValue)
{
  if (!IsSrcsetEnabled()) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> currentURI;
  GetCurrentURI(getter_AddRefs(currentURI));
  if (currentURI) {
    nsAutoCString spec;
    currentURI->GetSpec(spec);
    CopyUTF8toUTF16(spec, aValue);
  } else {
    SetDOMStringToNull(aValue);
  }

  return NS_OK;
}

void
HTMLImageElement::GetItemValueText(nsAString& aValue)
{
  GetSrc(aValue);
}

void
HTMLImageElement::SetItemValueText(const nsAString& aValue)
{
  SetSrc(aValue);
}

// crossorigin is not "limited to only known values" per spec, so it's
// just a string attr purposes of the DOM crossOrigin property.
// TODO: It is now (bug 880997).
NS_IMPL_STRING_ATTR(HTMLImageElement, CrossOrigin, crossorigin)

bool
HTMLImageElement::Draggable() const
{
  // images may be dragged unless the draggable attribute is false
  return !AttrValueIs(kNameSpaceID_None, nsGkAtoms::draggable,
                      nsGkAtoms::_false, eIgnoreCase);
}

bool
HTMLImageElement::Complete()
{
  if (!mCurrentRequest) {
    return true;
  }

  uint32_t status;
  mCurrentRequest->GetImageStatus(&status);
  return
    (status &
     (imgIRequest::STATUS_LOAD_COMPLETE | imgIRequest::STATUS_ERROR)) != 0;
}

NS_IMETHODIMP
HTMLImageElement::GetComplete(bool* aComplete)
{
  NS_PRECONDITION(aComplete, "Null out param!");

  *aComplete = Complete();

  return NS_OK;
}

CSSIntPoint
HTMLImageElement::GetXY()
{
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
  if (!frame) {
    return CSSIntPoint(0, 0);
  }

  nsIFrame* layer = nsLayoutUtils::GetClosestLayer(frame->GetParent());
  return CSSIntPoint::FromAppUnitsRounded(frame->GetOffsetTo(layer));
}

int32_t
HTMLImageElement::X()
{
  return GetXY().x;
}

int32_t
HTMLImageElement::Y()
{
  return GetXY().y;
}

NS_IMETHODIMP
HTMLImageElement::GetX(int32_t* aX)
{
  *aX = X();
  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::GetY(int32_t* aY)
{
  *aY = Y();
  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::GetHeight(uint32_t* aHeight)
{
  *aHeight = Height();

  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::SetHeight(uint32_t aHeight)
{
  ErrorResult rv;
  SetHeight(aHeight, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLImageElement::GetWidth(uint32_t* aWidth)
{
  *aWidth = Width();

  return NS_OK;
}

NS_IMETHODIMP
HTMLImageElement::SetWidth(uint32_t aWidth)
{
  ErrorResult rv;
  SetWidth(aWidth, rv);
  return rv.ErrorCode();
}

bool
HTMLImageElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::crossorigin) {
      ParseCORSValue(aValue, aResult);
      return true;
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

void
HTMLImageElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                                        nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
HTMLImageElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                         int32_t aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::usemap ||
      aAttribute == nsGkAtoms::ismap) {
    NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
  } else if (aAttribute == nsGkAtoms::alt) {
    if (aModType == nsIDOMMutationEvent::ADDITION ||
        aModType == nsIDOMMutationEvent::REMOVAL) {
      NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
    }
  }
  return retval;
}

NS_IMETHODIMP_(bool)
HTMLImageElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
    sImageAlignAttributeMap
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLImageElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}


nsresult
HTMLImageElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                const nsAttrValueOrString* aValue,
                                bool aNotify)
{

  if (aNameSpaceID == kNameSpaceID_None && mForm &&
      (aName == nsGkAtoms::name || aName == nsGkAtoms::id)) {
    // remove the image from the hashtable as needed
    nsAutoString tmp;
    GetAttr(kNameSpaceID_None, aName, tmp);

    if (!tmp.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, tmp,
                                         HTMLFormElement::AttributeUpdated);
    }
  }

  return nsGenericHTMLElement::BeforeSetAttr(aNameSpaceID, aName,
                                             aValue, aNotify);
}

nsresult
HTMLImageElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                               const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && mForm &&
      (aName == nsGkAtoms::name || aName == nsGkAtoms::id) &&
      aValue && !aValue->IsEmptyString()) {
    // add the image to the hashtable as needed
    NS_ABORT_IF_FALSE(aValue->Type() == nsAttrValue::eAtom,
      "Expected atom value for name/id");
    mForm->AddImageElementToTable(this,
      nsDependentAtomString(aValue->GetAtomValue()));
  }

  // Handle src/srcset/crossorigin updates. If aNotify is false, we are coming
  // from the parser or some such place; we'll get bound after all the
  // attributes have been set, so we'll do the image load from BindToTree.

  nsCOMPtr<nsIContent> thisContent = AsContent();
  if (aName == nsGkAtoms::src &&
      aNameSpaceID == kNameSpaceID_None) {
    // SetAttr handles setting src in the non-responsive case, so only handle it
    // for responsive mode or unsetting
    if (!aValue) {
      CancelImageRequests(aNotify);
    } else if (mResponsiveSelector) {
      mResponsiveSelector->SetDefaultSource(aValue ? aValue->GetStringValue()
                                                   : EmptyString());
      LoadSelectedImage(false, aNotify);
    }
  } else if (aName == nsGkAtoms::srcset &&
             aNameSpaceID == kNameSpaceID_None &&
             aNotify &&
             AsContent()->IsInDoc() &&
             IsSrcsetEnabled()) {
    // We currently don't handle responsive mode until BindToTree
    PictureSourceSrcsetChanged(thisContent,
                               aValue ? aValue->GetStringValue() : EmptyString(),
                               aNotify);
  } else if (aName == nsGkAtoms::sizes &&
             aNameSpaceID == kNameSpaceID_None &&
             thisContent->IsInDoc() &&
             HTMLPictureElement::IsPictureEnabled()) {
    PictureSourceSizesChanged(thisContent, aValue->GetStringValue(), aNotify);
  } else if (aName == nsGkAtoms::crossorigin &&
             aNameSpaceID == kNameSpaceID_None &&
             aNotify) {
    // We want aForce == true in this LoadImage call, because we want to force
    // a new load of the image with the new cross origin policy.
    nsCOMPtr<nsIURI> currentURI;
    if (NS_SUCCEEDED(GetCurrentURI(getter_AddRefs(currentURI))) && currentURI) {
      LoadImage(currentURI, true, aNotify);
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName,
                                            aValue, aNotify);
}


nsresult
HTMLImageElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  // If we are a map and get a mouse click, don't let it be handled by
  // the Generic Element as this could cause a click event to fire
  // twice, once by the image frame for the map and once by the Anchor
  // element. (bug 39723)
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  if (mouseEvent && mouseEvent->IsLeftClickEvent()) {
    bool isMap = false;
    GetIsMap(&isMap);
    if (isMap) {
      aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsGenericHTMLElement::PreHandleEvent(aVisitor);
}

bool
HTMLImageElement::IsHTMLFocusable(bool aWithMouse,
                                  bool *aIsFocusable, int32_t *aTabIndex)
{
  int32_t tabIndex = TabIndex();

  if (IsInDoc()) {
    nsAutoString usemap;
    GetUseMap(usemap);
    // XXXbz which document should this be using?  sXBL/XBL2 issue!  I
    // think that OwnerDoc() is right, since we don't want to
    // assume stuff about the document we're bound to.
    if (OwnerDoc()->FindImageMap(usemap)) {
      if (aTabIndex) {
        // Use tab index on individual map areas
        *aTabIndex = (sTabFocusModel & eTabFocus_linksMask)? 0 : -1;
      }
      // Image map is not focusable itself, but flag as tabbable
      // so that image map areas get walked into.
      *aIsFocusable = false;

      return false;
    }
  }

  if (aTabIndex) {
    // Can be in tab order if tabindex >=0 and form controls are tabbable.
    *aTabIndex = (sTabFocusModel & eTabFocus_formElementsMask)? tabIndex : -1;
  }

  *aIsFocusable = 
#ifdef XP_MACOSX
    (!aWithMouse || nsFocusManager::sMouseFocusesFormControl) &&
#endif
    (tabIndex >= 0 || HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex));

  return false;
}

nsresult
HTMLImageElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                          nsIAtom* aPrefix, const nsAString& aValue,
                          bool aNotify)
{
  // We need to force our image to reload.  This must be done here, not in
  // AfterSetAttr or BeforeSetAttr, because we want to do it even if the attr is
  // being set to its existing value, which is normally optimized away as a
  // no-op.
  //
  // If aNotify is false, we are coming from the parser or some such place;
  // we'll get bound after all the attributes have been set, so we'll do the
  // image load from BindToTree. Skip the LoadImage call in that case.
  //
  // If we are in responsive mode, we drop the forced reload behavior, and
  // handle updates in AfterSetAttr
  if (aNotify && !mResponsiveSelector &&
      aNameSpaceID == kNameSpaceID_None &&
      aName == nsGkAtoms::src) {

    // Prevent setting image.src by exiting early
    if (nsContentUtils::IsImageSrcSetDisabled()) {
      return NS_OK;
    }

    // A hack to get animations to reset. See bug 594771.
    mNewRequestsWillNeedAnimationReset = true;

    // Force image loading here, so that we'll try to load the image from
    // network if it's set to be not cacheable...  If we change things so that
    // the state gets in Element's attr-setting happen around this
    // LoadImage call, we could start passing false instead of aNotify
    // here.
    LoadImage(aValue, true, aNotify);

    mNewRequestsWillNeedAnimationReset = false;
  }

  return nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix, aValue,
                                       aNotify);
}

nsresult
HTMLImageElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsImageLoadingContent::BindToTree(aDocument, aParent, aBindingParent,
                                    aCompileEventHandlers);

  if (aParent) {
    UpdateFormOwner();
  }

  bool addedToPicture = aParent && aParent->Tag() == nsGkAtoms::picture &&
                        HTMLPictureElement::IsPictureEnabled();
  bool haveSrcset = IsSrcsetEnabled() &&
                    HasAttr(kNameSpaceID_None, nsGkAtoms::srcset);
  if (addedToPicture || haveSrcset ||
      HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
    // FIXME: Bug 660963 it would be nice if we could just have
    // ClearBrokenState update our state and do it fast...
    ClearBrokenState();
    RemoveStatesSilently(NS_EVENT_STATE_BROKEN);

    // We don't handle responsive changes when not bound to a tree, update them
    // now if necessary
    if (addedToPicture || haveSrcset) {
      MaybeUpdateResponsiveSelector();
    }

    // If loading is temporarily disabled, don't even launch MaybeLoadImage.
    // Otherwise MaybeLoadImage may run later when someone has reenabled
    // loading.
    if (LoadingEnabled()) {
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &HTMLImageElement::MaybeLoadImage));
    }
  }

  return rv;
}

void
HTMLImageElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mForm) {
    if (aNullParent || !FindAncestorForm(mForm)) {
      ClearForm(true);
    } else {
      UnsetFlags(MAYBE_ORPHAN_FORM_ELEMENT);
    }
  }

  mResponsiveSelector = nullptr;

  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

void
HTMLImageElement::UpdateFormOwner()
{
  if (!mForm) {
    mForm = FindAncestorForm();
  }

  if (mForm && !HasFlag(ADDED_TO_FORM)) {
    // Now we need to add ourselves to the form
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    SetFlags(ADDED_TO_FORM);

    mForm->AddImageElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->AddImageElementToTable(this, nameVal);
    }

    if (!idVal.IsEmpty()) {
      mForm->AddImageElementToTable(this, idVal);
    }
  }
}

void
HTMLImageElement::MaybeLoadImage()
{
  // Our base URI may have changed, or we may have had responsive parameters
  // change while not bound to the tree. Re-parse src/srcset and call LoadImage,
  // which is a no-op if it resolves to the same effective URI without aForce.

  // Note, check LoadingEnabled() after LoadImage call.

  nsresult rv = LoadSelectedImage(false, true);

  if (NS_FAILED(rv) || !LoadingEnabled()) {
    CancelImageRequests(true);
  }
}

EventStates
HTMLImageElement::IntrinsicState() const
{
  return nsGenericHTMLElement::IntrinsicState() |
    nsImageLoadingContent::ImageState();
}

// static
already_AddRefed<HTMLImageElement>
HTMLImageElement::Image(const GlobalObject& aGlobal,
                        const Optional<uint32_t>& aWidth,
                        const Optional<uint32_t>& aHeight,
                        ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  nsIDocument* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aError.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  already_AddRefed<mozilla::dom::NodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::img, nullptr,
                                        kNameSpaceID_XHTML,
                                        nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<HTMLImageElement> img = new HTMLImageElement(nodeInfo);

  if (aWidth.WasPassed()) {
    img->SetWidth(aWidth.Value(), aError);
    if (aError.Failed()) {
      return nullptr;
    }

    if (aHeight.WasPassed()) {
      img->SetHeight(aHeight.Value(), aError);
      if (aError.Failed()) {
        return nullptr;
      }
    }
  }

  return img.forget();
}

uint32_t
HTMLImageElement::NaturalHeight()
{
  uint32_t height;
  nsresult rv = nsImageLoadingContent::GetNaturalHeight(&height);

  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "GetNaturalHeight should not fail");
    return 0;
  }

  if (mResponsiveSelector) {
    double density = mResponsiveSelector->GetSelectedImageDensity();
    MOZ_ASSERT(IsFinite(density) && density > 0.0);
    height = NSToIntRound(double(height) / density);
    height = std::max(height, 0u);
  }

  return height;
}

NS_IMETHODIMP
HTMLImageElement::GetNaturalHeight(uint32_t* aNaturalHeight)
{
  *aNaturalHeight = NaturalHeight();
  return NS_OK;
}

uint32_t
HTMLImageElement::NaturalWidth()
{
  uint32_t width;
  nsresult rv = nsImageLoadingContent::GetNaturalWidth(&width);

  if (NS_FAILED(rv)) {
    MOZ_ASSERT(false, "GetNaturalWidth should not fail");
    return 0;
  }

  if (mResponsiveSelector) {
    double density = mResponsiveSelector->GetSelectedImageDensity();
    MOZ_ASSERT(IsFinite(density) && density > 0.0);
    width = NSToIntRound(double(width) / density);
    width = std::max(width, 0u);
  }

  return width;
}

NS_IMETHODIMP
HTMLImageElement::GetNaturalWidth(uint32_t* aNaturalWidth)
{
  *aNaturalWidth = NaturalWidth();
  return NS_OK;
}

nsresult
HTMLImageElement::CopyInnerTo(Element* aDest)
{
  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticImageClone(static_cast<HTMLImageElement*>(aDest));
  }
  return nsGenericHTMLElement::CopyInnerTo(aDest);
}

CORSMode
HTMLImageElement::GetCORSMode()
{
  return AttrValueToCORSMode(GetParsedAttr(nsGkAtoms::crossorigin));
}

JSObject*
HTMLImageElement::WrapNode(JSContext* aCx)
{
  return HTMLImageElementBinding::Wrap(aCx, this);
}

#ifdef DEBUG
nsIDOMHTMLFormElement*
HTMLImageElement::GetForm() const
{
  return mForm;
}
#endif

void
HTMLImageElement::SetForm(nsIDOMHTMLFormElement* aForm)
{
  NS_PRECONDITION(aForm, "Don't pass null here");
  NS_ASSERTION(!mForm,
               "We don't support switching from one non-null form to another.");

  mForm = static_cast<HTMLFormElement*>(aForm);
}

void
HTMLImageElement::ClearForm(bool aRemoveFromForm)
{
  NS_ASSERTION((mForm != nullptr) == HasFlag(ADDED_TO_FORM),
               "Form control should have had flag set correctly");

  if (!mForm) {
    return;
  }

  if (aRemoveFromForm) {
    nsAutoString nameVal, idVal;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, nameVal);
    GetAttr(kNameSpaceID_None, nsGkAtoms::id, idVal);

    mForm->RemoveImageElement(this);

    if (!nameVal.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, nameVal,
                                         HTMLFormElement::ElementRemoved);
    }

    if (!idVal.IsEmpty()) {
      mForm->RemoveImageElementFromTable(this, idVal,
                                         HTMLFormElement::ElementRemoved);
    }
  }

  UnsetFlags(ADDED_TO_FORM);
  mForm = nullptr;
}

nsresult
HTMLImageElement::LoadSelectedImage(bool aForce, bool aNotify)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mResponsiveSelector) {
    nsCOMPtr<nsIURI> url = mResponsiveSelector->GetSelectedImageURL();
    if (url) {
      rv = LoadImage(url, aForce, aNotify);
    } else {
      CancelImageRequests(aNotify);
      rv = NS_OK;
    }
  } else {
    nsAutoString src;
    if (!GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
      CancelImageRequests(aNotify);
      rv = NS_OK;
    } else {
      rv = LoadImage(src, aForce, aNotify);
      if (NS_FAILED(rv)) {
        CancelImageRequests(aNotify);
      }
    }
  }

  return rv;
}

void
HTMLImageElement::PictureSourceSrcsetChanged(nsIContent *aSourceNode,
                                             const nsAString& aNewValue,
                                             bool aNotify)
{
  if (aSourceNode != AsContent() && !HTMLPictureElement::IsPictureEnabled()) {
    // Don't consider <source> nodes if picture is pref'd off
    return;
  }

  nsIContent *currentSrc = mResponsiveSelector ? mResponsiveSelector->Content()
                                               : nullptr;

  if (aSourceNode == currentSrc) {
    // We're currently using this node as our responsive selector source.
    mResponsiveSelector->SetCandidatesFromSourceSet(aNewValue);
    // Search for a new source if we are no longer valid.
    MaybeUpdateResponsiveSelector(currentSrc);

    LoadSelectedImage(false, aNotify);
  } else if (currentSrc && IsPreviousSibling(currentSrc, aSourceNode)) {
      // If we have a source and it is previous to the one being updated, ignore
      return;
  } else {
    // This is previous to our current source or we don't have a current source,
    // use it if valid.
    if (TryCreateResponsiveSelector(aSourceNode, &aNewValue, nullptr)) {
      LoadSelectedImage(false, aNotify);
    }
  }
}

void
HTMLImageElement::PictureSourceSizesChanged(nsIContent *aSourceNode,
                                            const nsAString& aNewValue,
                                            bool aNotify)
{
  if (!HTMLPictureElement::IsPictureEnabled()) {
    // Don't consider sizes at all if picture support is disabled
    return;
  }

  nsIContent *currentSrc = mResponsiveSelector ? mResponsiveSelector->Content()
                                               : nullptr;

  if (aSourceNode == currentSrc) {
    // We're currently using this node as our responsive selector source.
    mResponsiveSelector->SetSizesFromDescriptor(aNewValue);
    LoadSelectedImage(false, aNotify);
  }
}

void
HTMLImageElement::PictureSourceAdded(nsIContent *aSourceNode)
{
  // If the source node is previous to our current one, or ourselves if we have
  // no responsive source, try to use it as a responsive source.
  nsIContent *currentSrc = mResponsiveSelector ? mResponsiveSelector->Content()
                                               : AsContent();

  if (HTMLPictureElement::IsPictureEnabled() &&
      IsPreviousSibling(aSourceNode, currentSrc) &&
      TryCreateResponsiveSelector(aSourceNode, nullptr, nullptr)) {
    LoadSelectedImage(false, true);
  }
}

void
HTMLImageElement::PictureSourceRemoved(nsIContent *aSourceNode)
{
  // If this is our current source, we'll need to find another one or leave
  // responsive mode.
  if (mResponsiveSelector && mResponsiveSelector->Content() == aSourceNode) {
    MaybeUpdateResponsiveSelector(aSourceNode, true);
    LoadSelectedImage(false, true);
  }
}

bool
HTMLImageElement::MaybeUpdateResponsiveSelector(nsIContent *aCurrentSource,
                                                bool aSourceRemoved)
{
  nsIContent *thisContent = AsContent();

  if (!aCurrentSource && mResponsiveSelector) {
    aCurrentSource = mResponsiveSelector->Content();
  }

  // If we have a source with candidates, no update is needed unless it is being
  // removed
  if (aCurrentSource && !aSourceRemoved &&
      mResponsiveSelector->NumCandidates()) {
    return false;
  }

  // Otherwise, invalidate
  bool hadSelector = !!mResponsiveSelector;
  mResponsiveSelector = nullptr;

  if (!IsSrcsetEnabled()) {
    return hadSelector;
  }

  // See if there's another source node we could use.
  bool pictureEnabled = HTMLPictureElement::IsPictureEnabled();
  nsIContent *nextSource = nullptr;
  if (pictureEnabled && aCurrentSource && aCurrentSource != thisContent) {
    // If current source is the <img> tag, there is no next candidate. Otherwise,
    // it's the next sibling of the current source.
    MOZ_ASSERT(IsPreviousSibling(aCurrentSource, thisContent) &&
               thisContent->GetParentNode()->Tag() == nsGkAtoms::picture);
    nextSource = aCurrentSource->GetNextSibling();
  } else if (!aCurrentSource) {
    // If no current source at all, start from the first possible source, which
    // is the first node of the <picture> element or ourselves if we're not a
    // picture
    nsINode *parent = pictureEnabled ? thisContent->GetParentNode() : nullptr;
    if (parent && parent->Tag() == nsGkAtoms::picture) {
      nextSource = parent->GetFirstChild();
    } else {
      nextSource = thisContent;
    }
  }

  while (nextSource) {
    if (nextSource == thisContent) {
      // We are the last possible source, so stop searching if we match or
      // not
      TryCreateResponsiveSelector(nextSource);
      break;
    } else if (nextSource->Tag() == nsGkAtoms::source &&
               TryCreateResponsiveSelector(nextSource)) {
      // If this led to a valid source, stop
      break;
    }

    nextSource = nextSource->GetNextSibling();
  }

  // State changed unless we didn't make a selector and didn't start with one
  return mResponsiveSelector || hadSelector;
}

bool
HTMLImageElement::TryCreateResponsiveSelector(nsIContent *aSourceNode,
                                              const nsAString *aSrcset,
                                              const nsAString *aSizes)
{
  if (!IsSrcsetEnabled()) {
    return false;
  }

  bool pictureEnabled = HTMLPictureElement::IsPictureEnabled();
  // Skip if this is not a <source> with matching media query
  bool isSourceTag = aSourceNode->Tag() == nsGkAtoms::source;
  if (isSourceTag) {
    DebugOnly<nsINode *> parent(nsINode::GetParentNode());
    MOZ_ASSERT(parent && parent->Tag() == nsGkAtoms::picture);
    MOZ_ASSERT(IsPreviousSibling(aSourceNode, AsContent()));
    MOZ_ASSERT(pictureEnabled);

    HTMLSourceElement *src = static_cast<HTMLSourceElement*>(aSourceNode);
    if (!src->MatchesCurrentMedia()) {
      return false;
    }
  } else if (aSourceNode->Tag() == nsGkAtoms::img) {
    // Otherwise this is the <img> tag itself
    MOZ_ASSERT(aSourceNode == AsContent());
  }

  // Skip if has no srcset or an empty srcset
  nsString srcset;
  if (aSrcset) {
    srcset = *aSrcset;
  } else if (!aSourceNode->GetAttr(kNameSpaceID_None, nsGkAtoms::srcset,
                                   srcset)) {
    return false;
  }

  if (srcset.IsEmpty()) {
    return false;
  }


  // Try to parse
  nsRefPtr<ResponsiveImageSelector> sel = new ResponsiveImageSelector(this);
  if (!sel->SetCandidatesFromSourceSet(srcset)) {
    // No possible candidates, don't need to bother parsing sizes
    return false;
  }

  if (pictureEnabled && aSizes) {
    sel->SetSizesFromDescriptor(*aSizes);
  } else if (pictureEnabled) {
    nsAutoString sizes;
    aSourceNode->GetAttr(kNameSpaceID_None, nsGkAtoms::sizes, sizes);
    sel->SetSizesFromDescriptor(sizes);
  }

  // If this is the <img> tag, also pull in src as the default source
  if (!isSourceTag) {
    MOZ_ASSERT(aSourceNode == AsContent());
    nsAutoString src;
    if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src) && !src.IsEmpty()) {
      sel->SetDefaultSource(src);
    }
  }

  mResponsiveSelector = sel;
  return true;
}

void
HTMLImageElement::DestroyContent()
{
  mResponsiveSelector = nullptr;
}

} // namespace dom
} // namespace mozilla

