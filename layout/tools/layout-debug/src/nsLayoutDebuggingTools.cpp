/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutDebuggingTools.h"

#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIDocumentViewer.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"

#include "nsAtom.h"

#include "nsIContent.h"

#include "nsCounterManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "nsIFrame.h"
#include "RetainedDisplayListBuilder.h"

#include "mozilla/dom/ChildIterator.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TreeIterator.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"

using namespace mozilla;
using mozilla::dom::Document;

static already_AddRefed<nsIDocumentViewer> doc_viewer(nsIDocShell* aDocShell) {
  if (!aDocShell) {
    return nullptr;
  }
  nsCOMPtr<nsIDocumentViewer> viewer;
  aDocShell->GetDocViewer(getter_AddRefs(viewer));
  return viewer.forget();
}

static PresShell* GetPresShell(nsIDocShell* aDocShell) {
  nsCOMPtr<nsIDocumentViewer> viewer = doc_viewer(aDocShell);
  if (!viewer) {
    return nullptr;
  }
  return viewer->GetPresShell();
}

static nsViewManager* view_manager(nsIDocShell* aDocShell) {
  PresShell* presShell = GetPresShell(aDocShell);
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetViewManager();
}

#ifdef DEBUG
static already_AddRefed<Document> document(nsIDocShell* aDocShell) {
  nsCOMPtr<nsIDocumentViewer> viewer(doc_viewer(aDocShell));
  if (!viewer) {
    return nullptr;
  }
  return do_AddRef(viewer->GetDocument());
}
#endif

nsLayoutDebuggingTools::nsLayoutDebuggingTools() { ForceRefresh(); }

nsLayoutDebuggingTools::~nsLayoutDebuggingTools() = default;

NS_IMPL_ISUPPORTS(nsLayoutDebuggingTools, nsILayoutDebuggingTools)

NS_IMETHODIMP
nsLayoutDebuggingTools::Init(mozIDOMWindow* aWin) {
  if (!Preferences::GetService()) {
    return NS_ERROR_UNEXPECTED;
  }

  {
    if (!aWin) {
      return NS_ERROR_UNEXPECTED;
    }
    auto* window = nsPIDOMWindowInner::From(aWin);
    mDocShell = window->GetDocShell();
  }
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_UNEXPECTED);

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetReflowCounts(bool aShow) {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef MOZ_REFLOW_PERF
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    presShell->SetPaintFrameCount(aShow);
  }
#else
  printf("************************************************\n");
  printf("Sorry, you have not built with MOZ_REFLOW_PERF=1\n");
  printf("************************************************\n");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetPagedMode(bool aPagedMode) {
  nsCOMPtr<nsIPrintSettingsService> printSettingsService =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  nsCOMPtr<nsIPrintSettings> printSettings;

  printSettingsService->CreateNewPrintSettings(getter_AddRefs(printSettings));

  // Use the same setup as setupPrintMode() in reftest-content.js.
  printSettings->SetPaperWidth(5);
  printSettings->SetPaperHeight(3);

  nsIntMargin unwriteableMargin(0, 0, 0, 0);
  printSettings->SetUnwriteableMarginInTwips(unwriteableMargin);

  printSettings->SetHeaderStrLeft(u""_ns);
  printSettings->SetHeaderStrCenter(u""_ns);
  printSettings->SetHeaderStrRight(u""_ns);

  printSettings->SetFooterStrLeft(u""_ns);
  printSettings->SetFooterStrCenter(u""_ns);
  printSettings->SetFooterStrRight(u""_ns);

  printSettings->SetPrintBGColors(true);
  printSettings->SetPrintBGImages(true);

  nsCOMPtr<nsIDocumentViewer> docViewer(doc_viewer(mDocShell));
  docViewer->SetPageModeForTesting(aPagedMode, printSettings);

  ForceRefresh();
  return NS_OK;
}

static void DumpContentRecur(nsIDocShell* aDocShell, FILE* out,
                             bool aAnonymousSubtrees) {
#ifdef DEBUG
  if (!aDocShell) {
    return;
  }

  fprintf(out, "docshell=%p \n", static_cast<void*>(aDocShell));
  RefPtr<Document> doc(document(aDocShell));
  if (!doc) {
    fputs("no document\n", out);
    return;
  }

  dom::Element* root = doc->GetRootElement();
  if (!root) {
    fputs("no root element\n", out);
    return;
  }

  // The content tree (without anonymous subtrees).
  root->List(out);

  // The anonymous subtrees.
  if (aAnonymousSubtrees) {
    dom::TreeIterator<dom::StyleChildrenIterator> iter(*root);
    while (nsIContent* current = iter.GetNext()) {
      if (!current->IsRootOfNativeAnonymousSubtree()) {
        continue;
      }

      fputs("--\n", out);
      if (current->IsElement() &&
          current->AsElement()->GetPseudoElementType() ==
              PseudoStyleType::mozSnapshotContainingBlock) {
        fprintf(out,
                "View Transition Tree "
                "[parent=%p][active-view-transition=%p]:\n",
                (void*)current->GetParent(),
                (void*)doc->GetActiveViewTransition());
      } else {
        fprintf(out, "Anonymous Subtree [parent=%p]:\n",
                (void*)current->GetParent());
      }
      current->List(out);
    }
  }
#endif
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpContent(bool aAnonymousSubtrees) {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpContentRecur(mDocShell, stdout, aAnonymousSubtrees);
  return NS_OK;
}

static void DumpFramesRecur(
    nsIDocShell* aDocShell, FILE* out,
    nsIFrame::ListFlags aFlags = nsIFrame::ListFlags()) {
  if (aFlags.contains(nsIFrame::ListFlag::DisplayInCSSPixels)) {
    fprintf(out, "Frame tree in CSS pixels:\n");
  } else {
    fprintf(out, "Frame tree in app units:\n");
  }

  fprintf(out, "docshell=%p \n", aDocShell);
  if (PresShell* presShell = GetPresShell(aDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->List(out, "", aFlags);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

static void DumpTextRunsRecur(nsIDocShell* aDocShell, FILE* out) {
  fprintf(out, "Text runs:\n");

  fprintf(out, "docshell=%p \n", aDocShell);
  if (PresShell* presShell = GetPresShell(aDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->ListTextRuns(out);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpFrames(uint8_t aFlags) {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  nsIFrame::ListFlags flags{};
  if (aFlags & nsILayoutDebuggingTools::DUMP_FRAME_FLAGS_CSS_PIXELS) {
    flags += nsIFrame::ListFlag::DisplayInCSSPixels;
  }
  if (aFlags & nsILayoutDebuggingTools::DUMP_FRAME_FLAGS_DETERMINISTIC) {
    flags += nsIFrame::ListFlag::OnlyListDeterministicInfo;
  }
  DumpFramesRecur(mDocShell, stdout, flags);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpTextRuns() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpTextRunsRecur(mDocShell, stdout);
  return NS_OK;
}

static void DumpViewsRecur(nsIDocShell* aDocShell, FILE* out) {
#ifdef DEBUG
  fprintf(out, "docshell=%p \n", static_cast<void*>(aDocShell));
  RefPtr<nsViewManager> vm(view_manager(aDocShell));
  if (vm) {
    nsView* root = vm->GetRootView();
    if (root) {
      root->List(out);
    }
  } else {
    fputs("null view manager\n", out);
  }
#endif  // DEBUG
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpViews() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpViewsRecur(mDocShell, stdout);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpCounterManager() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    presShell->FrameConstructor()->GetContainStyleScopeManager().DumpCounters();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpRetainedDisplayList() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  PresShell* presShell = GetPresShell(mDocShell);
  if (!presShell) {
    fputs("null pres shell\n", stdout);
    return NS_OK;
  }

  if (!nsLayoutUtils::AreRetainedDisplayListsEnabled()) {
    fputs("Retained display list is not enabled\n", stdout);
    return NS_OK;
  }

  nsIFrame* root = presShell->GetRootFrame();
  auto* RDLBuilder = nsLayoutUtils::GetRetainedDisplayListBuilder(root);
  if (!RDLBuilder) {
    fputs("no retained display list\n", stdout);
    return NS_OK;
  }
  nsDisplayListBuilder* builder = RDLBuilder->Builder();
  const nsDisplayList* list = RDLBuilder->List();
  if (!builder || !list) {
    fputs("no retained display list\n", stdout);
    return NS_OK;
  }

  fprintf(stdout, "Retained Display List (rootframe=%p) visible=%s:\n",
          nsLayoutUtils::GetDisplayRootFrame(root),
          ToString(builder->GetVisibleRect()).c_str());
  fputs("<\n", stdout);
  nsIFrame::PrintDisplayList(builder, *list, 1, false);
  fputs(">\n", stdout);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpStyleSheets() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#if defined(DEBUG) || defined(MOZ_LAYOUT_DEBUGGER)
  FILE* out = stdout;
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    presShell->ListStyleSheets(out);
  } else {
    fputs("null pres shell\n", out);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP nsLayoutDebuggingTools::DumpMatchedRules() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  FILE* out = stdout;
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->ListWithMatchedRules(out);
    }
  } else {
    fputs("null pres shell\n", out);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpComputedStyles() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
  FILE* out = stdout;
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    presShell->ListComputedStyles(out);
  } else {
    fputs("null pres shell\n", out);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpReflowStats() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
  if (RefPtr<PresShell> presShell = GetPresShell(mDocShell)) {
#  ifdef MOZ_REFLOW_PERF
    presShell->DumpReflows();
#  else
    printf("************************************************\n");
    printf("Sorry, you have not built with MOZ_REFLOW_PERF=1\n");
    printf("************************************************\n");
#  endif
  }
#endif
  return NS_OK;
}

nsresult nsLayoutDebuggingTools::ForceRefresh() {
  RefPtr<nsViewManager> vm(view_manager(mDocShell));
  if (!vm) {
    return NS_OK;
  }
  nsView* root = vm->GetRootView();
  if (root) {
    vm->InvalidateView(root);
  }
  return NS_OK;
}

nsresult nsLayoutDebuggingTools::SetBoolPrefAndRefresh(const char* aPrefName,
                                                       bool aNewVal) {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);

  nsIPrefService* prefService = Preferences::GetService();
  NS_ENSURE_TRUE(prefService && aPrefName, NS_OK);

  Preferences::SetBool(aPrefName, aNewVal);
  prefService->SavePrefFile(nullptr);

  ForceRefresh();

  return NS_OK;
}
