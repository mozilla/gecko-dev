/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

callback WorkerTestCallback = undefined ();

[Exposed=Worker, Pref="dom.workers.testing.enabled"]
namespace WorkerTestUtils {
  [Throws]
  unsigned long currentTimerNestingLevel();
  [Throws]
  boolean IsRunningInBackground();

  // Mint a (ThreadSafe)StrongWorkerRef and hold it until we see an observer
  // notification with the given string topic on the main thread.  The
  // StrongWorkerRef will prevent the worker from moving from the Canceling to
  // the Killing state and thereby prevent it from shutting down.
  [Throws]
  undefined holdStrongWorkerRefUntilMainThreadObserverNotified(UTF8String topic);

  // Create a monitor that blocks the worker entirely until we see an observer
  // notification with the given string topic on the main thread.  This is a
  // much more drastic variant of
  // holdStrongWorkerRefUntilMainThreadObserverNotified for when the goal is to
  // avoid letting WorkerRefs be notified.  A callback should be passed to be
  // synchronously invoked once the observer has been registered in order to
  // communicate that the test can move forward.
  [Throws]
  undefined blockUntilMainThreadObserverNotified(UTF8String topic,
                                                 WorkerTestCallback whenObserving);

  // Dispatch a runnable to the main thread to notify the observer service with
  // the given topic.  This is primarily intended for use with
  // `holdStrongWorkerRefUntilMainThreadObserverNotified` but might be useful in
  // other situations to cut down on test boilerplate.
  [Throws]
  undefined notifyObserverOnMainThread(UTF8String topic);
};
