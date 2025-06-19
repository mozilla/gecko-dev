/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { UrlbarUtils } from "resource:///modules/UrlbarUtils.sys.mjs";

const ABSOLUTE = ["celsius", "kelvin", "fahrenheit"];
const ALIAS = ["c", "k", "f"];
const UNITS = [...ABSOLUTE, ...ALIAS];

const NUMBER_REGEX = "-?\\d+(?:\\.\\d+)?\\s*";
const UNIT_REGEX = "\\w+";

// NOTE: This regex need to be localized upon supporting multi locales
//       since it supports en-US input format only.
const QUERY_REGEX = new RegExp(
  `^(${NUMBER_REGEX})(${UNIT_REGEX})(?:\\s+in\\s+|\\s+to\\s+|\\s*=\\s*)(${UNIT_REGEX})`,
  "i"
);

/**
 * This module converts temperature unit.
 */
export class UnitConverterTemperature {
  /**
   * Convert the given search string.
   *
   * @param {string} searchString
   *   The string to be converted
   * @returns {string} conversion result.
   */
  convert(searchString) {
    const regexResult = QUERY_REGEX.exec(searchString);
    if (!regexResult) {
      return null;
    }

    const target = findUnits(regexResult[2], regexResult[3]);

    if (!target) {
      return null;
    }

    const { inputUnit, outputUnit } = target;
    const inputNumber = Number(regexResult[1]);
    const inputChar = inputUnit.charAt(0);
    const outputChar = outputUnit.charAt(0);

    let outputNumber;
    if (inputChar === outputChar) {
      outputNumber = inputNumber;
    } else {
      outputNumber = this[`${inputChar}2${outputChar}`](inputNumber);
    }

    outputNumber = parseFloat(outputNumber);

    let formattedUnit;
    try {
      const formatter = new Intl.NumberFormat("en-US", {
        style: "unit",
        unit: outputUnit,
      });
      const parts = formatter.formatToParts(1);
      formattedUnit = parts.find(part => part.type == "unit").value;
    } catch (e) {
      formattedUnit = outputUnit;
    }

    let optionalSpace = formattedUnit[0] == "°" ? "" : " ";

    return `${UrlbarUtils.formatUnitConversionResult(outputNumber)}${optionalSpace}${formattedUnit}`;
  }

  c2k(t) {
    return t + 273.15;
  }

  c2f(t) {
    return t * 1.8 + 32;
  }

  k2c(t) {
    return t - 273.15;
  }

  k2f(t) {
    return this.c2f(this.k2c(t));
  }

  f2c(t) {
    return (t - 32) / 1.8;
  }

  f2k(t) {
    return this.c2k(this.f2c(t));
  }
}

/**
 * Returns the suitable units for the given two values.
 * If could not found suitable unit, returns null.
 *
 * @param {string} inputUnit
 *    A set of units to convert, mapped to the `inputUnit` value on the return
 * @param {string} outputUnit
 *    A set of units to convert, mapped to the `outputUnit` value on the return
 * @returns {{ inputUnit: string, outputUnit: string }} The suitable units.
 */
function findUnits(inputUnit, outputUnit) {
  inputUnit = inputUnit.toLowerCase();
  outputUnit = outputUnit.toLowerCase();

  if (!UNITS.includes(inputUnit) || !UNITS.includes(outputUnit)) {
    return null;
  }

  return {
    inputUnit: toAbsoluteUnit(inputUnit),
    outputUnit: toAbsoluteUnit(outputUnit),
  };
}

function toAbsoluteUnit(unit) {
  if (unit.length !== 1) {
    return unit;
  }

  return ABSOLUTE.find(a => a.startsWith(unit));
}
