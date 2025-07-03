/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  Directive,
  createRef,
  directive,
  html,
  ifDefined,
  noChange,
  nothing,
  ref,
  staticHtml,
  unsafeStatic,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * A Lit directive that applies all properties of an object to a DOM element.
 *
 * This directive interprets keys in the provided props object as follows:
 * - Keys starting with `?` set or remove boolean attributes using `toggleAttribute`.
 * - Keys starting with `.` set properties directly on the element.
 * - Keys starting with `@` are currently not supported and will throw an error.
 * - All other keys are applied as regular attributes using `setAttribute`.
 *
 * It avoids reapplying values that have not changed, but does not currently
 * remove properties that were previously set and are no longer present in the new input.
 *
 * This directive is useful to "spread" an object of attributes/properties declaratively onto an
 * element in a Lit template.
 */
class SpreadDirective extends Directive {
  /**
   * A record of previously applied properties to avoid redundant updates.
   * @type {Record<string, unknown>}
   */
  #prevProps = {};

  /**
   * Render nothing by default as all changes are made in update using DOM APIs
   * on the element directly.
   * @returns {typeof nothing}
   */
  render() {
    return nothing;
  }

  /**
   * Apply props to the element using DOM APIs, updating only changed values.
   * @param {AttributePart} part - The part of the template this directive is bound to.
   * @param {[Record<string, unknown>]} propsArray - An array with a single object containing props to apply.
   * @returns {typeof noChange} - Indicates to Lit that no re-render is needed.
   */
  update(part, [props]) {
    // TODO: This doesn't clear any values that were set in previous calls if
    // they are no longer present.
    // It isn't entirely clear to me (mstriemer) what we should do if a prop is
    // removed, or if the prop has changed from say ?foo to foo. By not
    // implementing the auto-clearing hopefully the consumer will do something
    // that fits their use case.

    /** @type {HTMLElement} */
    let el = part.element;

    for (let [key, value] of Object.entries(props)) {
      // Skip if the value hasn't changed since the last update.
      if (value === this.#prevProps[key]) {
        continue;
      }

      // Update the element based on the property key matching Lit's templates:
      //   ?key -> el.toggleAttribute(key, value)
      //   .key -> el.key = value
      //   key -> el.setAttribute(key, value)
      if (key.startsWith("?")) {
        el.toggleAttribute(key.slice(1), Boolean(value));
      } else if (key.startsWith(".")) {
        el[key.slice(1)] = value;
      } else if (key.startsWith("@")) {
        throw new Error(
          `Event listeners are not yet supported with spread (${key})`
        );
      } else {
        el.setAttribute(key, String(value));
      }
    }

    // Save current props for comparison in the next update.
    this.#prevProps = props;

    return noChange;
  }
}

const spread = directive(SpreadDirective);

/**
 * @type Map<string, HTMLElement>
 */
const controlInstances = new Map();
function getControlInstance(control = "moz-checkbox") {
  if (!controlInstances.has(control)) {
    controlInstances.set(control, document.createElement(control));
  }
  return controlInstances.get(control);
}

export class SettingControl extends MozLitElement {
  #lastSetting;

  static properties = {
    setting: { type: Object },
    config: { type: Object },
    value: {},
    parentDisabled: { type: Boolean },
  };

  constructor() {
    super();
    this.controlRef = createRef();
  }

  createRenderRoot() {
    return this;
  }

  get controlEl() {
    return this.controlRef.value;
  }

  async getUpdateComplete() {
    let result = await super.getUpdateComplete();
    await this.controlEl.updateComplete;
    return result;
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("setting")) {
      if (this.#lastSetting) {
        this.#lastSetting.off("change", this.setValue);
      }
      this.#lastSetting = this.setting;
      this.setValue();
      this.setting.on("change", this.setValue);
    }
  }

  /**
   * The default properties that a control accepts.
   */
  getPropertyMapping(config) {
    const props = {
      id: config.id,
      "data-l10n-id": config.l10nId,
      ".iconSrc": config.iconSrc,
      ".supportPage": config.supportPage,
      ".parentDisabled": this.parentDisabled,
      ".control": this,
      "data-subcategory": config.subcategory,
      "?disabled": this.setting.locked,
    };

    if (config.l10nArgs) {
      props["data-l10n-args"] = JSON.stringify(config.l10nArgs);
    }

    // Set the value based on the control's API.
    let instance = getControlInstance(config.control);
    if ("checked" in instance) {
      props[".checked"] = this.value;
    } else if ("pressed" in instance) {
      props[".pressed"] = this.value;
    } else if ("value" in instance) {
      props[".value"] = this.value;
    }

    return props;
  }

  getValue() {
    return this.setting.value;
  }

  setValue = () => {
    this.value = this.setting.value;
  };

  controlValue(el) {
    if (el.constructor.activatedProperty) {
      return el[el.constructor.activatedProperty];
    }
    return el.value;
  }

  // Called by our parent when our input changed.
  onChange(el) {
    this.setting.userChange(this.controlValue(el));
    this.setValue();
  }

  render() {
    // Allow the Setting to override the static config if necessary.
    this.config = this.setting.getControlConfig(this.config);
    let { config } = this;

    // Prepare nested item config and settings.
    let itemArgs =
      config.items
        ?.map(i => ({
          config: i,
          setting: this.getSetting(i.id),
        }))
        .filter(i => i.setting.visible) || [];
    let nestedSettings = itemArgs.map(
      opts =>
        html`<setting-control
          .config=${opts.config}
          .setting=${opts.setting}
          .getSetting=${this.getSetting}
          slot="nested"
        ></setting-control>`
    );

    // Get the properties for this element: id, fluent, disabled, etc.
    // These will be applied to the control using the spread directive.
    let controlProps = this.getPropertyMapping(config);

    // Prepare any children that this element may need.
    let controlChildren = nothing;
    if (config.control == "moz-select") {
      controlChildren = config.options.map(
        opt =>
          html`<moz-option
            .value=${opt.value}
            data-l10n-id=${opt.l10nId}
            data-l10n-args=${ifDefined(
              opt.l10nArgs && JSON.stringify(opt.l10nArgs)
            )}
          ></moz-option>`
      );
    }

    let tag = unsafeStatic(config.control || "moz-checkbox");
    return staticHtml`<${tag}
      ${spread(controlProps)}
      ${ref(this.controlRef)}
    >${controlChildren}${nestedSettings}</${tag}>`;
  }
}
customElements.define("setting-control", SettingControl);
