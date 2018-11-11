/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5ReleasableElementName_h
#define nsHtml5ReleasableElementName_h

#include "nsHtml5ElementName.h"
#include "mozilla/Attributes.h"

class nsHtml5ReleasableElementName final : public nsHtml5ElementName
{
  public:
    explicit nsHtml5ReleasableElementName(nsIAtom* name);
    virtual void release();
    virtual nsHtml5ElementName* cloneElementName(nsHtml5AtomTable* interner);
};

#endif // nsHtml5ReleasableElementName_h
