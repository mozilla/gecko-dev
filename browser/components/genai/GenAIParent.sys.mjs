/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  GenAI: "resource:///modules/GenAI.sys.mjs",
});

/**
 * JSWindowActor to pass data between GenAI singleton and content pages.
 */
export class GenAIParent extends JSWindowActorParent {
  receiveMessage({ data, name }) {
    lazy.GenAI.handleShortcutsMessage(
      name,
      data,
      this.browsingContext.topFrameElement
    );
  }
}
