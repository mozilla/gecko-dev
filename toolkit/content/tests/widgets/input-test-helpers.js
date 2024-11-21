/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);
const { BrowserTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BrowserTestUtils.sys.mjs"
);

class InputTestHelpers {
  /**
   * Imports the Lit library and exposes it for use in test helpers.
   *
   * @returns {object} The Lit library for use in consuming tests.
   */
  async setupLit() {
    let lit = await import("chrome://global/content/vendor/lit.all.mjs");
    ({
      html: this.html,
      staticHtml: this.staticHtml,
      render: this.render,
    } = lit);
    this.SpreadDirective = class extends lit.Directive {
      render() {
        return lit.nothing;
      }
      update(part, [attrs]) {
        for (let [key, value] of Object.entries(attrs)) {
          part.element.setAttribute(key, value);
        }
        return lit.noChange;
      }
    };
    this.spread = lit.directive(this.SpreadDirective);
    return lit;
  }

  /**
   * Sets up data used in test helpers and creates the DOM element that test
   * templates get rendered into.
   *
   * @param {object} [templateFn] - Template function to render the element and
   *     any associated markup. When called it will receive two positional
   *     argument `attrs` which should be applied to the element under test, and
   *     `children` which should be added as a child of the element under test.
   *     e.g. `(attrs, children) => <my-input ${attrs}>${children}</my-input>`
   */
  async setupInputTests({ templateFn }) {
    this.templateFn = (args = {}, children) =>
      templateFn(this.spread(args), children);
    this.renderTarget = document.createElement("div");
    document.body.append(this.renderTarget);
  }

  /**
   * Renders a template for use in tests.
   *
   * @param {object} [template] - Optional template to render specific markup.
   * @returns {object} DOM node containing the rendered template elements.
   */
  async renderInputElements(template = this.templateFn()) {
    this.render(this.html``, this.renderTarget);
    this.render(template, this.renderTarget);
    await this.renderTarget.firstElementChild.updateComplete;
    return this.renderTarget;
  }

  /**
   * Sets up helpers that can be used to verify the events emitted from custom
   * input elements.
   *
   * @returns {object} Event test helper functions.
   */
  getInputEventHelpers() {
    let seenEvents = [];
    function trackEvent(event) {
      seenEvents.push({
        type: event.type,
        value: event.target.value,
        checked: event.target.checked,
        localName: event.currentTarget.localName,
      });
    }
    function verifyEvents(expectedEvents) {
      is(
        seenEvents.length,
        expectedEvents.length,
        "Input elements emit the expected number of events."
      );
      expectedEvents.forEach((eventInfo, index) => {
        let seenEventInfo = seenEvents[index];
        is(seenEventInfo.type, eventInfo.type, "Event type is correct.");
        is(
          seenEventInfo.value,
          eventInfo.value,
          "Event target value is correct."
        );
        is(
          seenEventInfo.localName,
          eventInfo.localName,
          "Event is emitted from the correct element."
        );
        if (eventInfo.hasOwnProperty("checked")) {
          is(
            seenEventInfo.checked,
            eventInfo.checked,
            "Event checked state is correct."
          );
        }
      });
      // Reset seen events after verifying.
      seenEvents = [];
    }
    return { seenEvents, trackEvent, verifyEvents };
  }

  /**
   * Runs through a collection of tests of properties that should be common to
   * all reusable moz- input elements.
   *
   * @param {string} elementName - HTML tag of the element under test.
   */
  async testCommonInputProperties(elementName) {
    await this.verifyLabel(elementName);
    await this.verifyName(elementName);
    await this.verifyValue(elementName);
    await this.verifyIcon(elementName);
    await this.verifyDisabled(elementName);
    await this.verifyDescription(elementName);
    await this.verifySupportPage(elementName);
    await this.verifyAccesskey(elementName);
    if (this.checkable) {
      await this.verifyChecked(elementName);
    }
  }

  /**
   * Verifies input element label property is settable and dynamic.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyLabel(selector) {
    const INITIAL_LABEL = "This is a label.";
    const NEW_LABEL = "Testing...";

    let labelTemplate = this.templateFn({
      value: "label",
      label: INITIAL_LABEL,
    });
    let renderTarget = await this.renderInputElements(labelTemplate);
    let firstInput = renderTarget.querySelector(selector);

    is(
      firstInput.labelEl.innerText.trim(),
      INITIAL_LABEL,
      "Input label text is set."
    );

    firstInput.label = NEW_LABEL;
    await firstInput.updateComplete;
    is(
      firstInput.labelEl.innerText.trim(),
      NEW_LABEL,
      "Input label text is updated."
    );
  }

  /**
   * Verifies input element name property is settable and dynamic.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyName(selector) {
    const INITIAL_NAME = "name";
    const NEW_NAME = "new-name";

    let renderTarget = await this.renderInputElements();
    let firstInput = renderTarget.querySelector(selector);

    firstInput.name = INITIAL_NAME;
    await firstInput.updateComplete;
    is(firstInput.inputEl.name, INITIAL_NAME, "Input name is set.");

    firstInput.name = NEW_NAME;
    await firstInput.updateComplete;
    is(firstInput.inputEl.name, NEW_NAME, "Input name is updated.");
  }

  /**
   * Verifies input element value property is settable and dynamic.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyValue(selector) {
    const INITIAL_VALUE = "value";
    const NEW_VALUE = "new value";

    let valueTemplate = this.templateFn({
      label: "Testing value",
      value: INITIAL_VALUE,
    });
    let renderTarget = await this.renderInputElements(valueTemplate);
    let firstInput = renderTarget.querySelector(selector);

    is(firstInput.inputEl.value, INITIAL_VALUE, "Input value is set.");
    firstInput.value = NEW_VALUE;
    await firstInput.updateComplete;
    is(firstInput.inputEl.value, NEW_VALUE, "Input value is updated.");
  }

  /**
   * Verifies input element can display and icon.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyIcon(selector) {
    const ICON_SRC = "chrome://global/skin/icons/edit-copy.svg";

    let iconTemplate = this.templateFn({
      value: "icon",
      label: "Testing icon",
      iconsrc: ICON_SRC,
    });

    let renderTarget = await this.renderInputElements(iconTemplate);
    let firstInput = renderTarget.querySelector(selector);

    ok(firstInput.icon, "Input displays an icon.");
    is(
      firstInput.icon.getAttribute("src"),
      "chrome://global/skin/icons/edit-copy.svg",
      "Input icon uses the expected source."
    );

    firstInput.iconSrc = null;
    await firstInput.updateComplete;
    ok(!firstInput.icon, "Input icon can be removed.");
  }

  /**
   * Verifies it is possible to disable and re-enable input element.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyDisabled(selector) {
    let renderTarget = await this.renderInputElements();
    let firstInput = renderTarget.querySelector(selector);

    ok(!firstInput.disabled, "Input is enabled on initial render.");

    firstInput.disabled = true;
    await firstInput.updateComplete;

    ok(firstInput.disabled, "Input is disabled.");
    ok(firstInput.inputEl.disabled, "Disabled state is propagated.");

    firstInput.disabled = false;
    await firstInput.updateComplete;
    ok(!firstInput.disabled, "Input can be re-enabled.");
    ok(!firstInput.inputEl.disabled, "Disabled state is propagated.");
  }

  /**
   * Verifies different methods for providing a description to the input element.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyDescription(selector) {
    const ATTR_DESCRIPTION = "This description is set via an attribute.";
    const SLOTTED_DESCRIPTION = "This description is set via a slot.";

    let templatesArgs = [
      [{ description: ATTR_DESCRIPTION }],
      [{}, this.html`<span slot="description">${SLOTTED_DESCRIPTION}</span>`],
      [
        { description: ATTR_DESCRIPTION },
        this.html`<span slot="description">${SLOTTED_DESCRIPTION}</span>`,
      ],
    ];

    let renderTarget = await this.renderInputElements(
      templatesArgs.map(args => this.templateFn(...args))
    );
    let [firstInput, secondInput, thirdInput] =
      renderTarget.querySelectorAll(selector);

    is(
      firstInput.inputEl.getAttribute("aria-describedby"),
      "description",
      "Input is described by the description element."
    );
    is(
      firstInput.descriptionEl.innerText,
      ATTR_DESCRIPTION,
      "Description element has the expected text when set via an attribute."
    );

    let slottedText = secondInput.descriptionEl
      .querySelector("slot[name=description]")
      .assignedElements()[0].innerText;
    is(
      secondInput.inputEl.getAttribute("aria-describedby"),
      "description",
      "Input is described by the description element."
    );
    is(
      slottedText,
      SLOTTED_DESCRIPTION,
      "Description element has the expected text when set via a slot."
    );

    is(
      thirdInput.inputEl.getAttribute("aria-describedby"),
      "description",
      "Input is described by the description element."
    );
    is(
      thirdInput.descriptionEl.innerText,
      ATTR_DESCRIPTION,
      "Attribute text takes precedence over slotted text."
    );
  }

  /**
   * Verifies different methods for providing a support link for the input element.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifySupportPage(selector) {
    const LEARN_MORE_TEXT = "Learn more";

    let templatesArgs = [
      [{ "support-page": "test-page", label: "A label" }],
      [
        { label: "A label" },
        this.html`<a slot="support-link" href="www.example.com">Help me!</a>`,
      ],
    ];

    let renderTarget = await this.renderInputElements(
      templatesArgs.map(args => this.templateFn(...args))
    );
    let [firstInput, secondInput] = renderTarget.querySelectorAll(selector);

    let supportLink = firstInput.shadowRoot.querySelector(
      "a[is=moz-support-link]"
    );

    await BrowserTestUtils.waitForMutationCondition(
      supportLink,
      { childList: true },
      () => supportLink.textContent.trim()
    );

    ok(
      supportLink,
      "Support link is rendered when supportPage attribute is set."
    );
    ok(
      supportLink.href.includes("test-page"),
      "Support link href points to the expected SUMO page."
    );
    is(
      supportLink.innerText,
      LEARN_MORE_TEXT,
      "Support link uses the default label text."
    );

    let slottedSupportLink = secondInput.shadowRoot
      .querySelector("slot[name=support-link]")
      .assignedElements()[0];
    ok(
      slottedSupportLink,
      "Links can also be rendered using the support-link slot."
    );
    ok(
      slottedSupportLink.href.includes("www.example.com"),
      "Slotted link uses the expected url."
    );
    is(
      slottedSupportLink.innerText,
      "Help me!",
      "Slotted link uses non-default label text."
    );
  }

  /**
   * Verifies the accesskey behavior of the input element.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyAccesskey(selector) {
    const UNIQUE_ACCESS_KEY = "t";
    const SHARED_ACCESS_KEY = "d";

    let attrs = [
      { value: "first", label: "First", accesskey: UNIQUE_ACCESS_KEY },
      { value: "second", label: "Second", accesskey: SHARED_ACCESS_KEY },
      { value: "third", label: "Third", accesskey: SHARED_ACCESS_KEY },
    ];
    let accesskeyTemplate = this.html`${attrs.map(a => this.templateFn(a))}`;

    let renderTarget = await this.renderInputElements(accesskeyTemplate);
    let [firstInput, secondInput, thirdInput] =
      renderTarget.querySelectorAll(selector);

    // Validate that activating a unique accesskey focuses and checks the input.
    firstInput.blur();
    isnot(document.activeElement, firstInput, "First input is not focused.");
    isnot(
      firstInput.shadowRoot.activeElement,
      firstInput.inputEl,
      "Input element is not focused."
    );
    ok(!firstInput.checked, "Input is not checked.");

    synthesizeKey(
      UNIQUE_ACCESS_KEY,
      navigator.platform.includes("Mac")
        ? { altKey: true, ctrlKey: true }
        : { altKey: true, shiftKey: true }
    );

    is(
      document.activeElement,
      firstInput,
      "Input receives focus after accesskey is pressed."
    );
    is(
      firstInput.shadowRoot.activeElement,
      firstInput.inputEl,
      "Input element is focused after accesskey is pressed."
    );
    if (this.checkable) {
      ok(firstInput.checked, "Input is checked after accesskey is pressed.");
    }

    // Validate that activating a shared accesskey toggles focus between inputs.
    synthesizeKey(
      SHARED_ACCESS_KEY,
      navigator.platform.includes("Mac")
        ? { altKey: true, ctrlKey: true }
        : { altKey: true, shiftKey: true }
    );

    is(
      document.activeElement,
      secondInput,
      "Focus moves to the input with the shared accesskey."
    );
    if (this.checkable) {
      ok(!secondInput.checked, "Second input is not checked.");
    }

    synthesizeKey(
      SHARED_ACCESS_KEY,
      navigator.platform.includes("Mac")
        ? { altKey: true, ctrlKey: true }
        : { altKey: true, shiftKey: true }
    );

    is(
      document.activeElement,
      thirdInput,
      "Focus cycles between inputs with the same accesskey."
    );
    if (this.checkable) {
      ok(!thirdInput.checked, "Third input is not checked.");
    }
  }

  /**
   * Verifies the checked state of the input element.
   *
   * @param {string} selector - HTML tag of the element under test.
   */
  async verifyChecked(selector) {
    let renderTarget = await this.renderInputElements();
    let firstInput = renderTarget.querySelector(selector);
    ok(
      !firstInput.inputEl.checked,
      "Input name is not checked on initial render."
    );
    firstInput.checked = true;
    await firstInput.updateComplete;
    ok(firstInput.inputEl.checked, "Input is checked.");
    ok(firstInput.checked, "Checked state is propagated.");

    // Reset checked state so that the radio input doesn't
    // give a false negative
    firstInput.checked = false;
    await firstInput.updateComplete;

    synthesizeMouseAtCenter(firstInput.inputEl, {});
    await firstInput.updateComplete;
    ok(firstInput.inputEl.checked, "Input is checked via mouse.");
    ok(firstInput.checked, "Checked state is propagated.");
  }
}
