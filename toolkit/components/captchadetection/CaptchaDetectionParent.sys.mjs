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

ChromeUtils.defineESModuleGetters(lazy, {
  CaptchaDetectionPingUtils:
    "resource://gre/modules/CaptchaDetectionPingUtils.sys.mjs",
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
class CaptchaDetectionParent extends JSWindowActorParent {
  actorCreated() {
    lazy.console.debug("actorCreated");
  }

  #updateGRecaptchaV2State({ tabId, isPBM, state: { type, changes } }) {
    lazy.console.debug("updateGRecaptchaV2State", changes);

    if (changes === "ImagesShown") {
      tabState.update(tabId, state => {
        state.set(type + changes, true);
      });

      // We don't call maybeSubmitPing here because we might end up
      // submitting the ping without the "GotCheckmark" event.
      // maybeSubmitPing will be called when "GotCheckmark" event is
      // received, or when the daily maybeSubmitPing is called.
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
      lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
    }
  }

  #recordCFTurnstileResult({ isPBM, state: { result } }) {
    lazy.console.debug("recordCFTurnstileResult", result);
    const resultMetric =
      "cloudflareTurnstile" +
      (result === "Succeeded" ? "Cc" : "Cf") +
      (isPBM ? "Pbm" : "");
    Glean.captchaDetection[resultMetric].add(1);
    lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
  }

  async #datadomeInit() {
    const parent = this.browsingContext.parentWindowContext;
    if (!parent) {
      lazy.console.error("Datadome captcha loaded in a top-level window?");
      return;
    }

    let actor = null;
    try {
      actor = parent.getActor("CaptchaDetectionCommunication");
      if (!actor) {
        lazy.console.error("CaptchaDetection actor not found in parent window");
        return;
      }
    } catch (e) {
      lazy.console.error("Error getting actor", e);
      return;
    }

    await actor.sendQuery("Datadome:AddMessageListener");
  }

  #recordDatadomeEvent({ isPBM, state: { event, ...payload } }) {
    lazy.console.debug("recordDatadomeEvent", event, payload);
    const suffix = isPBM ? "Pbm" : "";
    if (event === "load") {
      if (payload.captchaShown) {
        Glean.captchaDetection["datadomePs" + suffix].add(1);
      } else if (payload.blocked) {
        Glean.captchaDetection["datadomeBl" + suffix].add(1);
      }
    } else if (event === "passed") {
      Glean.captchaDetection["datadomePc" + suffix].add(1);
    }

    lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
  }

  #recordHCaptchaState({ isPBM, tabId, state: { type, changes } }) {
    lazy.console.debug("recordHCaptchaEvent", changes);

    if (changes === "shown") {
      // I don't think HCaptcha supports auto-completion, but we act
      // as if it does just in case.
      tabState.update(tabId, state => {
        state.set(type + changes, true);
      });

      // We don't call maybeSubmitPing here because we might end up
      // submitting the ping without the "passed" event.
      // maybeSubmitPing will be called when "passed" event is
      // received, or when the daily maybeSubmitPing is called.
      const shownMetric = "hcaptchaPs" + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[shownMetric].add(1);
    } else if (changes === "passed") {
      const autoCompleted = !tabState.get(tabId)?.has(type + "shown");
      const resultMetric =
        "hcaptcha" + (autoCompleted ? "Ac" : "Pc") + (isPBM ? "Pbm" : "");
      Glean.captchaDetection[resultMetric].add(1);
      lazy.CaptchaDetectionPingUtils.maybeSubmitPing();
    }
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
          case "datadome":
            this.#recordDatadomeEvent(message.data);
            break;
          case "hCaptcha":
            this.#recordHCaptchaState(message.data);
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
      case "CaptchaDetection:Init":
        // message.name === "CaptchaDetection:Init"
        // => message.data = {
        //   type: string,
        // }
        if (message.data.type === "datadome") {
          return this.#datadomeInit();
        }
        break;
      default:
        lazy.console.error("Unknown message", message);
    }
    return null;
  }
}

export {
  CaptchaDetectionParent,
  CaptchaDetectionParent as CaptchaDetectionCommunicationParent,
};
