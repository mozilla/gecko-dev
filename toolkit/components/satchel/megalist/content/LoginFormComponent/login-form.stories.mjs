/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "./login-form.mjs";

export default {
  title: "Domain-specific UI Widgets/Credential Management/Login Form",
  component: "login-form",
};

window.MozXULElement.insertFTLIfNeeded("preview/megalist.ftl");

export const AddLoginForm = () => html`<login-form type="add"></login-form>`;
export const EditLoginForm = () => html`<login-form type="edit"></login-form>`;
