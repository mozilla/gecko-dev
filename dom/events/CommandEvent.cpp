/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CommandEvent.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/MiscEvents.h"
#include "nsContentUtils.h"
#include "prtime.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(CommandEvent)

NS_IMPL_ADDREF_INHERITED(CommandEvent, Event)
NS_IMPL_RELEASE_INHERITED(CommandEvent, Event)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CommandEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(CommandEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CommandEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CommandEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

bool CommandEvent::IsCallerChromeOrCommandForEnabled(JSContext* aCx,
                                                     JSObject* aGlobal) {
  return nsContentUtils::ThreadsafeIsSystemCaller(aCx) ||
         StaticPrefs::dom_element_commandfor_enabled();
}

already_AddRefed<CommandEvent> CommandEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const CommandEventInit& aEventInitDict) {
  nsCOMPtr<mozilla::dom::EventTarget> owner =
      do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict);
}

already_AddRefed<CommandEvent> CommandEvent::Constructor(
    mozilla::dom::EventTarget* aOwner, const nsAString& aType,
    const CommandEventInit& aEventInitDict) {
  RefPtr<CommandEvent> e = new CommandEvent(aOwner, nullptr, nullptr);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->mEvent->AsCommandEvent()->mCommand = NS_Atomize(aEventInitDict.mCommand);
  e->mSource = aEventInitDict.mSource;
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

CommandEvent::CommandEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                           WidgetCommandEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent ? aEvent : new WidgetCommandEvent()) {
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
  }
}

void CommandEvent::GetCommand(nsAString& aCommand) const {
  nsAtom* command = mEvent->AsCommandEvent()->mCommand;
  if (command) {
    command->ToString(aCommand);
  } else {
    aCommand.Truncate();
  }
}

Element* CommandEvent::GetSource() {
  EventTarget* currentTarget = GetCurrentTarget();
  if (currentTarget) {
    nsINode* currentTargetNode = currentTarget->GetAsNode();
    if (!currentTargetNode) {
      return nullptr;
    }
    nsINode* retargeted = nsContentUtils::Retarget(
        static_cast<nsINode*>(mSource), currentTargetNode);
    return retargeted ? retargeted->AsElement() : nullptr;
  }
  MOZ_ASSERT(!mEvent->mFlags.mIsBeingDispatched);
  return mSource;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<CommandEvent> NS_NewDOMCommandEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetCommandEvent* aEvent) {
  RefPtr<CommandEvent> it = new CommandEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
