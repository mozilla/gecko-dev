/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");

var EXPORTED_SYMBOLS = ["DateTimePickerChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");

/**
 * DateTimePickerChild is the communication channel between the input box
 * (content) for date/time input types and its picker (chrome).
 */
class DateTimePickerChild extends ActorChild {
  /**
   * On init, just listen for the event to open the picker, once the picker is
   * opened, we'll listen for update and close events.
   */
  constructor(dispatcher) {
    super(dispatcher);

    this._inputElement = null;
  }

  /**
   * Cleanup function called when picker is closed.
   */
  close() {
    this.removeListeners();
    let dateTimeBoxElement = this._inputElement.dateTimeBoxElement;
    if (!dateTimeBoxElement) {
      this._inputElement = null;
      return;
    }

    if (dateTimeBoxElement instanceof Ci.nsIDateTimeInputArea) {
      dateTimeBoxElement.wrappedJSObject.setPickerState(false);
    } else if (this._inputElement.openOrClosedShadowRoot) {
      // dateTimeBoxElement is within UA Widget Shadow DOM.
      // An event dispatch to it can't be accessed by document.
      let win = this._inputElement.ownerGlobal;
      dateTimeBoxElement.dispatchEvent(
        new win.CustomEvent("MozSetDateTimePickerState", { detail: false }));
    }

    this._inputElement = null;
  }

  /**
   * Called after picker is opened to start listening for input box update
   * events.
   */
  addListeners() {
    this.mm.addEventListener("MozUpdateDateTimePicker", this);
    this.mm.addEventListener("MozCloseDateTimePicker", this);
    this.mm.addEventListener("pagehide", this);

    this.mm.addMessageListener("FormDateTime:PickerValueChanged", this);
    this.mm.addMessageListener("FormDateTime:PickerClosed", this);
  }

  /**
   * Stop listeneing for events when picker is closed.
   */
  removeListeners() {
    this.mm.removeEventListener("MozUpdateDateTimePicker", this);
    this.mm.removeEventListener("MozCloseDateTimePicker", this);
    this.mm.removeEventListener("pagehide", this);

    this.mm.removeMessageListener("FormDateTime:PickerValueChanged", this);
    this.mm.removeMessageListener("FormDateTime:PickerClosed", this);
  }

  /**
   * Helper function that returns the CSS direction property of the element.
   */
  getComputedDirection(aElement) {
    return aElement.ownerGlobal.getComputedStyle(aElement)
      .getPropertyValue("direction");
  }

  /**
   * Helper function that returns the rect of the element, which is the position
   * relative to the left/top of the content area.
   */
  getBoundingContentRect(aElement) {
    return BrowserUtils.getElementBoundingRect(aElement);
  }

  getTimePickerPref() {
    return Services.prefs.getBoolPref("dom.forms.datetime.timepicker");
  }

  /**
   * nsIMessageListener.
   */
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "FormDateTime:PickerClosed": {
        this.close();
        break;
      }
      case "FormDateTime:PickerValueChanged": {
        let dateTimeBoxElement = this._inputElement.dateTimeBoxElement;
        if (!dateTimeBoxElement) {
          return;
        }

        let win = this._inputElement.ownerGlobal;

        if (dateTimeBoxElement instanceof Ci.nsIDateTimeInputArea) {
          dateTimeBoxElement.wrappedJSObject.setValueFromPicker(Cu.cloneInto(aMessage.data, win));
        } else if (this._inputElement.openOrClosedShadowRoot) {
          // dateTimeBoxElement is within UA Widget Shadow DOM.
          // An event dispatch to it can't be accessed by document.
          dateTimeBoxElement.dispatchEvent(
            new win.CustomEvent("MozPickerValueChanged",
              { detail: Cu.cloneInto(aMessage.data, win) }));
        }
        break;
      }
      default:
        break;
    }
  }

  /**
   * nsIDOMEventListener, for chrome events sent by the input element and other
   * DOM events.
   */
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozOpenDateTimePicker": {
        // Time picker is disabled when preffed off
        if (!(aEvent.originalTarget instanceof aEvent.originalTarget.ownerGlobal.HTMLInputElement) ||
            (aEvent.originalTarget.type == "time" && !this.getTimePickerPref())) {
          return;
        }

        if (this._inputElement) {
          // This happens when we're trying to open a picker when another picker
          // is still open. We ignore this request to let the first picker
          // close gracefully.
          return;
        }

        this._inputElement = aEvent.originalTarget;

        let dateTimeBoxElement = this._inputElement.dateTimeBoxElement;
        if (!dateTimeBoxElement) {
          throw new Error("How do we get this event without a UA Widget or XBL binding?");
        }

        if (dateTimeBoxElement instanceof Ci.nsIDateTimeInputArea) {
          dateTimeBoxElement.wrappedJSObject.setPickerState(true);
        } else if (this._inputElement.openOrClosedShadowRoot) {
          // dateTimeBoxElement is within UA Widget Shadow DOM.
          // An event dispatch to it can't be accessed by document, because
          // the event is not composed.
          let win = this._inputElement.ownerGlobal;
          dateTimeBoxElement.dispatchEvent(
            new win.CustomEvent("MozSetDateTimePickerState", { detail: true }));
        }

        this.addListeners();

        let value = this._inputElement.getDateTimeInputBoxValue();
        this.mm.sendAsyncMessage("FormDateTime:OpenPicker", {
          rect: this.getBoundingContentRect(this._inputElement),
          dir: this.getComputedDirection(this._inputElement),
          type: this._inputElement.type,
          detail: {
            // Pass partial value if it's available, otherwise pass input
            // element's value.
            value: Object.keys(value).length > 0 ? value
                                                 : this._inputElement.value,
            min: this._inputElement.getMinimum(),
            max: this._inputElement.getMaximum(),
            step: this._inputElement.getStep(),
            stepBase: this._inputElement.getStepBase(),
          },
        });
        break;
      }
      case "MozUpdateDateTimePicker": {
        let value = this._inputElement.getDateTimeInputBoxValue();
        value.type = this._inputElement.type;
        this.mm.sendAsyncMessage("FormDateTime:UpdatePicker", { value });
        break;
      }
      case "MozCloseDateTimePicker": {
        this.mm.sendAsyncMessage("FormDateTime:ClosePicker");
        this.close();
        break;
      }
      case "pagehide": {
        if (this._inputElement &&
            this._inputElement.ownerDocument == aEvent.target) {
          this.mm.sendAsyncMessage("FormDateTime:ClosePicker");
          this.close();
        }
        break;
      }
      default:
        break;
    }
  }
}
