/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file provides task queue functionality, with the following features:
 *
 * - Parallel reads + sequential writes. Running tasks as fast as possible.
 *
 * - Automatic lifetime management: When an extension is unloaded, the task
 *   queue is automatically released when the last task finishes.
 *
 * - Supports extension-specific operations without requiring the extension to
 *   be running: helpers take extensionId instead of Extension.
 *
 * See the ExtensionTaskScheduler class for documentation on usage.
 */

// Internal helper to wrap a callback, so that the queue manager
// (ReadWriteTaskQueue / ExtensionBoundTaskQueue) can call it now or later,
// and still get the return value immediately or later.
class Task {
  constructor(queue, callback, readOnly) {
    this.queue = queue;

    // callback is initially a Function, but null when it is called.
    this.callback = callback;
    this.readOnly = readOnly;

    this.returnThrow = false;
    this.returnValue = undefined;

    // If the task is deferred until later, then we create a Promise and
    // resolve it as soon as the callback returns/settles.
    this.promiseResolvers = undefined;
  }

  hasRun() {
    return this.callback === null;
  }

  runTask() {
    const callback = this.callback;
    // Clear callback so that runNextTasks() won't call runTask() again.
    this.callback = null;

    let returnedPromise = false;
    try {
      this.returnValue = callback();
      // Duck-typing a Promise:
      returnedPromise = typeof this.returnValue?.finally === "function";
    } catch (e) {
      this.returnThrow = true;
      this.returnValue = e;
    }

    if (returnedPromise) {
      // returnValue may be a rejected Promise. When we chain .finally() to it,
      // a new Promise is created, and we need to chain another .catch() to it
      // to avoid an uncaughtrejection error.
      this.returnValue
        .finally(() => this.queue._finishTask(this))
        .catch(() => {});
    } else {
      this.queue._finishTask(this);
    }
    if (this.promiseResolvers) {
      // getReturnValueOrPromise() was called before and waiting for a result.
      if (this.returnThrow) {
        this.promiseResolvers.reject(this.returnValue);
      } else {
        this.promiseResolvers.resolve(this.returnValue);
      }
    }
    // Otherwise getReturnValueOrPromise() was not called yet, but when the
    // caller calls it, it will return the result that we have.
  }

  // Called once, when the task creator is interested in the return value.
  getReturnValueOrPromise() {
    if (this.hasRun()) {
      if (this.returnThrow) {
        throw this.returnValue;
      }
      return this.returnValue;
    }
    // Callback will be called later.
    this.promiseResolvers = Promise.withResolvers();
    return this.promiseResolvers.promise;
  }
}

// Base class offering a queue that supports parallel "read" tasks, but allows
// only one "write" task at a time.
class ReadWriteTaskQueue {
  #tasks = [];
  #isSchedulingTasks = false;

  hasPendingTasks() {
    return !!this.#tasks.length;
  }

  /**
   * Tries to call callback() and return its return value. The call may be
   * delayed if there is a blocking write task, in which case a Promise will
   * be returned that resolves when the callback is ultimately called.
   * Multiple read operations can run in parallel.
   */
  runReadTask(callback) {
    return this.#runNewTask(callback, /* readOnly */ true);
  }

  /**
   * Tries to call callback() and return its return value. The call may be
   * delayed if there is any other pending task, in which case a Promise will
   * be returned that resolves when the callback is ultimately called. A write
   * task prevents other tasks from running until the callback finishes.
   */
  runWriteTask(callback) {
    return this.#runNewTask(callback, /* readOnly */ false);
  }

  // Implementation for runReadTask/runWriteTask - see their JSDoc.
  #runNewTask(callback, readOnly) {
    let task = new Task(this, callback, readOnly);
    this.#tasks.push(task);
    // If there are no other blocking tasks, we could run the task now.
    this.#runNextTasks();
    return task.getReturnValueOrPromise();
  }

  // Called by Task when the task from #runNewTask() completes.
  _finishTask(task) {
    let i = this.#tasks.indexOf(task);
    if (i !== -1) {
      this.#tasks.splice(i, 1);
      this.#runNextTasks();
    }
  }

  #runNextTasks() {
    if (this.#isSchedulingTasks) {
      return;
    }
    this.#isSchedulingTasks = true;
    const tasksToRun = [];
    for (let task of this.#tasks) {
      // Run every read-only task, unless we run into a write task.
      if (!task.readOnly) {
        // Write-task can only run if there are no other pending tasks in front.
        if (!task.hasRun() && this.#tasks[0] === task) {
          tasksToRun.push(task);
        }
        // A write task must complete before other tasks can run.
        break;
      }
      if (!task.hasRun()) {
        // This is a read task, can run in parallel with other read tasks.
        tasksToRun.push(task);
      }
    }
    for (let task of tasksToRun) {
      task.runTask();
    }
    this.#isSchedulingTasks = false;
    if (!this.#tasks.length) {
      this._onQueueEmpty();
    } else if (tasksToRun.length) {
      // If we ran a write task that blocked the queue, and the write task
      // returned synchronously, we can now check the rest of the queue.
      // Or if we were running a read task and it scheduled another read task,
      // we can immediately run that new read task.
      this.#runNextTasks();
    }
  }

  _onQueueEmpty() {
    // Can be overridden by subclass.
  }
}

/**
 * A task queue that lives at least as long as an associated Extension. The
 * queue is released when the queue is empty AND extension has unloaded. The
 * queue may be rebound to a new Extension instance if there was a pending task
 * that continued while the new Extension for the same extensionId started.
 */
class ExtensionBoundTaskQueue extends ReadWriteTaskQueue {
  #factory;
  #boundToExtension;
  constructor(factory, extensionId) {
    super();
    this.#factory = factory;
    this.extensionId = extensionId;
    factory._extensionBoundQueues.set(extensionId, this);
    this.#bindToExtension();
  }

  #destruct() {
    this.#factory._extensionBoundQueues.delete(this.extensionId);
    // The basic queuing functionality is implemented by the ReadWriteTaskQueue
    // superclass, and could in theory still work even after destruction.
    // Callers are discouraged from directly interacting with a saved
    // ExtensionBoundTaskQueue instance, because the ExtensionTaskScheduler
    // factory has forgotten about us.
    //
    // If there is ever a need to support reuse after "destruct", then we could
    // store this instance as a WeakRef, and dereference that WeakRef before
    // creating a new instance in ExtensionTaskScheduler.forExtensionId. This
    // would guarantee that there cannot be two ExtensionBoundTaskQueue
    // instances for a given extensionId + ExtensionTaskScheduler combination.
  }

  #bindToExtension() {
    if (!this.#boundToExtension) {
      let extension = WebExtensionPolicy.getByID(this.extensionId)?.extension;
      if (extension && !extension.hasShutdown) {
        this.#boundToExtension = extension;
        extension.callOnClose(this);
      }
    }
  }

  close() {
    // Forget the extension. Note that if the extension loaded again while a
    // task is running, the queue may be re-used for that new Extension!
    this.#boundToExtension = null;

    if (!this.hasPendingTasks()) {
      this.#destruct();
    }
  }

  // Overrides ReadWriteTaskQueue's _onQueueEmpty.
  _onQueueEmpty() {
    this.#bindToExtension();
    if (!this.#boundToExtension) {
      this.#destruct();
    }
  }
}

/**
 * This helper maintains independent extension-specific task queues, and
 * enables consumers to schedule tasks to run as soon as possible. Tasks are
 * executed in the order of scheduling. Read tasks can be run in parallel, but
 * write tasks can only run sequentially. As soon as a write task is scheduled,
 * no new tasks (not even read tasks) will be scheduled until the write task
 * completes.
 *
 * Task completion means that the callback() was invoked and that if the return
 * value was a Promise, that the Promise has settled. Errors in task execution
 * do not interrupt the execution of other tasks. If a task can run immediately
 * and returns a non-Promise value, then that value is returned.
 */
export class ExtensionTaskScheduler {
  _extensionBoundQueues = new Map();

  /**
   * Get the internal ExtensionBoundTaskQueue. This is mainly exposed for
   * testing purposes. When saving the instance for later use, make sure that
   * the extension is still running before using it.
   */
  forExtensionId(extensionId) {
    return (
      this._extensionBoundQueues.get(extensionId) ||
      new ExtensionBoundTaskQueue(this, extensionId)
    );
  }

  /**
   * Tries to call callback() and return its return value. The call may be
   * delayed if there is a blocking write task, in which case a Promise will
   * be returned that resolves when the callback is ultimately called.
   * Multiple read operations can run in parallel.
   */
  runReadTask(extensionId, callback) {
    return this.forExtensionId(extensionId).runReadTask(callback);
  }

  /**
   * Tries to call callback() and return its return value. The call may be
   * delayed if there is any other pending task, in which case a Promise will
   * be returned that resolves when the callback is ultimately called. A write
   * task prevents other tasks from running until the callback finishes.
   */
  runWriteTask(extensionId, callback) {
    return this.forExtensionId(extensionId).runWriteTask(callback);
  }
}
