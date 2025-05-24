/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

import js from "@eslint/js";
import mozilla from "eslint-plugin-mozilla";

export default [
  js.configs.recommended,
  ...mozilla.configs["flat/recommended"],
];
