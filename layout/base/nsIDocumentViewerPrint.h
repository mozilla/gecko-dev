/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIDocumentViewerPrint_h___
#define nsIDocumentViewerPrint_h___

#include "nsISupports.h"

class nsIDocument;
class nsStyleSet;
class nsIPresShell;
class nsPresContext;
class nsViewManager;

// {c6f255cf-cadd-4382-b57f-cd2a9874169b}
#define NS_IDOCUMENT_VIEWER_PRINT_IID \
{ 0xc6f255cf, 0xcadd, 0x4382, \
  { 0xb5, 0x7f, 0xcd, 0x2a, 0x98, 0x74, 0x16, 0x9b } }

/**
 * A DocumentViewerPrint is an INTERNAL Interface used for interaction
 * between the DocumentViewer and the PrintEngine
 */
class nsIDocumentViewerPrint : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_VIEWER_PRINT_IID)

  virtual void SetIsPrinting(bool aIsPrinting) = 0;
  virtual bool GetIsPrinting() = 0;

  virtual void SetIsPrintPreview(bool aIsPrintPreview) = 0;
  virtual bool GetIsPrintPreview() = 0;

  // The style set returned by CreateStyleSet is in the middle of an
  // update batch so that the caller can add sheets to it if needed.
  // Callers should call EndUpdate() on it when ready to use.
  virtual nsresult CreateStyleSet(nsIDocument* aDocument, nsStyleSet** aStyleSet) = 0;

  virtual void IncrementDestroyRefCount() = 0;

  virtual void ReturnToGalleyPresentation() = 0;

  virtual void OnDonePrinting() = 0;

  /**
   * Returns true is InitializeForPrintPreview() has been called.
   */
  virtual bool IsInitializedForPrintPreview() = 0;

  /**
   * Marks this viewer to be used for print preview.
   */
  virtual void InitializeForPrintPreview() = 0;

  /**
   * Replaces the current presentation with print preview presentation.
   */
  virtual void SetPrintPreviewPresentation(nsViewManager* aViewManager,
                                           nsPresContext* aPresContext,
                                           nsIPresShell* aPresShell) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentViewerPrint,
                              NS_IDOCUMENT_VIEWER_PRINT_IID)

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIDOCUMENTVIEWERPRINT \
  virtual void     SetIsPrinting(bool aIsPrinting) override; \
  virtual bool     GetIsPrinting() override; \
  virtual void     SetIsPrintPreview(bool aIsPrintPreview) override; \
  virtual bool     GetIsPrintPreview() override; \
  virtual nsresult CreateStyleSet(nsIDocument* aDocument, nsStyleSet** aStyleSet) override; \
  virtual void     IncrementDestroyRefCount() override; \
  virtual void     ReturnToGalleyPresentation() override; \
  virtual void     OnDonePrinting() override; \
  virtual bool     IsInitializedForPrintPreview() override; \
  virtual void     InitializeForPrintPreview() override; \
  virtual void     SetPrintPreviewPresentation(nsViewManager* aViewManager, \
                                               nsPresContext* aPresContext, \
                                               nsIPresShell* aPresShell) override;

#endif /* nsIDocumentViewerPrint_h___ */
