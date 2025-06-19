/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  Calculator: "resource:///modules/UrlbarProviderCalculator.sys.mjs",
});

const FORMULAS = [
  ["1+1", "2"],
  ["3+4*2/(1-5)", "1"],
  ["39+4*2/(1-5)", "37"],
  ["(39+4)*2/(1-5)", "-21.5"],
  ["4+-5", "-1"],
  ["-5*6", "-30"],
  ["-5.5*6", "-33"],
  ["-5.5*-6.4", "35.2"],
  ["-6-6-6", "-18"],
  ["6-6-6", "-6"],
  [".001 /2", "0.0005"],
  ["(0-.001)/2", "-0.0005"],
  ["-.001/(0-2)", "0.0005"],
  ["1000000000000000000000000+1", "1.0e24"],
  ["1000000000000000000000000-1", "1.0e24"],
  ["1e+30+10", "1.0e30"],
  ["1e+30*10", "1.0e31"],
  ["1e+30/100", "1.0e28"],
  ["10/1000000000000000000000000", "1.0e-23"],
  ["10/-1000000000000000000000000", "-1.0e-23"],
  ["1,500.5+2.5", "1503"], // Ignore commas when using decimal seperators
  ["1,5+2,5", "4"], // Support comma seperators
  ["1.500,5+2,5", "1503"], // Ignore periods when using comma decimal seperators
  ["3^3", "27"],
  ["2+3^3", "29"],
  ["4^0.5+4^-0.5", "2.5"],
  ["3+8*64/(1-3)^2^3", "5"],
  ["1/0", "undefined"],
  ["1/3", "0.333333333"],
  ["1/(3*10^10)", "3.33333333e-11"],
  ["1,000,000+500", "1000500"],
  ["1.000.000+500", "1000500"],
];

add_task(function test() {
  for (let [formula, result] of FORMULAS) {
    let postfix = Calculator.infix2postfix(formula);
    Assert.strictEqual(
      Calculator.evaluatePostfix(postfix),
      result,
      `${formula} should equal ${result}`
    );
  }
});
