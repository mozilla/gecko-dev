/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorker.h"

#include "nsPIDOMWindow.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/SharedWorkerBinding.h"
#include "nsContentUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIDOMEvent.h"

#include "MessagePort.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"

using mozilla::dom::Optional;
using mozilla::dom::Sequence;
using namespace mozilla;

USING_WORKERS_NAMESPACE

SharedWorker::SharedWorker(nsPIDOMWindow* aWindow,
                           WorkerPrivate* aWorkerPrivate)
: DOMEventTargetHelper(aWindow), mWorkerPrivate(aWorkerPrivate),
  mFrozen(false)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWorkerPrivate);

  mSerial = aWorkerPrivate->NextMessagePortSerial();

  mMessagePort = new MessagePort(aWindow, this, mSerial);
}

SharedWorker::~SharedWorker()
{
  AssertIsOnMainThread();
  Close();
  MOZ_ASSERT(!mWorkerPrivate);
}

// static
already_AddRefed<SharedWorker>
SharedWorker::Constructor(const GlobalObject& aGlobal, JSContext* aCx,
                          const nsAString& aScriptURL,
                          const mozilla::dom::Optional<nsAString>& aName,
                          ErrorResult& aRv)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetOrCreateService();
  if (!rts) {
    aRv = NS_ERROR_NOT_AVAILABLE;
    return nullptr;
  }

  nsCString name;
  if (aName.WasPassed()) {
    name = NS_ConvertUTF16toUTF8(aName.Value());
  }

  nsRefPtr<SharedWorker> sharedWorker;
  nsresult rv = rts->CreateSharedWorker(aGlobal, aScriptURL, name,
                                        getter_AddRefs(sharedWorker));
  if (NS_FAILED(rv)) {
    aRv = rv;
    return nullptr;
  }

  return sharedWorker.forget();
}

already_AddRefed<mozilla::dom::workers::MessagePort>
SharedWorker::Port()
{
  AssertIsOnMainThread();

  nsRefPtr<MessagePort> messagePort = mMessagePort;
  return messagePort.forget();
}

void
SharedWorker::Freeze()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsFrozen());

  mFrozen = true;
}

void
SharedWorker::Thaw()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsFrozen());

  mFrozen = false;

  if (!mFrozenEvents.IsEmpty()) {
    nsTArray<nsCOMPtr<nsIDOMEvent>> events;
    mFrozenEvents.SwapElements(events);

    for (uint32_t index = 0; index < events.Length(); index++) {
      nsCOMPtr<nsIDOMEvent>& event = events[index];
      MOZ_ASSERT(event);

      nsCOMPtr<nsIDOMEventTarget> target;
      if (NS_SUCCEEDED(event->GetTarget(getter_AddRefs(target)))) {
        bool ignored;
        if (NS_FAILED(target->DispatchEvent(event, &ignored))) {
          NS_WARNING("Failed to dispatch event!");
        }
      } else {
        NS_WARNING("Failed to get target!");
      }
    }
  }
}

void
SharedWorker::QueueEvent(nsIDOMEvent* aEvent)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(IsFrozen());

  mFrozenEvents.AppendElement(aEvent);
}

void
SharedWorker::Close()
{
  AssertIsOnMainThread();

  if (mMessagePort) {
    mMessagePort->Close();
  }

  if (mWorkerPrivate) {
    AutoSafeJSContext cx;
    NoteDeadWorker(cx);
  }
}

void
SharedWorker::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                          const Optional<Sequence<JS::Value>>& aTransferable,
                          ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWorkerPrivate);
  MOZ_ASSERT(mMessagePort);

  mWorkerPrivate->PostMessageToMessagePort(aCx, mMessagePort->Serial(),
                                           aMessage, aTransferable, aRv);
}

void
SharedWorker::NoteDeadWorker(JSContext* aCx)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWorkerPrivate);

  mWorkerPrivate->UnregisterSharedWorker(aCx, this);
  mWorkerPrivate = nullptr;
}

NS_IMPL_ADDREF_INHERITED(SharedWorker, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SharedWorker, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SharedWorker)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(SharedWorker)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SharedWorker,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrozenEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(SharedWorker,
                                                DOMEventTargetHelper)
  tmp->Close();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessagePort)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFrozenEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
SharedWorker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnMainThread();

  return SharedWorkerBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
SharedWorker::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnMainThread();

  nsIDOMEvent*& event = aVisitor.mDOMEvent;

  if (IsFrozen() && event) {
    QueueEvent(event);

    aVisitor.mCanHandle = false;
    aVisitor.mParentTarget = nullptr;
    return NS_OK;
  }

  return DOMEventTargetHelper::PreHandleEvent(aVisitor);
}
