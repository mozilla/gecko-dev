/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ElementInlines_h
#define mozilla_dom_ElementInlines_h

#include "mozilla/dom/Element.h"
#include "nsIDocument.h"

namespace mozilla {
namespace dom {

inline void
Element::RegisterActivityObserver()
{
  OwnerDoc()->RegisterActivityObserver(this);
}

inline void
Element::UnregisterActivityObserver()
{
  OwnerDoc()->UnregisterActivityObserver(this);
}

}
}

#endif // mozilla_dom_ElementInlines_h
