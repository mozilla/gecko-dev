/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ComposerCommandsUpdater.h"

#include "mozilla/mozalloc.h"            // for operator new
#include "mozilla/TransactionManager.h"  // for TransactionManager
#include "mozilla/dom/Selection.h"
#include "nsAString.h"
#include "nsComponentManagerUtils.h"     // for do_CreateInstance
#include "nsDebug.h"                     // for NS_ENSURE_TRUE, etc
#include "nsError.h"                     // for NS_OK, NS_ERROR_FAILURE, etc
#include "nsICommandManager.h"           // for nsICommandManager
#include "nsID.h"                        // for NS_GET_IID, etc
#include "nsIDOMWindow.h"                // for nsIDOMWindow
#include "nsIDocShell.h"                 // for nsIDocShell
#include "nsIInterfaceRequestorUtils.h"  // for do_GetInterface
#include "nsITransactionManager.h"       // for nsITransactionManager
#include "nsLiteralString.h"             // for NS_LITERAL_STRING
#include "nsPICommandUpdater.h"          // for nsPICommandUpdater
#include "nsPIDOMWindow.h"               // for nsPIDOMWindow

class nsITransaction;

namespace mozilla {

ComposerCommandsUpdater::ComposerCommandsUpdater()
    : mDirtyState(eStateUninitialized),
      mSelectionCollapsed(eStateUninitialized),
      mFirstDoOfFirstUndo(true) {}

ComposerCommandsUpdater::~ComposerCommandsUpdater() {
  // cancel any outstanding update timer
  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(ComposerCommandsUpdater)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ComposerCommandsUpdater)

NS_INTERFACE_MAP_BEGIN(ComposerCommandsUpdater)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentStateListener)
  NS_INTERFACE_MAP_ENTRY(nsITransactionListener)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocumentStateListener)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(ComposerCommandsUpdater)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ComposerCommandsUpdater, mUpdateTimer, mDOMWindow,
                         mDocShell)

#if 0
#pragma mark -
#endif

NS_IMETHODIMP
ComposerCommandsUpdater::NotifyDocumentCreated() {
  // Trigger an nsIObserve notification that the document has been created
  UpdateOneCommand("obs_documentCreated");
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::NotifyDocumentWillBeDestroyed() {
  // cancel any outstanding update timer
  if (mUpdateTimer) {
    mUpdateTimer->Cancel();
    mUpdateTimer = nullptr;
  }

  // We can't call this right now; it is too late in some cases and the window
  // is already partially destructed (e.g. JS objects may be gone).
#if 0
  // Trigger an nsIObserve notification that the document will be destroyed
  UpdateOneCommand("obs_documentWillBeDestroyed");
#endif
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::NotifyDocumentStateChanged(bool aNowDirty) {
  // update document modified. We should have some other notifications for this
  // too.
  return UpdateDirtyState(aNowDirty);
}

#if 0
#pragma mark -
#endif

NS_IMETHODIMP
ComposerCommandsUpdater::WillDo(nsITransactionManager* aManager,
                                nsITransaction* aTransaction,
                                bool* aInterrupt) {
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::DidDo(nsITransactionManager* aManager,
                               nsITransaction* aTransaction,
                               nsresult aDoResult) {
  // only need to update if the status of the Undo menu item changes.
  size_t undoCount = aManager->AsTransactionManager()->NumberOfUndoItems();
  if (undoCount == 1) {
    if (mFirstDoOfFirstUndo) {
      UpdateCommandGroup(NS_LITERAL_STRING("undo"));
    }
    mFirstDoOfFirstUndo = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::WillUndo(nsITransactionManager* aManager,
                                  nsITransaction* aTransaction,
                                  bool* aInterrupt) {
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::DidUndo(nsITransactionManager* aManager,
                                 nsITransaction* aTransaction,
                                 nsresult aUndoResult) {
  size_t undoCount = aManager->AsTransactionManager()->NumberOfUndoItems();
  if (!undoCount) {
    mFirstDoOfFirstUndo = true;  // reset the state for the next do
  }
  UpdateCommandGroup(NS_LITERAL_STRING("undo"));
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::WillRedo(nsITransactionManager* aManager,
                                  nsITransaction* aTransaction,
                                  bool* aInterrupt) {
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::DidRedo(nsITransactionManager* aManager,
                                 nsITransaction* aTransaction,
                                 nsresult aRedoResult) {
  UpdateCommandGroup(NS_LITERAL_STRING("undo"));
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::WillBeginBatch(nsITransactionManager* aManager,
                                        bool* aInterrupt) {
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::DidBeginBatch(nsITransactionManager* aManager,
                                       nsresult aResult) {
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::WillEndBatch(nsITransactionManager* aManager,
                                      bool* aInterrupt) {
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::DidEndBatch(nsITransactionManager* aManager,
                                     nsresult aResult) {
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::WillMerge(nsITransactionManager* aManager,
                                   nsITransaction* aTopTransaction,
                                   nsITransaction* aTransactionToMerge,
                                   bool* aInterrupt) {
  *aInterrupt = false;
  return NS_OK;
}

NS_IMETHODIMP
ComposerCommandsUpdater::DidMerge(nsITransactionManager* aManager,
                                  nsITransaction* aTopTransaction,
                                  nsITransaction* aTransactionToMerge,
                                  bool aDidMerge, nsresult aMergeResult) {
  return NS_OK;
}

#if 0
#pragma mark -
#endif

nsresult ComposerCommandsUpdater::Init(nsPIDOMWindowOuter* aDOMWindow) {
  if (NS_WARN_IF(!aDOMWindow)) {
    return NS_ERROR_INVALID_ARG;
  }
  mDOMWindow = aDOMWindow;
  mDocShell = aDOMWindow->GetDocShell();
  return NS_OK;
}

nsresult ComposerCommandsUpdater::PrimeUpdateTimer() {
  if (!mUpdateTimer) {
    mUpdateTimer = NS_NewTimer();
    ;
    NS_ENSURE_TRUE(mUpdateTimer, NS_ERROR_OUT_OF_MEMORY);
  }

  const uint32_t kUpdateTimerDelay = 150;
  return mUpdateTimer->InitWithCallback(static_cast<nsITimerCallback*>(this),
                                        kUpdateTimerDelay,
                                        nsITimer::TYPE_ONE_SHOT);
}

void ComposerCommandsUpdater::TimerCallback() {
  // if the selection state has changed, update stuff
  bool isCollapsed = SelectionIsCollapsed();
  if (static_cast<int8_t>(isCollapsed) != mSelectionCollapsed) {
    UpdateCommandGroup(NS_LITERAL_STRING("select"));
    mSelectionCollapsed = isCollapsed;
  }

  // isn't this redundant with the UpdateCommandGroup above?
  // can we just nuke the above call? or create a meta command group?
  UpdateCommandGroup(NS_LITERAL_STRING("style"));
}

nsresult ComposerCommandsUpdater::UpdateDirtyState(bool aNowDirty) {
  if (mDirtyState != static_cast<int8_t>(aNowDirty)) {
    UpdateCommandGroup(NS_LITERAL_STRING("save"));
    UpdateCommandGroup(NS_LITERAL_STRING("undo"));
    mDirtyState = aNowDirty;
  }

  return NS_OK;
}

nsresult ComposerCommandsUpdater::UpdateCommandGroup(
    const nsAString& aCommandGroup) {
  nsCOMPtr<nsPICommandUpdater> commandUpdater = GetCommandUpdater();
  NS_ENSURE_TRUE(commandUpdater, NS_ERROR_FAILURE);

  if (aCommandGroup.EqualsLiteral("undo")) {
    commandUpdater->CommandStatusChanged("cmd_undo");
    commandUpdater->CommandStatusChanged("cmd_redo");
    return NS_OK;
  }

  if (aCommandGroup.EqualsLiteral("select") ||
      aCommandGroup.EqualsLiteral("style")) {
    commandUpdater->CommandStatusChanged("cmd_bold");
    commandUpdater->CommandStatusChanged("cmd_italic");
    commandUpdater->CommandStatusChanged("cmd_underline");
    commandUpdater->CommandStatusChanged("cmd_tt");

    commandUpdater->CommandStatusChanged("cmd_strikethrough");
    commandUpdater->CommandStatusChanged("cmd_superscript");
    commandUpdater->CommandStatusChanged("cmd_subscript");
    commandUpdater->CommandStatusChanged("cmd_nobreak");

    commandUpdater->CommandStatusChanged("cmd_em");
    commandUpdater->CommandStatusChanged("cmd_strong");
    commandUpdater->CommandStatusChanged("cmd_cite");
    commandUpdater->CommandStatusChanged("cmd_abbr");
    commandUpdater->CommandStatusChanged("cmd_acronym");
    commandUpdater->CommandStatusChanged("cmd_code");
    commandUpdater->CommandStatusChanged("cmd_samp");
    commandUpdater->CommandStatusChanged("cmd_var");

    commandUpdater->CommandStatusChanged("cmd_increaseFont");
    commandUpdater->CommandStatusChanged("cmd_decreaseFont");

    commandUpdater->CommandStatusChanged("cmd_paragraphState");
    commandUpdater->CommandStatusChanged("cmd_fontFace");
    commandUpdater->CommandStatusChanged("cmd_fontColor");
    commandUpdater->CommandStatusChanged("cmd_backgroundColor");
    commandUpdater->CommandStatusChanged("cmd_highlight");
    return NS_OK;
  }

  if (aCommandGroup.EqualsLiteral("save")) {
    // save commands (most are not in C++)
    commandUpdater->CommandStatusChanged("cmd_setDocumentModified");
    commandUpdater->CommandStatusChanged("cmd_save");
    return NS_OK;
  }

  return NS_OK;
}

nsresult ComposerCommandsUpdater::UpdateOneCommand(const char* aCommand) {
  nsCOMPtr<nsPICommandUpdater> commandUpdater = GetCommandUpdater();
  NS_ENSURE_TRUE(commandUpdater, NS_ERROR_FAILURE);

  commandUpdater->CommandStatusChanged(aCommand);

  return NS_OK;
}

bool ComposerCommandsUpdater::SelectionIsCollapsed() {
  if (NS_WARN_IF(!mDOMWindow)) {
    return true;
  }

  RefPtr<dom::Selection> domSelection = mDOMWindow->GetSelection();
  if (NS_WARN_IF(!domSelection)) {
    return false;
  }

  return domSelection->IsCollapsed();
}

already_AddRefed<nsPICommandUpdater>
ComposerCommandsUpdater::GetCommandUpdater() {
  if (NS_WARN_IF(!mDocShell)) {
    return nullptr;
  }

  nsCOMPtr<nsICommandManager> manager = mDocShell->GetCommandManager();
  nsCOMPtr<nsPICommandUpdater> updater = do_QueryInterface(manager);
  return updater.forget();
}

NS_IMETHODIMP
ComposerCommandsUpdater::GetName(nsACString& aName) {
  aName.AssignLiteral("ComposerCommandsUpdater");
  return NS_OK;
}

#if 0
#pragma mark -
#endif

nsresult ComposerCommandsUpdater::Notify(nsITimer* aTimer) {
  NS_ASSERTION(aTimer == mUpdateTimer.get(), "Hey, this ain't my timer!");
  TimerCallback();
  return NS_OK;
}

#if 0
#pragma mark -
#endif

}  // namespace mozilla
