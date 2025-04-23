/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLDialogElement.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/CloseWatcher.h"
#include "mozilla/dom/CloseWatcherManager.h"
#include "mozilla/dom/HTMLDialogElementBinding.h"

#include "nsIDOMEventListener.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Dialog)

namespace mozilla::dom {

HTMLDialogElement::~HTMLDialogElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLDialogElement)

class DialogCloseWatcherListener : public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS

  explicit DialogCloseWatcherListener(HTMLDialogElement* aDialog) {
    mDialog = do_GetWeakReference(aDialog);
  }

  NS_IMETHODIMP HandleEvent(Event* aEvent) override {
    RefPtr<nsINode> node = do_QueryReferent(mDialog);
    if (HTMLDialogElement* dialog = HTMLDialogElement::FromNodeOrNull(node)) {
      nsAutoString eventType;
      aEvent->GetType(eventType);
      if (eventType.EqualsLiteral("cancel")) {
        bool defaultAction = true;
        auto cancelable =
            aEvent->Cancelable() ? Cancelable::eYes : Cancelable::eNo;
        nsContentUtils::DispatchTrustedEvent(dialog->OwnerDoc(), dialog,
                                             u"cancel"_ns, CanBubble::eNo,
                                             cancelable, &defaultAction);
        if (!defaultAction) {
          aEvent->PreventDefault();
        }
      } else if (eventType.EqualsLiteral("close")) {
        Optional<nsAString> retValue;
        dialog->Close(retValue);
      }
    }
    return NS_OK;
  }

 private:
  virtual ~DialogCloseWatcherListener() = default;
  nsWeakPtr mDialog;
};
NS_IMPL_ISUPPORTS(DialogCloseWatcherListener, nsIDOMEventListener)

void HTMLDialogElement::Close(
    const mozilla::dom::Optional<nsAString>& aReturnValue) {
  if (!Open()) {
    return;
  }

  if (StaticPrefs::dom_element_dialog_toggle_events_enabled()) {
    FireToggleEvent(u"open"_ns, u"closed"_ns, u"beforetoggle"_ns);
    if (!Open()) {
      return;
    }
    QueueToggleEventTask();
  }

  if (aReturnValue.WasPassed()) {
    SetReturnValue(aReturnValue.Value());
  }

  SetOpen(false, IgnoreErrors());

  RemoveFromTopLayerIfNeeded();

  RefPtr<Element> previouslyFocusedElement =
      do_QueryReferent(mPreviouslyFocusedElement);

  if (previouslyFocusedElement) {
    mPreviouslyFocusedElement = nullptr;

    FocusOptions options;
    options.mPreventScroll = true;
    previouslyFocusedElement->Focus(options, CallerType::NonSystem,
                                    IgnoredErrorResult());
  }

  RefPtr<AsyncEventDispatcher> eventDispatcher =
      new AsyncEventDispatcher(this, u"close"_ns, CanBubble::eNo);
  eventDispatcher->PostDOMEvent();

  if (mCloseWatcher) {
    mCloseWatcher->Destroy();
    mCloseWatcher = nullptr;
  }
}

void HTMLDialogElement::Show(ErrorResult& aError) {
  if (Open()) {
    if (!IsInTopLayer()) {
      return;
    }
    return aError.ThrowInvalidStateError(
        "Cannot call show() on an open modal dialog.");
  }

  if (StaticPrefs::dom_element_dialog_toggle_events_enabled()) {
    if (FireToggleEvent(u"closed"_ns, u"open"_ns, u"beforetoggle"_ns)) {
      return;
    }
    if (Open()) {
      return;
    }
    QueueToggleEventTask();
  }

  SetOpen(true, IgnoreErrors());

  StorePreviouslyFocusedElement();

  RefPtr<nsINode> hideUntil = GetTopmostPopoverAncestor(nullptr, false);
  if (!hideUntil) {
    hideUntil = OwnerDoc();
  }

  OwnerDoc()->HideAllPopoversUntil(*hideUntil, false, true);
  FocusDialog();
}

bool HTMLDialogElement::IsInTopLayer() const {
  return State().HasState(ElementState::MODAL);
}

void HTMLDialogElement::AddToTopLayerIfNeeded() {
  MOZ_ASSERT(IsInComposedDoc());
  if (IsInTopLayer()) {
    return;
  }

  OwnerDoc()->AddModalDialog(*this);
}

void HTMLDialogElement::RemoveFromTopLayerIfNeeded() {
  if (!IsInTopLayer()) {
    return;
  }
  OwnerDoc()->RemoveModalDialog(*this);
}

void HTMLDialogElement::StorePreviouslyFocusedElement() {
  if (Element* element = nsFocusManager::GetFocusedElementStatic()) {
    if (NS_SUCCEEDED(nsContentUtils::CheckSameOrigin(this, element))) {
      mPreviouslyFocusedElement = do_GetWeakReference(element);
    }
  } else if (Document* doc = GetComposedDoc()) {
    // Looks like there's a discrepancy sometimes when focus is moved
    // to a different in-process window.
    if (nsIContent* unretargetedFocus = doc->GetUnretargetedFocusedContent()) {
      mPreviouslyFocusedElement = do_GetWeakReference(unretargetedFocus);
    }
  }
}

void HTMLDialogElement::UnbindFromTree(UnbindContext& aContext) {
  RemoveFromTopLayerIfNeeded();

  if (mCloseWatcher) {
    mCloseWatcher->Destroy();
    mCloseWatcher = nullptr;
  }

  nsGenericHTMLElement::UnbindFromTree(aContext);
}

void HTMLDialogElement::ShowModal(ErrorResult& aError) {
  if (Open()) {
    if (IsInTopLayer()) {
      return;
    }
    return aError.ThrowInvalidStateError(
        "Cannot call showModal() on an open non-modal dialog.");
  }

  if (!IsInComposedDoc()) {
    return aError.ThrowInvalidStateError("Dialog element is not connected");
  }

  if (IsPopoverOpen()) {
    return aError.ThrowInvalidStateError(
        "Dialog element is already an open popover.");
  }

  if (StaticPrefs::dom_element_dialog_toggle_events_enabled()) {
    if (FireToggleEvent(u"closed"_ns, u"open"_ns, u"beforetoggle"_ns)) {
      return;
    }
    if (Open() || !IsInComposedDoc() || IsPopoverOpen()) {
      return;
    }
    QueueToggleEventTask();
  }

  AddToTopLayerIfNeeded();

  SetOpen(true, aError);

  StorePreviouslyFocusedElement();

  if (StaticPrefs::dom_closewatcher_enabled()) {
    RefPtr<Document> doc = OwnerDoc();
    if (doc->IsActive() && doc->IsCurrentActiveDocument()) {
      if (RefPtr window = OwnerDoc()->GetInnerWindow()) {
        mCloseWatcher = new CloseWatcher(window);
        RefPtr<DialogCloseWatcherListener> eventListener =
            new DialogCloseWatcherListener(this);
        mCloseWatcher->AddSystemEventListener(u"cancel"_ns, eventListener,
                                              false /* aUseCapture */,
                                              false /* aWantsUntrusted */);
        mCloseWatcher->AddSystemEventListener(u"close"_ns, eventListener,
                                              false /* aUseCapture */,
                                              false /* aWantsUntrusted */);
        window->EnsureCloseWatcherManager()->Add(*mCloseWatcher);
      }
    }
  }

  RefPtr<nsINode> hideUntil = GetTopmostPopoverAncestor(nullptr, false);
  if (!hideUntil) {
    hideUntil = OwnerDoc();
  }

  OwnerDoc()->HideAllPopoversUntil(*hideUntil, false, true);
  FocusDialog();

  aError.SuppressException();
}

void HTMLDialogElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                     const nsAttrValue* aValue,
                                     const nsAttrValue* aOldValue,
                                     nsIPrincipal* aMaybeScriptedPrincipal,
                                     bool aNotify) {
  nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue, aOldValue,
                                     aMaybeScriptedPrincipal, aNotify);
  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::open) {
    MOZ_ASSERT(Open() == !!aValue);
    SetStates(ElementState::OPEN, !!aValue);
  }
}

void HTMLDialogElement::AsyncEventRunning(AsyncEventDispatcher* aEvent) {
  if (mToggleEventDispatcher == aEvent) {
    mToggleEventDispatcher = nullptr;
  }
}

void HTMLDialogElement::FocusDialog() {
  // 1) If subject is inert, return.
  // 2) Let control be the first descendant element of subject, in tree
  // order, that is not inert and has the autofocus attribute specified.
  RefPtr<Document> doc = OwnerDoc();
  if (IsInComposedDoc()) {
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  RefPtr<Element> control = HasAttr(nsGkAtoms::autofocus)
                                ? this
                                : GetFocusDelegate(IsFocusableFlags(0));

  // If there isn't one of those either, then let control be subject.
  if (!control) {
    control = this;
  }

  FocusCandidate(control, IsInTopLayer());
}

int32_t HTMLDialogElement::TabIndexDefault() { return 0; }

void HTMLDialogElement::QueueCancelDialog() {
  // queues an element task on the user interaction task source
  OwnerDoc()->Dispatch(
      NewRunnableMethod("HTMLDialogElement::RunCancelDialogSteps", this,
                        &HTMLDialogElement::RunCancelDialogSteps));
}

void HTMLDialogElement::RunCancelDialogSteps() {
  // 1) Let close be the result of firing an event named cancel at dialog, with
  // the cancelable attribute initialized to true.
  bool defaultAction = true;
  nsContentUtils::DispatchTrustedEvent(OwnerDoc(), this, u"cancel"_ns,
                                       CanBubble::eNo, Cancelable::eYes,
                                       &defaultAction);

  // 2) If close is true and dialog has an open attribute, then close the dialog
  // with no return value.
  if (defaultAction) {
    Optional<nsAString> retValue;
    Close(retValue);
  }
}

bool HTMLDialogElement::IsValidInvokeAction(InvokeAction aAction) const {
  return nsGenericHTMLElement::IsValidInvokeAction(aAction) ||
         aAction == InvokeAction::ShowModal || aAction == InvokeAction::Close;
}

bool HTMLDialogElement::HandleInvokeInternal(Element* aInvoker,
                                             InvokeAction aAction,
                                             ErrorResult& aRv) {
  if (nsGenericHTMLElement::HandleInvokeInternal(aInvoker, aAction, aRv)) {
    return true;
  }

  MOZ_ASSERT(IsValidInvokeAction(aAction));

  const bool actionMayClose =
      aAction == InvokeAction::Auto || aAction == InvokeAction::Close;
  const bool actionMayOpen =
      aAction == InvokeAction::Auto || aAction == InvokeAction::ShowModal;

  if (actionMayClose && Open()) {
    Optional<nsAString> retValue;
    Close(retValue);
    return true;
  }

  if (IsInComposedDoc() && !Open() && actionMayOpen) {
    ShowModal(aRv);
    return true;
  }

  return false;
}

void HTMLDialogElement::QueueToggleEventTask() {
  nsAutoString oldState;
  auto newState = Open() ? u"closed"_ns : u"open"_ns;
  if (mToggleEventDispatcher) {
    oldState.Truncate();
    static_cast<ToggleEvent*>(mToggleEventDispatcher->mEvent.get())
        ->GetOldState(oldState);
    mToggleEventDispatcher->Cancel();
  } else {
    oldState.Assign(Open() ? u"open"_ns : u"closed"_ns);
  }
  RefPtr<ToggleEvent> toggleEvent =
      CreateToggleEvent(u"toggle"_ns, oldState, newState, Cancelable::eNo);
  mToggleEventDispatcher = new AsyncEventDispatcher(this, toggleEvent.forget());
  mToggleEventDispatcher->PostDOMEvent();
}

JSObject* HTMLDialogElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLDialogElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
