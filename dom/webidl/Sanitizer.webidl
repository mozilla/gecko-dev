/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://wicg.github.io/sanitizer-api/#idl-index
 *
 * Copyright © 2020 the Contributors to the HTML Sanitizer API Specification,
 * published by the Web Platform Incubator Community Group under the W3C Community Contributor License Agreement (CLA).
 */

enum SanitizerPresets { "default" };
dictionary SetHTMLOptions {
  (Sanitizer or SanitizerConfig or SanitizerPresets) sanitizer = "default";
};
/*
dictionary SetHTMLUnsafeOptions {
  (Sanitizer or SanitizerConfig or SanitizerPresets) sanitizer = {};
};
*/

dictionary SanitizerElementNamespace {
  required DOMString name;
  DOMString? _namespace = "http://www.w3.org/1999/xhtml";
};

// Used by "elements"
dictionary SanitizerElementNamespaceWithAttributes : SanitizerElementNamespace {
  sequence<SanitizerAttribute> attributes;
  sequence<SanitizerAttribute> removeAttributes;
};

typedef (DOMString or SanitizerElementNamespace) SanitizerElement;
typedef (DOMString or SanitizerElementNamespaceWithAttributes) SanitizerElementWithAttributes;

dictionary SanitizerAttributeNamespace {
  required DOMString name;
  DOMString? _namespace = null;
};
typedef (DOMString or SanitizerAttributeNamespace) SanitizerAttribute;

dictionary SanitizerConfig {
  sequence<SanitizerElementWithAttributes> elements;
  sequence<SanitizerElement> removeElements;
  sequence<SanitizerElement> replaceWithChildrenElements;

  sequence<SanitizerAttribute> attributes;
  sequence<SanitizerAttribute> removeAttributes;

  boolean comments;
  boolean dataAttributes;
};

[Exposed=Window, Pref="dom.security.sanitizer.enabled"]
interface Sanitizer {
  [Throws, UseCounter]
  constructor(optional (SanitizerConfig or SanitizerPresets) configuration = "default");

  // Query configuration:
  SanitizerConfig get();

  // Modify a Sanitizer’s lists and fields:
  undefined allowElement(SanitizerElementWithAttributes element);
  undefined removeElement(SanitizerElement element);
  undefined replaceElementWithChildren(SanitizerElement element);
  undefined allowAttribute(SanitizerAttribute attribute);
  undefined removeAttribute(SanitizerAttribute attribute);
  undefined setComments(boolean allow);
  undefined setDataAttributes(boolean allow);

  // Remove markup that executes script. May modify multiple lists:
  undefined removeUnsafe();
};
