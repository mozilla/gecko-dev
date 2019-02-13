/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHTMLEditorEventListener_h__
#define nsHTMLEditorEventListener_h__

#include "nsEditorEventListener.h"
#include "nscore.h"

class nsEditor;
class nsHTMLEditor;

class nsHTMLEditorEventListener : public nsEditorEventListener
{
public:
  nsHTMLEditorEventListener() :
    nsEditorEventListener()
  {
  }

  virtual ~nsHTMLEditorEventListener()
  {
  }

#ifdef DEBUG
  // WARNING: You must be use nsHTMLEditor or its sub class for this class.
  virtual nsresult Connect(nsEditor* aEditor) override;
#endif

protected:
  virtual nsresult MouseDown(nsIDOMMouseEvent* aMouseEvent) override;
  virtual nsresult MouseUp(nsIDOMMouseEvent* aMouseEvent) override;
  virtual nsresult MouseClick(nsIDOMMouseEvent* aMouseEvent) override;

  inline nsHTMLEditor* GetHTMLEditor();
};

#endif // nsHTMLEditorEventListener_h__

