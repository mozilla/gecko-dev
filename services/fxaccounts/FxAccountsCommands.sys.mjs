/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  CLIENT_IS_THUNDERBIRD,
  COMMAND_SENDTAB,
  COMMAND_SENDTAB_TAIL,
  COMMAND_CLOSETAB,
  COMMAND_CLOSETAB_TAIL,
  SCOPE_OLD_SYNC,
  log,
} from "resource://gre/modules/FxAccountsCommon.sys.mjs";

import { clearTimeout, setTimeout } from "resource://gre/modules/Timer.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { Observers } from "resource://services-common/observers.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BulkKeyBundle: "resource://services-sync/keys.sys.mjs",
  CryptoWrapper: "resource://services-sync/record.sys.mjs",
  PushCrypto: "resource://gre/modules/PushCrypto.sys.mjs",
  getRemoteCommandStore: "resource://services-sync/TabsStore.sys.mjs",
  RemoteCommand: "resource://services-sync/TabsStore.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "INVALID_SHAREABLE_SCHEMES",
  "services.sync.engine.tabs.filteredSchemes",
  "",
  null,
  val => {
    return new Set(val.split("|"));
  }
);

const TOPIC_TABS_CHANGED = "services.sync.tabs.changed";

export class FxAccountsCommands {
  constructor(fxAccountsInternal) {
    this._fxai = fxAccountsInternal;
    this.sendTab = new SendTab(this, fxAccountsInternal);
    this.closeTab = new CloseRemoteTab(this, fxAccountsInternal);
    this._invokeRateLimitExpiry = 0;
  }

  async availableCommands() {
    let commands = {};

    if (!CLIENT_IS_THUNDERBIRD) {
      // Invalid keys usually means the account is not verified yet.
      const encryptedSendTabKeys = await this.sendTab.getEncryptedSendTabKeys();

      if (encryptedSendTabKeys) {
        commands[COMMAND_SENDTAB] = encryptedSendTabKeys;
      }

      // Close Tab is still a worked-on feature, so we should not broadcast it widely yet
      let closeTabEnabled = Services.prefs.getBoolPref(
        "identity.fxaccounts.commands.remoteTabManagement.enabled",
        false
      );
      if (closeTabEnabled) {
        const encryptedCloseTabKeys =
          await this.closeTab.getEncryptedCloseTabKeys();
        if (encryptedCloseTabKeys) {
          commands[COMMAND_CLOSETAB] = encryptedCloseTabKeys;
        }
      }
    }

    return commands;
  }

  async invoke(command, device, payload) {
    const { sessionToken } = await this._fxai.getUserAccountData([
      "sessionToken",
    ]);
    const client = this._fxai.fxAccountsClient;
    const now = Date.now();
    if (now < this._invokeRateLimitExpiry) {
      const remaining = (this._invokeRateLimitExpiry - now) / 1000;
      throw new Error(
        `Invoke for ${command} is rate-limited for ${remaining} seconds.`
      );
    }
    try {
      let info = await client.invokeCommand(
        sessionToken,
        command,
        device.id,
        payload
      );
      if (!info.enqueued || !info.notified) {
        // We want an error log here to help diagnose users who report failure.
        log.error("Sending was only partially successful", info);
      } else {
        log.info("Successfully sent", info);
      }
    } catch (err) {
      if (err.code && err.code === 429 && err.retryAfter) {
        this._invokeRateLimitExpiry = Date.now() + err.retryAfter * 1000;
      }
      throw err;
    }
    log.info(`Payload sent to device ${device.id}.`);
  }

  /**
   * Poll and handle device commands for the current device.
   * This method can be called either in response to a Push message,
   * or by itself as a "commands recovery" mechanism.
   *
   * @param {Number} notifiedIndex "Command received" push messages include
   * the index of the command that triggered the message. We use it as a
   * hint when we have no "last command index" stored.
   */
  async pollDeviceCommands(notifiedIndex = 0) {
    // Whether the call to `pollDeviceCommands` was initiated by a Push message from the FxA
    // servers in response to a message being received or simply scheduled in order
    // to fetch missed messages.
    log.info(`Polling device commands.`);
    await this._fxai.withCurrentAccountState(async state => {
      const { device } = await state.getUserAccountData(["device"]);
      if (!device) {
        throw new Error("No device registration.");
      }
      // We increment lastCommandIndex by 1 because the server response includes the current index.
      // If we don't have a `lastCommandIndex` stored, we fall back on the index from the push message we just got.
      const lastCommandIndex = device.lastCommandIndex + 1 || notifiedIndex;
      // We have already received this message before.
      if (notifiedIndex > 0 && notifiedIndex < lastCommandIndex) {
        return;
      }
      const { index, messages } = await this._fetchDeviceCommands(
        lastCommandIndex
      );
      if (messages.length) {
        await state.updateUserAccountData({
          device: { ...device, lastCommandIndex: index },
        });
        log.info(`Handling ${messages.length} messages`);
        await this._handleCommands(messages, notifiedIndex);
      }
    });
    return true;
  }

  async _fetchDeviceCommands(index, limit = null) {
    const userData = await this._fxai.getUserAccountData();
    if (!userData) {
      throw new Error("No user.");
    }
    const { sessionToken } = userData;
    if (!sessionToken) {
      throw new Error("No session token.");
    }
    const client = this._fxai.fxAccountsClient;
    const opts = { index };
    if (limit != null) {
      opts.limit = limit;
    }
    return client.getCommands(sessionToken, opts);
  }

  _getReason(notifiedIndex, messageIndex) {
    // The returned reason value represents an explanation for why the command associated with the
    // message of the given `messageIndex` is being handled. If `notifiedIndex` is zero the command
    // is a part of a fallback polling process initiated by "Sync Now" ["poll"]. If `notifiedIndex` is
    // greater than `messageIndex` this is a push command that was previously missed ["push-missed"],
    // otherwise we assume this is a push command with no missed messages ["push"].
    if (notifiedIndex == 0) {
      return "poll";
    } else if (notifiedIndex > messageIndex) {
      return "push-missed";
    }
    // Note: The returned reason might be "push" in the case where a user sends multiple tabs
    // in quick succession. We are not attempting to distinguish this from other push cases at
    // present.
    return "push";
  }

  async _handleCommands(messages, notifiedIndex) {
    try {
      await this._fxai.device.refreshDeviceList();
    } catch (e) {
      log.warn("Error refreshing device list", e);
    }
    // We debounce multiple incoming tabs so we show a single notification.
    const tabsReceived = [];
    const tabsToClose = [];
    for (const { index, data } of messages) {
      const { command, payload, sender: senderId } = data;
      const reason = this._getReason(notifiedIndex, index);
      const sender =
        senderId && this._fxai.device.recentDeviceList
          ? this._fxai.device.recentDeviceList.find(d => d.id == senderId)
          : null;
      if (!sender) {
        log.warn(
          "Incoming command is from an unknown device (maybe disconnected?)"
        );
      }
      switch (command) {
        case COMMAND_CLOSETAB:
          try {
            const { urls } = await this.closeTab.handleTabClose(
              senderId,
              payload,
              reason
            );
            log.info(
              `Close Tab received with FxA commands: "${urls.length} tabs"
               from ${sender ? sender.name : "Unknown device"}.`
            );
            // URLs are PII, so only logged at trace.
            log.trace(`Close Remote Tabs received URLs: ${urls}`);
            tabsToClose.push({ urls, sender });
          } catch (e) {
            log.error(`Error while handling incoming Close Tab payload.`, e);
          }
          break;
        case COMMAND_SENDTAB:
          try {
            const { title, uri } = await this.sendTab.handle(
              senderId,
              payload,
              reason
            );
            log.info(
              `Tab received with FxA commands: "${
                title || "<no title>"
              }" from ${sender ? sender.name : "Unknown device"}.`
            );
            // URLs are PII, so only logged at trace.
            log.trace(`Tab received URL: ${uri}`);
            // This should eventually be rare to hit as all platforms will be using the same
            // scheme filter list, but we have this here in the case other platforms
            // haven't caught up and/or trying to send invalid uris using older versions
            const scheme = Services.io.newURI(uri).scheme;
            if (lazy.INVALID_SHAREABLE_SCHEMES.has(scheme)) {
              throw new Error("Invalid scheme found for received URI.");
            }
            tabsReceived.push({ title, uri, sender });
          } catch (e) {
            log.error(`Error while handling incoming Send Tab payload.`, e);
          }
          break;
        default:
          log.info(`Unknown command: ${command}.`);
      }
    }
    if (tabsReceived.length) {
      this._notifyFxATabsReceived(tabsReceived);
    }
    if (tabsToClose.length) {
      this._notifyFxATabsClosed(tabsToClose);
    }
  }

  _notifyFxATabsReceived(tabsReceived) {
    Observers.notify("fxaccounts:commands:open-uri", tabsReceived);
  }

  _notifyFxATabsClosed(tabsToClose) {
    Observers.notify("fxaccounts:commands:close-uri", tabsToClose);
  }
}

/**
 * Send Tab is built on top of FxA commands.
 *
 * Devices exchange keys wrapped in the oldsync key between themselves (getEncryptedSendTabKeys)
 * during the device registration flow. The FxA server can theoretically never
 * retrieve the send tab keys since it doesn't know the oldsync key.
 *
 * Note about the keys:
 * The server has the `pushPublicKey`. The FxA server encrypt the send-tab payload again using the
 * push keys - after the client has encrypted the payload using the send-tab keys.
 * The push keys are different from the send-tab keys. The FxA server uses
 * the push keys to deliver the tabs using same mechanism we use for web-push.
 * However, clients use the send-tab keys for end-to-end encryption.
 */
export class SendTab {
  constructor(commands, fxAccountsInternal) {
    this._commands = commands;
    this._fxai = fxAccountsInternal;
  }
  /**
   * @param {Device[]} to - Device objects (typically returned by fxAccounts.getDevicesList()).
   * @param {Object} tab
   * @param {string} tab.url
   * @param {string} tab.title
   * @returns A report object, in the shape of
   *          {succeded: [Device], error: [{device: Device, error: Exception}]}
   */
  async send(to, tab) {
    log.info(`Sending a tab to ${to.length} devices.`);
    const flowID = this._fxai.telemetry.generateFlowID();
    const encoder = new TextEncoder();
    const data = { entries: [{ title: tab.title, url: tab.url }] };
    const report = {
      succeeded: [],
      failed: [],
    };
    for (let device of to) {
      try {
        const streamID = this._fxai.telemetry.generateFlowID();
        const targetData = Object.assign({ flowID, streamID }, data);
        const bytes = encoder.encode(JSON.stringify(targetData));
        const encrypted = await this._encrypt(bytes, device);
        // FxA expects an object as the payload, but we only have a single encrypted string; wrap it.
        // If you add any plaintext items to this payload, please carefully consider the privacy implications
        // of revealing that data to the FxA server.
        const payload = { encrypted };
        await this._commands.invoke(COMMAND_SENDTAB, device, payload);
        this._fxai.telemetry.recordEvent(
          "command-sent",
          COMMAND_SENDTAB_TAIL,
          this._fxai.telemetry.sanitizeDeviceId(device.id),
          { flowID, streamID }
        );
        report.succeeded.push(device);
      } catch (error) {
        log.error("Error while invoking a send tab command.", error);
        report.failed.push({ device, error });
      }
    }
    return report;
  }

  // Returns true if the target device is compatible with FxA Commands Send tab.
  isDeviceCompatible(device) {
    return (
      device.availableCommands && device.availableCommands[COMMAND_SENDTAB]
    );
  }

  // Handle incoming send tab payload, called by FxAccountsCommands.
  async handle(senderID, { encrypted }, reason) {
    const bytes = await this._decrypt(encrypted);
    const decoder = new TextDecoder("utf8");
    const data = JSON.parse(decoder.decode(bytes));
    const { flowID, streamID, entries } = data;
    const current = data.hasOwnProperty("current")
      ? data.current
      : entries.length - 1;
    const { title, url: uri } = entries[current];
    // `flowID` and `streamID` are in the top-level of the JSON, `entries` is
    // an array of "tabs" with `current` being what index is the one we care
    // about, or the last one if not specified.
    this._fxai.telemetry.recordEvent(
      "command-received",
      COMMAND_SENDTAB_TAIL,
      this._fxai.telemetry.sanitizeDeviceId(senderID),
      { flowID, streamID, reason }
    );

    return {
      title,
      uri,
    };
  }

  async _encrypt(bytes, device) {
    let bundle = device.availableCommands[COMMAND_SENDTAB];
    if (!bundle) {
      throw new Error(`Device ${device.id} does not have send tab keys.`);
    }
    const oldsyncKey = await this._fxai.keys.getKeyForScope(SCOPE_OLD_SYNC);
    // Older clients expect this to be hex, due to pre-JWK sync key ids :-(
    const ourKid = this._fxai.keys.kidAsHex(oldsyncKey);
    const { kid: theirKid } = JSON.parse(
      device.availableCommands[COMMAND_SENDTAB]
    );
    if (theirKid != ourKid) {
      throw new Error("Target Send Tab key ID is different from ours");
    }
    const json = JSON.parse(bundle);
    const wrapper = new lazy.CryptoWrapper();
    wrapper.deserialize({ payload: json });
    const syncKeyBundle = lazy.BulkKeyBundle.fromJWK(oldsyncKey);
    let { publicKey, authSecret } = await wrapper.decrypt(syncKeyBundle);
    authSecret = urlsafeBase64Decode(authSecret);
    publicKey = urlsafeBase64Decode(publicKey);

    const { ciphertext: encrypted } = await lazy.PushCrypto.encrypt(
      bytes,
      publicKey,
      authSecret
    );
    return urlsafeBase64Encode(encrypted);
  }

  async _getPersistedSendTabKeys() {
    const { device } = await this._fxai.getUserAccountData(["device"]);
    return device && device.sendTabKeys;
  }

  async _decrypt(ciphertext) {
    let { privateKey, publicKey, authSecret } =
      await this._getPersistedSendTabKeys();
    publicKey = urlsafeBase64Decode(publicKey);
    authSecret = urlsafeBase64Decode(authSecret);
    ciphertext = new Uint8Array(urlsafeBase64Decode(ciphertext));
    return lazy.PushCrypto.decrypt(
      privateKey,
      publicKey,
      authSecret,
      // The only Push encoding we support.
      { encoding: "aes128gcm" },
      ciphertext
    );
  }

  async _generateAndPersistSendTabKeys() {
    let [publicKey, privateKey] = await lazy.PushCrypto.generateKeys();
    publicKey = urlsafeBase64Encode(publicKey);
    let authSecret = lazy.PushCrypto.generateAuthenticationSecret();
    authSecret = urlsafeBase64Encode(authSecret);
    const sendTabKeys = {
      publicKey,
      privateKey,
      authSecret,
    };
    await this._fxai.withCurrentAccountState(async state => {
      const { device } = await state.getUserAccountData(["device"]);
      await state.updateUserAccountData({
        device: {
          ...device,
          sendTabKeys,
        },
      });
    });
    return sendTabKeys;
  }

  async _getPersistedEncryptedSendTabKey() {
    const { encryptedSendTabKeys } = await this._fxai.getUserAccountData([
      "encryptedSendTabKeys",
    ]);
    return encryptedSendTabKeys;
  }

  async _generateAndPersistEncryptedSendTabKey() {
    let sendTabKeys = await this._getPersistedSendTabKeys();
    if (!sendTabKeys) {
      log.info("Could not find sendtab keys, generating them");
      sendTabKeys = await this._generateAndPersistSendTabKeys();
    }
    // Strip the private key from the bundle to encrypt.
    const keyToEncrypt = {
      publicKey: sendTabKeys.publicKey,
      authSecret: sendTabKeys.authSecret,
    };
    if (!(await this._fxai.keys.canGetKeyForScope(SCOPE_OLD_SYNC))) {
      log.info("Can't fetch keys, so unable to determine sendtab keys");
      return null;
    }
    let oldsyncKey;
    try {
      oldsyncKey = await this._fxai.keys.getKeyForScope(SCOPE_OLD_SYNC);
    } catch (ex) {
      log.warn("Failed to fetch keys, so unable to determine sendtab keys", ex);
      return null;
    }
    const wrapper = new lazy.CryptoWrapper();
    wrapper.cleartext = keyToEncrypt;
    const keyBundle = lazy.BulkKeyBundle.fromJWK(oldsyncKey);
    await wrapper.encrypt(keyBundle);
    const encryptedSendTabKeys = JSON.stringify({
      // This is expected in hex, due to pre-JWK sync key ids :-(
      kid: this._fxai.keys.kidAsHex(oldsyncKey),
      IV: wrapper.IV,
      hmac: wrapper.hmac,
      ciphertext: wrapper.ciphertext,
    });
    await this._fxai.withCurrentAccountState(async state => {
      await state.updateUserAccountData({
        encryptedSendTabKeys,
      });
    });
    return encryptedSendTabKeys;
  }

  async getEncryptedSendTabKeys() {
    let encryptedSendTabKeys = await this._getPersistedEncryptedSendTabKey();
    const sendTabKeys = await this._getPersistedSendTabKeys();
    if (!encryptedSendTabKeys || !sendTabKeys) {
      log.info("Generating and persisting encrypted sendtab keys");
      // `_generateAndPersistEncryptedKeys` requires the sync key
      // which cannot be accessed if the login manager is locked
      // (i.e when the primary password is locked) or if the sync keys
      // aren't accessible (account isn't verified)
      // so this function could fail to retrieve the keys
      // however, device registration will trigger when the account
      // is verified, so it's OK
      // Note that it's okay to persist those keys, because they are
      // already persisted in plaintext and the encrypted bundle
      // does not include the sync-key (the sync key is used to encrypt
      // it though)
      encryptedSendTabKeys =
        await this._generateAndPersistEncryptedSendTabKey();
    }
    return encryptedSendTabKeys;
  }
}

/**
 * Close Tabs is built on-top of device commands and handles
 * actions a client wants to perform on tabs found on other devices.
 * This class is very similar to the Send Tab component in FxAccountsCommands
 * (and indeed, it might make sense to combine these so there's better
 * error handling/retry logic for send tab)
 *
 * Devices exchange keys wrapped in the oldsync key between themselves (getEncryptedCloseTabKeys)
 * during the device registration flow. The FxA server can theoretically never
 * retrieve the close tab keys since it doesn't know the oldsync key.
 *
 * Note: Close Tabs does things slightly different from SendTab
 * The sender encrypts the close-tab command using the receiver's public key,
 * and the FxA server stores it (without re-encrypting).
 * A web-push notifies the receiver that a new command is available.
 * The receiver decrypts the payload using its private key.
 *
 * This implementation uses the "tabs store" to persist the close requests, and also
 * to act as the queue for commands to send. There's simple retry logic etc so that
 * even if the device is offline the command should be sent when the network comes back up.
 *
 * This means that consumers generally *do not* call this object directly - consumers will
 * typically use the capabilities exposed in SyncedTabs.sys.mjs to queue the commands. This
 * code listens for observer notifications sent by that module to handle the queue.
 */
export class CloseRemoteTab {
  // The delay between a command being queued and it being actioned. This delay
  // is primarily to support "undo" functionality in the UI.
  DELAY = 5000;

  // The timer ID if we have one scheduled, otherwise null
  #timer = null;

  // Since we only ever show one notification to the user
  // we keep track of how many tabs have actually been closed
  // and update the count, user dismissing the notification will
  // reset the count
  closeTabNotificationCount = 0;
  hasPendingCloseTabNotification = false;

  constructor(commands, fxAccountsInternal) {
    this._commands = commands;
    this._fxai = fxAccountsInternal;
    Services.obs.addObserver(this, "services.sync.tabs.command-queued");
    log.trace("CloseRemoteTab observer created");
  }

  // Used for tests - when in the browser this object lives forever.
  shutdown() {
    if (this.#timer) {
      clearTimeout(this.#timer);
    }
    Services.obs.removeObserver(this, "services.sync.tabs.command-queued");
  }

  observe(subject, topic, data) {
    log.trace(
      `CloseRemoteTab observed topic=${topic}, data=${data}, subject=${subject}`
    );
    switch (topic) {
      case "services.sync.tabs.command-queued":
        this.flushQueue().catch(e => {
          log.error("Failed to flush the outgoing queue", e);
        });
        break;
      default:
        log.error(`unexpected observer topic: ${topic}`);
    }
  }

  async flushQueue() {
    // get all the queued items to work out what's ready to send. If a device has queued item less than
    // our pushDelay, then we don't send *any* command for that device yet, but ensure a timer is set
    // for the delay.
    let store = await lazy.getRemoteCommandStore();
    let pending = await store.getUnsentCommands();
    log.trace("flushQueue total queued items", pending.length);
    // any timeRequested less than `sendThreshold` should be sent now.
    let now = this.now();
    let sendThreshold = now - this.DELAY;
    // make a map of deviceId -> device
    let recentDevices = this._fxai.device.recentDeviceList;
    if (!recentDevices.length) {
      // If we can't map a device ID to the device with the keys etc, we are screwed!
      log.error(
        "Trying to handle a queued tab command but no devices are available"
      );
      return;
    }
    let deviceMap = new Map(recentDevices.map(d => [d.id, d]));
    // make a map of commands keyed by device ID.
    let byDevice = Map.groupBy(pending, c => c.deviceId);
    let nextTime = Infinity;
    let didSend = false;
    for (let [deviceId, commands] of byDevice) {
      let device = deviceMap.get(deviceId);
      if (!device) {
        // If we can't map *this* device ID to a device with the keys etc, we are screwed!
        // This however *is* possible if the target device was disconnected before we had a chance to send it,
        // so remove this item.
        log.warn(
          "Trying to handle a queued tab command for an unknown device",
          deviceId
        );
        for (const command of commands) {
          await store.removeRemoteCommand(deviceId, command);
        }
        continue;
      }
      let toSend = [];
      for (const command of commands) {
        if (!(command.command instanceof lazy.RemoteCommand)) {
          log.error(`ignoring unknown pending command ${command}`);
          // I guess we should try and delete it, but this is already "impossible", so :shrug
          continue;
        }
        if (command.timeRequested <= sendThreshold) {
          log.trace(
            `command for url ${command.command.url} was queued for sending ${
              (now - command.timeRequested) / 1000
            }s ago, so sending it now`
          );
          toSend.push(command);
        } else {
          log.trace(
            `command for url ${command.command.url} was queued for sending ${
              (now - command.timeRequested) / 1000
            }s ago, so ensuring the next timer is set for it.`
          );
          nextTime = Math.min(nextTime, command.timeRequested + this.DELAY);
        }
      }
      if (toSend.length) {
        let urls = toSend.map(c => c.command.url);
        // XXX - failure should cause a new error handling timer strategy (eg, ideally exponential backoff etc)
        if (await this._sendCloseTabPush(device, urls)) {
          // success! Mark them as sent.
          for (let cmd of toSend) {
            log.trace(
              `Setting pending command for device ${deviceId} as sent`,
              cmd
            );
            await store.setPendingCommandSent(cmd);
            didSend = true;
          }
        } else {
          // We should investigate a better backoff strategy
          // https://bugzilla.mozilla.org/show_bug.cgi?id=1899433
          // For now just say 60s.
          nextTime = Math.min(nextTime, now + 60000);
        }
      }
    }
    if (didSend) {
      Services.obs.notifyObservers(null, TOPIC_TABS_CHANGED);
    }

    if (nextTime == Infinity) {
      log.info(`No new close-tab timer needed because there's nothing to do`);
    } else {
      // We set the timer to be just a little bit more than requested (XXX - probably not necessary?!)
      let delay = nextTime - now + 10;
      this._ensureTimer(delay);
    }
  }

  async _ensureTimer(timeout) {
    log.info(
      `Setting a new close-tab timer with delay=${timeout} with existing timer=${!!this
        .#timer}`
    );

    if (this.#timer) {
      clearTimeout(this.#timer);
    }

    // If the browser shuts down while a timer exists we should force the send
    // While we should pick up the command after a restart, we don't know
    // how long that will be.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1888299
    this.#timer = setTimeout(async () => {
      // XXX - this might be racey - if a new timer fires before this promise resolves - it
      // might seem unlikely, but network is involved!
      await this.flushQueue();
      this.#timer = null;
    }, timeout);
  }

  /**
   * @param {Device} target - Device object (typically returned by fxAccounts.getDevicesList()).
   * @param {String[]} urls - array of urls that should be closed on the remote device
   */
  async _sendCloseTabPush(target, urls) {
    log.info(`Sending tab closures to ${target.id} device.`);
    const flowID = this._fxai.telemetry.generateFlowID();
    const encoder = new TextEncoder();
    try {
      const streamID = this._fxai.telemetry.generateFlowID();
      const targetData = { flowID, streamID, urls };
      const bytes = encoder.encode(JSON.stringify(targetData));
      const encrypted = await this._encrypt(bytes, target);
      // FxA expects an object as the payload, but we only have a single encrypted string; wrap it.
      // If you add any plaintext items to this payload, please carefully consider the privacy implications
      // of revealing that data to the FxA server.
      const payload = { encrypted };
      await this._commands.invoke(COMMAND_CLOSETAB, target, payload);
      this._fxai.telemetry.recordEvent(
        "command-sent",
        COMMAND_CLOSETAB_TAIL,
        this._fxai.telemetry.sanitizeDeviceId(target.id),
        { flowID, streamID }
      );
      return true;
    } catch (error) {
      // We should also show the user there was some kind've error
      log.error("Error while invoking a send tab command.", error);
      return false;
    }
  }

  // Returns true if the target device is compatible with FxA Commands Send tab.
  isDeviceCompatible(device) {
    let pref = Services.prefs.getBoolPref(
      "identity.fxaccounts.commands.remoteTabManagement.enabled",
      false
    );
    return (
      pref &&
      device.availableCommands &&
      device.availableCommands[COMMAND_CLOSETAB]
    );
  }

  // Handle incoming remote tab payload, called by FxAccountsCommands.
  async handleTabClose(senderID, { encrypted }, reason) {
    const bytes = await this._decrypt(encrypted);
    const decoder = new TextDecoder("utf8");
    const data = JSON.parse(decoder.decode(bytes));
    // urls is an array of strings
    const { flowID, streamID, urls } = data;
    this._fxai.telemetry.recordEvent(
      "command-received",
      COMMAND_CLOSETAB_TAIL,
      this._fxai.telemetry.sanitizeDeviceId(senderID),
      { flowID, streamID, reason }
    );

    return {
      urls,
    };
  }

  async _encrypt(bytes, device) {
    let bundle = device.availableCommands[COMMAND_CLOSETAB];
    if (!bundle) {
      throw new Error(`Device ${device.id} does not have close tab keys.`);
    }
    const oldsyncKey = await this._fxai.keys.getKeyForScope(SCOPE_OLD_SYNC);
    const json = JSON.parse(bundle);
    const wrapper = new lazy.CryptoWrapper();
    wrapper.deserialize({ payload: json });
    const syncKeyBundle = lazy.BulkKeyBundle.fromJWK(oldsyncKey);
    let { publicKey, authSecret } = await wrapper.decrypt(syncKeyBundle);
    authSecret = urlsafeBase64Decode(authSecret);
    publicKey = urlsafeBase64Decode(publicKey);

    const { ciphertext: encrypted } = await lazy.PushCrypto.encrypt(
      bytes,
      publicKey,
      authSecret
    );
    return urlsafeBase64Encode(encrypted);
  }

  async _getPersistedCloseTabKeys() {
    const { device } = await this._fxai.getUserAccountData(["device"]);
    return device && device.closeTabKeys;
  }

  async _decrypt(ciphertext) {
    let { privateKey, publicKey, authSecret } =
      await this._getPersistedCloseTabKeys();
    publicKey = urlsafeBase64Decode(publicKey);
    authSecret = urlsafeBase64Decode(authSecret);
    ciphertext = new Uint8Array(urlsafeBase64Decode(ciphertext));
    return lazy.PushCrypto.decrypt(
      privateKey,
      publicKey,
      authSecret,
      // The only Push encoding we support.
      { encoding: "aes128gcm" },
      ciphertext
    );
  }

  async _generateAndPersistCloseTabKeys() {
    let [publicKey, privateKey] = await lazy.PushCrypto.generateKeys();
    publicKey = urlsafeBase64Encode(publicKey);
    let authSecret = lazy.PushCrypto.generateAuthenticationSecret();
    authSecret = urlsafeBase64Encode(authSecret);
    const closeTabKeys = {
      publicKey,
      privateKey,
      authSecret,
    };
    await this._fxai.withCurrentAccountState(async state => {
      const { device } = await state.getUserAccountData(["device"]);
      await state.updateUserAccountData({
        device: {
          ...device,
          closeTabKeys,
        },
      });
    });
    return closeTabKeys;
  }

  async _getPersistedEncryptedCloseTabKey() {
    const { encryptedCloseTabKeys } = await this._fxai.getUserAccountData([
      "encryptedCloseTabKeys",
    ]);
    return encryptedCloseTabKeys;
  }

  async _generateAndPersistEncryptedCloseTabKeys() {
    let closeTabKeys = await this._getPersistedCloseTabKeys();
    if (!closeTabKeys) {
      log.info("Could not find closeTab keys, generating them");
      closeTabKeys = await this._generateAndPersistCloseTabKeys();
    }
    // Strip the private key from the bundle to encrypt.
    const keyToEncrypt = {
      publicKey: closeTabKeys.publicKey,
      authSecret: closeTabKeys.authSecret,
    };
    if (!(await this._fxai.keys.canGetKeyForScope(SCOPE_OLD_SYNC))) {
      log.info("Can't fetch keys, so unable to determine closeTab keys");
      return null;
    }
    let oldsyncKey;
    try {
      oldsyncKey = await this._fxai.keys.getKeyForScope(SCOPE_OLD_SYNC);
    } catch (ex) {
      log.warn(
        "Failed to fetch keys, so unable to determine closeTab keys",
        ex
      );
      return null;
    }
    const wrapper = new lazy.CryptoWrapper();
    wrapper.cleartext = keyToEncrypt;
    const keyBundle = lazy.BulkKeyBundle.fromJWK(oldsyncKey);
    await wrapper.encrypt(keyBundle);
    const encryptedCloseTabKeys = JSON.stringify({
      // This is expected in hex, due to pre-JWK sync key ids :-(
      kid: this._fxai.keys.kidAsHex(oldsyncKey),
      IV: wrapper.IV,
      hmac: wrapper.hmac,
      ciphertext: wrapper.ciphertext,
    });
    await this._fxai.withCurrentAccountState(async state => {
      await state.updateUserAccountData({
        encryptedCloseTabKeys,
      });
    });
    return encryptedCloseTabKeys;
  }

  async getEncryptedCloseTabKeys() {
    let encryptedCloseTabKeys = await this._getPersistedEncryptedCloseTabKey();
    const closeTabKeys = await this._getPersistedCloseTabKeys();
    if (!encryptedCloseTabKeys || !closeTabKeys) {
      log.info("Generating and persisting encrypted closeTab keys");
      // `_generateAndPersistEncryptedCloseTabKeys` requires the sync key
      // which cannot be accessed if the login manager is locked
      // (i.e when the primary password is locked) or if the sync keys
      // aren't accessible (account isn't verified)
      // so this function could fail to retrieve the keys
      // however, device registration will trigger when the account
      // is verified, so it's OK
      // Note that it's okay to persist those keys, because they are
      // already persisted in plaintext and the encrypted bundle
      // does not include the sync-key (the sync key is used to encrypt
      // it though)
      encryptedCloseTabKeys =
        await this._generateAndPersistEncryptedCloseTabKeys();
    }
    return encryptedCloseTabKeys;
  }

  // hook points for tests.
  now() {
    return Date.now();
  }
}

function urlsafeBase64Encode(buffer) {
  return ChromeUtils.base64URLEncode(new Uint8Array(buffer), { pad: false });
}

function urlsafeBase64Decode(str) {
  return ChromeUtils.base64URLDecode(str, { padding: "reject" });
}
