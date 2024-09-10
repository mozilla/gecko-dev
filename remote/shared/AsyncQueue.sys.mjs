/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Manages a queue of asynchronous tasks, ensuring they are processed sequentially.
 */
export class AsyncQueue {
  #processing;
  #queue;

  constructor() {
    this.#queue = [];
    this.#processing = false;
  }

  /**
   * Dequeue a task.
   *
   * @returns {Promise}
   *     The wrapped task appearing as first item in the queue.
   */
  #dequeue() {
    return this.#queue.shift();
  }

  /**
   * Dequeue and try to process all the queued tasks.
   *
   * @returns {Promise<undefined>}
   *     Promise that resolves when processing the queue is done.
   */
  async #processQueue() {
    // The queue is already processed or no tasks queued up.
    if (this.#processing || this.#queue.length === 0) {
      return;
    }

    this.#processing = true;

    while (this.#queue.length) {
      const wrappedTask = this.#dequeue();
      await wrappedTask();
    }

    this.#processing = false;
  }

  /**
   * Enqueue a task.
   *
   * @param {Function} task
   *     The task to queue.
   *
   * @returns {Promise<object>}
   *     Promise that resolves when the task is completed, with the resolved
   *     value being the result of the task.
   */
  enqueue(task) {
    const onTaskExecuted = new Promise((resolve, reject) => {
      // Wrap the task in a function that will resolve or reject the Promise.
      const wrappedTask = async () => {
        try {
          const result = await task();
          resolve(result);
        } catch (error) {
          reject(error);
        }
      };

      // Add the wrapped task to the queue
      this.#queue.push(wrappedTask);
      this.#processQueue();
    });

    return onTaskExecuted;
  }
}
