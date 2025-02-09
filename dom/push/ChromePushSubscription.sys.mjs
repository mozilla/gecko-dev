/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/** `ChromePushSubscription` instances are passed to all subscription callbacks. */
export class ChromePushSubscription {
  #props;

  constructor(props) {
    this.#props = props;
  }

  QueryInterface = ChromeUtils.generateQI(["nsIPushSubscription"]);

  /** The URL for sending messages to this subscription. */
  get endpoint() {
    return this.#props.endpoint;
  }

  /** The last time a message was sent to this subscription. */
  get lastPush() {
    return this.#props.lastPush;
  }

  /** The total number of messages sent to this subscription. */
  get pushCount() {
    return this.#props.pushCount;
  }

  /** The number of remaining background messages that can be sent to this
   * subscription, or -1 of the subscription is exempt from the quota.
   */
  get quota() {
    return this.#props.quota;
  }

  /**
   * Indicates whether this subscription was created with the system principal.
   * System subscriptions are exempt from the background message quota and
   * permission checks.
   */
  get isSystemSubscription() {
    return !!this.#props.systemRecord;
  }

  /** The private key used to decrypt incoming push messages, in JWK format */
  get p256dhPrivateKey() {
    return this.#props.p256dhPrivateKey;
  }

  /**
   * Indicates whether this subscription is subject to the background message
   * quota.
   */
  quotaApplies() {
    return this.quota >= 0;
  }

  /**
   * Indicates whether this subscription exceeded the background message quota,
   * or the user revoked the notification permission. The caller must request a
   * new subscription to continue receiving push messages.
   */
  isExpired() {
    return this.quota === 0;
  }

  /**
   * Returns a key for encrypting messages sent to this subscription. JS
   * callers receive the key buffer as a return value, while C++ callers
   * receive the key size and buffer as out parameters.
   */
  getKey(name) {
    switch (name) {
      case "p256dh":
        return this.#getRawKey(this.#props.p256dhKey);

      case "auth":
        return this.#getRawKey(this.#props.authenticationSecret);

      case "appServer":
        return this.#getRawKey(this.#props.appServerKey);
    }
    return [];
  }

  #getRawKey(key) {
    if (!key) {
      return [];
    }
    return new Uint8Array(key);
  }
}
