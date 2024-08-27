/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const OutputParser = require("resource://devtools/client/shared/output-parser.js");
loader.lazyGetter(this, "L10N_EMPTY", function () {
  const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
  const L10N = new LocalizationHelper(
    "devtools/shared/locales/styleinspector.properties"
  );
  return L10N.getStr("rule.variableEmpty");
});

/**
 * Set the tooltip content of a provided HTMLTooltip instance to display a
 * variable preview matching the provided text.
 *
 * @param  {HTMLTooltip} tooltip
 *         The tooltip instance on which the text preview content should be set.
 * @param  {Document} doc
 *         A document element to create the HTML elements needed for the tooltip.
 * @param  {Object} params
 * @param  {String} params.computed
 *         The computed value for the variable.
 * @param  {Object} params.outputParserOptions
 *         Options to pass to the OutputParser. At the moment, this is the same object that
 *         we use in the Rules view, so we have the same output in the variable tooltip
 *         than in the Rules view.
 * @param  {Object} params.registeredProperty
 *         Contains the registered property data, if the variable was registered (@property or CSS.registerProperty)
 * @param  {String} params.registeredProperty.syntax
 *         The registered property `syntax` value
 * @param  {Boolean} params.registeredProperty.inherits
 *         The registered property `inherits` value
 * @param  {String} params.registeredProperty.initialValue
 *         The registered property `initial-value`
 * @param  {String} params.startingStyle
 *         The text for @starting-style value (e.g. `red`)
 * @param  {String} params.topSectionText
 *         Text to display in the top section of tooltip (e.g. "blue" or "--x is not defined").
 * @param  {String} params.variableName
 *         The name of the variable we're showing the tooltip for
 */
function setVariableTooltip(
  tooltip,
  doc,
  {
    computed,
    cssProperties,
    outputParserOptions,
    registeredProperty,
    startingStyle,
    topSectionText,
    variableName,
  }
) {
  // Create tooltip content
  const div = doc.createElementNS(XHTML_NS, "div");
  div.classList.add("devtools-monospace", "devtools-tooltip-css-variable");

  const outputParser = new OutputParser(doc, cssProperties);
  const parse = value =>
    outputParser.parseCssProperty(variableName, value, outputParserOptions);

  const valueEl = doc.createElementNS(XHTML_NS, "section");
  valueEl.classList.add("variable-value");
  const varData = outputParserOptions.getVariableData(variableName);
  // If the variable is not defined, append the text as is so we don't get the additional
  // class added by appendValue.
  if (
    typeof varData.value !== "string" &&
    typeof registeredProperty?.initialValue !== "string"
  ) {
    valueEl.append(doc.createTextNode(topSectionText));
  } else {
    appendValue(doc, valueEl, topSectionText, parse);
  }
  div.appendChild(valueEl);

  if (typeof computed !== "undefined") {
    const section = doc.createElementNS(XHTML_NS, "section");
    section.classList.add("computed", "variable-tooltip-section");

    const h2 = doc.createElementNS(XHTML_NS, "h2");
    h2.append(doc.createTextNode("computed value"));
    const computedValueEl = doc.createElementNS(XHTML_NS, "div");
    appendValue(doc, computedValueEl, computed, parse);
    section.append(h2, computedValueEl);

    div.appendChild(section);
  }

  if (typeof startingStyle !== "undefined") {
    const section = doc.createElementNS(XHTML_NS, "section");
    section.classList.add("starting-style", "variable-tooltip-section");

    const h2 = doc.createElementNS(XHTML_NS, "h2");
    h2.append(doc.createTextNode("@starting-style"));
    const startingStyleValue = doc.createElementNS(XHTML_NS, "div");
    appendValue(doc, startingStyleValue, startingStyle, parse);
    section.append(h2, startingStyleValue);

    div.appendChild(section);
  }

  // A registered property always have a non-falsy syntax
  if (registeredProperty?.syntax) {
    const section = doc.createElementNS(XHTML_NS, "section");
    section.classList.add("registered-property", "variable-tooltip-section");

    const h2 = doc.createElementNS(XHTML_NS, "h2");
    h2.append(doc.createTextNode("@property"));

    const dl = doc.createElementNS(XHTML_NS, "dl");
    const addProperty = ({ label, value, parseValue, lineBreak }) => {
      const dt = doc.createElementNS(XHTML_NS, "dt");
      dt.append(doc.createTextNode(label));
      const dd = doc.createElementNS(XHTML_NS, "dd");
      appendValue(doc, dd, value, parseValue ? parse : null);
      dl.append(dt, dd);
      if (lineBreak) {
        dl.append(doc.createElementNS(XHTML_NS, "br"));
      }
    };

    const hasInitialValue = typeof registeredProperty.initialValue === "string";

    addProperty({
      label: "syntax:",
      value: `"${registeredProperty.syntax}"`,
      parseValue: false,
      lineBreak: true,
    });
    addProperty({
      label: "inherits:",
      value: registeredProperty.inherits,
      parseValue: false,
      lineBreak: hasInitialValue,
    });
    if (hasInitialValue) {
      addProperty({
        label: "initial-value:",
        value: registeredProperty.initialValue,
        parseValue: true,
        lineBreak: false,
      });
    }

    section.append(h2, dl);
    div.appendChild(section);
  }

  tooltip.panel.innerHTML = "";
  tooltip.panel.appendChild(div);
  tooltip.setContentSize({ width: "auto", height: "auto" });
}

/**
 * Append a value into the passed element.
 *
 * @param {Document} doc: A document that will be used to create elements
 * @param {Element} el: The element into which the rendered value will be appended
 * @param {String} value: The value we want to append
 * @param {Function} parse: An optional function that will be called with `value`, and whose
 *                   result will be appended to `el`. If not passed, `value` will be appended
 *                   as is in `el`, as a text node (if it's not empty).
 */
function appendValue(doc, el, value, parse) {
  if (value !== "") {
    const frag = parse && parse(value);
    if (frag) {
      el.append(frag);
    } else {
      el.append(doc.createTextNode(value));
    }
    el.classList.add("theme-fg-color1");
  } else {
    el.append(doc.createTextNode(`<${L10N_EMPTY}>`));
    el.classList.add("empty-css-variable");
  }
}

module.exports.setVariableTooltip = setVariableTooltip;
