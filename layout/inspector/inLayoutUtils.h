/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __inLayoutUtils_h__
#define __inLayoutUtils_h__

class nsIDocument;
class nsINode;

namespace mozilla {
class EventStateManager;
namespace dom {
class Element;
}  // namespace dom
}  // namespace mozilla

class inLayoutUtils {
 public:
  static mozilla::EventStateManager* GetEventStateManagerFor(
      mozilla::dom::Element& aElement);
  static nsIDocument* GetSubDocumentFor(nsINode* aNode);
  static nsINode* GetContainerFor(const nsIDocument& aDoc);
};

#endif  // __inLayoutUtils_h__
