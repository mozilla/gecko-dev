/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "nsDOMClassInfoID.h"

DOMCI_CLASS(Window)
DOMCI_CLASS(DOMPrototype)
DOMCI_CLASS(DOMConstructor)

// CSS classes
DOMCI_CLASS(CSSStyleRule)
DOMCI_CLASS(CSSCharsetRule)
DOMCI_CLASS(CSSImportRule)
DOMCI_CLASS(CSSMediaRule)
DOMCI_CLASS(CSSNameSpaceRule)

// XUL classes
#ifdef MOZ_XUL
DOMCI_CLASS(XULCommandDispatcher)
#endif
DOMCI_CLASS(XULControllers)
DOMCI_CLASS(BoxObject)
#ifdef MOZ_XUL
DOMCI_CLASS(TreeSelection)
DOMCI_CLASS(TreeContentView)
#endif

// DOM Chrome Window class, almost identical to Window
DOMCI_CLASS(ChromeWindow)

#ifdef MOZ_XUL
DOMCI_CLASS(XULTemplateBuilder)
DOMCI_CLASS(XULTreeBuilder)
#endif

#ifdef MOZ_XUL
DOMCI_CLASS(TreeColumn)
#endif

DOMCI_CLASS(CSSMozDocumentRule)
DOMCI_CLASS(CSSSupportsRule)

// XSLTProcessor
DOMCI_CLASS(XSLTProcessor)

// DOM Level 3 XPath objects
DOMCI_CLASS(XPathNSResolver)

DOMCI_CLASS(Blob)
DOMCI_CLASS(File)

// DOM modal content window class, almost identical to Window
DOMCI_CLASS(ModalContentWindow)

DOMCI_CLASS(MozSmsMessage)
DOMCI_CLASS(MozMmsMessage)
DOMCI_CLASS(MozSmsFilter)
DOMCI_CLASS(MozMobileMessageThread)

// @font-face in CSS
DOMCI_CLASS(CSSFontFaceRule)

DOMCI_CLASS(ContentFrameMessageManager)
DOMCI_CLASS(ChromeMessageBroadcaster)
DOMCI_CLASS(ChromeMessageSender)

DOMCI_CLASS(MozCSSKeyframeRule)
DOMCI_CLASS(MozCSSKeyframesRule)

// @counter-style in CSS
DOMCI_CLASS(CSSCounterStyleRule)

DOMCI_CLASS(CSSPageRule)

DOMCI_CLASS(CSSFontFeatureValuesRule)

DOMCI_CLASS(UserDataHandler)
DOMCI_CLASS(XULControlElement)
DOMCI_CLASS(XULLabeledControlElement)
DOMCI_CLASS(XULButtonElement)
DOMCI_CLASS(XULCheckboxElement)
DOMCI_CLASS(XULPopupElement)
