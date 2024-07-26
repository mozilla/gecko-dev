/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NetworkDecodedBodySizeMap {
  #channelIdToBodySizePromiseMap;

  constructor() {
    this.#channelIdToBodySizePromiseMap = new Map();
  }

  destroy() {
    this.#channelIdToBodySizePromiseMap = null;
  }

  async getDecodedBodySize(channelId) {
    if (!this.#channelIdToBodySizePromiseMap.has(channelId)) {
      const { promise, resolve } = Promise.withResolvers();
      this.#channelIdToBodySizePromiseMap.set(channelId, {
        promise,
        resolve,
      });

      await promise;
    }
    const mapEntry = this.#channelIdToBodySizePromiseMap.get(channelId);
    return mapEntry.decodedBodySize;
  }

  setDecodedBodySize(channelId, decodedBodySize) {
    if (!this.#channelIdToBodySizePromiseMap.has(channelId)) {
      const { promise, resolve } = Promise.withResolvers();
      this.#channelIdToBodySizePromiseMap.set(channelId, {
        decodedBodySize,
        promise,
        resolve,
      });
    }
    const mapEntry = this.#channelIdToBodySizePromiseMap.get(channelId);

    mapEntry.decodedBodySize = decodedBodySize;
    mapEntry.resolve(decodedBodySize);
  }

  delete(channelId) {
    this.#channelIdToBodySizePromiseMap.delete(channelId);
  }
}
