/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These types are commonly passed as parameters with the Urlbar code. We
// define them here to avoid having to `@import` them into each module.
// TypeScript will still warn about attempting to call `new UrlbarController()`
// and similar actions because these are only defined as types and not values.
type UrlbarController = import("../UrlbarController.sys.mjs").UrlbarController;
type UrlbarInput = import("../UrlbarInput.sys.mjs").UrlbarInput;
type UrlbarQueryContext = import("../UrlbarUtils.sys.mjs").UrlbarQueryContext;
type UrlbarResult = import("../UrlbarResult.sys.mjs").UrlbarResult;

type Values<T> = T[keyof T];
