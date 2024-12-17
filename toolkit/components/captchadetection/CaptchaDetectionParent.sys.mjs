/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    prefix: "CaptchaDetectionParent",
    maxLogLevelPref: "captchadetection.loglevel",
  });
});

/**
 * Holds the state of each tab.
 * The state is an object with the following structure:
 * [key: tabId]: typeof ReturnType<TabState.#defaultValue()>
 */
class TabState {
  #state;

  constructor() {
    this.#state = new Map();
  }

  get(tabId) {
    return this.#state.get(tabId);
  }

  static #defaultValue() {
    return new Map();
  }

  update(tabId, updateFunction) {
    if (!this.#state.has(tabId)) {
      this.#state.set(tabId, TabState.#defaultValue());
    }
    updateFunction(this.#state.get(tabId));
  }

  clear(tabId) {
    this.#state.delete(tabId);
  }
}

const tabState = new TabState();

/**
 * This actor parent is responsible for recording the state of captchas
 * or communicating with parent browsing context.
 */
export class CaptchaDetectionParent extends JSWindowActorParent {
  actorCreated() {
    lazy.console.debug("actorCreated");
  }

  #updateGRecaptchaV2State({ tabId, isPBM, state: { type, changes } }) {
    lazy.console.debug("updateGRecaptchaV2State", changes);

    if (changes === "ImagesShown") {
      tabState.update(tabId, state => {
        state.set(type + changes, true);
      });

      const shownMetric = "googleRecaptchaV2Ps" + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[shownMetric].add(1);
    } else if (changes === "GotCheckmark") {
      const autoCompleted = !tabState.get(tabId)?.has(type + "ImagesShown");
      lazy.console.debug(
        "GotCheckmark" +
          (autoCompleted ? " (auto-completed)" : " (manually-completed)")
      );
      const resultMetric =
        "googleRecaptchaV2" +
        (autoCompleted ? "Ac" : "Pc") +
        (isPBM ? "Pbm" : "");
      Glean.captchaDetection[resultMetric].add(1);
    }
  }

  #recordCFTurnstileResult({ isPBM, state: { result } }) {
    lazy.console.debug("recordCFTurnstileResult", result);
    const resultMetric =
      "cloudflareTurnstile" +
      (result === "Succeeded" ? "Cc" : "Cf") +
      (isPBM ? "Pbm" : "");
    Glean.captchaDetection[resultMetric].add(1);
  }

  async receiveMessage(message) {
    lazy.console.debug("receiveMessage", JSON.stringify(message));

    switch (message.name) {
      case "CaptchaState:Update":
        // message.name === "CaptchaState:Update"
        // => message.data = {
        //   tabId: number,
        //   isPBM: boolean,
        //   state: {
        //     type: string,
        //     ...<type specific payload>
        //   }
        // }
        switch (message.data.state.type) {
          case "g-recaptcha-v2":
            this.#updateGRecaptchaV2State(message.data);
            break;
          case "cf-turnstile":
            this.#recordCFTurnstileResult(message.data);
            break;
        }
        break;
      case "TabState:Closed":
        // message.name === "TabState:Closed"
        // => message.data = {
        //   tabId: number,
        // }
        tabState.clear(message.data.tabId);
        break;
      default:
        lazy.console.error("Unknown message", message);
    }
    return null;
  }
}
