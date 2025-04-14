/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This map will return the default value of 0 if the decoded body size for
 * a given channel was not received yet.
 *
 * Bug 1959614: The NetworkDecodedBodySizeMap used to setup promises to wait for
 * the decoded body size to be ready. However the timing differences from this
 * change leads to regressions in Playwright tests. The new implementation of
 * the map is only synchronous and if the size was not received yet, 0 will be
 * returned. In theory, the decoded body size should be set via
 * http-on-before-stop-request which should be received before the request is
 * stopped.
 */
export class NetworkDecodedBodySizeMap {
  #authenticationAttemptsMap;
  #channelIdToBodySizeMap;

  constructor() {
    this.#authenticationAttemptsMap = new Map();
    this.#channelIdToBodySizeMap = new Map();
  }

  destroy() {
    this.#authenticationAttemptsMap = null;
    this.#channelIdToBodySizeMap = null;
  }

  getDecodedBodySize(channelId) {
    const decodedBodySize = this.#channelIdToBodySizeMap.get(channelId);
    return typeof decodedBodySize === "number" ? decodedBodySize : 0;
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
    if (previousChannelId === nextChannelId) {
      // If the preference network.auth.use_redirect_for_retries is set to false
      // all channels in the authentication chain will share the same channelId.
      // In this case there is no need to set anything in the map, the content
      // size will be set for the correct channelId by design.
      return;
    }
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

    this.#channelIdToBodySizeMap.set(channelId, decodedBodySize);
  }

  delete(channelId) {
    this.#authenticationAttemptsMap.delete(channelId);
    this.#channelIdToBodySizeMap.delete(channelId);
  }
}
