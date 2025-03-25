/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NetworkDecodedBodySizeMap {
  #authenticationAttemptsMap;
  #channelIdToBodySizePromiseMap;

  constructor() {
    this.#authenticationAttemptsMap = new Map();
    this.#channelIdToBodySizePromiseMap = new Map();
  }

  destroy() {
    this.#authenticationAttemptsMap = null;
    this.#channelIdToBodySizePromiseMap = null;
  }

  async getDecodedBodySize(channelId) {
    if (!this.#channelIdToBodySizePromiseMap.has(channelId)) {
      const { promise, resolve } = Promise.withResolvers();
      this.#channelIdToBodySizePromiseMap.set(channelId, {
        promise,
        resolve,
      });
    }
    const mapEntry = this.#channelIdToBodySizePromiseMap.get(channelId);
    await mapEntry.promise;
    return mapEntry.decodedBodySize;
  }

  /**
   * For authentication attempts for the same request, keep track of the
   * authentication chain in order to resolve the final getDecodedBodySize when
   * the information is received from the content process.
   *
   * @param {number} previousChannelId
   *     The channel ID of the previous channel in the authentication chain.
   * @param {number} nextChannelId
   *     The channel ID of the new channel in the authentication chain.
   */
  setAuthenticationAttemptMapping(previousChannelId, nextChannelId) {
    this.#authenticationAttemptsMap.set(previousChannelId, nextChannelId);
  }

  /**
   * Set the decodedBodySize for the provided channelId. If the channel is part
   * of an authentication chain, the decoded body size will instead be set for
   * the current channel of the chain.
   *
   * @param {number} channelId
   *     The id of the channel to update.
   * @param {number} decodedBodySize
   *     The decoded body size for the channel.
   */
  setDecodedBodySize(channelId, decodedBodySize) {
    if (this.#authenticationAttemptsMap.has(channelId)) {
      // If there is an auth chain for this channel, setDecodedBodySize will be
      // called for the original channel ID. Traverse the authentication attempt
      // map to set the decoded body size for the current channel in the chain.
      this.setDecodedBodySize(
        this.#authenticationAttemptsMap.get(channelId),
        decodedBodySize
      );
      return;
    }

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
    this.#authenticationAttemptsMap.delete(channelId);
    this.#channelIdToBodySizePromiseMap.delete(channelId);
  }
}
