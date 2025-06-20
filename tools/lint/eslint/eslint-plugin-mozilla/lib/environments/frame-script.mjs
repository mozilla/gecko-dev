/**
 * @file Defines the environment for frame scripts.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

export default {
  globals: {
    // dom/chrome-webidl/MessageManager.webidl

    // MessageManagerGlobal
    dump: "readonly",
    atob: "readonly",
    btoa: "readonly",

    // MessageListenerManagerMixin
    addMessageListener: "readonly",
    removeMessageListener: "readonly",
    addWeakMessageListener: "readonly",
    removeWeakMessageListener: "readonly",

    // MessageSenderMixin
    sendAsyncMessage: "readonly",
    processMessageManager: "readonly",
    remoteType: "readonly",

    // SyncMessageSenderMixin
    sendSyncMessage: "readonly",

    // ContentFrameMessageManager
    content: "readonly",
    docShell: "readonly",
    tabEventTarget: "readonly",
  },
};
