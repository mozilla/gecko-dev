// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/waitable_event_watcher.h"

#include "base/condition_variable.h"
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/waitable_event.h"

#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

namespace base {

// -----------------------------------------------------------------------------
// WaitableEventWatcher (async waits).
//
// The basic design is that we add an AsyncWaiter to the wait-list of the event.
// That AsyncWaiter has a pointer to MessageLoop, and a Task to be posted to it.
// The MessageLoop ends up running the task, which calls the delegate.
//
// Since the wait can be canceled, we have a thread-safe Flag object which is
// set when the wait has been canceled. At each stage in the above, we check the
// flag before going onto the next stage. Since the wait may only be canceled in
// the MessageLoop which runs the Task, we are assured that the delegate cannot
// be called after canceling...

// -----------------------------------------------------------------------------
// A thread-safe, reference-counted, write-once flag.
// -----------------------------------------------------------------------------
class Flag final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Flag)
  Flag() { flag_ = false; }

  void Set() {
    AutoLock locked(lock_);
    flag_ = true;
  }

  bool value() const {
    AutoLock locked(lock_);
    return flag_;
  }

 protected:
  ~Flag() {}
 private:
  mutable Lock lock_;
  bool flag_;
};

// -----------------------------------------------------------------------------
// This is an asynchronous waiter which posts a task to a MessageLoop when
// fired. An AsyncWaiter may only be in a single wait-list.
// -----------------------------------------------------------------------------
class AsyncWaiter final : public WaitableEvent::Waiter {
 public:
  AsyncWaiter(MessageLoop* message_loop, Task* task, Flag* flag)
      : message_loop_(message_loop),
        cb_task_(task),
        flag_(flag) { }

  bool Fire(WaitableEvent* event) {
    if (flag_->value()) {
      // If the callback has been canceled, we don't enqueue the task, we just
      // delete it instead.
      delete cb_task_;
    } else {
      message_loop_->PostTask(FROM_HERE, cb_task_);
    }

    // We are removed from the wait-list by the WaitableEvent itself. It only
    // remains to delete ourselves.
    delete this;

    // We can always return true because an AsyncWaiter is never in two
    // different wait-lists at the same time.
    return true;
  }

  // See StopWatching for discussion
  bool Compare(void* tag) {
    return tag == flag_.get();
  }

 private:
  MessageLoop *const message_loop_;
  Task *const cb_task_;
  nsRefPtr<Flag> flag_;
};

// -----------------------------------------------------------------------------
// For async waits we need to make a callback in a MessageLoop thread. We do
// this by posting this task, which calls the delegate and keeps track of when
// the event is canceled.
// -----------------------------------------------------------------------------
class AsyncCallbackTask : public Task {
 public:
  AsyncCallbackTask(Flag* flag, WaitableEventWatcher::Delegate* delegate,
                    WaitableEvent* event)
      : flag_(flag),
        delegate_(delegate),
        event_(event) {
  }

  void Run() {
    // Runs in MessageLoop thread.
    if (!flag_->value()) {
      // This is to let the WaitableEventWatcher know that the event has occured
      // because it needs to be able to return NULL from GetWatchedObject
      flag_->Set();
      delegate_->OnWaitableEventSignaled(event_);
    }

    // We are deleted by the MessageLoop
  }

 private:
  nsRefPtr<Flag> flag_;
  WaitableEventWatcher::Delegate *const delegate_;
  WaitableEvent *const event_;
};

WaitableEventWatcher::WaitableEventWatcher()
    : event_(NULL),
      message_loop_(NULL),
      cancel_flag_(NULL),
      callback_task_(NULL) {
}

WaitableEventWatcher::~WaitableEventWatcher() {
  StopWatching();
}

// -----------------------------------------------------------------------------
// The Handle is how the user cancels a wait. After deleting the Handle we
// insure that the delegate cannot be called.
// -----------------------------------------------------------------------------
bool WaitableEventWatcher::StartWatching
    (WaitableEvent* event, WaitableEventWatcher::Delegate* delegate) {
  MessageLoop *const current_ml = MessageLoop::current();
  DCHECK(current_ml) << "Cannot create WaitableEventWatcher without a "
                        "current MessageLoop";

  // A user may call StartWatching from within the callback function. In this
  // case, we won't know that we have finished watching, expect that the Flag
  // will have been set in AsyncCallbackTask::Run()
  if (cancel_flag_.get() && cancel_flag_->value()) {
    if (message_loop_) {
      message_loop_->RemoveDestructionObserver(this);
      message_loop_ = NULL;
    }

    cancel_flag_ = NULL;
  }

  DCHECK(!cancel_flag_.get()) << "StartWatching called while still watching";

  cancel_flag_ = new Flag;
  callback_task_ = new AsyncCallbackTask(cancel_flag_, delegate, event);
  WaitableEvent::WaitableEventKernel* kernel = event->kernel_.get();

  AutoLock locked(kernel->lock_);

  if (kernel->signaled_) {
    if (!kernel->manual_reset_)
      kernel->signaled_ = false;

    // No hairpinning - we can't call the delegate directly here. We have to
    // enqueue a task on the MessageLoop as normal.
    current_ml->PostTask(FROM_HERE, callback_task_);
    return true;
  }

  message_loop_ = current_ml;
  current_ml->AddDestructionObserver(this);

  event_ = event;
  kernel_ = kernel;
  waiter_ = new AsyncWaiter(current_ml, callback_task_, cancel_flag_);
  event->Enqueue(waiter_);

  return true;
}

void WaitableEventWatcher::StopWatching() {
  if (message_loop_) {
    message_loop_->RemoveDestructionObserver(this);
    message_loop_ = NULL;
  }

  if (!cancel_flag_.get())  // if not currently watching...
    return;

  if (cancel_flag_->value()) {
    // In this case, the event has fired, but we haven't figured that out yet.
    // The WaitableEvent may have been deleted too.
    cancel_flag_ = NULL;
    return;
  }

  if (!kernel_.get()) {
    // We have no kernel. This means that we never enqueued a Waiter on an
    // event because the event was already signaled when StartWatching was
    // called.
    //
    // In this case, a task was enqueued on the MessageLoop and will run.
    // We set the flag in case the task hasn't yet run. The flag will stop the
    // delegate getting called. If the task has run then we have the last
    // reference to the flag and it will be deleted immedately after.
    cancel_flag_->Set();
    cancel_flag_ = NULL;
    return;
  }

  AutoLock locked(kernel_->lock_);
  // We have a lock on the kernel. No one else can signal the event while we
  // have it.

  // We have a possible ABA issue here. If Dequeue was to compare only the
  // pointer values then it's possible that the AsyncWaiter could have been
  // fired, freed and the memory reused for a different Waiter which was
  // enqueued in the same wait-list. We would think that that waiter was our
  // AsyncWaiter and remove it.
  //
  // To stop this, Dequeue also takes a tag argument which is passed to the
  // virtual Compare function before the two are considered a match. So we need
  // a tag which is good for the lifetime of this handle: the Flag. Since we
  // have a reference to the Flag, its memory cannot be reused while this object
  // still exists. So if we find a waiter with the correct pointer value, and
  // which shares a Flag pointer, we have a real match.
  if (kernel_->Dequeue(waiter_, cancel_flag_.get())) {
    // Case 2: the waiter hasn't been signaled yet; it was still on the wait
    // list. We've removed it, thus we can delete it and the task (which cannot
    // have been enqueued with the MessageLoop because the waiter was never
    // signaled)
    delete waiter_;
    delete callback_task_;
    cancel_flag_ = NULL;
    return;
  }

  // Case 3: the waiter isn't on the wait-list, thus it was signaled. It may
  // not have run yet, so we set the flag to tell it not to bother enqueuing the
  // task on the MessageLoop, but to delete it instead. The Waiter deletes
  // itself once run.
  cancel_flag_->Set();
  cancel_flag_ = NULL;

  // If the waiter has already run then the task has been enqueued. If the Task
  // hasn't yet run, the flag will stop the delegate from getting called. (This
  // is thread safe because one may only delete a Handle from the MessageLoop
  // thread.)
  //
  // If the delegate has already been called then we have nothing to do. The
  // task has been deleted by the MessageLoop.
}

WaitableEvent* WaitableEventWatcher::GetWatchedEvent() {
  if (!cancel_flag_.get())
    return NULL;

  if (cancel_flag_->value())
    return NULL;

  return event_;
}

// -----------------------------------------------------------------------------
// This is called when the MessageLoop which the callback will be run it is
// deleted. We need to cancel the callback as if we had been deleted, but we
// will still be deleted at some point in the future.
// -----------------------------------------------------------------------------
void WaitableEventWatcher::WillDestroyCurrentMessageLoop() {
  StopWatching();
}

}  // namespace base
