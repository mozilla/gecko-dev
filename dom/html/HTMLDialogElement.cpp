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

static constexpr nsAttrValue::EnumTableEntry kClosedbyTable[] = {
    {"", HTMLDialogElement::ClosedBy::Auto},
    {"none", HTMLDialogElement::ClosedBy::None},
    {"any", HTMLDialogElement::ClosedBy::Any},
    {"closerequest", HTMLDialogElement::ClosedBy::CloseRequest},
};

static constexpr const nsAttrValue::EnumTableEntry* kClosedbyAuto =
    &kClosedbyTable[0];
static constexpr const nsAttrValue::EnumTableEntry* kClosedbyDefault =
    &kClosedbyTable[1];
static constexpr const nsAttrValue::EnumTableEntry* kClosedbyModalDefault =
    &kClosedbyTable[3];

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
        retValue = &dialog->RequestCloseReturnValue();
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

// https://html.spec.whatwg.org/#computed-closed-by-state
void HTMLDialogElement::GetClosedBy(nsAString& aResult) const {
  aResult.Truncate();
  MOZ_ASSERT(StaticPrefs::dom_dialog_light_dismiss_enabled());
  const nsAttrValue* val = mAttrs.GetAttr(nsGkAtoms::closedby);
  // 1. If the state of dialog's closedby attribute is Auto:
  if (!val || val->GetEnumValue() == kClosedbyAuto->value) {
    //  1.1. If dialog's is modal is true, then return Close Request.
    //  1.2. Return None.
    const char* tag =
        (IsInTopLayer() ? kClosedbyModalDefault->tag : kClosedbyDefault->tag);
    AppendASCIItoUTF16(nsDependentCString(tag), aResult);
    return;
  }
  // 2. Return the state of dialog's closedby attribute.
  val->GetEnumString(aResult, true);
}

// https://html.spec.whatwg.org/#computed-closed-by-state
HTMLDialogElement::ClosedBy HTMLDialogElement::GetClosedBy() const {
  if (!StaticPrefs::dom_dialog_light_dismiss_enabled()) {
    return static_cast<ClosedBy>(IsInTopLayer() ? kClosedbyModalDefault->value
                                                : kClosedbyDefault->value);
  }
  const nsAttrValue* val = mAttrs.GetAttr(nsGkAtoms::closedby);
  // 1. If the state of dialog's closedby attribute is Auto:
  if (!val || val->GetEnumValue() == kClosedbyAuto->value) {
    //  1.1. If dialog's is modal is true, then return Close Request.
    //  1.2. Return None.
    return static_cast<ClosedBy>(IsInTopLayer() ? kClosedbyModalDefault->value
                                                : kClosedbyDefault->value);
  }
  // 2. Return the state of dialog's closedby attribute.
  return static_cast<ClosedBy>(val->GetEnumValue());
}

bool HTMLDialogElement::ParseClosedByAttribute(const nsAString& aValue,
                                               nsAttrValue& aResult) {
  return aResult.ParseEnumValue(aValue, kClosedbyTable,
                                /* aCaseSensitive = */ false, kClosedbyAuto);
}

bool HTMLDialogElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (StaticPrefs::dom_dialog_light_dismiss_enabled() &&
        aAttribute == nsGkAtoms::closedby) {
      return ParseClosedByAttribute(aValue, aResult);
    }
  }
  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

void HTMLDialogElement::Close(
    const mozilla::dom::Optional<nsAString>& aReturnValue) {
  if (!Open()) {
    return;
  }

  FireToggleEvent(u"open"_ns, u"closed"_ns, u"beforetoggle"_ns);
  if (!Open()) {
    return;
  }
  QueueToggleEventTask();

  if (aReturnValue.WasPassed()) {
    SetReturnValue(aReturnValue.Value());
  }

  SetOpen(false, IgnoreErrors());

  RemoveFromTopLayerIfNeeded();

  MOZ_ASSERT(!OwnerDoc()->DialogIsInOpenDialogsList(*this),
             "Dialog should not being in Open Dialog List");

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

// https://html.spec.whatwg.org/#dom-dialog-requestclose
void HTMLDialogElement::RequestClose(
    const mozilla::dom::Optional<nsAString>& aReturnValue) {
  // 1. If this does not have an open attribute, then return.
  if (!Open()) {
    return;
  }

  // 2. Assert: this's close watcher is not null.
  // TODO(keithamus): RequestClose uses CloseWatcher's requestClose to dispatch
  // cancel & close events, but there are also several issues with the spec
  // which make it untenable to implement until they're resolved. Instead, we
  // can use `RunCancelDialogSteps` to cause the same behaviour, but when
  // https://github.com/whatwg/html/issues/11230 is resolved we will need to
  // revisit this.

  // 3. Set dialog's enable close watcher for requestClose() to true.
  // TODO(keithamus): CloseWatcher does not have this flag yet.

  // 4. If returnValue is not given, then set it to null.
  // 5. Set this's request close return value to returnValue.
  if (aReturnValue.WasPassed()) {
    mRequestCloseReturnValue = aReturnValue.Value();
  } else {
    mRequestCloseReturnValue.SetIsVoid(true);
  }

  // 6. Request to close dialog's close watcher with false.
  RunCancelDialogSteps();

  // 7. Set dialog's enable close watcher for requestClose() to false.
  // TODO(keithamus): CloseWatcher does not have this flag yet.
}

// https://html.spec.whatwg.org/#dom-dialog-show
void HTMLDialogElement::Show(ErrorResult& aError) {
  // 1. If this has an open attribute and is modal of this is false, then
  // return.
  if (Open()) {
    if (!IsInTopLayer()) {
      return;
    }

    // 2. If this has an open attribute, then throw an "InvalidStateError"
    // DOMException.
    return aError.ThrowInvalidStateError(
        "Cannot call show() on an open modal dialog.");
  }

  // 3. If the result of firing an event named beforetoggle, using ToggleEvent,
  // with the cancelable attribute initialized to true, the oldState attribute
  // initialized to "closed", and the newState attribute initialized to "open"
  // at this is false, then return.
  if (FireToggleEvent(u"closed"_ns, u"open"_ns, u"beforetoggle"_ns)) {
    return;
  }

  // 4. If this has an open attribute, then return.
  if (Open()) {
    return;
  }

  // 5. Queue a dialog toggle event task given this, "closed", and "open".
  QueueToggleEventTask();

  // 6. Add an open attribute to this, whose value is the empty string.
  SetOpen(true, IgnoreErrors());

  // 7. Assert: this's node document's open dialogs list does not contain this.
  // 8. Add this to this's node document's open dialogs list.
  // XXX: Step 7 & 8 don't really belong here; following the spec precisely
  // would cause issues. See for example:
  // https://github.com/whatwg/html/issues/11259 Instead we implement parts of
  // https://github.com/whatwg/html/pull/10954 which adds AttributeChanged steps
  // for Dialogs, to ensure dialogs with the open attribute get added to the
  // open dialogs list. See also https://github.com/whatwg/html/issues/10982

  // 9. Set the dialog close watcher with this.
  if (StaticPrefs::dom_closewatcher_enabled()) {
    SetDialogCloseWatcherIfNeeded();
  }

  // 10. Set this's previously focused element to the focused element.
  StorePreviouslyFocusedElement();

  // 11. Let document be this's node document.

  // 12. Let hideUntil be the result of running topmost popover ancestor given
  // this, document's showing hint popover list, null, and false.
  RefPtr<nsINode> hideUntil = GetTopmostPopoverAncestor(nullptr, false);

  // 13. If hideUntil is null, then set hideUntil to the result of running
  // topmost popover ancestor given this, document's showing auto popover list,
  // null, and false.
  if (!hideUntil) {
    hideUntil = OwnerDoc();
  }

  // 14. If hideUntil is null, then set hideUntil to document.
  OwnerDoc()->HideAllPopoversUntil(*hideUntil, false, true);

  // 15. Run the dialog focusing steps given this.
  FocusDialog();
}

bool HTMLDialogElement::Open() const {
  MOZ_ASSERT(GetBoolAttr(nsGkAtoms::open) ==
             State().HasState(ElementState::OPEN));
  return State().HasState(ElementState::OPEN);
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

nsresult HTMLDialogElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  MOZ_TRY(nsGenericHTMLElement::BindToTree(aContext, aParent));

  // https://whatpr.org/html/10954/interactive-elements.html#the-dialog-element:html-element-insertion-steps
  // 1. If insertedNode has an open attribute:
  if (Open()) {
    // 1.1 Run the dialog setup steps given insertedNode.
    SetupSteps();
  }

  return NS_OK;
}

void HTMLDialogElement::UnbindFromTree(UnbindContext& aContext) {
  // https://whatpr.org/html/10954/interactive-elements.html#the-dialog-element:html-element-removing-steps
  // 1. If removedNode has an open attribute:
  if (Open()) {
    // 2. Run the dialog cleanup steps given removedNode.
    CleanupSteps();
  }

  // 2. If removedNode's node document's top layer contains removedNode, then
  // remove an element from the top layer immediately given removedNode.
  RemoveFromTopLayerIfNeeded();

  // 3. Set is modal of removedNode to false.

  nsGenericHTMLElement::UnbindFromTree(aContext);
}

// https://html.spec.whatwg.org/#show-a-modal-dialog
void HTMLDialogElement::ShowModal(ErrorResult& aError) {
  // 1. If subject has an open attribute and is modal of subject is true, then
  // return.
  if (Open()) {
    if (IsInTopLayer()) {
      return;
    }

    // 2. If subject has an open attribute, then throw an "InvalidStateError"
    // DOMException.
    return aError.ThrowInvalidStateError(
        "Cannot call showModal() on an open non-modal dialog.");
  }

  // 3. If subject's node document is not fully active, then throw an
  // "InvalidStateError" DOMException.
  if (!OwnerDoc()->IsFullyActive()) {
    return aError.ThrowInvalidStateError(
        "The owner document is not fully active");
  }

  // 4. If subject is not connected, then throw an "InvalidStateError"
  // DOMException.
  if (!IsInComposedDoc()) {
    return aError.ThrowInvalidStateError("Dialog element is not connected");
  }

  // 5. If subject is in the popover showing state, then throw an
  // "InvalidStateError" DOMException.
  if (IsPopoverOpen()) {
    return aError.ThrowInvalidStateError(
        "Dialog element is already an open popover.");
  }

  // 6. If the result of firing an event named beforetoggle, using
  // ToggleEvent, with the cancelable attribute initialized to true, the
  // oldState attribute initialized to "closed", and the newState attribute
  // initialized to "open" at subject is false, then return.
  if (FireToggleEvent(u"closed"_ns, u"open"_ns, u"beforetoggle"_ns)) {
    return;
  }

  // 7. If subject has an open attribute, then return.
  // 8. If subject is not connected, then return.
  // 9. If subject is in the popover showing state, then return.
  if (Open() || !IsInComposedDoc() || IsPopoverOpen()) {
    return;
  }

  // 10. Queue a dialog toggle event task given subject, "closed", and "open".
  QueueToggleEventTask();

  // 11. Add an open attribute to subject, whose value is the empty string.
  SetOpen(true, aError);

  // 12. Set is modal of subject to true.

  // 13. Assert: subject's node document's open dialogs list does not contain
  // subject.
  // 14. Add subject to subject's node document's open dialogs list.
  // XXX: Step 7 & 8 don't really belong here; following the spec precisely
  // would cause issues. See for example:
  // https://github.com/whatwg/html/issues/11259 Instead we implement parts of
  // https://github.com/whatwg/html/pull/10954 which adds AttributeChanged steps
  // for Dialogs, to ensure dialogs with the open attribute get added to the
  // open dialogs list. See also https://github.com/whatwg/html/issues/10982

  // 15. Let subject's node document be blocked by the modal dialog subject.

  // 16. If subject's node document's top layer does not already contain
  // subject, then add an element to the top layer given subject.
  AddToTopLayerIfNeeded();

  if (StaticPrefs::dom_closewatcher_enabled()) {
    // 17. Set the dialog close watcher with subject.
    SetDialogCloseWatcherIfNeeded();
  }

  // 18. Set subject's previously focused element to the focused element.
  StorePreviouslyFocusedElement();

  // 19. Let document be subject's node document.
  // 20. Let hideUntil be the result of running topmost popover ancestor given
  // subject, document's showing hint popover list, null, and false.
  // 21. If hideUntil is null, then set hideUntil to the result of running
  // topmost popover ancestor given subject, document's showing auto popover
  // list, null, and false.
  RefPtr<nsINode> hideUntil = GetTopmostPopoverAncestor(nullptr, false);

  // 22. If hideUntil is null, then set hideUntil to document.
  if (!hideUntil) {
    hideUntil = OwnerDoc();
  }

  // 23. Run hide all popovers until given hideUntil, false, and true.
  OwnerDoc()->HideAllPopoversUntil(*hideUntil, false, true);

  // 24. Run the dialog focusing steps given subject.
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
  // XXX: https://github.com/whatwg/html/pull/10954
  // Attribute change Steps for HTMLDialogElement
  // 1. If namespace is not null, then return.
  if (aNameSpaceID != kNameSpaceID_None) {
    return;
  }

  // https://html.spec.whatwg.org/#set-the-dialog-close-watcher
  // https://github.com/whatwg/html/issues/11267
  // XXX: CloseWatcher currently uses a `getEnabledState` algorithm to set a
  // boolean, but this is quite a lot of additional infrastructure which could
  // be simplified by CloseWatcher having an "Enabled" state, which is what we
  // do. Here if closedby is added, we need to setup the close watcher if
  // it isn't set up, which in turn will also call SetCloseWatcherEnabledState.
  if (aName == nsGkAtoms::closedby) {
    if (StaticPrefs::dom_closewatcher_enabled() && IsInComposedDoc() &&
        Open()) {
      SetDialogCloseWatcherIfNeeded();
    }
  }

  // 2. If localName is not open, then return.
  if (aName != nsGkAtoms::open) {
    return;
  }

  bool wasOpen = !!aOldValue;
  bool isOpen = !!aValue;

  MOZ_ASSERT(GetBoolAttr(nsGkAtoms::open) == isOpen);
  SetStates(ElementState::OPEN, isOpen);

  // 3. If value is null, and oldValue is not null, then run the dialog
  // cleanup steps given element.
  if (!isOpen && wasOpen) {
    CleanupSteps();
  }

  // 4. If value is not null, and oldValue is null, then run the dialog
  // setup steps given element.
  if (isOpen && !wasOpen) {
    SetupSteps();
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
  // with ~~no return value.~~
  // XXX(keithamus): RequestClose's steps expect the return value to be
  // RequestCloseReturnValue. RunCancelDialogSteps has been refactored out of
  // the spec, over CloseWatcher though, so one day this code will need to be
  // refactored when the CloseWatcher specifications settle.
  if (defaultAction) {
    Optional<nsAString> retValue;
    retValue = &RequestCloseReturnValue();
    Close(retValue);
  }
}

bool HTMLDialogElement::IsValidCommandAction(Command aCommand) const {
  return nsGenericHTMLElement::IsValidCommandAction(aCommand) ||
         aCommand == Command::ShowModal || aCommand == Command::Close;
}

bool HTMLDialogElement::HandleCommandInternal(Element* aSource,
                                              Command aCommand,
                                              ErrorResult& aRv) {
  if (nsGenericHTMLElement::HandleCommandInternal(aSource, aCommand, aRv)) {
    return true;
  }

  MOZ_ASSERT(IsValidCommandAction(aCommand));

  const bool actionMayClose =
      aCommand == Command::Auto || aCommand == Command::Close;
  const bool actionMayOpen =
      aCommand == Command::Auto || aCommand == Command::ShowModal;

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

// https://html.spec.whatwg.org/#set-the-dialog-close-watcher
void HTMLDialogElement::SetDialogCloseWatcherIfNeeded() {
  MOZ_ASSERT(StaticPrefs::dom_closewatcher_enabled(), "CloseWatcher enabled");
  if (mCloseWatcher) {
    SetCloseWatcherEnabledState();
    return;
  }

  RefPtr<Document> doc = OwnerDoc();
  RefPtr window = doc->GetInnerWindow();
  // XXX: Spec does not assert that the dialog is connected to a window.
  // There are cases (document.implementation) where `window` might be
  // null. These cases should not establish a CloseWatcher.
  if (!window) {
    return;
  }

  // 1. Set dialog's close watcher to the result of establishing a close watcher
  // given dialog's relevant global object, with:
  mCloseWatcher = new CloseWatcher(window);
  RefPtr<DialogCloseWatcherListener> eventListener =
      new DialogCloseWatcherListener(this);

  // - cancelAction given canPreventClose being to return the result of firing
  // an event named cancel at dialog, with the cancelable attribute initialized
  // to canPreventClose.
  mCloseWatcher->AddSystemEventListener(u"cancel"_ns, eventListener,
                                        false /* aUseCapture */,
                                        false /* aWantsUntrusted */);

  // - closeAction being to close the dialog given dialog and dialog's request
  // close return value.
  mCloseWatcher->AddSystemEventListener(u"close"_ns, eventListener,
                                        false /* aUseCapture */,
                                        false /* aWantsUntrusted */);

  // - getEnabledState being to return true if dialog's enable close watcher for
  // requestClose() is true or dialog's computed closed-by state is not None;
  // otherwise false.
  SetCloseWatcherEnabledState();

  window->EnsureCloseWatcherManager()->Add(*mCloseWatcher);
}

// https://whatpr.org/html/10954/interactive-elements.html#dialog-setup-steps
// TODO(keithamus): revisit once https://github.com/whatwg/html/pull/10954 is
// merged.
void HTMLDialogElement::SetupSteps() {
  // 1. Set the dialog close watcher with subject.
  if (StaticPrefs::dom_closewatcher_enabled()) {
    SetDialogCloseWatcherIfNeeded();
  }

  // 2. If subject is not connected, return.
  if (!IsInComposedDoc()) {
    return;
  }

  // 3. Assert: subject's node document's open dialogs list does not contain
  // subject.
  // XXX: This is the same as https://html.spec.whatwg.org/#dom-dialog-show step
  // 7, but moved here.
  MOZ_ASSERT(!OwnerDoc()->DialogIsInOpenDialogsList(*this));

  // 4. Add subject to subject's node document's open dialogs list.
  // XXX: This is the same as https://html.spec.whatwg.org/#dom-dialog-show step
  // 8, but moved here.
  OwnerDoc()->AddOpenDialog(*this);
}

void HTMLDialogElement::SetCloseWatcherEnabledState() {
  if (StaticPrefs::dom_closewatcher_enabled()) {
    MOZ_ASSERT(mCloseWatcher);
    mCloseWatcher->SetEnabled(GetClosedBy() != ClosedBy::None);
  }
}

// https://whatpr.org/html/10954/interactive-elements.html#dialog-setup-steps
// TODO(keithamus): revisit once https://github.com/whatwg/html/pull/10954 is
// merged.
void HTMLDialogElement::CleanupSteps() {
  // 1. Remove subject from subject's node document's open dialogs list.
  OwnerDoc()->RemoveOpenDialog(*this);

  // 2. If subject's close watcher is not null, and subject does not have an
  // open attribute, then:
  if (mCloseWatcher) {
    // 3. Destroy subject's close watcher.
    mCloseWatcher->Destroy();

    // 4. Set subject's close watcher to null.
    mCloseWatcher = nullptr;
  }
}

JSObject* HTMLDialogElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLDialogElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
