/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_messageport_h_
#define mozilla_dom_workers_messageport_h_

#include "mozilla/dom/workers/Workers.h"

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/MessagePort.h"

class nsIDOMEvent;
class nsPIDOMWindow;

namespace mozilla {
class EventChainPreVisitor;
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class SharedWorker;
class WorkerPrivate;

class MessagePort final : public mozilla::dom::MessagePortBase
{
  friend class SharedWorker;
  friend class WorkerPrivate;

  typedef mozilla::ErrorResult ErrorResult;

  nsRefPtr<SharedWorker> mSharedWorker;
  WorkerPrivate* mWorkerPrivate;
  nsTArray<nsCOMPtr<nsIDOMEvent>> mQueuedEvents;
  uint64_t mSerial;
  bool mStarted;

public:
  static bool
  PrefEnabled();

  virtual void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv) override;

  virtual void
  Start() override;

  virtual void
  Close() override;

  uint64_t
  Serial() const
  {
    return mSerial;
  }

  void
  QueueEvent(nsIDOMEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MessagePort, DOMEventTargetHelper)

  virtual EventHandlerNonNull*
  GetOnmessage() override;

  virtual void
  SetOnmessage(EventHandlerNonNull* aCallback) override;

  virtual bool
  CloneAndDisentangle(MessagePortIdentifier& aIdentifier) override;

  bool
  IsClosed() const
  {
    return !mSharedWorker && !mWorkerPrivate;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual nsresult
  PreHandleEvent(EventChainPreVisitor& aVisitor) override;

#ifdef DEBUG
  void
  AssertCorrectThread() const;
#else
  void
  AssertCorrectThread() const { }
#endif

private:
  // This class can only be created by SharedWorker or WorkerPrivate.
  MessagePort(nsPIDOMWindow* aWindow, SharedWorker* aSharedWorker,
              uint64_t aSerial);
  MessagePort(WorkerPrivate* aWorkerPrivate, uint64_t aSerial);

  // This class is reference-counted and will be destroyed from Release().
  ~MessagePort();

  void
  CloseInternal();
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_messageport_h_
