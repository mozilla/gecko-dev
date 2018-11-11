/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsDOMClassInfoID.h"

DOMCI_CLASS(DOMPrototype)
DOMCI_CLASS(DOMConstructor)

// CSS classes
DOMCI_CLASS(CSSStyleRule)
DOMCI_CLASS(CSSImportRule)
DOMCI_CLASS(CSSMediaRule)
DOMCI_CLASS(CSSNameSpaceRule)

// XUL classes
#ifdef MOZ_XUL
DOMCI_CLASS(XULCommandDispatcher)
#endif
DOMCI_CLASS(XULControllers)
#ifdef MOZ_XUL
DOMCI_CLASS(TreeSelection)
DOMCI_CLASS(TreeContentView)
#endif

#ifdef MOZ_XUL
DOMCI_CLASS(XULTemplateBuilder)
DOMCI_CLASS(XULTreeBuilder)
#endif

DOMCI_CLASS(CSSMozDocumentRule)
DOMCI_CLASS(CSSSupportsRule)

// @font-face in CSS
DOMCI_CLASS(CSSFontFaceRule)

DOMCI_CLASS(ContentFrameMessageManager)
DOMCI_CLASS(ContentProcessMessageManager)
DOMCI_CLASS(ChromeMessageBroadcaster)
DOMCI_CLASS(ChromeMessageSender)

DOMCI_CLASS(CSSKeyframeRule)
DOMCI_CLASS(CSSKeyframesRule)

// @counter-style in CSS
DOMCI_CLASS(CSSCounterStyleRule)

DOMCI_CLASS(CSSPageRule)

DOMCI_CLASS(CSSFontFeatureValuesRule)

DOMCI_CLASS(XULControlElement)
DOMCI_CLASS(XULLabeledControlElement)
DOMCI_CLASS(XULButtonElement)
DOMCI_CLASS(XULCheckboxElement)
DOMCI_CLASS(XULPopupElement)
