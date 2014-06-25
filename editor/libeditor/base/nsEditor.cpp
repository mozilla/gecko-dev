/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"          // for DebugOnly

#include <stdio.h>                      // for nullptr, stdout
#include <string.h>                     // for strcmp

#include "ChangeAttributeTxn.h"         // for ChangeAttributeTxn
#include "CreateElementTxn.h"           // for CreateElementTxn
#include "DeleteNodeTxn.h"              // for DeleteNodeTxn
#include "DeleteRangeTxn.h"             // for DeleteRangeTxn
#include "DeleteTextTxn.h"              // for DeleteTextTxn
#include "EditAggregateTxn.h"           // for EditAggregateTxn
#include "EditTxn.h"                    // for EditTxn
#include "IMETextTxn.h"                 // for IMETextTxn
#include "InsertElementTxn.h"           // for InsertElementTxn
#include "InsertTextTxn.h"              // for InsertTextTxn
#include "JoinElementTxn.h"             // for JoinElementTxn
#include "PlaceholderTxn.h"             // for PlaceholderTxn
#include "SplitElementTxn.h"            // for SplitElementTxn
#include "mozFlushType.h"               // for mozFlushType::Flush_Frames
#include "mozISpellCheckingEngine.h"
#include "mozInlineSpellChecker.h"      // for mozInlineSpellChecker
#include "mozilla/IMEStateManager.h"    // for IMEStateManager
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/dom/Selection.h"      // for Selection, etc
#include "mozilla/Services.h"           // for GetObserverService
#include "mozilla/TextComposition.h"    // for TextComposition
#include "mozilla/TextEvents.h"
#include "mozilla/dom/Element.h"        // for Element, nsINode::AsElement
#include "mozilla/dom/Text.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAString.h"                  // for nsAString_internal::Length, etc
#include "nsCCUncollectableMarker.h"    // for nsCCUncollectableMarker
#include "nsCaret.h"                    // for nsCaret
#include "nsCaseTreatment.h"
#include "nsCharTraits.h"               // for NS_IS_HIGH_SURROGATE, etc
#include "nsComponentManagerUtils.h"    // for do_CreateInstance
#include "nsComputedDOMStyle.h"         // for nsComputedDOMStyle
#include "nsContentUtils.h"             // for nsContentUtils
#include "nsDOMString.h"                // for DOMStringIsNull
#include "nsDebug.h"                    // for NS_ENSURE_TRUE, etc
#include "nsEditProperty.h"             // for nsEditProperty, etc
#include "nsEditor.h"
#include "nsEditorEventListener.h"      // for nsEditorEventListener
#include "nsEditorUtils.h"              // for nsAutoRules, etc
#include "nsError.h"                    // for NS_OK, etc
#include "nsFocusManager.h"             // for nsFocusManager
#include "nsFrameSelection.h"           // for nsFrameSelection
#include "nsGkAtoms.h"                  // for nsGkAtoms, nsGkAtoms::dir
#include "nsIAbsorbingTransaction.h"    // for nsIAbsorbingTransaction
#include "nsIAtom.h"                    // for nsIAtom
#include "nsIContent.h"                 // for nsIContent
#include "nsIDOMAttr.h"                 // for nsIDOMAttr
#include "nsIDOMCharacterData.h"        // for nsIDOMCharacterData
#include "nsIDOMDocument.h"             // for nsIDOMDocument
#include "nsIDOMElement.h"              // for nsIDOMElement
#include "nsIDOMEvent.h"                // for nsIDOMEvent
#include "nsIDOMEventListener.h"        // for nsIDOMEventListener
#include "nsIDOMEventTarget.h"          // for nsIDOMEventTarget
#include "nsIDOMHTMLElement.h"          // for nsIDOMHTMLElement
#include "nsIDOMKeyEvent.h"             // for nsIDOMKeyEvent, etc
#include "nsIDOMMozNamedAttrMap.h"      // for nsIDOMMozNamedAttrMap
#include "nsIDOMMouseEvent.h"           // for nsIDOMMouseEvent
#include "nsIDOMNode.h"                 // for nsIDOMNode, etc
#include "nsIDOMNodeList.h"             // for nsIDOMNodeList
#include "nsIDOMRange.h"                // for nsIDOMRange
#include "nsIDOMText.h"                 // for nsIDOMText
#include "nsIDocument.h"                // for nsIDocument
#include "nsIDocumentStateListener.h"   // for nsIDocumentStateListener
#include "nsIEditActionListener.h"      // for nsIEditActionListener
#include "nsIEditorObserver.h"          // for nsIEditorObserver
#include "nsIEditorSpellCheck.h"        // for nsIEditorSpellCheck
#include "nsIFrame.h"                   // for nsIFrame
#include "nsIHTMLDocument.h"            // for nsIHTMLDocument
#include "nsIInlineSpellChecker.h"      // for nsIInlineSpellChecker, etc
#include "nsNameSpaceManager.h"        // for kNameSpaceID_None, etc
#include "nsINode.h"                    // for nsINode, etc
#include "nsIObserverService.h"         // for nsIObserverService
#include "nsIPlaintextEditor.h"         // for nsIPlaintextEditor, etc
#include "nsIPresShell.h"               // for nsIPresShell
#include "nsISelection.h"               // for nsISelection, etc
#include "nsISelectionController.h"     // for nsISelectionController, etc
#include "nsISelectionDisplay.h"        // for nsISelectionDisplay, etc
#include "nsISelectionPrivate.h"        // for nsISelectionPrivate, etc
#include "nsISupportsBase.h"            // for nsISupports
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_IF_ADDREF
#include "nsITransaction.h"             // for nsITransaction
#include "nsITransactionManager.h"
#include "nsIWeakReference.h"           // for nsISupportsWeakReference
#include "nsIWidget.h"                  // for nsIWidget, IMEState, etc
#include "nsPIDOMWindow.h"              // for nsPIDOMWindow
#include "nsPresContext.h"              // for nsPresContext
#include "nsRange.h"                    // for nsRange
#include "nsReadableUtils.h"            // for EmptyString, ToNewCString
#include "nsString.h"                   // for nsAutoString, nsString, etc
#include "nsStringFwd.h"                // for nsAFlatString
#include "nsStyleConsts.h"              // for NS_STYLE_DIRECTION_RTL, etc
#include "nsStyleContext.h"             // for nsStyleContext
#include "nsStyleSheetTxns.h"           // for AddStyleSheetTxn, etc
#include "nsStyleStruct.h"              // for nsStyleDisplay, nsStyleText, etc
#include "nsStyleStructFwd.h"           // for nsIFrame::StyleUIReset, etc
#include "nsTextEditUtils.h"            // for nsTextEditUtils
#include "nsTextNode.h"                 // for nsTextNode
#include "nsThreadUtils.h"              // for nsRunnable
#include "nsTransactionManager.h"       // for nsTransactionManager
#include "prtime.h"                     // for PR_Now

class nsIOutputStream;
class nsIParserService;
class nsITransferable;

#ifdef DEBUG
#include "nsIDOMHTMLDocument.h"         // for nsIDOMHTMLDocument
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::widget;

// Defined in nsEditorRegistration.cpp
extern nsIParserService *sParserService;

//---------------------------------------------------------------------------
//
// nsEditor: base editor class implementation
//
//---------------------------------------------------------------------------

nsEditor::nsEditor()
:  mPlaceHolderName(nullptr)
,  mSelState(nullptr)
,  mPhonetic(nullptr)
,  mModCount(0)
,  mFlags(0)
,  mUpdateCount(0)
,  mPlaceHolderBatch(0)
,  mAction(EditAction::none)
,  mIMETextOffset(0)
,  mDirection(eNone)
,  mDocDirtyState(-1)
,  mSpellcheckCheckboxState(eTriUnset)
,  mShouldTxnSetSelection(true)
,  mDidPreDestroy(false)
,  mDidPostCreate(false)
,  mDispatchInputEvent(true)
{
}

nsEditor::~nsEditor()
{
  NS_ASSERTION(!mDocWeak || mDidPreDestroy, "Why PreDestroy hasn't been called?");

  mTxnMgr = nullptr;

  delete mPhonetic;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEditor)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsEditor)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mTxnMgr)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_UNLINK(mEventListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsEditor)
 nsIDocument* currentDoc =
   tmp->mRootElement ? tmp->mRootElement->GetCurrentDoc() : nullptr;
 if (currentDoc &&
     nsCCUncollectableMarker::InGeneration(cb, currentDoc->GetMarkedCCGeneration())) {
   return NS_SUCCESS_INTERRUPTED_TRAVERSE;
 }
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootElement)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInlineSpellChecker)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTxnMgr)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIMETextNode)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mActionListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEditorObservers)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocStateListeners)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventTarget)
 NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEventListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsEditor)
 NS_INTERFACE_MAP_ENTRY(nsIPhonetic)
 NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
 NS_INTERFACE_MAP_ENTRY(nsIEditorIMESupport)
 NS_INTERFACE_MAP_ENTRY(nsIEditor)
 NS_INTERFACE_MAP_ENTRY(nsIObserver)
 NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditor)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEditor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEditor)


NS_IMETHODIMP
nsEditor::Init(nsIDOMDocument *aDoc, nsIContent *aRoot,
               nsISelectionController *aSelCon, uint32_t aFlags,
               const nsAString& aValue)
{
  NS_PRECONDITION(aDoc, "bad arg");
  if (!aDoc)
    return NS_ERROR_NULL_POINTER;

  // First only set flags, but other stuff shouldn't be initialized now.
  // Don't move this call after initializing mDocWeak.
  // SetFlags() can check whether it's called during initialization or not by
  // them.  Note that SetFlags() will be called by PostCreate().
#ifdef DEBUG
  nsresult rv =
#endif
  SetFlags(aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "SetFlags() failed");

  mDocWeak = do_GetWeakReference(aDoc);  // weak reference to doc
  // HTML editors currently don't have their own selection controller,
  // so they'll pass null as aSelCon, and we'll get the selection controller
  // off of the presshell.
  nsCOMPtr<nsISelectionController> selCon;
  if (aSelCon) {
    mSelConWeak = do_GetWeakReference(aSelCon);   // weak reference to selectioncontroller
    selCon = aSelCon;
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selCon = do_QueryInterface(presShell);
  }
  NS_ASSERTION(selCon, "Selection controller should be available at this point");

  //set up root element if we are passed one.  
  if (aRoot)
    mRootElement = do_QueryInterface(aRoot);

  mUpdateCount=0;

  /* initialize IME stuff */
  mIMETextNode = nullptr;
  mIMETextOffset = 0;
  /* Show the caret */
  selCon->SetCaretReadOnly(false);
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);

  selCon->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);//we want to see all the selection reflected to user

  NS_POSTCONDITION(mDocWeak, "bad state");

  // Make sure that the editor will be destroyed properly
  mDidPreDestroy = false;
  // Make sure that the ediotr will be created properly
  mDidPostCreate = false;

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::PostCreate()
{
  // Synchronize some stuff for the flags.  SetFlags() will initialize
  // something by the flag difference.  This is first time of that, so, all
  // initializations must be run.  For such reason, we need to invert mFlags
  // value first.
  mFlags = ~mFlags;
  nsresult rv = SetFlags(~mFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // These operations only need to happen on the first PostCreate call
  if (!mDidPostCreate) {
    mDidPostCreate = true;

    // Set up listeners
    CreateEventListeners();
    rv = InstallEventListeners();
    NS_ENSURE_SUCCESS(rv, rv);

    // nuke the modification count, so the doc appears unmodified
    // do this before we notify listeners
    ResetModificationCount();

    // update the UI with our state
    NotifyDocumentListeners(eDocumentCreated);
    NotifyDocumentListeners(eDocumentStateChanged);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this,
                       SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION,
                       false);
    }
  }

  // update nsTextStateManager and caret if we have focus
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(focusedContent);
    if (target) {
      InitializeSelection(target);
    }

    // If the text control gets reframed during focus, Focus() would not be
    // called, so take a chance here to see if we need to spell check the text
    // control.
    nsEditorEventListener* listener =
      reinterpret_cast<nsEditorEventListener*> (mEventListener.get());
    listener->SpellCheckIfNeeded();

    IMEState newState;
    rv = GetPreferredIMEState(&newState);
    NS_ENSURE_SUCCESS(rv, NS_OK);
    nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
    IMEStateManager::UpdateIMEState(newState, content);
  }
  return NS_OK;
}

/* virtual */
void
nsEditor::CreateEventListeners()
{
  // Don't create the handler twice
  if (!mEventListener) {
    mEventListener = new nsEditorEventListener();
  }
}

nsresult
nsEditor::InstallEventListeners()
{
  NS_ENSURE_TRUE(mDocWeak && mEventListener,
                 NS_ERROR_NOT_INITIALIZED);

  // Initialize the event target.
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  NS_ENSURE_TRUE(rootContent, NS_ERROR_NOT_AVAILABLE);
  mEventTarget = do_QueryInterface(rootContent->GetParent());
  NS_ENSURE_TRUE(mEventTarget, NS_ERROR_NOT_AVAILABLE);

  nsEditorEventListener* listener =
    reinterpret_cast<nsEditorEventListener*>(mEventListener.get());
  return listener->Connect(this);
}

void
nsEditor::RemoveEventListeners()
{
  if (!mDocWeak || !mEventListener) {
    return;
  }
  reinterpret_cast<nsEditorEventListener*>(mEventListener.get())->Disconnect();
  if (mComposition) {
    mComposition->EndHandlingComposition(this);
    mComposition = nullptr;
  }
  mEventTarget = nullptr;
}

bool
nsEditor::GetDesiredSpellCheckState()
{
  // Check user override on this element
  if (mSpellcheckCheckboxState != eTriUnset) {
    return (mSpellcheckCheckboxState == eTriTrue);
  }

  // Check user preferences
  int32_t spellcheckLevel = Preferences::GetInt("layout.spellcheckDefault", 1);

  if (spellcheckLevel == 0) {
    return false;                    // Spellchecking forced off globally
  }

  if (!CanEnableSpellCheck()) {
    return false;
  }

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  if (presShell) {
    nsPresContext* context = presShell->GetPresContext();
    if (context && !context->IsDynamic()) {
      return false;
    }
  }

  // Check DOM state
  nsCOMPtr<nsIContent> content = GetExposedRoot();
  if (!content) {
    return false;
  }

  nsCOMPtr<nsIDOMHTMLElement> element = do_QueryInterface(content);
  if (!element) {
    return false;
  }

  if (!IsPlaintextEditor()) {
    // Some of the page content might be editable and some not, if spellcheck=
    // is explicitly set anywhere, so if there's anything editable on the page,
    // return true and let the spellchecker figure it out.
    nsCOMPtr<nsIHTMLDocument> doc = do_QueryInterface(content->GetCurrentDoc());
    return doc && doc->IsEditingOn();
  }

  bool enable;
  element->GetSpellcheck(&enable);

  return enable;
}

NS_IMETHODIMP
nsEditor::PreDestroy(bool aDestroyingFrames)
{
  if (mDidPreDestroy)
    return NS_OK;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this,
                        SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION);
  }

  // Let spellchecker clean up its observers etc. It is important not to
  // actually free the spellchecker here, since the spellchecker could have
  // caused flush notifications, which could have gotten here if a textbox
  // is being removed. Setting the spellchecker to nullptr could free the
  // object that is still in use! It will be freed when the editor is
  // destroyed.
  if (mInlineSpellChecker)
    mInlineSpellChecker->Cleanup(aDestroyingFrames);

  // tell our listeners that the doc is going away
  NotifyDocumentListeners(eDocumentToBeDestroyed);

  // Unregister event listeners
  RemoveEventListeners();
  mActionListeners.Clear();
  mEditorObservers.Clear();
  mDocStateListeners.Clear();
  mInlineSpellChecker = nullptr;
  mSpellcheckCheckboxState = eTriUnset;
  mRootElement = nullptr;

  mDidPreDestroy = true;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetFlags(uint32_t *aFlags)
{
  *aFlags = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetFlags(uint32_t aFlags)
{
  if (mFlags == aFlags) {
    return NS_OK;
  }

  bool spellcheckerWasEnabled = CanEnableSpellCheck();
  mFlags = aFlags;

  if (!mDocWeak) {
    // If we're initializing, we shouldn't do anything now.
    // SetFlags() will be called by PostCreate(),
    // we should synchronize some stuff for the flags at that time.
    return NS_OK;
  }

  // The flag change may cause the spellchecker state change
  if (CanEnableSpellCheck() != spellcheckerWasEnabled) {
    nsresult rv = SyncRealTimeSpell();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If this is called from PostCreate(), it will update the IME state if it's
  // necessary.
  if (!mDidPostCreate) {
    return NS_OK;
  }

  // Might be changing editable state, so, we need to reset current IME state
  // if we're focused and the flag change causes IME state change.
  nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
  if (focusedContent) {
    IMEState newState;
    nsresult rv = GetPreferredIMEState(&newState);
    if (NS_SUCCEEDED(rv)) {
      // NOTE: When the enabled state isn't going to be modified, this method
      // is going to do nothing.
      nsCOMPtr<nsIContent> content = GetFocusedContentForIME();
      IMEStateManager::UpdateIMEState(newState, content);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsSelectionEditable(bool *aIsSelectionEditable)
{
  NS_ENSURE_ARG_POINTER(aIsSelectionEditable);

  // get current selection
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // XXX we just check that the anchor node is editable at the moment
  //     we should check that all nodes in the selection are editable
  nsCOMPtr<nsIDOMNode> anchorNode;
  selection->GetAnchorNode(getter_AddRefs(anchorNode));
  *aIsSelectionEditable = anchorNode && IsEditable(anchorNode);

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetIsDocumentEditable(bool *aIsDocumentEditable)
{
  NS_ENSURE_ARG_POINTER(aIsDocumentEditable);
  nsCOMPtr<nsIDOMDocument> doc = GetDOMDocument();
  *aIsDocumentEditable = !!doc;

  return NS_OK;
}

already_AddRefed<nsIDocument>
nsEditor::GetDocument()
{
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  return doc.forget();
}

already_AddRefed<nsIDOMDocument>
nsEditor::GetDOMDocument()
{
  NS_PRECONDITION(mDocWeak, "bad state, mDocWeak weak pointer not initialized");
  nsCOMPtr<nsIDOMDocument> doc = do_QueryReferent(mDocWeak);
  return doc.forget();
}

NS_IMETHODIMP 
nsEditor::GetDocument(nsIDOMDocument **aDoc)
{
  nsCOMPtr<nsIDOMDocument> doc = GetDOMDocument();
  doc.forget(aDoc);
  return *aDoc ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

already_AddRefed<nsIPresShell>
nsEditor::GetPresShell()
{
  NS_PRECONDITION(mDocWeak, "bad state, null mDocWeak");
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, nullptr);
  nsCOMPtr<nsIPresShell> ps = doc->GetShell();
  return ps.forget();
}

already_AddRefed<nsIWidget>
nsEditor::GetWidget()
{
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, nullptr);
  nsPresContext* pc = ps->GetPresContext();
  NS_ENSURE_TRUE(pc, nullptr);
  nsCOMPtr<nsIWidget> widget = pc->GetRootWidget();
  NS_ENSURE_TRUE(widget.get(), nullptr);
  return widget.forget();
}

/* attribute string contentsMIMEType; */
NS_IMETHODIMP
nsEditor::GetContentsMIMEType(char * *aContentsMIMEType)
{
  NS_ENSURE_ARG_POINTER(aContentsMIMEType);
  *aContentsMIMEType = ToNewCString(mContentMIMEType);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetContentsMIMEType(const char * aContentsMIMEType)
{
  mContentMIMEType.Assign(aContentsMIMEType ? aContentsMIMEType : "");
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetSelectionController(nsISelectionController **aSel)
{
  NS_ENSURE_TRUE(aSel, NS_ERROR_NULL_POINTER);
  *aSel = nullptr; // init out param
  nsCOMPtr<nsISelectionController> selCon;
  if (mSelConWeak) {
    selCon = do_QueryReferent(mSelConWeak);
  } else {
    nsCOMPtr<nsIPresShell> presShell = GetPresShell();
    selCon = do_QueryInterface(presShell);
  }
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);
  NS_ADDREF(*aSel = selCon);
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DeleteSelection(EDirection aAction, EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);
  return DeleteSelectionImpl(aAction, aStripWrappers);
}



NS_IMETHODIMP
nsEditor::GetSelection(nsISelection **aSelection)
{
  NS_ENSURE_TRUE(aSelection, NS_ERROR_NULL_POINTER);
  *aSelection = nullptr;
  nsCOMPtr<nsISelectionController> selcon;
  GetSelectionController(getter_AddRefs(selcon));
  NS_ENSURE_TRUE(selcon, NS_ERROR_NOT_INITIALIZED);
  return selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);  // does an addref
}

Selection*
nsEditor::GetSelection()
{
  nsCOMPtr<nsISelection> sel;
  nsresult res = GetSelection(getter_AddRefs(sel));
  NS_ENSURE_SUCCESS(res, nullptr);

  return static_cast<Selection*>(sel.get());
}

NS_IMETHODIMP
nsEditor::DoTransaction(nsITransaction* aTxn)
{
  if (mPlaceHolderBatch && !mPlaceHolderTxn) {
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = new PlaceholderTxn();

    // save off weak reference to placeholder txn
    mPlaceHolderTxn = do_GetWeakReference(plcTxn);
    plcTxn->Init(mPlaceHolderName, mSelState, this);
    // placeholder txn took ownership of this pointer
    mSelState = nullptr;

    // QI to an nsITransaction since that's what DoTransaction() expects
    nsCOMPtr<nsITransaction> theTxn = do_QueryInterface(plcTxn);
    // we will recurse, but will not hit this case in the nested call
    DoTransaction(theTxn);

    if (mTxnMgr) {
      nsCOMPtr<nsITransaction> topTxn = mTxnMgr->PeekUndoStack();
      if (topTxn) {
        plcTxn = do_QueryInterface(topTxn);
        if (plcTxn) {
          // there is a placeholder transaction on top of the undo stack.  It
          // is either the one we just created, or an earlier one that we are
          // now merging into.  From here on out remember this placeholder
          // instead of the one we just created.
          mPlaceHolderTxn = do_GetWeakReference(plcTxn);
        }
      }
    }
  }

  if (aTxn) {
    // XXX: Why are we doing selection specific batching stuff here?
    // XXX: Most entry points into the editor have auto variables that
    // XXX: should trigger Begin/EndUpdateViewBatch() calls that will make
    // XXX: these selection batch calls no-ops.
    // XXX:
    // XXX: I suspect that this was placed here to avoid multiple
    // XXX: selection changed notifications from happening until after
    // XXX: the transaction was done. I suppose that can still happen
    // XXX: if an embedding application called DoTransaction() directly
    // XXX: to pump its own transactions through the system, but in that
    // XXX: case, wouldn't we want to use Begin/EndUpdateViewBatch() or
    // XXX: its auto equivalent nsAutoUpdateViewBatch to ensure that
    // XXX: selection listeners have access to accurate frame data?
    // XXX:
    // XXX: Note that if we did add Begin/EndUpdateViewBatch() calls
    // XXX: we will need to make sure that they are disabled during
    // XXX: the init of the editor for text widgets to avoid layout
    // XXX: re-entry during initial reflow. - kin

    // get the selection and start a batch change
    nsRefPtr<Selection> selection = GetSelection();
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    selection->StartBatchChanges();

    nsresult res;
    if (mTxnMgr) {
      res = mTxnMgr->DoTransaction(aTxn);
    } else {
      res = aTxn->DoTransaction();
    }
    if (NS_SUCCEEDED(res)) {
      DoAfterDoTransaction(aTxn);
    }

    // no need to check res here, don't lose result of operation
    selection->EndBatchChanges();

    NS_ENSURE_SUCCESS(res, res);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::EnableUndo(bool aEnable)
{
  if (aEnable) {
    if (!mTxnMgr) {
      mTxnMgr = new nsTransactionManager();
    }
    mTxnMgr->SetMaxTransactionCount(-1);
  } else if (mTxnMgr) {
    // disable the transaction manager if it is enabled
    mTxnMgr->Clear();
    mTxnMgr->SetMaxTransactionCount(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetNumberOfUndoItems(int32_t* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfUndoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
nsEditor::GetNumberOfRedoItems(int32_t* aNumItems)
{
  *aNumItems = 0;
  return mTxnMgr ? mTxnMgr->GetNumberOfRedoItems(aNumItems) : NS_OK;
}

NS_IMETHODIMP
nsEditor::GetTransactionManager(nsITransactionManager* *aTxnManager)
{
  NS_ENSURE_ARG_POINTER(aTxnManager);
  
  *aTxnManager = nullptr;
  NS_ENSURE_TRUE(mTxnMgr, NS_ERROR_FAILURE);

  NS_ADDREF(*aTxnManager = mTxnMgr);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetTransactionManager(nsITransactionManager *aTxnManager)
{
  NS_ENSURE_TRUE(aTxnManager, NS_ERROR_FAILURE);

  // nsITransactionManager is builtinclass, so this is safe
  mTxnMgr = static_cast<nsTransactionManager*>(aTxnManager);
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::Undo(uint32_t aCount)
{
  ForceCompositionEnd();

  bool hasTxnMgr, hasTransaction = false;
  CanUndo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  nsAutoRules beginRulesSniffing(this, EditAction::undo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < aCount; ++i) {
    nsresult rv = mTxnMgr->UndoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterUndoTransaction();
  }

  return NS_OK;
}


NS_IMETHODIMP nsEditor::CanUndo(bool *aIsEnabled, bool *aCanUndo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanUndo, NS_ERROR_NULL_POINTER);
  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    int32_t numTxns = 0;
    mTxnMgr->GetNumberOfUndoItems(&numTxns);
    *aCanUndo = !!numTxns;
  } else {
    *aCanUndo = false;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::Redo(uint32_t aCount)
{
  bool hasTxnMgr, hasTransaction = false;
  CanRedo(&hasTxnMgr, &hasTransaction);
  NS_ENSURE_TRUE(hasTransaction, NS_OK);

  nsAutoRules beginRulesSniffing(this, EditAction::redo, nsIEditor::eNone);

  if (!mTxnMgr) {
    return NS_OK;
  }

  for (uint32_t i = 0; i < aCount; ++i) {
    nsresult rv = mTxnMgr->RedoTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    DoAfterRedoTransaction();
  }

  return NS_OK;
}


NS_IMETHODIMP nsEditor::CanRedo(bool *aIsEnabled, bool *aCanRedo)
{
  NS_ENSURE_TRUE(aIsEnabled && aCanRedo, NS_ERROR_NULL_POINTER);

  *aIsEnabled = !!mTxnMgr;
  if (*aIsEnabled) {
    int32_t numTxns = 0;
    mTxnMgr->GetNumberOfRedoItems(&numTxns);
    *aCanRedo = !!numTxns;
  } else {
    *aCanRedo = false;
  }
  return NS_OK;
}


NS_IMETHODIMP 
nsEditor::BeginTransaction()
{
  BeginUpdateViewBatch();

  if (mTxnMgr) {
    mTxnMgr->BeginBatch(nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndTransaction()
{
  if (mTxnMgr) {
    mTxnMgr->EndBatch(false);
  }

  EndUpdateViewBatch();

  return NS_OK;
}


// These two routines are similar to the above, but do not use
// the transaction managers batching feature.  Instead we use
// a placeholder transaction to wrap up any further transaction
// while the batch is open.  The advantage of this is that
// placeholder transactions can later merge, if needed.  Merging
// is unavailable between transaction manager batches.

NS_IMETHODIMP 
nsEditor::BeginPlaceHolderTransaction(nsIAtom *aName)
{
  NS_PRECONDITION(mPlaceHolderBatch >= 0, "negative placeholder batch count!");
  if (!mPlaceHolderBatch)
  {
    // time to turn on the batch
    BeginUpdateViewBatch();
    mPlaceHolderTxn = nullptr;
    mPlaceHolderName = aName;
    nsRefPtr<Selection> selection = GetSelection();
    if (selection) {
      mSelState = new nsSelectionState();
      mSelState->SaveSelection(selection);
    }
  }
  mPlaceHolderBatch++;

  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::EndPlaceHolderTransaction()
{
  NS_PRECONDITION(mPlaceHolderBatch > 0, "zero or negative placeholder batch count when ending batch!");
  if (mPlaceHolderBatch == 1)
  {
    nsCOMPtr<nsISelection>selection;
    GetSelection(getter_AddRefs(selection));

    nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(selection));

    // By making the assumption that no reflow happens during the calls
    // to EndUpdateViewBatch and ScrollSelectionIntoView, we are able to
    // allow the selection to cache a frame offset which is used by the
    // caret drawing code. We only enable this cache here; at other times,
    // we have no way to know whether reflow invalidates it
    // See bugs 35296 and 199412.
    if (selPrivate) {
      selPrivate->SetCanCacheFrameOffset(true);
    }

    {
      // Hide the caret here to avoid hiding it twice, once in EndUpdateViewBatch
      // and once in ScrollSelectionIntoView.
      nsRefPtr<nsCaret> caret;
      nsCOMPtr<nsIPresShell> presShell = GetPresShell();

      if (presShell)
        caret = presShell->GetCaret();

      // time to turn off the batch
      EndUpdateViewBatch();
      // make sure selection is in view

      // After ScrollSelectionIntoView(), the pending notifications might be
      // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
      ScrollSelectionIntoView(false);
    }

    // cached for frame offset are Not available now
    if (selPrivate) {
      selPrivate->SetCanCacheFrameOffset(false);
    }

    if (mSelState)
    {
      // we saved the selection state, but never got to hand it to placeholder 
      // (else we ould have nulled out this pointer), so destroy it to prevent leaks.
      delete mSelState;
      mSelState = nullptr;
    }
    if (mPlaceHolderTxn)  // we might have never made a placeholder if no action took place
    {
      nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryReferent(mPlaceHolderTxn);
      if (plcTxn) 
      {
        plcTxn->EndPlaceHolderBatch();
      }
      else  
      {
        // in the future we will check to make sure undo is off here,
        // since that is the only known case where the placeholdertxn would disappear on us.
        // For now just removing the assert.
      }
      // notify editor observers of action but if composing, it's done by
      // text event handler.
      if (!mComposition) {
        NotifyEditorObservers();
      }
    }
  }
  mPlaceHolderBatch--;
  
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ShouldTxnSetSelection(bool *aResult)
{
  NS_ENSURE_TRUE(aResult, NS_ERROR_NULL_POINTER);
  *aResult = mShouldTxnSetSelection;
  return NS_OK;
}

NS_IMETHODIMP  
nsEditor::SetShouldTxnSetSelection(bool aShould)
{
  mShouldTxnSetSelection = aShould;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentIsEmpty(bool *aDocumentIsEmpty)
{
  *aDocumentIsEmpty = true;

  dom::Element* root = GetRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_NULL_POINTER); 

  *aDocumentIsEmpty = !root->HasChildren();
  return NS_OK;
}


// XXX: the rule system should tell us which node to select all on (ie, the root, or the body)
NS_IMETHODIMP nsEditor::SelectAll()
{
  if (!mDocWeak) { return NS_ERROR_NOT_INITIALIZED; }
  ForceCompositionEnd();

  nsCOMPtr<nsISelectionController> selCon;
  GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_NOT_INITIALIZED);
  nsCOMPtr<nsISelection> selection;
  nsresult result = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_SUCCEEDED(result) && selection)
  {
    result = SelectEntireDocument(selection);
  }
  return result;
}

NS_IMETHODIMP nsEditor::BeginningOfDocument()
{
  if (!mDocWeak) { return NS_ERROR_NOT_INITIALIZED; }

  // get the selection
  nsCOMPtr<nsISelection> selection;
  nsresult result = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(selection, NS_ERROR_NOT_INITIALIZED);
    
  // get the root element 
  dom::Element* rootElement = GetRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_NULL_POINTER); 
  
  // find first editable thingy
  nsCOMPtr<nsINode> firstNode = GetFirstEditableNode(rootElement);
  if (!firstNode) {
    // just the root node, set selection to inside the root
    return selection->CollapseNative(rootElement, 0);
  }

  if (firstNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    // If firstNode is text, set selection to beginning of the text node.
    return selection->CollapseNative(firstNode, 0);
  }

  // Otherwise, it's a leaf node and we set the selection just in front of it.
  nsCOMPtr<nsIContent> parent = firstNode->GetParent();
  if (!parent) {
    return NS_ERROR_NULL_POINTER;
  }

  int32_t offsetInParent = parent->IndexOf(firstNode);
  return selection->CollapseNative(parent, offsetInParent);
}

NS_IMETHODIMP
nsEditor::EndOfDocument()
{ 
  NS_ENSURE_TRUE(mDocWeak, NS_ERROR_NOT_INITIALIZED);

  // get selection 
  nsCOMPtr<nsISelection> selection; 
  nsresult rv = GetSelection(getter_AddRefs(selection)); 
  NS_ENSURE_SUCCESS(rv, rv); 
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER); 
  
  // get the root element 
  nsINode* node = GetRoot();
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER); 
  nsINode* child = node->GetLastChild();

  while (child && IsContainer(child->AsDOMNode())) {
    node = child;
    child = node->GetLastChild();
  }

  uint32_t length = node->Length();
  return selection->CollapseNative(node, int32_t(length));
} 
  
NS_IMETHODIMP
nsEditor::GetDocumentModified(bool *outDocModified)
{
  NS_ENSURE_TRUE(outDocModified, NS_ERROR_NULL_POINTER);

  int32_t  modCount = 0;
  GetModificationCount(&modCount);

  *outDocModified = (modCount != 0);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetDocumentCharacterSet(nsACString &characterSet)
{
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  characterSet = doc->GetDocumentCharacterSet();
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetDocumentCharacterSet(const nsACString& characterSet)
{
  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  doc->SetDocumentCharacterSet(characterSet);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::Cut()
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanCut(bool *aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::Copy()
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanCopy(bool *aCanCut)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::Paste(int32_t aSelectionType)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::PasteTransferable(nsITransferable *aTransferable)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanPaste(int32_t aSelectionType, bool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsEditor::CanPasteTransferable(nsITransferable *aTransferable, bool *aCanPaste)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP 
nsEditor::SetAttribute(nsIDOMElement *aElement, const nsAString & aAttribute, const nsAString & aValue)
{
  nsRefPtr<ChangeAttributeTxn> txn;
  nsresult result = CreateTxnForSetAttribute(aElement, aAttribute, aValue,
                                             getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }
  return result;
}

NS_IMETHODIMP 
nsEditor::GetAttributeValue(nsIDOMElement *aElement, 
                            const nsAString & aAttribute, 
                            nsAString & aResultValue, 
                            bool *aResultIsSet)
{
  NS_ENSURE_TRUE(aResultIsSet, NS_ERROR_NULL_POINTER);
  *aResultIsSet = false;
  if (!aElement) {
    return NS_OK;
  }
  nsAutoString value;
  nsresult rv = aElement->GetAttribute(aAttribute, value);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!DOMStringIsNull(value)) {
    *aResultIsSet = true;
    aResultValue = value;
  }
  return rv;
}

NS_IMETHODIMP 
nsEditor::RemoveAttribute(nsIDOMElement *aElement, const nsAString& aAttribute)
{
  nsRefPtr<ChangeAttributeTxn> txn;
  nsresult result = CreateTxnForRemoveAttribute(aElement, aAttribute,
                                                getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }
  return result;
}


bool
nsEditor::OutputsMozDirty()
{
  // Return true for Composer (!eEditorAllowInteraction) or mail
  // (eEditorMailMask), but false for webpages.
  return !(mFlags & nsIPlaintextEditor::eEditorAllowInteraction) ||
          (mFlags & nsIPlaintextEditor::eEditorMailMask);
}


NS_IMETHODIMP
nsEditor::MarkNodeDirty(nsIDOMNode* aNode)
{  
  // Mark the node dirty, but not for webpages (bug 599983)
  if (!OutputsMozDirty()) {
    return NS_OK;
  }
  nsCOMPtr<dom::Element> element = do_QueryInterface(aNode);
  if (element) {
    element->SetAttr(kNameSpaceID_None, nsEditProperty::mozdirty,
                     EmptyString(), false);
  }
  return NS_OK;
}

NS_IMETHODIMP nsEditor::GetInlineSpellChecker(bool autoCreate,
                                              nsIInlineSpellChecker ** aInlineSpellChecker)
{
  NS_ENSURE_ARG_POINTER(aInlineSpellChecker);

  if (mDidPreDestroy) {
    // Don't allow people to get or create the spell checker once the editor
    // is going away.
    *aInlineSpellChecker = nullptr;
    return autoCreate ? NS_ERROR_NOT_AVAILABLE : NS_OK;
  }

  // We don't want to show the spell checking UI if there are no spell check dictionaries available.
  bool canSpell = mozInlineSpellChecker::CanEnableInlineSpellChecking();
  if (!canSpell) {
    *aInlineSpellChecker = nullptr;
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (!mInlineSpellChecker && autoCreate) {
    mInlineSpellChecker = do_CreateInstance(MOZ_INLINESPELLCHECKER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mInlineSpellChecker) {
    rv = mInlineSpellChecker->Init(this);
    if (NS_FAILED(rv))
      mInlineSpellChecker = nullptr;
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_IF_ADDREF(*aInlineSpellChecker = mInlineSpellChecker);

  return NS_OK;
}

NS_IMETHODIMP nsEditor::Observe(nsISupports* aSubj, const char *aTopic,
                                const char16_t *aData)
{
  NS_ASSERTION(!strcmp(aTopic,
                       SPELLCHECK_DICTIONARY_UPDATE_NOTIFICATION),
               "Unexpected observer topic");

  // When mozInlineSpellChecker::CanEnableInlineSpellChecking changes
  SyncRealTimeSpell();

  // When nsIEditorSpellCheck::GetCurrentDictionary changes
  if (mInlineSpellChecker) {
    // if the current dictionary is no longer available, find another one
    nsCOMPtr<nsIEditorSpellCheck> editorSpellCheck;
    mInlineSpellChecker->GetSpellChecker(getter_AddRefs(editorSpellCheck));
    if (editorSpellCheck) {
      // Note: This might change the current dictionary, which may call
      // this observer recursively.
      editorSpellCheck->CheckCurrentDictionary();
    }

    // update the inline spell checker to reflect the new current dictionary
    mInlineSpellChecker->SpellCheckRange(nullptr); // causes recheck
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditor::SyncRealTimeSpell()
{
  bool enable = GetDesiredSpellCheckState();

  // Initializes mInlineSpellChecker
  nsCOMPtr<nsIInlineSpellChecker> spellChecker;
  GetInlineSpellChecker(enable, getter_AddRefs(spellChecker));

  if (mInlineSpellChecker) {
    // We might have a mInlineSpellChecker even if there are no dictionaries
    // available since we don't destroy the mInlineSpellChecker when the last
    // dictionariy is removed, but in that case spellChecker is null
    mInlineSpellChecker->SetEnableRealTimeSpell(enable && spellChecker);
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditor::SetSpellcheckUserOverride(bool enable)
{
  mSpellcheckCheckboxState = enable ? eTriTrue : eTriFalse;

  return SyncRealTimeSpell();
}

NS_IMETHODIMP nsEditor::CreateNode(const nsAString& aTag,
                                   nsIDOMNode *    aParent,
                                   int32_t         aPosition,
                                   nsIDOMNode **   aNewNode)
{
  int32_t i;

  nsAutoRules beginRulesSniffing(this, EditAction::createNode, nsIEditor::eNext);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillCreateNode(aTag, aParent, aPosition);

  nsRefPtr<CreateElementTxn> txn;
  nsresult result = CreateTxnForCreateElement(aTag, aParent, aPosition,
                                              getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))
  {
    result = DoTransaction(txn);
    if (NS_SUCCEEDED(result))
    {
      result = txn->GetNewNode(aNewNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "GetNewNode can't fail if txn::DoTransaction succeeded.");
    }
  }

  mRangeUpdater.SelAdjCreateNode(aParent, aPosition);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidCreateNode(aTag, *aNewNode, aParent, aPosition, result);

  return result;
}


nsresult
nsEditor::InsertNode(nsIContent* aContent, nsINode* aParent, int32_t aPosition)
{
  MOZ_ASSERT(aContent && aParent);
  return InsertNode(GetAsDOMNode(aContent), GetAsDOMNode(aParent), aPosition);
}

NS_IMETHODIMP nsEditor::InsertNode(nsIDOMNode * aNode,
                                   nsIDOMNode * aParent,
                                   int32_t      aPosition)
{
  int32_t i;
  nsAutoRules beginRulesSniffing(this, EditAction::insertNode, nsIEditor::eNext);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillInsertNode(aNode, aParent, aPosition);

  nsRefPtr<InsertElementTxn> txn;
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  nsresult result = CreateTxnForInsertElement(node->AsDOMNode(), parent->AsDOMNode(),
                                              aPosition, getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);
  }

  mRangeUpdater.SelAdjInsertNode(aParent, aPosition);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidInsertNode(aNode, aParent, aPosition, result);

  return result;
}


NS_IMETHODIMP 
nsEditor::SplitNode(nsIDOMNode * aNode,
                    int32_t      aOffset,
                    nsIDOMNode **aNewLeftNode)
{
  int32_t i;
  nsAutoRules beginRulesSniffing(this, EditAction::splitNode, nsIEditor::eNext);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillSplitNode(aNode, aOffset);

  nsRefPtr<SplitElementTxn> txn;
  nsresult result = CreateTxnForSplitNode(aNode, aOffset, getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))
  {
    result = DoTransaction(txn);
    if (NS_SUCCEEDED(result))
    {
      result = txn->GetNewNode(aNewLeftNode);
      NS_ASSERTION((NS_SUCCEEDED(result)), "result must succeeded for GetNewNode");
    }
  }

  mRangeUpdater.SelAdjSplitNode(aNode, aOffset, *aNewLeftNode);

  for (i = 0; i < mActionListeners.Count(); i++)
  {
    nsIDOMNode *ptr = *aNewLeftNode;
    mActionListeners[i]->DidSplitNode(aNode, aOffset, ptr, result);
  }

  return result;
}


nsresult
nsEditor::JoinNodes(nsINode* aNodeToKeep, nsIContent* aNodeToMove)
{
  // We don't really need aNodeToMove's parent to be non-null -- we could just
  // skip adjusting any ranges in aNodeToMove's parent if there is none.  But
  // the current implementation requires it.
  MOZ_ASSERT(aNodeToKeep && aNodeToMove && aNodeToMove->GetParentNode());
  nsresult res = JoinNodes(aNodeToKeep->AsDOMNode(), aNodeToMove->AsDOMNode(),
                           aNodeToMove->GetParentNode()->AsDOMNode());
  NS_ASSERTION(NS_SUCCEEDED(res), "JoinNodes failed");
  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::JoinNodes(nsIDOMNode * aLeftNode,
                    nsIDOMNode * aRightNode,
                    nsIDOMNode * aParent)
{
  int32_t i;
  nsAutoRules beginRulesSniffing(this, EditAction::joinNode, nsIEditor::ePrevious);

  // remember some values; later used for saved selection updating.
  // find the offset between the nodes to be joined.
  int32_t offset = GetChildOffset(aRightNode, aParent);
  // find the number of children of the lefthand node
  uint32_t oldLeftNodeLen;
  nsresult result = GetLengthOfDOMNode(aLeftNode, oldLeftNodeLen);
  NS_ENSURE_SUCCESS(result, result);

  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillJoinNodes(aLeftNode, aRightNode, aParent);

  nsRefPtr<JoinElementTxn> txn;
  result = CreateTxnForJoinNode(aLeftNode, aRightNode, getter_AddRefs(txn));
  if (NS_SUCCEEDED(result))  {
    result = DoTransaction(txn);  
  }

  mRangeUpdater.SelAdjJoinNodes(aLeftNode, aRightNode, aParent, offset, (int32_t)oldLeftNodeLen);
  
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidJoinNodes(aLeftNode, aRightNode, aParent, result);

  return result;
}


NS_IMETHODIMP
nsEditor::DeleteNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);
  return DeleteNode(node);
}

nsresult
nsEditor::DeleteNode(nsINode* aNode)
{
  nsAutoRules beginRulesSniffing(this, EditAction::createNode, nsIEditor::ePrevious);

  // save node location for selection updating code.
  for (int32_t i = 0; i < mActionListeners.Count(); i++) {
    mActionListeners[i]->WillDeleteNode(aNode->AsDOMNode());
  }

  nsRefPtr<DeleteNodeTxn> txn;
  nsresult res = CreateTxnForDeleteNode(aNode, getter_AddRefs(txn));
  if (NS_SUCCEEDED(res))  {
    res = DoTransaction(txn);
  }

  for (int32_t i = 0; i < mActionListeners.Count(); i++) {
    mActionListeners[i]->DidDeleteNode(aNode->AsDOMNode(), res);
  }

  NS_ENSURE_SUCCESS(res, res);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////
// ReplaceContainer: replace inNode with a new node (outNode) which is contructed 
//                   to be of type aNodeType.  Put inNodes children into outNode.
//                   Callers responsibility to make sure inNode's children can 
//                   go in outNode.
nsresult
nsEditor::ReplaceContainer(nsIDOMNode *inNode, 
                           nsCOMPtr<nsIDOMNode> *outNode, 
                           const nsAString &aNodeType,
                           const nsAString *aAttribute,
                           const nsAString *aValue,
                           bool aCloneAttributes)
{
  NS_ENSURE_TRUE(inNode && outNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  NS_ENSURE_STATE(node);

  nsCOMPtr<dom::Element> element;
  nsresult rv = ReplaceContainer(node, getter_AddRefs(element), aNodeType,
                                 aAttribute, aValue, aCloneAttributes);
  *outNode = element ? element->AsDOMNode() : nullptr;
  return rv;
}

nsresult
nsEditor::ReplaceContainer(nsINode* aNode,
                           dom::Element** outNode,
                           const nsAString& aNodeType,
                           const nsAString* aAttribute,
                           const nsAString* aValue,
                           bool aCloneAttributes)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(outNode);

  *outNode = nullptr;

  nsCOMPtr<nsIContent> parent = aNode->GetParent();
  NS_ENSURE_STATE(parent);

  int32_t offset = parent->IndexOf(aNode);

  // create new container
  ErrorResult rv;
  *outNode = CreateHTMLContent(aNodeType, rv).take();
  NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());

  nsCOMPtr<nsIDOMElement> elem = do_QueryInterface(*outNode);
  
  nsIDOMNode* inNode = aNode->AsDOMNode();

  // set attribute if needed
  nsresult res;
  if (aAttribute && aValue && !aAttribute->IsEmpty()) {
    res = elem->SetAttribute(*aAttribute, *aValue);
    NS_ENSURE_SUCCESS(res, res);
  }
  if (aCloneAttributes) {
    res = CloneAttributes(elem, inNode);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // notify our internal selection state listener
  // (Note: A nsAutoSelectionReset object must be created
  //  before calling this to initialize mRangeUpdater)
  nsAutoReplaceContainerSelNotify selStateNotify(mRangeUpdater, inNode, elem);
  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    while (aNode->HasChildren()) {
      nsCOMPtr<nsIDOMNode> child = aNode->GetFirstChild()->AsDOMNode();

      res = DeleteNode(child);
      NS_ENSURE_SUCCESS(res, res);

      res = InsertNode(child, elem, -1);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  // insert new container into tree
  res = InsertNode(elem, parent->AsDOMNode(), offset);
  NS_ENSURE_SUCCESS(res, res);
  
  // delete old container
  return DeleteNode(inNode);
}

///////////////////////////////////////////////////////////////////////////
// RemoveContainer: remove inNode, reparenting its children into their
//                  the parent of inNode
//
nsresult
nsEditor::RemoveContainer(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return RemoveContainer(node);
}

nsresult
nsEditor::RemoveContainer(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> parent = aNode->GetParentNode();
  NS_ENSURE_STATE(parent);

  int32_t offset = parent->IndexOf(aNode);
  
  // loop through the child nodes of inNode and promote them
  // into inNode's parent.
  uint32_t nodeOrigLen = aNode->GetChildCount();

  // notify our internal selection state listener
  nsAutoRemoveContainerSelNotify selNotify(mRangeUpdater, aNode, parent, offset, nodeOrigLen);
                                   
  while (aNode->HasChildren()) {
    nsCOMPtr<nsIContent> child = aNode->GetLastChild();
    nsresult rv = DeleteNode(child->AsDOMNode());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InsertNode(child->AsDOMNode(), parent->AsDOMNode(), offset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return DeleteNode(aNode->AsDOMNode());
}


///////////////////////////////////////////////////////////////////////////
// InsertContainerAbove:  insert a new parent for inNode, returned in outNode,
//                   which is contructed to be of type aNodeType.  outNode becomes
//                   a child of inNode's earlier parent.
//                   Callers responsibility to make sure inNode's can be child
//                   of outNode, and outNode can be child of old parent.
nsresult
nsEditor::InsertContainerAbove( nsIDOMNode *inNode, 
                                nsCOMPtr<nsIDOMNode> *outNode, 
                                const nsAString &aNodeType,
                                const nsAString *aAttribute,
                                const nsAString *aValue)
{
  NS_ENSURE_TRUE(inNode && outNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIContent> node = do_QueryInterface(inNode);
  NS_ENSURE_STATE(node);

  nsCOMPtr<dom::Element> element;
  nsresult rv = InsertContainerAbove(node, getter_AddRefs(element), aNodeType,
                                     aAttribute, aValue);
  *outNode = element ? element->AsDOMNode() : nullptr;
  return rv;
}

nsresult
nsEditor::InsertContainerAbove(nsIContent* aNode,
                               dom::Element** aOutNode,
                               const nsAString& aNodeType,
                               const nsAString* aAttribute,
                               const nsAString* aValue)
{
  MOZ_ASSERT(aNode);

  nsCOMPtr<nsIContent> parent = aNode->GetParent();
  NS_ENSURE_STATE(parent);
  int32_t offset = parent->IndexOf(aNode);

  // create new container
  ErrorResult rv;
  nsCOMPtr<Element> newContent = CreateHTMLContent(aNodeType, rv);
  NS_ENSURE_SUCCESS(rv.ErrorCode(), rv.ErrorCode());

  // set attribute if needed
  nsresult res;
  if (aAttribute && aValue && !aAttribute->IsEmpty()) {
    nsIDOMNode* elem = newContent->AsDOMNode();
    res = static_cast<nsIDOMElement*>(elem)->SetAttribute(*aAttribute, *aValue);
    NS_ENSURE_SUCCESS(res, res);
  }
  
  // notify our internal selection state listener
  nsAutoInsertContainerSelNotify selNotify(mRangeUpdater);
  
  // put inNode in new parent, outNode
  res = DeleteNode(aNode->AsDOMNode());
  NS_ENSURE_SUCCESS(res, res);

  {
    nsAutoTxnsConserveSelection conserveSelection(this);
    res = InsertNode(aNode->AsDOMNode(), newContent->AsDOMNode(), 0);
    NS_ENSURE_SUCCESS(res, res);
  }

  // put new parent in doc
  res = InsertNode(newContent->AsDOMNode(), parent->AsDOMNode(), offset);
  newContent.forget(aOutNode);
  return res;  
}

///////////////////////////////////////////////////////////////////////////
// MoveNode:  move aNode to {aParent,aOffset}
nsresult
nsEditor::MoveNode(nsIDOMNode* aNode, nsIDOMNode* aParent, int32_t aOffset)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_STATE(node);

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  NS_ENSURE_STATE(parent);

  return MoveNode(node, parent, aOffset);
}

nsresult
nsEditor::MoveNode(nsINode* aNode, nsINode* aParent, int32_t aOffset)
{
  MOZ_ASSERT(aNode);
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aOffset == -1 ||
             (0 <= aOffset && SafeCast<uint32_t>(aOffset) <= aParent->Length()));

  int32_t oldOffset;
  nsCOMPtr<nsINode> oldParent = GetNodeLocation(aNode, &oldOffset);
  
  if (aOffset == -1) {
    // Magic value meaning "move to end of aParent".
    aOffset = SafeCast<int32_t>(aParent->Length());
  }
  
  // Don't do anything if it's already in right place.
  if (aParent == oldParent && aOffset == oldOffset) {
    return NS_OK;
  }
  
  // Notify our internal selection state listener.
  nsAutoMoveNodeSelNotify selNotify(mRangeUpdater, oldParent, oldOffset,
                                    aParent, aOffset);
  
  // Need to adjust aOffset if we are moving aNode further along in its current
  // parent.
  if (aParent == oldParent && oldOffset < aOffset) {
    // This is because when we delete aNode, it will make the offsets after it
    // off by one.
    aOffset--;
  }

  // Hold a reference so aNode doesn't go away when we remove it (bug 772282).
  nsCOMPtr<nsINode> kungFuDeathGrip = aNode;

  nsresult rv = DeleteNode(aNode);
  NS_ENSURE_SUCCESS(rv, rv);

  return InsertNode(aNode->AsDOMNode(), aParent->AsDOMNode(), aOffset);
}


NS_IMETHODIMP
nsEditor::AddEditorObserver(nsIEditorObserver *aObserver)
{
  // we don't keep ownership of the observers.  They must
  // remove themselves as observers before they are destroyed.
  
  NS_ENSURE_TRUE(aObserver, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (mEditorObservers.IndexOf(aObserver) == -1) 
  {
    if (!mEditorObservers.AppendObject(aObserver))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditorObserver(nsIEditorObserver *aObserver)
{
  NS_ENSURE_TRUE(aObserver, NS_ERROR_FAILURE);

  if (!mEditorObservers.RemoveObject(aObserver))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

class EditorInputEventDispatcher : public nsRunnable
{
public:
  EditorInputEventDispatcher(nsEditor* aEditor,
                             nsIContent* aTarget,
                             bool aIsComposing)
    : mEditor(aEditor)
    , mTarget(aTarget)
    , mIsComposing(aIsComposing)
  {
  }

  NS_IMETHOD Run()
  {
    // Note that we don't need to check mDispatchInputEvent here.  We need
    // to check it only when the editor requests to dispatch the input event.

    if (!mTarget->IsInDoc()) {
      return NS_OK;
    }

    nsCOMPtr<nsIPresShell> ps = mEditor->GetPresShell();
    if (!ps) {
      return NS_OK;
    }

    nsCOMPtr<nsIWidget> widget = mEditor->GetWidget();
    if (!widget) {
      return NS_OK;
    }

    // Even if the change is caused by untrusted event, we need to dispatch
    // trusted input event since it's a fact.
    InternalEditorInputEvent inputEvent(true, NS_EDITOR_INPUT, widget);
    inputEvent.time = static_cast<uint64_t>(PR_Now() / 1000);
    inputEvent.mIsComposing = mIsComposing;
    nsEventStatus status = nsEventStatus_eIgnore;
    nsresult rv =
      ps->HandleEventWithTarget(&inputEvent, nullptr, mTarget, &status);
    NS_ENSURE_SUCCESS(rv, NS_OK); // print the warning if error
    return NS_OK;
  }

private:
  nsRefPtr<nsEditor> mEditor;
  nsCOMPtr<nsIContent> mTarget;
  bool mIsComposing;
};

void nsEditor::NotifyEditorObservers(void)
{
  for (int32_t i = 0; i < mEditorObservers.Count(); i++) {
    mEditorObservers[i]->EditAction();
  }

  if (!mDispatchInputEvent) {
    return;
  }

  FireInputEvent();
}

void
nsEditor::FireInputEvent()
{
  // We don't need to dispatch multiple input events if there is a pending
  // input event.  However, it may have different event target.  If we resolved
  // this issue, we need to manage the pending events in an array.  But it's
  // overwork.  We don't need to do it for the very rare case.

  nsCOMPtr<nsIContent> target = GetInputEventTargetContent();
  NS_ENSURE_TRUE_VOID(target);

  // NOTE: Don't refer IsIMEComposing() because it returns false even before
  //       compositionend.  However, DOM Level 3 Events defines it should be
  //       true after compositionstart and before compositionend.
  nsContentUtils::AddScriptRunner(
    new EditorInputEventDispatcher(this, target, !!GetComposition()));
}

NS_IMETHODIMP
nsEditor::AddEditActionListener(nsIEditActionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  // Make sure the listener isn't already on the list
  if (mActionListeners.IndexOf(aListener) == -1) 
  {
    if (!mActionListeners.AppendObject(aListener))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveEditActionListener(nsIEditActionListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

  if (!mActionListeners.RemoveObject(aListener))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::AddDocumentStateListener(nsIDocumentStateListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  if (mDocStateListeners.IndexOf(aListener) == -1)
  {
    if (!mDocStateListeners.AppendObject(aListener))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsEditor::RemoveDocumentStateListener(nsIDocumentStateListener *aListener)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_NULL_POINTER);

  if (!mDocStateListeners.RemoveObject(aListener))
    return NS_ERROR_FAILURE;

  return NS_OK;
}


NS_IMETHODIMP nsEditor::OutputToString(const nsAString& aFormatType,
                                       uint32_t aFlags,
                                       nsAString& aOutputString)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::OutputToStream(nsIOutputStream* aOutputStream,
                         const nsAString& aFormatType,
                         const nsACString& aCharsetOverride,
                         uint32_t aFlags)
{
  // these should be implemented by derived classes.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEditor::DumpContentTree()
{
#ifdef DEBUG
  if (mRootElement) {
    mRootElement->List(stdout);
  }
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugDumpContent()
{
#ifdef DEBUG
  nsCOMPtr<nsIDOMHTMLDocument> doc = do_QueryReferent(mDocWeak);
  NS_ENSURE_TRUE(doc, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIDOMHTMLElement>bodyElem;
  doc->GetBody(getter_AddRefs(bodyElem));
  nsCOMPtr<nsIContent> content = do_QueryInterface(bodyElem);
  if (content)
    content->List();
#endif
  return NS_OK;
}


NS_IMETHODIMP
nsEditor::DebugUnitTests(int32_t *outNumTests, int32_t *outNumTestsFailed)
{
#ifdef DEBUG
  NS_NOTREACHED("This should never get called. Overridden by subclasses");
#endif
  return NS_OK;
}


bool     
nsEditor::ArePreservingSelection()
{
  return !(mSavedSel.IsEmpty());
}

void
nsEditor::PreserveSelectionAcrossActions(Selection* aSel)
{
  mSavedSel.SaveSelection(aSel);
  mRangeUpdater.RegisterSelectionState(mSavedSel);
}

nsresult 
nsEditor::RestorePreservedSelection(nsISelection *aSel)
{
  if (mSavedSel.IsEmpty()) return NS_ERROR_FAILURE;
  mSavedSel.RestoreSelection(aSel);
  StopPreservingSelection();
  return NS_OK;
}

void     
nsEditor::StopPreservingSelection()
{
  mRangeUpdater.DropSelectionState(mSavedSel);
  mSavedSel.MakeEmpty();
}

void
nsEditor::EnsureComposition(mozilla::WidgetGUIEvent* aEvent)
{
  if (mComposition) {
    return;
  }
  // The compositionstart event must cause creating new TextComposition
  // instance at being dispatched by IMEStateManager.
  mComposition = IMEStateManager::GetTextCompositionFor(aEvent);
  if (!mComposition) {
    MOZ_CRASH("IMEStateManager doesn't return proper composition");
  }
  mComposition->StartHandlingComposition(this);
}

nsresult
nsEditor::BeginIMEComposition(WidgetCompositionEvent* aCompositionEvent)
{
  MOZ_ASSERT(!mComposition, "There is composition already");
  EnsureComposition(aCompositionEvent);
  if (mPhonetic) {
    mPhonetic->Truncate(0);
  }
  return NS_OK;
}

void
nsEditor::EndIMEComposition()
{
  NS_ENSURE_TRUE_VOID(mComposition); // nothing to do

  // commit the IME transaction..we can get at it via the transaction mgr.
  // Note that this means IME won't work without an undo stack!
  if (mTxnMgr) {
    nsCOMPtr<nsITransaction> txn = mTxnMgr->PeekUndoStack();
    nsCOMPtr<nsIAbsorbingTransaction> plcTxn = do_QueryInterface(txn);
    if (plcTxn) {
      DebugOnly<nsresult> rv = plcTxn->Commit();
      NS_ASSERTION(NS_SUCCEEDED(rv),
                   "nsIAbsorbingTransaction::Commit() failed");
    }
  }

  /* reset the data we need to construct a transaction */
  mIMETextNode = nullptr;
  mIMETextOffset = 0;
  mComposition->EndHandlingComposition(this);
  mComposition = nullptr;

  // notify editor observers of action
  NotifyEditorObservers();
}


NS_IMETHODIMP
nsEditor::GetPhonetic(nsAString& aPhonetic)
{
  if (mPhonetic)
    aPhonetic = *mPhonetic;
  else
    aPhonetic.Truncate(0);

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::ForceCompositionEnd()
{
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  if (!ps) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsPresContext* pc = ps->GetPresContext();
  if (!pc) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mComposition) {
    // XXXmnakano see bug 558976, ResetInputState() has two meaning which are
    // "commit the composition" and "cursor is moved".  This method name is
    // "ForceCompositionEnd", so, ResetInputState() should be used only for the
    // former here.  However, ResetInputState() is also used for the latter here
    // because even if we don't have composition, we call ResetInputState() on
    // Linux.  Currently, nsGtkIMModule can know the timing of the cursor move,
    // so, the latter meaning should be gone.
    // XXX This may commit a composition in another editor.
    return IMEStateManager::NotifyIME(NOTIFY_IME_OF_CURSOR_POS_CHANGED, pc);
  }

  return IMEStateManager::NotifyIME(REQUEST_TO_COMMIT_COMPOSITION, pc);
}

NS_IMETHODIMP
nsEditor::GetPreferredIMEState(IMEState *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  aState->mEnabled = IMEState::ENABLED;
  aState->mOpen = IMEState::DONT_CHANGE_OPEN_STATE;

  if (IsReadonly() || IsDisabled()) {
    aState->mEnabled = IMEState::DISABLED;
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = GetRoot();
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsIFrame* frame = content->GetPrimaryFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  switch (frame->StyleUIReset()->mIMEMode) {
    case NS_STYLE_IME_MODE_AUTO:
      if (IsPasswordEditor())
        aState->mEnabled = IMEState::PASSWORD;
      break;
    case NS_STYLE_IME_MODE_DISABLED:
      // we should use password state for |ime-mode: disabled;|.
      aState->mEnabled = IMEState::PASSWORD;
      break;
    case NS_STYLE_IME_MODE_ACTIVE:
      aState->mOpen = IMEState::OPEN;
      break;
    case NS_STYLE_IME_MODE_INACTIVE:
      aState->mOpen = IMEState::CLOSED;
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::GetComposing(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = IsIMEComposing();
  return NS_OK;
}


/* Non-interface, public methods */

NS_IMETHODIMP
nsEditor::GetRootElement(nsIDOMElement **aRootElement)
{
  NS_ENSURE_ARG_POINTER(aRootElement);
  NS_ENSURE_TRUE(mRootElement, NS_ERROR_NOT_AVAILABLE);
  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(mRootElement);
  rootElement.forget(aRootElement);
  return NS_OK;
}


/** All editor operations which alter the doc should be prefaced
 *  with a call to StartOperation, naming the action and direction */
NS_IMETHODIMP
nsEditor::StartOperation(EditAction opID, nsIEditor::EDirection aDirection)
{
  mAction = opID;
  mDirection = aDirection;
  return NS_OK;
}


/** All editor operations which alter the doc should be followed
 *  with a call to EndOperation */
NS_IMETHODIMP
nsEditor::EndOperation()
{
  mAction = EditAction::none;
  mDirection = eNone;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::CloneAttribute(const nsAString & aAttribute,
                         nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  NS_ENSURE_TRUE(aDestNode && aSourceNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  NS_ENSURE_TRUE(destElement && sourceElement, NS_ERROR_NO_INTERFACE);

  nsAutoString attrValue;
  bool isAttrSet;
  nsresult rv = GetAttributeValue(sourceElement,
                                  aAttribute,
                                  attrValue,
                                  &isAttrSet);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isAttrSet)
    rv = SetAttribute(destElement, aAttribute, attrValue);
  else
    rv = RemoveAttribute(destElement, aAttribute);

  return rv;
}

// Objects must be DOM elements
NS_IMETHODIMP
nsEditor::CloneAttributes(nsIDOMNode *aDestNode, nsIDOMNode *aSourceNode)
{
  NS_ENSURE_TRUE(aDestNode && aSourceNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsIDOMElement> destElement = do_QueryInterface(aDestNode);
  nsCOMPtr<nsIDOMElement> sourceElement = do_QueryInterface(aSourceNode);
  NS_ENSURE_TRUE(destElement && sourceElement, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIDOMMozNamedAttrMap> sourceAttributes;
  sourceElement->GetAttributes(getter_AddRefs(sourceAttributes));
  nsCOMPtr<nsIDOMMozNamedAttrMap> destAttributes;
  destElement->GetAttributes(getter_AddRefs(destAttributes));
  NS_ENSURE_TRUE(sourceAttributes && destAttributes, NS_ERROR_FAILURE);

  nsAutoEditBatch beginBatching(this);

  // Use transaction system for undo only if destination
  //   is already in the document
  nsCOMPtr<nsIDOMNode> p = aDestNode;
  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetRoot());
  NS_ENSURE_TRUE(rootNode, NS_ERROR_NULL_POINTER);
  bool destInBody = true;
  while (p && p != rootNode)
  {
    nsCOMPtr<nsIDOMNode> tmp;
    if (NS_FAILED(p->GetParentNode(getter_AddRefs(tmp))) || !tmp)
    {
      destInBody = false;
      break;
    }
    p = tmp;
  }

  uint32_t sourceCount;
  sourceAttributes->GetLength(&sourceCount);
  uint32_t destCount;
  destAttributes->GetLength(&destCount);
  nsCOMPtr<nsIDOMAttr> attr;

  // Clear existing attributes
  for (uint32_t i = 0; i < destCount; i++) {
    // always remove item number 0 (first item in list)
    if (NS_SUCCEEDED(destAttributes->Item(0, getter_AddRefs(attr))) && attr) {
      nsString str;
      if (NS_SUCCEEDED(attr->GetName(str))) {
        if (destInBody) {
          RemoveAttribute(destElement, str);
        } else {
          destElement->RemoveAttribute(str);
        }
      }
    }
  }

  nsresult result = NS_OK;

  // Set just the attributes that the source element has
  for (uint32_t i = 0; i < sourceCount; i++)
  {
    if (NS_SUCCEEDED(sourceAttributes->Item(i, getter_AddRefs(attr))) && attr) {
      nsString sourceAttrName;
      if (NS_SUCCEEDED(attr->GetName(sourceAttrName))) {
        nsString sourceAttrValue;
        /* Presence of an attribute in the named node map indicates that it was
         * set on the element even if it has no value.
         */
        if (NS_SUCCEEDED(attr->GetValue(sourceAttrValue))) {
          if (destInBody) {
            result = SetAttributeOrEquivalent(destElement, sourceAttrName, sourceAttrValue, false);
          } else {
            // the element is not inserted in the document yet, we don't want to put a
            // transaction on the UndoStack
            result = SetAttributeOrEquivalent(destElement, sourceAttrName, sourceAttrValue, true);
          }
        } else {
          // Do we ever get here?
#if DEBUG_cmanske
          printf("Attribute in sourceAttribute has empty value in nsEditor::CloneAttributes()\n");
#endif
        }
      }
    }
  }
  return result;
}


NS_IMETHODIMP nsEditor::ScrollSelectionIntoView(bool aScrollToAnchor)
{
  nsCOMPtr<nsISelectionController> selCon;
  if (NS_SUCCEEDED(GetSelectionController(getter_AddRefs(selCon))) && selCon)
  {
    int16_t region = nsISelectionController::SELECTION_FOCUS_REGION;

    if (aScrollToAnchor)
      region = nsISelectionController::SELECTION_ANCHOR_REGION;

    selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
      region, nsISelectionController::SCROLL_OVERFLOW_HIDDEN);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::InsertTextImpl(const nsAString& aStringToInsert,
                         nsCOMPtr<nsIDOMNode>* aInOutNode,
                         int32_t* aInOutOffset,
                         nsIDOMDocument* aDoc)
{
  // NOTE: caller *must* have already used nsAutoTxnsConserveSelection
  // stack-based class to turn off txn selection updating.  Caller also turned
  // on rules sniffing if desired.

  NS_ENSURE_TRUE(aInOutNode && *aInOutNode && aInOutOffset && aDoc,
                 NS_ERROR_NULL_POINTER);
  if (!mComposition && aStringToInsert.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> node = do_QueryInterface(*aInOutNode);
  NS_ENSURE_STATE(node);
  uint32_t offset = static_cast<uint32_t>(*aInOutOffset);

  if (!node->IsNodeOfType(nsINode::eTEXT) && IsPlaintextEditor()) {
    nsCOMPtr<nsINode> root = GetRoot();
    // In some cases, node is the anonymous DIV, and offset is 0.  To avoid
    // injecting unneeded text nodes, we first look to see if we have one
    // available.  In that case, we'll just adjust node and offset accordingly.
    if (node == root && offset == 0 && node->HasChildren() &&
        node->GetFirstChild()->IsNodeOfType(nsINode::eTEXT)) {
      node = node->GetFirstChild();
    }
    // In some other cases, node is the anonymous DIV, and offset points to the
    // terminating mozBR.  In that case, we'll adjust aInOutNode and
    // aInOutOffset to the preceding text node, if any.
    if (node == root && offset > 0 && node->GetChildAt(offset - 1) &&
        node->GetChildAt(offset - 1)->IsNodeOfType(nsINode::eTEXT)) {
      node = node->GetChildAt(offset - 1);
      offset = node->Length();
    }
    // Sometimes, node is the mozBR element itself.  In that case, we'll adjust
    // the insertion point to the previous text node, if one exists, or to the
    // parent anonymous DIV.
    if (nsTextEditUtils::IsMozBR(node) && offset == 0) {
      if (node->GetPreviousSibling() &&
          node->GetPreviousSibling()->IsNodeOfType(nsINode::eTEXT)) {
        node = node->GetPreviousSibling();
        offset = node->Length();
      } else if (node->GetParentNode() && node->GetParentNode() == root) {
        node = node->GetParentNode();
      }
    }
  }

  nsresult res;
  if (mComposition) {
    if (!node->IsNodeOfType(nsINode::eTEXT)) {
      // create a text node
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
      NS_ENSURE_STATE(doc);
      nsRefPtr<nsTextNode> newNode = doc->CreateTextNode(EmptyString());
      // then we insert it into the dom tree
      res = InsertNode(newNode->AsDOMNode(), node->AsDOMNode(), offset);
      NS_ENSURE_SUCCESS(res, res);
      node = newNode;
      offset = 0;
    }
    nsCOMPtr<nsIDOMCharacterData> charDataNode = do_QueryInterface(node);
    NS_ENSURE_STATE(charDataNode);
    res = InsertTextIntoTextNodeImpl(aStringToInsert, charDataNode, offset);
    NS_ENSURE_SUCCESS(res, res);
    offset += aStringToInsert.Length();
  } else {
    if (node->IsNodeOfType(nsINode::eTEXT)) {
      // we are inserting text into an existing text node.
      nsCOMPtr<nsIDOMCharacterData> charDataNode = do_QueryInterface(node);
      NS_ENSURE_STATE(charDataNode);
      res = InsertTextIntoTextNodeImpl(aStringToInsert, charDataNode, offset);
      NS_ENSURE_SUCCESS(res, res);
      offset += aStringToInsert.Length();
    } else {
      // we are inserting text into a non-text node.  first we have to create a
      // textnode (this also populates it with the text)
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
      NS_ENSURE_STATE(doc);
      nsRefPtr<nsTextNode> newNode = doc->CreateTextNode(aStringToInsert);
      // then we insert it into the dom tree
      res = InsertNode(newNode->AsDOMNode(), node->AsDOMNode(), offset);
      NS_ENSURE_SUCCESS(res, res);
      node = newNode;
      offset = aStringToInsert.Length();
    }
  }

  *aInOutNode = node->AsDOMNode();
  *aInOutOffset = static_cast<int32_t>(offset);
  return NS_OK;
}


nsresult nsEditor::InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert,
                                              nsINode* aTextNode,
                                              int32_t aOffset,
                                              bool aSuppressIME)
{
  return InsertTextIntoTextNodeImpl(aStringToInsert,
      static_cast<nsIDOMCharacterData*>(GetAsDOMNode(aTextNode)),
      aOffset, aSuppressIME);
}

nsresult nsEditor::InsertTextIntoTextNodeImpl(const nsAString& aStringToInsert, 
                                              nsIDOMCharacterData *aTextNode, 
                                              int32_t aOffset,
                                              bool aSuppressIME)
{
  nsRefPtr<EditTxn> txn;
  nsresult result = NS_OK;
  bool isIMETransaction = false;
  // aSuppressIME is used when editor must insert text, yet this text is not
  // part of current ime operation.  example: adjusting whitespace around an ime insertion.
  if (mComposition && !aSuppressIME) {
    if (!mIMETextNode) {
      mIMETextNode = aTextNode;
      mIMETextOffset = aOffset;
    }
    // Modify mPhonetic with raw text input clauses.
    const TextRangeArray* ranges = mComposition->GetRanges();
    for (uint32_t i = 0; i < (ranges ? ranges->Length() : 0); ++i) {
      const TextRange& textRange = ranges->ElementAt(i);
      if (!textRange.Length() ||
          textRange.mRangeType != NS_TEXTRANGE_RAWINPUT) {
        continue;
      }
      if (!mPhonetic) {
        mPhonetic = new nsString();
      }
      nsAutoString stringToInsert(aStringToInsert);
      stringToInsert.Mid(*mPhonetic,
                         textRange.mStartOffset, textRange.Length());
    }

    nsRefPtr<IMETextTxn> imeTxn;
    result = CreateTxnForIMEText(aStringToInsert, getter_AddRefs(imeTxn));
    txn = imeTxn;
    isIMETransaction = true;
  }
  else
  {
    nsRefPtr<InsertTextTxn> insertTxn;
    result = CreateTxnForInsertText(aStringToInsert, aTextNode, aOffset,
                                    getter_AddRefs(insertTxn));
    txn = insertTxn;
  }
  NS_ENSURE_SUCCESS(result, result);

  // let listeners know what's up
  int32_t i;
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->WillInsertText(aTextNode, aOffset, aStringToInsert);
  
  // XXX we may not need these view batches anymore.  This is handled at a higher level now I believe
  BeginUpdateViewBatch();
  result = DoTransaction(txn);
  EndUpdateViewBatch();

  mRangeUpdater.SelAdjInsertText(aTextNode, aOffset, aStringToInsert);
  
  // let listeners know what happened
  for (i = 0; i < mActionListeners.Count(); i++)
    mActionListeners[i]->DidInsertText(aTextNode, aOffset, aStringToInsert, result);

  // Added some cruft here for bug 43366.  Layout was crashing because we left an 
  // empty text node lying around in the document.  So I delete empty text nodes
  // caused by IME.  I have to mark the IME transaction as "fixed", which means
  // that furure ime txns won't merge with it.  This is because we don't want
  // future ime txns trying to put their text into a node that is no longer in
  // the document.  This does not break undo/redo, because all these txns are 
  // wrapped in a parent PlaceHolder txn, and placeholder txns are already 
  // savvy to having multiple ime txns inside them.
  
  // delete empty ime text node if there is one
  if (isIMETransaction && mIMETextNode)
  {
    uint32_t len;
    mIMETextNode->GetLength(&len);
    if (!len)
    {
      DeleteNode(mIMETextNode);
      mIMETextNode = nullptr;
      static_cast<IMETextTxn*>(txn.get())->MarkFixed();  // mark the ime txn "fixed"
    }
  }
  
  return result;
}


NS_IMETHODIMP nsEditor::SelectEntireDocument(nsISelection *aSelection)
{
  if (!aSelection) { return NS_ERROR_NULL_POINTER; }

  nsCOMPtr<nsIDOMElement> rootElement = do_QueryInterface(GetRoot());
  if (!rootElement) { return NS_ERROR_NOT_INITIALIZED; }

  return aSelection->SelectAllChildren(rootElement);
}


nsINode*
nsEditor::GetFirstEditableNode(nsINode* aRoot)
{
  MOZ_ASSERT(aRoot);

  nsIContent* node = GetLeftmostChild(aRoot);
  if (node && !IsEditable(node)) {
    node = GetNextNode(node, /* aEditableNode = */ true);
  }

  return (node != aRoot) ? node : nullptr;
}


NS_IMETHODIMP
nsEditor::NotifyDocumentListeners(TDocumentListenerNotification aNotificationType)
{
  int32_t numListeners = mDocStateListeners.Count();
  if (!numListeners)    // maybe there just aren't any.
    return NS_OK;
 
  nsCOMArray<nsIDocumentStateListener> listeners(mDocStateListeners);
  nsresult rv = NS_OK;
  int32_t i;

  switch (aNotificationType)
  {
    case eDocumentCreated:
      for (i = 0; i < numListeners;i++)
      {
        rv = listeners[i]->NotifyDocumentCreated();
        if (NS_FAILED(rv))
          break;
      }
      break;
      
    case eDocumentToBeDestroyed:
      for (i = 0; i < numListeners;i++)
      {
        rv = listeners[i]->NotifyDocumentWillBeDestroyed();
        if (NS_FAILED(rv))
          break;
      }
      break;

    case eDocumentStateChanged:
      {
        bool docIsDirty;
        rv = GetDocumentModified(&docIsDirty);
        NS_ENSURE_SUCCESS(rv, rv);

        if (static_cast<int8_t>(docIsDirty) == mDocDirtyState)
          return NS_OK;

        mDocDirtyState = docIsDirty;

        for (i = 0; i < numListeners;i++)
        {
          rv = listeners[i]->NotifyDocumentStateChanged(mDocDirtyState);
          if (NS_FAILED(rv))
            break;
        }
      }
      break;
    
    default:
      NS_NOTREACHED("Unknown notification");
  }

  return rv;
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertText(const nsAString & aStringToInsert,
                                               nsIDOMCharacterData *aTextNode,
                                               int32_t aOffset,
                                               InsertTextTxn ** aTxn)
{
  NS_ENSURE_TRUE(aTextNode && aTxn, NS_ERROR_NULL_POINTER);
  nsresult rv;

  nsRefPtr<InsertTextTxn> txn = new InsertTextTxn();
  rv = txn->Init(aTextNode, aOffset, aStringToInsert, this);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP nsEditor::DeleteText(nsIDOMCharacterData *aElement,
                              uint32_t             aOffset,
                              uint32_t             aLength)
{
  nsRefPtr<DeleteTextTxn> txn;
  nsresult result = CreateTxnForDeleteText(aElement, aOffset, aLength,
                                           getter_AddRefs(txn));
  nsAutoRules beginRulesSniffing(this, EditAction::deleteText, nsIEditor::ePrevious);
  if (NS_SUCCEEDED(result))  
  {
    // let listeners know what's up
    int32_t i;
    for (i = 0; i < mActionListeners.Count(); i++)
      mActionListeners[i]->WillDeleteText(aElement, aOffset, aLength);
    
    result = DoTransaction(txn); 
    
    // let listeners know what happened
    for (i = 0; i < mActionListeners.Count(); i++)
      mActionListeners[i]->DidDeleteText(aElement, aOffset, aLength, result);
  }
  return result;
}


nsresult
nsEditor::CreateTxnForDeleteText(nsIDOMCharacterData* aElement,
                                 uint32_t             aOffset,
                                 uint32_t             aLength,
                                 DeleteTextTxn**      aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsRefPtr<DeleteTextTxn> txn = new DeleteTextTxn();

  nsresult res = txn->Init(this, aElement, aOffset, aLength, &mRangeUpdater);
  NS_ENSURE_SUCCESS(res, res);

  txn.forget(aTxn);
  return NS_OK;
}




NS_IMETHODIMP nsEditor::CreateTxnForSplitNode(nsIDOMNode *aNode,
                                         uint32_t    aOffset,
                                         SplitElementTxn **aTxn)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsRefPtr<SplitElementTxn> txn = new SplitElementTxn();

  nsresult rv = txn->Init(this, aNode, aOffset);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}

NS_IMETHODIMP nsEditor::CreateTxnForJoinNode(nsIDOMNode  *aLeftNode,
                                             nsIDOMNode  *aRightNode,
                                             JoinElementTxn **aTxn)
{
  NS_ENSURE_TRUE(aLeftNode && aRightNode, NS_ERROR_NULL_POINTER);

  nsRefPtr<JoinElementTxn> txn = new JoinElementTxn();

  nsresult rv = txn->Init(this, aLeftNode, aRightNode);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


// END nsEditor core implementation


// BEGIN nsEditor public helper methods

nsresult
nsEditor::SplitNodeImpl(nsIDOMNode * aExistingRightNode,
                        int32_t      aOffset,
                        nsIDOMNode*  aNewLeftNode,
                        nsIDOMNode*  aParent)
{
  NS_ASSERTION(((nullptr!=aExistingRightNode) &&
                (nullptr!=aNewLeftNode) &&
                (nullptr!=aParent)),
                "null arg");
  nsresult result;
  if ((nullptr!=aExistingRightNode) &&
      (nullptr!=aNewLeftNode) &&
      (nullptr!=aParent))
  {
    // get selection
    nsCOMPtr<nsISelection> selection;
    result = GetSelection(getter_AddRefs(selection));
    NS_ENSURE_SUCCESS(result, result);
    NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

    // remember some selection points
    nsCOMPtr<nsIDOMNode> selStartNode, selEndNode;
    int32_t selStartOffset, selEndOffset;
    result = GetStartNodeAndOffset(selection, getter_AddRefs(selStartNode), &selStartOffset);
    if (NS_FAILED(result)) selStartNode = nullptr;  // if selection is cleared, remember that
    result = GetEndNodeAndOffset(selection, getter_AddRefs(selEndNode), &selEndOffset);
    if (NS_FAILED(result)) selStartNode = nullptr;  // if selection is cleared, remember that

    nsCOMPtr<nsIDOMNode> resultNode;
    result = aParent->InsertBefore(aNewLeftNode, aExistingRightNode, getter_AddRefs(resultNode));
    //printf("  after insert\n"); content->List();  // DEBUG
    if (NS_SUCCEEDED(result))
    {
      // split the children between the 2 nodes
      // at this point, aExistingRightNode has all the children
      // move all the children whose index is < aOffset to aNewLeftNode
      if (0<=aOffset) // don't bother unless we're going to move at least one child
      {
        // if it's a text node, just shuffle around some text
        nsCOMPtr<nsIDOMCharacterData> rightNodeAsText( do_QueryInterface(aExistingRightNode) );
        nsCOMPtr<nsIDOMCharacterData> leftNodeAsText( do_QueryInterface(aNewLeftNode) );
        if (leftNodeAsText && rightNodeAsText)
        {
          // fix right node
          nsAutoString leftText;
          rightNodeAsText->SubstringData(0, aOffset, leftText);
          rightNodeAsText->DeleteData(0, aOffset);
          // fix left node
          leftNodeAsText->SetData(leftText);
          // moose          
        }
        else
        {  // otherwise it's an interior node, so shuffle around the children
           // go through list backwards so deletes don't interfere with the iteration
          nsCOMPtr<nsIDOMNodeList> childNodes;
          result = aExistingRightNode->GetChildNodes(getter_AddRefs(childNodes));
          if ((NS_SUCCEEDED(result)) && (childNodes))
          {
            int32_t i=aOffset-1;
            for ( ; ((NS_SUCCEEDED(result)) && (0<=i)); i--)
            {
              nsCOMPtr<nsIDOMNode> childNode;
              result = childNodes->Item(i, getter_AddRefs(childNode));
              if ((NS_SUCCEEDED(result)) && (childNode))
              {
                result = aExistingRightNode->RemoveChild(childNode, getter_AddRefs(resultNode));
                //printf("  after remove\n"); content->List();  // DEBUG
                if (NS_SUCCEEDED(result))
                {
                  nsCOMPtr<nsIDOMNode> firstChild;
                  aNewLeftNode->GetFirstChild(getter_AddRefs(firstChild));
                  result = aNewLeftNode->InsertBefore(childNode, firstChild, getter_AddRefs(resultNode));
                  //printf("  after append\n"); content->List();  // DEBUG
                }
              }
            }
          }        
        }
        // handle selection
        nsCOMPtr<nsIPresShell> ps = GetPresShell();
        if (ps)
          ps->FlushPendingNotifications(Flush_Frames);

        if (GetShouldTxnSetSelection())
        {
          // editor wants us to set selection at split point
          selection->Collapse(aNewLeftNode, aOffset);
        }
        else if (selStartNode)   
        {
          // else adjust the selection if needed.  if selStartNode is null, then there was no selection.
          // HACK: this is overly simplified - multi-range selections need more work than this
          if (selStartNode.get() == aExistingRightNode)
          {
            if (selStartOffset < aOffset)
            {
              selStartNode = aNewLeftNode;
            }
            else
            {
              selStartOffset -= aOffset;
            }
          }
          if (selEndNode.get() == aExistingRightNode)
          {
            if (selEndOffset < aOffset)
            {
              selEndNode = aNewLeftNode;
            }
            else
            {
              selEndOffset -= aOffset;
            }
          }
          selection->Collapse(selStartNode,selStartOffset);
          selection->Extend(selEndNode,selEndOffset);
        }
      }
    }
  }
  else
    result = NS_ERROR_INVALID_ARG;

  return result;
}

nsresult
nsEditor::JoinNodesImpl(nsINode* aNodeToKeep,
                        nsINode* aNodeToJoin,
                        nsINode* aParent)
{
  MOZ_ASSERT(aNodeToKeep);
  MOZ_ASSERT(aNodeToJoin);
  MOZ_ASSERT(aParent);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  // remember some selection points
  nsCOMPtr<nsINode> selStartNode;
  int32_t selStartOffset;
  nsresult result = GetStartNodeAndOffset(selection, getter_AddRefs(selStartNode), &selStartOffset);
  if (NS_FAILED(result)) {
    selStartNode = nullptr;
  }

  nsCOMPtr<nsINode> selEndNode;
  int32_t selEndOffset;
  result = GetEndNodeAndOffset(selection, getter_AddRefs(selEndNode), &selEndOffset);
  // Joe or Kin should comment here on why the following line is not a copy/paste error
  if (NS_FAILED(result)) {
    selStartNode = nullptr;
  }

  uint32_t firstNodeLength = aNodeToJoin->Length();

  int32_t joinOffset;
  GetNodeLocation(aNodeToJoin, &joinOffset);
  int32_t keepOffset;
  nsINode* parent = GetNodeLocation(aNodeToKeep, &keepOffset);

  // if selection endpoint is between the nodes, remember it as being
  // in the one that is going away instead.  This simplifies later selection
  // adjustment logic at end of this method.
  if (selStartNode) {
    if (selStartNode == parent &&
        joinOffset < selStartOffset && selStartOffset <= keepOffset) {
      selStartNode = aNodeToJoin;
      selStartOffset = firstNodeLength;
    }
    if (selEndNode == parent &&
        joinOffset < selEndOffset && selEndOffset <= keepOffset) {
      selEndNode = aNodeToJoin;
      selEndOffset = firstNodeLength;
    }
  }

  // ok, ready to do join now.
  // if it's a text node, just shuffle around some text
  nsCOMPtr<nsIDOMCharacterData> keepNodeAsText( do_QueryInterface(aNodeToKeep) );
  nsCOMPtr<nsIDOMCharacterData> joinNodeAsText( do_QueryInterface(aNodeToJoin) );
  if (keepNodeAsText && joinNodeAsText) {
    nsAutoString rightText;
    nsAutoString leftText;
    keepNodeAsText->GetData(rightText);
    joinNodeAsText->GetData(leftText);
    leftText += rightText;
    keepNodeAsText->SetData(leftText);
  } else {
    // otherwise it's an interior node, so shuffle around the children
    nsCOMPtr<nsINodeList> childNodes = aNodeToJoin->ChildNodes();
    MOZ_ASSERT(childNodes);

    // remember the first child in aNodeToKeep, we'll insert all the children of aNodeToJoin in front of it
    // GetFirstChild returns nullptr firstNode if aNodeToKeep has no children, that's ok.
    nsCOMPtr<nsIContent> firstNode = aNodeToKeep->GetFirstChild();

    // have to go through the list backwards to keep deletes from interfering with iteration
    for (uint32_t i = childNodes->Length(); i > 0; --i) {
      nsCOMPtr<nsIContent> childNode = childNodes->Item(i - 1);
      if (childNode) {
        // prepend children of aNodeToJoin
        ErrorResult err;
        aNodeToKeep->InsertBefore(*childNode, firstNode, err);
        NS_ENSURE_SUCCESS(err.ErrorCode(), err.ErrorCode());
        firstNode = childNode.forget();
      }
    }
  }

  // delete the extra node
  ErrorResult err;
  aParent->RemoveChild(*aNodeToJoin, err);

  if (GetShouldTxnSetSelection()) {
    // editor wants us to set selection at join point
    selection->Collapse(aNodeToKeep, SafeCast<int32_t>(firstNodeLength));
  } else if (selStartNode) {
    // and adjust the selection if needed
    // HACK: this is overly simplified - multi-range selections need more work than this
    bool bNeedToAdjust = false;

    // check to see if we joined nodes where selection starts
    if (selStartNode == aNodeToJoin) {
      bNeedToAdjust = true;
      selStartNode = aNodeToKeep;
    } else if (selStartNode == aNodeToKeep) {
      bNeedToAdjust = true;
      selStartOffset += firstNodeLength;
    }

    // check to see if we joined nodes where selection ends
    if (selEndNode == aNodeToJoin) {
      bNeedToAdjust = true;
      selEndNode = aNodeToKeep;
    } else if (selEndNode == aNodeToKeep) {
      bNeedToAdjust = true;
      selEndOffset += firstNodeLength;
    }

    // adjust selection if needed
    if (bNeedToAdjust) {
      selection->Collapse(selStartNode, selStartOffset);
      selection->Extend(selEndNode, selEndOffset);
    }
  }

  return err.ErrorCode();
}


int32_t
nsEditor::GetChildOffset(nsIDOMNode* aChild, nsIDOMNode* aParent)
{
  MOZ_ASSERT(aChild && aParent);

  nsCOMPtr<nsINode> parent = do_QueryInterface(aParent);
  nsCOMPtr<nsINode> child = do_QueryInterface(aChild);
  MOZ_ASSERT(parent && child);

  int32_t idx = parent->IndexOf(child);
  MOZ_ASSERT(idx != -1);
  return idx;
}

// static
already_AddRefed<nsIDOMNode>
nsEditor::GetNodeLocation(nsIDOMNode* aChild, int32_t* outOffset)
{
  MOZ_ASSERT(aChild && outOffset);
  NS_ENSURE_TRUE(aChild && outOffset, nullptr);
  *outOffset = -1;

  nsCOMPtr<nsIDOMNode> parent;

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    aChild->GetParentNode(getter_AddRefs(parent))));
  if (parent) {
    *outOffset = GetChildOffset(aChild, parent);
  }

  return parent.forget();
}

nsINode*
nsEditor::GetNodeLocation(nsINode* aChild, int32_t* aOffset)
{
  MOZ_ASSERT(aChild);
  MOZ_ASSERT(aOffset);

  nsINode* parent = aChild->GetParentNode();
  if (parent) {
    *aOffset = parent->IndexOf(aChild);
    MOZ_ASSERT(*aOffset != -1);
  } else {
    *aOffset = -1;
  }
  return parent;
}

// returns the number of things inside aNode.  
// If aNode is text, returns number of characters. If not, returns number of children nodes.
nsresult
nsEditor::GetLengthOfDOMNode(nsIDOMNode *aNode, uint32_t &aCount) 
{
  aCount = 0;
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node, NS_ERROR_NULL_POINTER);
  aCount = node->Length();
  return NS_OK;
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aParentNode, 
                       int32_t      aOffset, 
                       bool         aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);
  *aResultNode = nullptr;

  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetPriorNode(parentNode, aOffset,
                                                aEditableNode,
                                                bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetPriorNode(nsINode* aParentNode,
                       int32_t aOffset,
                       bool aEditableNode,
                       bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // If we are at the beginning of the node, or it is a text node, then just
  // look before it.
  if (!aOffset || aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    if (aNoBlockCrossing && IsBlockNode(aParentNode)) {
      // If we aren't allowed to cross blocks, don't look before this block.
      return nullptr;
    }
    return GetPriorNode(aParentNode, aEditableNode, aNoBlockCrossing);
  }

  // else look before the child at 'aOffset'
  if (nsIContent* child = aParentNode->GetChildAt(aOffset)) {
    return GetPriorNode(child, aEditableNode, aNoBlockCrossing);
  }

  // unless there isn't one, in which case we are at the end of the node
  // and want the deep-right child.
  nsIContent* resultNode = GetRightmostChild(aParentNode, aNoBlockCrossing);
  if (!resultNode || !aEditableNode || IsEditable(resultNode)) {
    return resultNode;
  }

  // restart the search from the non-editable node we just found
  return GetPriorNode(resultNode, aEditableNode, aNoBlockCrossing);
}


nsresult 
nsEditor::GetNextNode(nsIDOMNode   *aParentNode, 
                      int32_t      aOffset, 
                      bool         aEditableNode, 
                      nsCOMPtr<nsIDOMNode> *aResultNode,
                      bool         bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);
  *aResultNode = nullptr;

  nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParentNode);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetNextNode(parentNode, aOffset,
                                               aEditableNode,
                                               bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetNextNode(nsINode* aParentNode,
                      int32_t aOffset,
                      bool aEditableNode,
                      bool aNoBlockCrossing)
{
  MOZ_ASSERT(aParentNode);

  // if aParentNode is a text node, use its location instead
  if (aParentNode->NodeType() == nsIDOMNode::TEXT_NODE) {
    nsINode* parent = aParentNode->GetParentNode();
    NS_ENSURE_TRUE(parent, nullptr);
    aOffset = parent->IndexOf(aParentNode) + 1; // _after_ the text node
    aParentNode = parent;
  }

  // look at the child at 'aOffset'
  nsIContent* child = aParentNode->GetChildAt(aOffset);
  if (child) {
    if (aNoBlockCrossing && IsBlockNode(child)) {
      return child;
    }

    nsIContent* resultNode = GetLeftmostChild(child, aNoBlockCrossing);
    if (!resultNode) {
      return child;
    }

    if (!IsDescendantOfEditorRoot(resultNode)) {
      return nullptr;
    }

    if (!aEditableNode || IsEditable(resultNode)) {
      return resultNode;
    }

    // restart the search from the non-editable node we just found
    return GetNextNode(resultNode, aEditableNode, aNoBlockCrossing);
  }
    
  // unless there isn't one, in which case we are at the end of the node
  // and want the next one.
  if (aNoBlockCrossing && IsBlockNode(aParentNode)) {
    // don't cross out of parent block
    return nullptr;
  }

  return GetNextNode(aParentNode, aEditableNode, aNoBlockCrossing);
}


nsresult 
nsEditor::GetPriorNode(nsIDOMNode  *aCurrentNode, 
                       bool         aEditableNode, 
                       nsCOMPtr<nsIDOMNode> *aResultNode,
                       bool         bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aResultNode, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> currentNode = do_QueryInterface(aCurrentNode);
  NS_ENSURE_TRUE(currentNode, NS_ERROR_NULL_POINTER);

  *aResultNode = do_QueryInterface(GetPriorNode(currentNode, aEditableNode,
                                                bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetPriorNode(nsINode* aCurrentNode, bool aEditableNode,
                       bool aNoBlockCrossing /* = false */)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nullptr;
  }

  return FindNode(aCurrentNode, false, aEditableNode, aNoBlockCrossing);
}

nsIContent*
nsEditor::FindNextLeafNode(nsINode  *aCurrentNode, 
                           bool      aGoForward,
                           bool      bNoBlockCrossing)
{
  // called only by GetPriorNode so we don't need to check params.
  NS_PRECONDITION(IsDescendantOfEditorRoot(aCurrentNode) &&
                  !IsEditorRoot(aCurrentNode),
                  "Bogus arguments");

  nsINode* cur = aCurrentNode;
  for (;;) {
    // if aCurrentNode has a sibling in the right direction, return
    // that sibling's closest child (or itself if it has no children)
    nsIContent* sibling =
      aGoForward ? cur->GetNextSibling() : cur->GetPreviousSibling();
    if (sibling) {
      if (bNoBlockCrossing && IsBlockNode(sibling)) {
        // don't look inside prevsib, since it is a block
        return sibling;
      }
      nsIContent *leaf =
        aGoForward ? GetLeftmostChild(sibling, bNoBlockCrossing) :
                     GetRightmostChild(sibling, bNoBlockCrossing);
      if (!leaf) { 
        return sibling;
      }

      return leaf;
    }

    nsINode *parent = cur->GetParentNode();
    if (!parent) {
      return nullptr;
    }

    NS_ASSERTION(IsDescendantOfEditorRoot(parent),
                 "We started with a proper descendant of root, and should stop "
                 "if we ever hit the root, so we better have a descendant of "
                 "root now!");
    if (IsEditorRoot(parent) ||
        (bNoBlockCrossing && IsBlockNode(parent))) {
      return nullptr;
    }

    cur = parent;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

nsresult
nsEditor::GetNextNode(nsIDOMNode* aCurrentNode,
                      bool aEditableNode,
                      nsCOMPtr<nsIDOMNode> *aResultNode,
                      bool bNoBlockCrossing)
{
  nsCOMPtr<nsINode> currentNode = do_QueryInterface(aCurrentNode);
  if (!currentNode || !aResultNode) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResultNode = do_QueryInterface(GetNextNode(currentNode, aEditableNode,
                                               bNoBlockCrossing));
  return NS_OK;
}

nsIContent*
nsEditor::GetNextNode(nsINode* aCurrentNode,
                      bool aEditableNode,
                      bool bNoBlockCrossing)
{
  MOZ_ASSERT(aCurrentNode);

  if (!IsDescendantOfEditorRoot(aCurrentNode)) {
    return nullptr;
  }

  return FindNode(aCurrentNode, true, aEditableNode, bNoBlockCrossing);
}

nsIContent*
nsEditor::FindNode(nsINode *aCurrentNode,
                   bool     aGoForward,
                   bool     aEditableNode,
                   bool     bNoBlockCrossing)
{
  if (IsEditorRoot(aCurrentNode)) {
    // Don't allow traversal above the root node! This helps
    // prevent us from accidentally editing browser content
    // when the editor is in a text widget.

    return nullptr;
  }

  nsCOMPtr<nsIContent> candidate =
    FindNextLeafNode(aCurrentNode, aGoForward, bNoBlockCrossing);
  
  if (!candidate) {
    return nullptr;
  }

  if (!aEditableNode || IsEditable(candidate)) {
    return candidate;
  }

  return FindNode(candidate, aGoForward, aEditableNode, bNoBlockCrossing);
}

nsIDOMNode*
nsEditor::GetRightmostChild(nsIDOMNode* aCurrentNode,
                            bool bNoBlockCrossing)
{
  nsCOMPtr<nsINode> currentNode = do_QueryInterface(aCurrentNode);
  nsIContent* result = GetRightmostChild(currentNode, bNoBlockCrossing);
  return result ? result->AsDOMNode() : nullptr;
}

nsIContent*
nsEditor::GetRightmostChild(nsINode *aCurrentNode,
                            bool     bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent *cur = aCurrentNode->GetLastChild();
  if (!cur) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && IsBlockNode(cur)) {
      return cur;
    }
    nsIContent* next = cur->GetLastChild();
    if (!next) {
      return cur;
    }
    cur = next;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIContent*
nsEditor::GetLeftmostChild(nsINode *aCurrentNode,
                           bool     bNoBlockCrossing)
{
  NS_ENSURE_TRUE(aCurrentNode, nullptr);
  nsIContent *cur = aCurrentNode->GetFirstChild();
  if (!cur) {
    return nullptr;
  }
  for (;;) {
    if (bNoBlockCrossing && IsBlockNode(cur)) {
      return cur;
    }
    nsIContent *next = cur->GetFirstChild();
    if (!next) {
      return cur;
    }
    cur = next;
  }

  NS_NOTREACHED("What part of for(;;) do you not understand?");
  return nullptr;
}

nsIDOMNode*
nsEditor::GetLeftmostChild(nsIDOMNode* aCurrentNode,
                           bool bNoBlockCrossing)
{
  nsCOMPtr<nsINode> currentNode = do_QueryInterface(aCurrentNode);
  nsIContent* result = GetLeftmostChild(currentNode, bNoBlockCrossing);
  return result ? result->AsDOMNode() : nullptr;
}

bool
nsEditor::IsBlockNode(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsBlockNode(node);
}

bool
nsEditor::IsBlockNode(nsINode* aNode)
{
  // stub to be overridden in nsHTMLEditor.
  // screwing around with the class hierarchy here in order
  // to not duplicate the code in GetNextNode/GetPrevNode
  // across both nsEditor/nsHTMLEditor.
  return false;
}

bool
nsEditor::CanContain(nsIDOMNode* aParent, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(parent, false);

  switch (parent->NodeType()) {
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContain(parent->Tag(), aChild);
  }
  return false;
}

bool
nsEditor::CanContainTag(nsIDOMNode* aParent, nsIAtom* aChildTag)
{
  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);
  NS_ENSURE_TRUE(parent, false);

  switch (parent->NodeType()) {
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContainTag(parent->Tag(), aChildTag);
  }
  return false;
}

bool 
nsEditor::TagCanContain(nsIAtom* aParentTag, nsIDOMNode* aChild)
{
  nsCOMPtr<nsIContent> child = do_QueryInterface(aChild);
  NS_ENSURE_TRUE(child, false);

  switch (child->NodeType()) {
  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::ELEMENT_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
    return TagCanContainTag(aParentTag, child->Tag());
  }
  return false;
}

bool 
nsEditor::TagCanContainTag(nsIAtom* aParentTag, nsIAtom* aChildTag)
{
  return true;
}

bool
nsEditor::IsRoot(nsIDOMNode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(GetRoot());

  return inNode == rootNode;
}

bool 
nsEditor::IsRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);

  nsCOMPtr<nsINode> rootNode = GetRoot();

  return inNode == rootNode;
}

bool
nsEditor::IsEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsINode> rootNode = GetEditorRoot();
  return aNode == rootNode;
}

bool 
nsEditor::IsDescendantOfRoot(nsIDOMNode* inNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(inNode);
  return IsDescendantOfRoot(node);
}

bool
nsEditor::IsDescendantOfRoot(nsINode* inNode)
{
  NS_ENSURE_TRUE(inNode, false);
  nsCOMPtr<nsIContent> root = GetRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(inNode, root);
}

bool
nsEditor::IsDescendantOfEditorRoot(nsIDOMNode* aNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  return IsDescendantOfEditorRoot(node);
}

bool
nsEditor::IsDescendantOfEditorRoot(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);
  nsCOMPtr<nsIContent> root = GetEditorRoot();
  NS_ENSURE_TRUE(root, false);

  return nsContentUtils::ContentIsDescendantOf(aNode, root);
}

bool
nsEditor::IsContainer(nsINode* aNode)
{
  return aNode ? true : false;
}

bool
nsEditor::IsContainer(nsIDOMNode* aNode)
{
  return aNode ? true : false;
}

static inline bool
IsElementVisible(dom::Element* aElement)
{
  if (aElement->GetPrimaryFrame()) {
    // It's visible, for our purposes
    return true;
  }

  nsIContent *cur = aElement;
  for (; ;) {
    // Walk up the tree looking for the nearest ancestor with a frame.
    // The state of the child right below it will determine whether
    // we might possibly have a frame or not.
    bool haveLazyBitOnChild = cur->HasFlag(NODE_NEEDS_FRAME);
    cur = cur->GetFlattenedTreeParent();
    if (!cur) {
      if (!haveLazyBitOnChild) {
        // None of our ancestors have lazy bits set, so we shouldn't
        // have a frame
        return false;
      }

      // The root has a lazy frame construction bit.  We need to check
      // our style.
      break;
    }

    if (cur->GetPrimaryFrame()) {
      if (!haveLazyBitOnChild) {
        // Our ancestor directly under |cur| doesn't have lazy bits;
        // that means we won't get a frame
        return false;
      }

      if (cur->GetPrimaryFrame()->IsLeaf()) {
        // Nothing under here will ever get frames
        return false;
      }

      // Otherwise, we might end up with a frame when that lazy bit is
      // processed.  Figure out our actual style.
      break;
    }
  }

  // Now it might be that we have no frame because we're in a
  // display:none subtree, or it might be that we're just dealing with
  // lazy frame construction and it hasn't happened yet.  Check which
  // one it is.
  nsRefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextForElementNoFlush(aElement,
                                                         nullptr, nullptr);
  if (styleContext) {
    return styleContext->StyleDisplay()->mDisplay != NS_STYLE_DISPLAY_NONE;
  }
  return false;
}

bool 
nsEditor::IsEditable(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  return IsEditable(content);
}

bool
nsEditor::IsEditable(nsINode* aNode)
{
  NS_ENSURE_TRUE(aNode, false);

  if (!aNode->IsNodeOfType(nsINode::eCONTENT) || IsMozEditorBogusNode(aNode) ||
      !IsModifiableNode(aNode)) {
    return false;
  }

  // see if it has a frame.  If so, we'll edit it.
  // special case for textnodes: frame must have width.
  if (aNode->IsElement() && !IsElementVisible(aNode->AsElement())) {
    // If the element has no frame, it's not editable.  Note that we
    // need to check IsElement() here, because some of our tests
    // rely on frameless textnodes being visible.
    return false;
  }
  switch (aNode->NodeType()) {
    case nsIDOMNode::ELEMENT_NODE:
    case nsIDOMNode::TEXT_NODE:
      return true; // element or text node; not invisible
    default:
      return false;
  }
}

bool
nsEditor::IsMozEditorBogusNode(nsINode* element)
{
  return element && element->IsElement() &&
         element->AsElement()->AttrValueIs(kNameSpaceID_None,
             kMOZEditorBogusNodeAttrAtom, kMOZEditorBogusNodeValue,
             eCaseMatters);
}

uint32_t
nsEditor::CountEditableChildren(nsINode* aNode)
{
  MOZ_ASSERT(aNode);
  uint32_t count = 0;
  for (nsIContent* child = aNode->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (IsEditable(child)) {
      ++count;
    }
  }
  return count;
}

//END nsEditor static utility methods


NS_IMETHODIMP nsEditor::IncrementModificationCount(int32_t inNumMods)
{
  uint32_t oldModCount = mModCount;

  mModCount += inNumMods;

  if ((oldModCount == 0 && mModCount != 0)
   || (oldModCount != 0 && mModCount == 0))
    NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}


NS_IMETHODIMP nsEditor::GetModificationCount(int32_t *outModCount)
{
  NS_ENSURE_ARG_POINTER(outModCount);
  *outModCount = mModCount;
  return NS_OK;
}


NS_IMETHODIMP nsEditor::ResetModificationCount()
{
  bool doNotify = (mModCount != 0);

  mModCount = 0;

  if (doNotify)
    NotifyDocumentListeners(eDocumentStateChanged);
  return NS_OK;
}

//END nsEditor Private methods



///////////////////////////////////////////////////////////////////////////
// GetTag: digs out the atom for the tag of this node
//
nsIAtom *
nsEditor::GetTag(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);

  if (!content) 
  {
    NS_ASSERTION(aNode, "null node passed to nsEditor::Tag()");

    return nullptr;
  }
  
  return content->Tag();
}


///////////////////////////////////////////////////////////////////////////
// GetTagString: digs out string for the tag of this node
//                    
nsresult 
nsEditor::GetTagString(nsIDOMNode *aNode, nsAString& outString)
{
  if (!aNode) 
  {
    NS_NOTREACHED("null node passed to nsEditor::GetTag()");
    return NS_ERROR_NULL_POINTER;
  }
  
  nsIAtom *atom = GetTag(aNode);
  if (!atom)
  {
    return NS_ERROR_FAILURE;
  }

  atom->ToString(outString);
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// NodesSameType: do these nodes have the same tag?
//                    
bool 
nsEditor::NodesSameType(nsIDOMNode *aNode1, nsIDOMNode *aNode2)
{
  if (!aNode1 || !aNode2) {
    NS_NOTREACHED("null node passed to nsEditor::NodesSameType()");
    return false;
  }

  nsCOMPtr<nsIContent> content1 = do_QueryInterface(aNode1);
  NS_ENSURE_TRUE(content1, false);

  nsCOMPtr<nsIContent> content2 = do_QueryInterface(aNode2);
  NS_ENSURE_TRUE(content2, false);

  return AreNodesSameType(content1, content2);
}

/* virtual */
bool
nsEditor::AreNodesSameType(nsIContent* aNode1, nsIContent* aNode2)
{
  MOZ_ASSERT(aNode1);
  MOZ_ASSERT(aNode2);
  return aNode1->Tag() == aNode2->Tag();
}


///////////////////////////////////////////////////////////////////////////
// IsTextNode: true if node of dom type text
//               
bool
nsEditor::IsTextNode(nsIDOMNode *aNode)
{
  if (!aNode)
  {
    NS_NOTREACHED("null node passed to IsTextNode()");
    return false;
  }
  
  uint16_t nodeType;
  aNode->GetNodeType(&nodeType);
  return (nodeType == nsIDOMNode::TEXT_NODE);
}

bool
nsEditor::IsTextNode(nsINode *aNode)
{
  return aNode->NodeType() == nsIDOMNode::TEXT_NODE;
}

///////////////////////////////////////////////////////////////////////////
// GetChildAt: returns the node at this position index in the parent
//
nsCOMPtr<nsIDOMNode> 
nsEditor::GetChildAt(nsIDOMNode *aParent, int32_t aOffset)
{
  nsCOMPtr<nsIDOMNode> resultNode;
  
  nsCOMPtr<nsIContent> parent = do_QueryInterface(aParent);

  NS_ENSURE_TRUE(parent, resultNode);

  resultNode = do_QueryInterface(parent->GetChildAt(aOffset));

  return resultNode;
}

///////////////////////////////////////////////////////////////////////////
// GetNodeAtRangeOffsetPoint: returns the node at this position in a range,
// assuming that aParentOrNode is the node itself if it's a text node, or
// the node's parent otherwise.
//
nsCOMPtr<nsIDOMNode>
nsEditor::GetNodeAtRangeOffsetPoint(nsIDOMNode* aParentOrNode, int32_t aOffset)
{
  if (IsTextNode(aParentOrNode)) {
    return aParentOrNode;
  }
  return GetChildAt(aParentOrNode, aOffset);
}


///////////////////////////////////////////////////////////////////////////
// GetStartNodeAndOffset: returns whatever the start parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetStartNodeAndOffset(nsISelection *aSelection,
                                       nsIDOMNode **outStartNode,
                                       int32_t *outStartOffset)
{
  NS_ENSURE_TRUE(outStartNode && outStartOffset && aSelection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> startNode;
  nsresult rv = GetStartNodeAndOffset(static_cast<Selection*>(aSelection),
                                      getter_AddRefs(startNode),
                                      outStartOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (startNode) {
    NS_ADDREF(*outStartNode = startNode->AsDOMNode());
  } else {
    *outStartNode = nullptr;
  }
  return NS_OK;
}

nsresult
nsEditor::GetStartNodeAndOffset(Selection* aSelection, nsINode** aStartNode,
                                int32_t* aStartOffset)
{
  MOZ_ASSERT(aSelection);
  MOZ_ASSERT(aStartNode);
  MOZ_ASSERT(aStartOffset);

  *aStartNode = nullptr;
  *aStartOffset = 0;

  NS_ENSURE_TRUE(aSelection->GetRangeCount(), NS_ERROR_FAILURE);

  const nsRange* range = aSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  NS_ENSURE_TRUE(range->IsPositioned(), NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aStartNode = range->GetStartParent());
  *aStartOffset = range->StartOffset();
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// GetEndNodeAndOffset: returns whatever the end parent & offset is of 
//                        the first range in the selection.
nsresult 
nsEditor::GetEndNodeAndOffset(nsISelection *aSelection,
                                       nsIDOMNode **outEndNode,
                                       int32_t *outEndOffset)
{
  NS_ENSURE_TRUE(outEndNode && outEndOffset && aSelection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> endNode;
  nsresult rv = GetEndNodeAndOffset(static_cast<Selection*>(aSelection),
                                    getter_AddRefs(endNode),
                                    outEndOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  if (endNode) {
    NS_ADDREF(*outEndNode = endNode->AsDOMNode());
  } else {
    *outEndNode = nullptr;
  }
  return NS_OK;
}

nsresult
nsEditor::GetEndNodeAndOffset(Selection* aSelection, nsINode** aEndNode,
                              int32_t* aEndOffset)
{
  MOZ_ASSERT(aSelection);
  MOZ_ASSERT(aEndNode);
  MOZ_ASSERT(aEndOffset);

  *aEndNode = nullptr;
  *aEndOffset = 0;

  NS_ENSURE_TRUE(aSelection->GetRangeCount(), NS_ERROR_FAILURE);

  const nsRange* range = aSelection->GetRangeAt(0);
  NS_ENSURE_TRUE(range, NS_ERROR_FAILURE);

  NS_ENSURE_TRUE(range->IsPositioned(), NS_ERROR_FAILURE);

  NS_IF_ADDREF(*aEndNode = range->GetEndParent());
  *aEndOffset = range->EndOffset();
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// IsPreformatted: checks the style info for the node for the preformatted
//                 text style.
nsresult 
nsEditor::IsPreformatted(nsIDOMNode *aNode, bool *aResult)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  
  NS_ENSURE_TRUE(aResult && content, NS_ERROR_NULL_POINTER);
  
  nsCOMPtr<nsIPresShell> ps = GetPresShell();
  NS_ENSURE_TRUE(ps, NS_ERROR_NOT_INITIALIZED);

  // Look at the node (and its parent if it's not an element), and grab its style context
  nsRefPtr<nsStyleContext> elementStyle;
  if (!content->IsElement()) {
    content = content->GetParent();
  }
  if (content && content->IsElement()) {
    elementStyle = nsComputedDOMStyle::GetStyleContextForElementNoFlush(content->AsElement(),
                                                                        nullptr,
                                                                        ps);
  }

  if (!elementStyle)
  {
    // Consider nodes without a style context to be NOT preformatted:
    // For instance, this is true of JS tags inside the body (which show
    // up as #text nodes but have no style context).
    *aResult = false;
    return NS_OK;
  }

  const nsStyleText* styleText = elementStyle->StyleText();

  *aResult = styleText->WhiteSpaceIsSignificant();
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// SplitNodeDeep: this splits a node "deeply", splitting children as 
//                appropriate.  The place to split is represented by
//                a dom point at {splitPointParent, splitPointOffset}.
//                That dom point must be inside aNode, which is the node to 
//                split.  outOffset is set to the offset in the parent of aNode where
//                the split terminates - where you would want to insert 
//                a new element, for instance, if that's why you were splitting 
//                the node.
//
nsresult
nsEditor::SplitNodeDeep(nsIDOMNode *aNode, 
                        nsIDOMNode *aSplitPointParent, 
                        int32_t aSplitPointOffset,
                        int32_t *outOffset,
                        bool    aNoEmptyContainers,
                        nsCOMPtr<nsIDOMNode> *outLeftNode,
                        nsCOMPtr<nsIDOMNode> *outRightNode)
{
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  NS_ENSURE_TRUE(node && aSplitPointParent && outOffset, NS_ERROR_NULL_POINTER);
  int32_t offset = aSplitPointOffset;

  if (outLeftNode)  *outLeftNode  = nullptr;
  if (outRightNode) *outRightNode = nullptr;

  nsCOMPtr<nsINode> nodeToSplit = do_QueryInterface(aSplitPointParent);
  while (nodeToSplit) {
    // need to insert rules code call here to do things like
    // not split a list if you are after the last <li> or before the first, etc.
    // for now we just have some smarts about unneccessarily splitting
    // textnodes, which should be universal enough to put straight in
    // this nsEditor routine.
    
    nsCOMPtr<nsIDOMCharacterData> nodeAsText = do_QueryInterface(nodeToSplit);
    uint32_t len = nodeToSplit->Length();
    bool bDoSplit = false;
    
    if (!(aNoEmptyContainers || nodeAsText) || (offset && (offset != (int32_t)len)))
    {
      bDoSplit = true;
      nsCOMPtr<nsIDOMNode> tempNode;
      nsresult rv = SplitNode(nodeToSplit->AsDOMNode(), offset,
                              getter_AddRefs(tempNode));
      NS_ENSURE_SUCCESS(rv, rv);

      if (outRightNode) {
        *outRightNode = nodeToSplit->AsDOMNode();
      }
      if (outLeftNode) {
        *outLeftNode = tempNode;
      }
    }

    nsINode* parentNode = nodeToSplit->GetParentNode();
    NS_ENSURE_TRUE(parentNode, NS_ERROR_FAILURE);

    if (!bDoSplit && offset) {
      // must be "end of text node" case, we didn't split it, just move past it
      offset = parentNode->IndexOf(nodeToSplit) + 1;
      if (outLeftNode) {
        *outLeftNode = nodeToSplit->AsDOMNode();
      }
    } else {
      offset = parentNode->IndexOf(nodeToSplit);
      if (outRightNode) {
        *outRightNode = nodeToSplit->AsDOMNode();
      }
    }

    if (nodeToSplit == node) {
      // we split all the way up to (and including) aNode; we're done
      break;
    }

    nodeToSplit = parentNode;
  }

  if (!nodeToSplit) {
    NS_NOTREACHED("null node obtained in nsEditor::SplitNodeDeep()");
    return NS_ERROR_FAILURE;
  }

  *outOffset = offset;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////
// JoinNodeDeep:  this joins two like nodes "deeply", joining children as 
//                appropriate.  
nsresult
nsEditor::JoinNodeDeep(nsIDOMNode *aLeftNode, 
                       nsIDOMNode *aRightNode,
                       nsCOMPtr<nsIDOMNode> *aOutJoinNode, 
                       int32_t *outOffset)
{
  NS_ENSURE_TRUE(aLeftNode && aRightNode && aOutJoinNode && outOffset, NS_ERROR_NULL_POINTER);

  // while the rightmost children and their descendants of the left node 
  // match the leftmost children and their descendants of the right node
  // join them up.  Can you say that three times fast?
  
  nsCOMPtr<nsIDOMNode> leftNodeToJoin = do_QueryInterface(aLeftNode);
  nsCOMPtr<nsIDOMNode> rightNodeToJoin = do_QueryInterface(aRightNode);
  nsCOMPtr<nsIDOMNode> parentNode,tmp;
  nsresult res = NS_OK;
  
  rightNodeToJoin->GetParentNode(getter_AddRefs(parentNode));
  
  while (leftNodeToJoin && rightNodeToJoin && parentNode &&
          NodesSameType(leftNodeToJoin, rightNodeToJoin))
  {
    // adjust out params
    uint32_t length;
    res = GetLengthOfDOMNode(leftNodeToJoin, length);
    NS_ENSURE_SUCCESS(res, res);
    
    *aOutJoinNode = rightNodeToJoin;
    *outOffset = length;
    
    // do the join
    res = JoinNodes(leftNodeToJoin, rightNodeToJoin, parentNode);
    NS_ENSURE_SUCCESS(res, res);
    
    if (IsTextNode(parentNode)) // we've joined all the way down to text nodes, we're done!
      return NS_OK;

    else
    {
      // get new left and right nodes, and begin anew
      parentNode = rightNodeToJoin;
      leftNodeToJoin = GetChildAt(parentNode, length-1);
      rightNodeToJoin = GetChildAt(parentNode, length);

      // skip over non-editable nodes
      while (leftNodeToJoin && !IsEditable(leftNodeToJoin))
      {
        leftNodeToJoin->GetPreviousSibling(getter_AddRefs(tmp));
        leftNodeToJoin = tmp;
      }
      if (!leftNodeToJoin) break;
    
      while (rightNodeToJoin && !IsEditable(rightNodeToJoin))
      {
        rightNodeToJoin->GetNextSibling(getter_AddRefs(tmp));
        rightNodeToJoin = tmp;
      }
      if (!rightNodeToJoin) break;
    }
  }
  
  return res;
}

void
nsEditor::BeginUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount >= 0, "bad state");

  if (0 == mUpdateCount)
  {
    // Turn off selection updates and notifications.

    nsCOMPtr<nsISelection> selection;
    GetSelection(getter_AddRefs(selection));

    if (selection) 
    {
      nsCOMPtr<nsISelectionPrivate> selPrivate(do_QueryInterface(selection));
      selPrivate->StartBatchChanges();
    }
  }

  mUpdateCount++;
}


nsresult nsEditor::EndUpdateViewBatch()
{
  NS_PRECONDITION(mUpdateCount > 0, "bad state");
  
  if (mUpdateCount <= 0)
  {
    mUpdateCount = 0;
    return NS_ERROR_FAILURE;
  }

  mUpdateCount--;

  if (0 == mUpdateCount)
  {
    // Turn selection updating and notifications back on.

    nsCOMPtr<nsISelection>selection;
    GetSelection(getter_AddRefs(selection));

    if (selection) {
      nsCOMPtr<nsISelectionPrivate>selPrivate(do_QueryInterface(selection));
      selPrivate->EndBatchChanges();
    }
  }

  return NS_OK;
}

bool 
nsEditor::GetShouldTxnSetSelection()
{
  return mShouldTxnSetSelection;
}


NS_IMETHODIMP 
nsEditor::DeleteSelectionImpl(EDirection aAction,
                              EStripWrappers aStripWrappers)
{
  MOZ_ASSERT(aStripWrappers == eStrip || aStripWrappers == eNoStrip);

  nsCOMPtr<nsISelection>selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  nsRefPtr<EditAggregateTxn> txn;
  nsCOMPtr<nsINode> deleteNode;
  int32_t deleteCharOffset = 0, deleteCharLength = 0;
  res = CreateTxnForDeleteSelection(aAction, getter_AddRefs(txn),
                                    getter_AddRefs(deleteNode),
                                    &deleteCharOffset, &deleteCharLength);
  nsCOMPtr<nsIDOMCharacterData> deleteCharData(do_QueryInterface(deleteNode));

  if (NS_SUCCEEDED(res))  
  {
    nsAutoRules beginRulesSniffing(this, EditAction::deleteSelection, aAction);
    int32_t i;
    // Notify nsIEditActionListener::WillDelete[Selection|Text|Node]
    if (!deleteNode)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->WillDeleteSelection(selection);
    else if (deleteCharData)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->WillDeleteText(deleteCharData, deleteCharOffset, 1);
    else
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->WillDeleteNode(deleteNode->AsDOMNode());

    // Delete the specified amount
    res = DoTransaction(txn);  

    // Notify nsIEditActionListener::DidDelete[Selection|Text|Node]
    if (!deleteNode)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->DidDeleteSelection(selection);
    else if (deleteCharData)
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->DidDeleteText(deleteCharData, deleteCharOffset, 1, res);
    else
      for (i = 0; i < mActionListeners.Count(); i++)
        mActionListeners[i]->DidDeleteNode(deleteNode->AsDOMNode(), res);
  }

  return res;
}

// XXX: error handling in this routine needs to be cleaned up!
NS_IMETHODIMP
nsEditor::DeleteSelectionAndCreateNode(const nsAString& aTag,
                                           nsIDOMNode ** aNewNode)
{
  nsresult result = DeleteSelectionAndPrepareToCreateNode();
  NS_ENSURE_SUCCESS(result, result);

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);

  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  uint32_t offset = selection->AnchorOffset();

  nsCOMPtr<nsIDOMNode> newNode;
  result = CreateNode(aTag, node->AsDOMNode(), offset,
                      getter_AddRefs(newNode));
  // XXX: ERROR_HANDLING  check result, and make sure aNewNode is set correctly
  // in success/failure cases
  *aNewNode = newNode;
  NS_IF_ADDREF(*aNewNode);

  // we want the selection to be just after the new node
  return selection->Collapse(node, offset + 1);
}


/* Non-interface, protected methods */

TextComposition*
nsEditor::GetComposition() const
{
  return mComposition;
}

bool
nsEditor::IsIMEComposing() const
{
  return mComposition && mComposition->IsComposing();
}

nsresult
nsEditor::DeleteSelectionAndPrepareToCreateNode()
{
  nsresult res;
  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_TRUE(selection, NS_ERROR_NULL_POINTER);
  MOZ_ASSERT(selection->GetAnchorFocusRange());

  if (!selection->GetAnchorFocusRange()->Collapsed()) {
    res = DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);
    NS_ENSURE_SUCCESS(res, res);

    MOZ_ASSERT(selection->GetAnchorFocusRange() &&
               selection->GetAnchorFocusRange()->Collapsed(),
               "Selection not collapsed after delete");
  }

  // If the selection is a chardata node, split it if necessary and compute
  // where to put the new node
  nsCOMPtr<nsINode> node = selection->GetAnchorNode();
  MOZ_ASSERT(node, "Selection has no ranges in it");

  if (node && node->IsNodeOfType(nsINode::eDATA_NODE)) {
    NS_ASSERTION(node->GetParentNode(),
                 "It's impossible to insert into chardata with no parent -- "
                 "fix the caller");
    NS_ENSURE_STATE(node->GetParentNode());

    uint32_t offset = selection->AnchorOffset();

    if (offset == 0) {
      res = selection->Collapse(node->GetParentNode(),
                                node->GetParentNode()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    } else if (offset == node->Length()) {
      res = selection->Collapse(node->GetParentNode(),
                                node->GetParentNode()->IndexOf(node) + 1);
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    } else {
      nsCOMPtr<nsIDOMNode> tmp;
      res = SplitNode(node->AsDOMNode(), offset, getter_AddRefs(tmp));
      NS_ENSURE_SUCCESS(res, res);
      res = selection->Collapse(node->GetParentNode(),
                                node->GetParentNode()->IndexOf(node));
      MOZ_ASSERT(NS_SUCCEEDED(res));
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  return NS_OK;
}



void
nsEditor::DoAfterDoTransaction(nsITransaction *aTxn)
{
  bool isTransientTransaction;
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    aTxn->GetIsTransient(&isTransientTransaction)));
  
  if (!isTransientTransaction)
  {
    // we need to deal here with the case where the user saved after some
    // edits, then undid one or more times. Then, the undo count is -ve,
    // but we can't let a do take it back to zero. So we flip it up to
    // a +ve number.
    int32_t modCount;
    GetModificationCount(&modCount);
    if (modCount < 0)
      modCount = -modCount;
        
    // don't count transient transactions
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      IncrementModificationCount(1)));
  }
}


void
nsEditor::DoAfterUndoTransaction()
{
  // all undoable transactions are non-transient
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    IncrementModificationCount(-1)));
}

void
nsEditor::DoAfterRedoTransaction()
{
  // all redoable transactions are non-transient
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
    IncrementModificationCount(1)));
}

NS_IMETHODIMP 
nsEditor::CreateTxnForSetAttribute(nsIDOMElement *aElement, 
                                   const nsAString& aAttribute, 
                                   const nsAString& aValue,
                                   ChangeAttributeTxn ** aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsRefPtr<ChangeAttributeTxn> txn = new ChangeAttributeTxn();

  nsresult rv = txn->Init(this, aElement, aAttribute, aValue, false);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveAttribute(nsIDOMElement *aElement, 
                                      const nsAString& aAttribute,
                                      ChangeAttributeTxn ** aTxn)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsRefPtr<ChangeAttributeTxn> txn = new ChangeAttributeTxn();

  nsresult rv = txn->Init(this, aElement, aAttribute, EmptyString(), true);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP nsEditor::CreateTxnForCreateElement(const nsAString& aTag,
                                                  nsIDOMNode     *aParent,
                                                  int32_t         aPosition,
                                                  CreateElementTxn ** aTxn)
{
  NS_ENSURE_TRUE(aParent, NS_ERROR_NULL_POINTER);

  nsRefPtr<CreateElementTxn> txn = new CreateElementTxn();

  nsresult rv = txn->Init(this, aTag, aParent, aPosition);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP nsEditor::CreateTxnForInsertElement(nsIDOMNode * aNode,
                                                  nsIDOMNode * aParent,
                                                  int32_t      aPosition,
                                                  InsertElementTxn ** aTxn)
{
  NS_ENSURE_TRUE(aNode && aParent, NS_ERROR_NULL_POINTER);

  nsRefPtr<InsertElementTxn> txn = new InsertElementTxn();

  nsresult rv = txn->Init(aNode, aParent, aPosition, this);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}

nsresult
nsEditor::CreateTxnForDeleteNode(nsINode* aNode, DeleteNodeTxn** aTxn)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);

  nsRefPtr<DeleteNodeTxn> txn = new DeleteNodeTxn();

  nsresult res = txn->Init(this, aNode, &mRangeUpdater);
  NS_ENSURE_SUCCESS(res, res);

  txn.forget(aTxn);
  return NS_OK;
}

NS_IMETHODIMP 
nsEditor::CreateTxnForIMEText(const nsAString& aStringToInsert,
                              IMETextTxn ** aTxn)
{
  NS_ASSERTION(aTxn, "illegal value- null ptr- aTxn");
     
  nsRefPtr<IMETextTxn> txn = new IMETextTxn();

  // During handling IME composition, mComposition must have been initialized.
  // TODO: We can simplify IMETextTxn::Init() with TextComposition class.
  nsresult rv = txn->Init(mIMETextNode, mIMETextOffset,
                          mComposition->String().Length(),
                          mComposition->GetRanges(), aStringToInsert, this);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


NS_IMETHODIMP 
nsEditor::CreateTxnForAddStyleSheet(CSSStyleSheet* aSheet, AddStyleSheetTxn* *aTxn)
{
  nsRefPtr<AddStyleSheetTxn> txn = new AddStyleSheetTxn();

  nsresult rv = txn->Init(this, aSheet);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}



NS_IMETHODIMP 
nsEditor::CreateTxnForRemoveStyleSheet(CSSStyleSheet* aSheet, RemoveStyleSheetTxn* *aTxn)
{
  nsRefPtr<RemoveStyleSheetTxn> txn = new RemoveStyleSheetTxn();

  nsresult rv = txn->Init(this, aSheet);
  if (NS_SUCCEEDED(rv))
  {
    txn.forget(aTxn);
  }

  return rv;
}


nsresult
nsEditor::CreateTxnForDeleteSelection(EDirection aAction,
                                      EditAggregateTxn** aTxn,
                                      nsINode** aNode,
                                      int32_t* aOffset,
                                      int32_t* aLength)
{
  MOZ_ASSERT(aTxn);
  *aTxn = nullptr;

  nsRefPtr<Selection> selection = GetSelection();
  NS_ENSURE_STATE(selection);

  // Check whether the selection is collapsed and we should do nothing:
  if (selection->Collapsed() && aAction == eNone) {
    return NS_OK;
  }

  // allocate the out-param transaction
  nsRefPtr<EditAggregateTxn> aggTxn = new EditAggregateTxn();

  for (int32_t rangeIdx = 0; rangeIdx < selection->GetRangeCount(); ++rangeIdx) {
    nsRefPtr<nsRange> range = selection->GetRangeAt(rangeIdx);
    NS_ENSURE_STATE(range);

    // Same with range as with selection; if it is collapsed and action
    // is eNone, do nothing.
    if (!range->Collapsed()) {
      nsRefPtr<DeleteRangeTxn> txn = new DeleteRangeTxn();
      txn->Init(this, range, &mRangeUpdater);
      aggTxn->AppendChild(txn);
    } else if (aAction != eNone) {
      // we have an insertion point.  delete the thing in front of it or
      // behind it, depending on aAction
      nsresult res = CreateTxnForDeleteInsertionPoint(range, aAction, aggTxn,
                                                      aNode, aOffset, aLength);
      NS_ENSURE_SUCCESS(res, res);
    }
  }

  aggTxn.forget(aTxn);

  return NS_OK;
}

nsresult
nsEditor::CreateTxnForDeleteCharacter(nsIDOMCharacterData* aData,
                                      uint32_t             aOffset,
                                      EDirection           aDirection,
                                      DeleteTextTxn**      aTxn)
{
  NS_ASSERTION(aDirection == eNext || aDirection == ePrevious,
               "invalid direction");
  nsAutoString data;
  aData->GetData(data);
  NS_ASSERTION(data.Length(), "Trying to delete from a zero-length node");
  NS_ENSURE_STATE(data.Length());

  uint32_t segOffset = aOffset, segLength = 1;
  if (aDirection == eNext) {
    if (segOffset + 1 < data.Length() &&
        NS_IS_HIGH_SURROGATE(data[segOffset]) &&
        NS_IS_LOW_SURROGATE(data[segOffset+1])) {
      // delete both halves of the surrogate pair
      ++segLength;
    }
  } else if (aOffset > 0) {
    --segOffset;
    if (segOffset > 0 &&
      NS_IS_LOW_SURROGATE(data[segOffset]) &&
      NS_IS_HIGH_SURROGATE(data[segOffset-1])) {
      ++segLength;
      --segOffset;
    }
  } else {
    return NS_ERROR_FAILURE;
  }
  return CreateTxnForDeleteText(aData, segOffset, segLength, aTxn);
}

//XXX: currently, this doesn't handle edge conditions because GetNext/GetPrior
//are not implemented
nsresult
nsEditor::CreateTxnForDeleteInsertionPoint(nsRange*          aRange,
                                           EDirection        aAction,
                                           EditAggregateTxn* aTxn,
                                           nsINode**         aNode,
                                           int32_t*          aOffset,
                                           int32_t*          aLength)
{
  MOZ_ASSERT(aAction != eNone);

  nsresult res;

  // get the node and offset of the insertion point
  nsCOMPtr<nsINode> node = aRange->GetStartParent();
  NS_ENSURE_STATE(node);

  int32_t offset = aRange->StartOffset();

  // determine if the insertion point is at the beginning, middle, or end of
  // the node
  nsCOMPtr<nsIDOMCharacterData> nodeAsCharData = do_QueryInterface(node);

  uint32_t count = node->Length();

  bool isFirst = (0 == offset);
  bool isLast  = (count == (uint32_t)offset);

  // XXX: if isFirst && isLast, then we'll need to delete the node
  //      as well as the 1 child

  // build a transaction for deleting the appropriate data
  // XXX: this has to come from rule section
  if (aAction == ePrevious && isFirst) {
    // we're backspacing from the beginning of the node.  Delete the first
    // thing to our left
    nsCOMPtr<nsIContent> priorNode = GetPriorNode(node, true);
    NS_ENSURE_STATE(priorNode);

    // there is a priorNode, so delete its last child (if chardata, delete the
    // last char). if it has no children, delete it
    nsCOMPtr<nsIDOMCharacterData> priorNodeAsCharData =
      do_QueryInterface(priorNode);
    if (priorNodeAsCharData) {
      uint32_t length = priorNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      NS_ENSURE_STATE(length);
      nsRefPtr<DeleteTextTxn> txn;
      res = CreateTxnForDeleteCharacter(priorNodeAsCharData, length,
                                        ePrevious, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);

      *aOffset = txn->GetOffset();
      *aLength = txn->GetNumCharsToDelete();
      aTxn->AppendChild(txn);
    } else {
      // priorNode is not chardata, so tell its parent to delete it
      nsRefPtr<DeleteNodeTxn> txn;
      res = CreateTxnForDeleteNode(priorNode, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);

      aTxn->AppendChild(txn);
    }

    NS_ADDREF(*aNode = priorNode);

    return NS_OK;
  }

  if (aAction == eNext && isLast) {
    // we're deleting from the end of the node.  Delete the first thing to our
    // right
    nsCOMPtr<nsIContent> nextNode = GetNextNode(node, true);
    NS_ENSURE_STATE(nextNode);

    // there is a nextNode, so delete its first child (if chardata, delete the
    // first char). if it has no children, delete it
    nsCOMPtr<nsIDOMCharacterData> nextNodeAsCharData =
      do_QueryInterface(nextNode);
    if (nextNodeAsCharData) {
      uint32_t length = nextNode->Length();
      // Bail out for empty chardata XXX: Do we want to do something else?
      NS_ENSURE_STATE(length);
      nsRefPtr<DeleteTextTxn> txn;
      res = CreateTxnForDeleteCharacter(nextNodeAsCharData, 0, eNext,
                                        getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);

      *aOffset = txn->GetOffset();
      *aLength = txn->GetNumCharsToDelete();
      aTxn->AppendChild(txn);
    } else {
      // nextNode is not chardata, so tell its parent to delete it
      nsRefPtr<DeleteNodeTxn> txn;
      res = CreateTxnForDeleteNode(nextNode, getter_AddRefs(txn));
      NS_ENSURE_SUCCESS(res, res);
      aTxn->AppendChild(txn);
    }

    NS_ADDREF(*aNode = nextNode);

    return NS_OK;
  }

  if (nodeAsCharData) {
    // we have chardata, so delete a char at the proper offset
    nsRefPtr<DeleteTextTxn> txn;
    res = CreateTxnForDeleteCharacter(nodeAsCharData, offset, aAction,
                                      getter_AddRefs(txn));
    NS_ENSURE_SUCCESS(res, res);

    aTxn->AppendChild(txn);
    NS_ADDREF(*aNode = node);
    *aOffset = txn->GetOffset();
    *aLength = txn->GetNumCharsToDelete();
  } else {
    // we're either deleting a node or chardata, need to dig into the next/prev
    // node to find out
    nsCOMPtr<nsINode> selectedNode;
    if (aAction == ePrevious) {
      selectedNode = GetPriorNode(node, offset, true);
    } else if (aAction == eNext) {
      selectedNode = GetNextNode(node, offset, true);
    }

    while (selectedNode &&
           selectedNode->IsNodeOfType(nsINode::eDATA_NODE) &&
           !selectedNode->Length()) {
      // Can't delete an empty chardata node (bug 762183)
      if (aAction == ePrevious) {
        selectedNode = GetPriorNode(selectedNode, true);
      } else if (aAction == eNext) {
        selectedNode = GetNextNode(selectedNode, true);
      }
    }
    NS_ENSURE_STATE(selectedNode);

    nsCOMPtr<nsIDOMCharacterData> selectedNodeAsCharData =
      do_QueryInterface(selectedNode);
    if (selectedNodeAsCharData) {
      // we are deleting from a chardata node, so do a character deletion
      uint32_t position = 0;
      if (aAction == ePrevious) {
        position = selectedNode->Length();
      }
      nsRefPtr<DeleteTextTxn> delTextTxn;
      res = CreateTxnForDeleteCharacter(selectedNodeAsCharData, position,
                                        aAction, getter_AddRefs(delTextTxn));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(delTextTxn, NS_ERROR_NULL_POINTER);

      aTxn->AppendChild(delTextTxn);
      *aOffset = delTextTxn->GetOffset();
      *aLength = delTextTxn->GetNumCharsToDelete();
    } else {
      nsRefPtr<DeleteNodeTxn> delElementTxn;
      res = CreateTxnForDeleteNode(selectedNode, getter_AddRefs(delElementTxn));
      NS_ENSURE_SUCCESS(res, res);
      NS_ENSURE_TRUE(delElementTxn, NS_ERROR_NULL_POINTER);

      aTxn->AppendChild(delElementTxn);
    }

    NS_ADDREF(*aNode = selectedNode);
  }

  return NS_OK;
}

nsresult 
nsEditor::CreateRange(nsIDOMNode *aStartParent, int32_t aStartOffset,
                      nsIDOMNode *aEndParent, int32_t aEndOffset,
                      nsIDOMRange **aRange)
{
  return nsRange::CreateRange(aStartParent, aStartOffset, aEndParent,
                              aEndOffset, aRange);
}

nsresult 
nsEditor::AppendNodeToSelectionAsRange(nsIDOMNode *aNode)
{
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsISelection> selection;
  nsresult res = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  if(!selection) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> parentNode;
  res = aNode->GetParentNode(getter_AddRefs(parentNode));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(parentNode, NS_ERROR_NULL_POINTER);
  
  int32_t offset = GetChildOffset(aNode, parentNode);
  
  nsCOMPtr<nsIDOMRange> range;
  res = CreateRange(parentNode, offset, parentNode, offset+1, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(range, NS_ERROR_NULL_POINTER);

  return selection->AddRange(range);
}

nsresult nsEditor::ClearSelection()
{
  nsCOMPtr<nsISelection> selection;
  nsresult res = nsEditor::GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(res, res);
  NS_ENSURE_TRUE(selection, NS_ERROR_FAILURE);
  return selection->RemoveAllRanges();  
}

already_AddRefed<Element>
nsEditor::CreateHTMLContent(const nsAString& aTag, ErrorResult& rv)
{
  nsCOMPtr<nsIDocument> doc = GetDocument();
  if (!doc) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // XXX Wallpaper over editor bug (editor tries to create elements with an
  //     empty nodename).
  if (aTag.IsEmpty()) {
    NS_ERROR("Don't pass an empty tag to nsEditor::CreateHTMLContent, "
             "check caller.");
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIContent> ret;
  nsresult res = doc->CreateElem(aTag, nullptr, kNameSpaceID_XHTML,
                                 getter_AddRefs(ret));
  if (NS_FAILED(res)) {
    rv.Throw(res);
  }
  return dont_AddRef(ret.forget().take()->AsElement());
}

nsresult
nsEditor::SetAttributeOrEquivalent(nsIDOMElement * aElement,
                                   const nsAString & aAttribute,
                                   const nsAString & aValue,
                                   bool aSuppressTransaction)
{
  return SetAttribute(aElement, aAttribute, aValue);
}

nsresult
nsEditor::RemoveAttributeOrEquivalent(nsIDOMElement * aElement,
                                      const nsAString & aAttribute,
                                      bool aSuppressTransaction)
{
  return RemoveAttribute(aElement, aAttribute);
}

nsresult
nsEditor::HandleKeyPressEvent(nsIDOMKeyEvent* aKeyEvent)
{
  // NOTE: When you change this method, you should also change:
  //   * editor/libeditor/text/tests/test_texteditor_keyevent_handling.html
  //   * editor/libeditor/html/tests/test_htmleditor_keyevent_handling.html
  //
  // And also when you add new key handling, you need to change the subclass's
  // HandleKeyPressEvent()'s switch statement.

  WidgetKeyboardEvent* nativeKeyEvent =
    aKeyEvent->GetInternalNSEvent()->AsKeyboardEvent();
  NS_ENSURE_TRUE(nativeKeyEvent, NS_ERROR_UNEXPECTED);
  NS_ASSERTION(nativeKeyEvent->message == NS_KEY_PRESS,
               "HandleKeyPressEvent gets non-keypress event");

  // if we are readonly or disabled, then do nothing.
  if (IsReadonly() || IsDisabled()) {
    // consume backspace for disabled and readonly textfields, to prevent
    // back in history, which could be confusing to users
    if (nativeKeyEvent->keyCode == nsIDOMKeyEvent::DOM_VK_BACK_SPACE) {
      aKeyEvent->PreventDefault();
    }
    return NS_OK;
  }

  switch (nativeKeyEvent->keyCode) {
    case nsIDOMKeyEvent::DOM_VK_META:
    case nsIDOMKeyEvent::DOM_VK_WIN:
    case nsIDOMKeyEvent::DOM_VK_SHIFT:
    case nsIDOMKeyEvent::DOM_VK_CONTROL:
    case nsIDOMKeyEvent::DOM_VK_ALT:
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
    case nsIDOMKeyEvent::DOM_VK_BACK_SPACE:
      if (nativeKeyEvent->IsControl() || nativeKeyEvent->IsAlt() ||
          nativeKeyEvent->IsMeta() || nativeKeyEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::ePrevious, nsIEditor::eStrip);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK;
    case nsIDOMKeyEvent::DOM_VK_DELETE:
      // on certain platforms (such as windows) the shift key
      // modifies what delete does (cmd_cut in this case).
      // bailing here to allow the keybindings to do the cut.
      if (nativeKeyEvent->IsShift() || nativeKeyEvent->IsControl() ||
          nativeKeyEvent->IsAlt() || nativeKeyEvent->IsMeta() ||
          nativeKeyEvent->IsOS()) {
        return NS_OK;
      }
      DeleteSelection(nsIEditor::eNext, nsIEditor::eStrip);
      aKeyEvent->PreventDefault(); // consumed
      return NS_OK; 
  }
  return NS_OK;
}

nsresult
nsEditor::HandleInlineSpellCheck(EditAction action,
                                   nsISelection *aSelection,
                                   nsIDOMNode *previousSelectedNode,
                                   int32_t previousSelectedOffset,
                                   nsIDOMNode *aStartNode,
                                   int32_t aStartOffset,
                                   nsIDOMNode *aEndNode,
                                   int32_t aEndOffset)
{
  // Have to cast action here because this method is from an IDL
  return mInlineSpellChecker ? mInlineSpellChecker->SpellCheckAfterEditorChange(
                                 (int32_t)action, aSelection,
                                 previousSelectedNode, previousSelectedOffset,
                                 aStartNode, aStartOffset, aEndNode,
                                 aEndOffset)
                             : NS_OK;
}

already_AddRefed<nsIContent>
nsEditor::FindSelectionRoot(nsINode *aNode)
{
  nsCOMPtr<nsIContent> rootContent = GetRoot();
  return rootContent.forget();
}

nsresult
nsEditor::InitializeSelection(nsIDOMEventTarget* aFocusEventTarget)
{
  nsCOMPtr<nsINode> targetNode = do_QueryInterface(aFocusEventTarget);
  NS_ENSURE_TRUE(targetNode, NS_ERROR_INVALID_ARG);
  nsCOMPtr<nsIContent> selectionRootContent = FindSelectionRoot(targetNode);
  if (!selectionRootContent) {
    return NS_OK;
  }

  bool isTargetDoc =
    targetNode->NodeType() == nsIDOMNode::DOCUMENT_NODE &&
    targetNode->HasFlag(NODE_IS_EDITABLE);

  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelection(getter_AddRefs(selection));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE(presShell, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISelectionController> selCon;
  rv = GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISelectionPrivate> selectionPrivate =
    do_QueryInterface(selection);
  NS_ENSURE_TRUE(selectionPrivate, NS_ERROR_UNEXPECTED);

  // Init the caret
  nsRefPtr<nsCaret> caret = presShell->GetCaret();
  NS_ENSURE_TRUE(caret, NS_ERROR_UNEXPECTED);
  caret->SetIgnoreUserModify(false);
  caret->SetCaretDOMSelection(selection);
  selCon->SetCaretReadOnly(IsReadonly());
  selCon->SetCaretEnabled(true);

  // Init selection
  selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
  selCon->SetSelectionFlags(nsISelectionDisplay::DISPLAY_ALL);
  selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
  // If the computed selection root isn't root content, we should set it
  // as selection ancestor limit.  However, if that is root element, it means
  // there is not limitation of the selection, then, we must set nullptr.
  // NOTE: If we set a root element to the ancestor limit, some selection
  // methods don't work fine.
  if (selectionRootContent->GetParent()) {
    selectionPrivate->SetAncestorLimiter(selectionRootContent);
  } else {
    selectionPrivate->SetAncestorLimiter(nullptr);
  }

  // XXX What case needs this?
  if (isTargetDoc) {
    int32_t rangeCount;
    selection->GetRangeCount(&rangeCount);
    if (rangeCount == 0) {
      BeginningOfDocument();
    }
  }

  return NS_OK;
}

void
nsEditor::FinalizeSelection()
{
  nsCOMPtr<nsISelectionController> selCon;
  nsresult rv = GetSelectionController(getter_AddRefs(selCon));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsISelection> selection;
  rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                            getter_AddRefs(selection));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsISelectionPrivate> selectionPrivate = do_QueryInterface(selection);
  NS_ENSURE_TRUE_VOID(selectionPrivate);

  selectionPrivate->SetAncestorLimiter(nullptr);

  nsCOMPtr<nsIPresShell> presShell = GetPresShell();
  NS_ENSURE_TRUE_VOID(presShell);

  selCon->SetCaretEnabled(false);

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE_VOID(fm);
  fm->UpdateCaretForCaretBrowsingMode();

  if (!HasIndependentSelection()) {
    // If this editor doesn't have an independent selection, i.e., it must
    // mean that it is an HTML editor, the selection controller is shared with
    // presShell.  So, even this editor loses focus, other part of the document
    // may still have focus.
    nsCOMPtr<nsIDocument> doc = GetDocument();
    ErrorResult ret;
    if (!doc || !doc->HasFocus(ret)) {
      // If the document already lost focus, mark the selection as disabled.
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
    } else {
      // Otherwise, mark selection as normal because outside of a
      // contenteditable element should be selected with normal selection
      // color after here.
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
    }
  } else if (IsFormWidget() || IsPasswordEditor() ||
             IsReadonly() || IsDisabled() || IsInputFiltered()) {
    // In <input> or <textarea>, the independent selection should be hidden
    // while this editor doesn't have focus.
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
  } else {
    // Otherwise, although we're not sure how this case happens, the
    // independent selection should be marked as disabled.
    selCon->SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
  }

  selCon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
}

dom::Element *
nsEditor::GetRoot()
{
  if (!mRootElement)
  {
    nsCOMPtr<nsIDOMElement> root;

    // Let GetRootElement() do the work
    GetRootElement(getter_AddRefs(root));
  }

  return mRootElement;
}

dom::Element*
nsEditor::GetEditorRoot()
{
  return GetRoot();
}

Element*
nsEditor::GetExposedRoot()
{
  Element* rootElement = GetRoot();

  // For plaintext editors, we need to ask the input/textarea element directly.
  if (rootElement && rootElement->IsRootOfNativeAnonymousSubtree()) {
    rootElement = rootElement->GetParent()->AsElement();
  }

  return rootElement;
}

nsresult
nsEditor::DetermineCurrentDirection()
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();
  NS_ENSURE_TRUE(rootElement, NS_ERROR_FAILURE);

  // If we don't have an explicit direction, determine our direction
  // from the content's direction
  if (!(mFlags & (nsIPlaintextEditor::eEditorLeftToRight |
                  nsIPlaintextEditor::eEditorRightToLeft))) {

    nsIFrame* frame = rootElement->GetPrimaryFrame();
    NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

    // Set the flag here, to enable us to use the same code path below.
    // It will be flipped before returning from the function.
    if (frame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    } else {
      mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SwitchTextDirection()
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();

  nsresult rv = DetermineCurrentDirection();
  NS_ENSURE_SUCCESS(rv, rv);

  // Apply the opposite direction
  if (mFlags & nsIPlaintextEditor::eEditorRightToLeft) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorLeftToRight),
                 "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), true);
  } else if (mFlags & nsIPlaintextEditor::eEditorLeftToRight) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorRightToLeft),
                 "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), true);
  }

  if (NS_SUCCEEDED(rv)) {
    FireInputEvent();
  }

  return rv;
}

void
nsEditor::SwitchTextDirectionTo(uint32_t aDirection)
{
  // Get the current root direction from its frame
  nsIContent* rootElement = GetExposedRoot();

  nsresult rv = DetermineCurrentDirection();
  NS_ENSURE_SUCCESS_VOID(rv);

  // Apply the requested direction
  if (aDirection == nsIPlaintextEditor::eEditorLeftToRight &&
      (mFlags & nsIPlaintextEditor::eEditorRightToLeft)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorLeftToRight),
                 "Unexpected mutually exclusive flag");
    mFlags &= ~nsIPlaintextEditor::eEditorRightToLeft;
    mFlags |= nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("ltr"), true);
  } else if (aDirection == nsIPlaintextEditor::eEditorRightToLeft &&
             (mFlags & nsIPlaintextEditor::eEditorLeftToRight)) {
    NS_ASSERTION(!(mFlags & nsIPlaintextEditor::eEditorRightToLeft),
                 "Unexpected mutually exclusive flag");
    mFlags |= nsIPlaintextEditor::eEditorRightToLeft;
    mFlags &= ~nsIPlaintextEditor::eEditorLeftToRight;
    rv = rootElement->SetAttr(kNameSpaceID_None, nsGkAtoms::dir, NS_LITERAL_STRING("rtl"), true);
  }

  if (NS_SUCCEEDED(rv)) {
    FireInputEvent();
  }
}

#if DEBUG_JOE
void
nsEditor::DumpNode(nsIDOMNode *aNode, int32_t indent)
{
  int32_t i;
  for (i=0; i<indent; i++)
    printf("  ");
  
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  nsCOMPtr<nsIDOMDocumentFragment> docfrag = do_QueryInterface(aNode);
  
  if (element || docfrag)
  { 
    if (element)
    {
      nsAutoString tag;
      element->GetTagName(tag);
      printf("<%s>\n", NS_LossyConvertUTF16toASCII(tag).get());
    }
    else
    {
      printf("<document fragment>\n");
    }
    nsCOMPtr<nsIDOMNodeList> childList;
    aNode->GetChildNodes(getter_AddRefs(childList));
    NS_ENSURE_TRUE(childList, NS_ERROR_NULL_POINTER);
    uint32_t numChildren;
    childList->GetLength(&numChildren);
    nsCOMPtr<nsIDOMNode> child, tmp;
    aNode->GetFirstChild(getter_AddRefs(child));
    for (i=0; i<numChildren; i++)
    {
      DumpNode(child, indent+1);
      child->GetNextSibling(getter_AddRefs(tmp));
      child = tmp;
    }
  }
  else if (IsTextNode(aNode))
  {
    nsCOMPtr<nsIDOMCharacterData> textNode = do_QueryInterface(aNode);
    nsAutoString str;
    textNode->GetData(str);
    nsAutoCString cstr;
    LossyCopyUTF16toASCII(str, cstr);
    cstr.ReplaceChar('\n', ' ');
    printf("<textnode> %s\n", cstr.get());
  }
}
#endif

bool
nsEditor::IsModifiableNode(nsIDOMNode *aNode)
{
  return true;
}

bool
nsEditor::IsModifiableNode(nsINode *aNode)
{
  return true;
}

already_AddRefed<nsIContent>
nsEditor::GetFocusedContent()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return nullptr;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, nullptr);

  nsCOMPtr<nsIContent> content = fm->GetFocusedContent();
  return SameCOMIdentity(content, piTarget) ? content.forget() : nullptr;
}

already_AddRefed<nsIContent>
nsEditor::GetFocusedContentForIME()
{
  return GetFocusedContent();
}

bool
nsEditor::IsActiveInDOMWindow()
{
  nsCOMPtr<nsIDOMEventTarget> piTarget = GetDOMEventTarget();
  if (!piTarget) {
    return false;
  }

  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, false);

  nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocWeak);
  nsPIDOMWindow* ourWindow = doc->GetWindow();
  nsCOMPtr<nsPIDOMWindow> win;
  nsIContent* content =
    nsFocusManager::GetFocusedDescendant(ourWindow, false,
                                         getter_AddRefs(win));
  return SameCOMIdentity(content, piTarget);
}

bool
nsEditor::IsAcceptableInputEvent(nsIDOMEvent* aEvent)
{
  // If the event is trusted, the event should always cause input.
  NS_ENSURE_TRUE(aEvent, false);

  // If this is mouse event but this editor doesn't have focus, we shouldn't
  // handle it.
  nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(aEvent);
  if (mouseEvent) {
    nsCOMPtr<nsIContent> focusedContent = GetFocusedContent();
    if (!focusedContent) {
      return false;
    }
  } else {
    nsAutoString eventType;
    aEvent->GetType(eventType);
    // If composition event or text event isn't dispatched via widget,
    // we need to ignore them since they cannot be managed by TextComposition.
    // E.g., the event was created by chrome JS.
    // Note that if we allow to handle such events, editor may be confused by
    // strange event order.
    if (eventType.EqualsLiteral("text") ||
        eventType.EqualsLiteral("compositionstart") ||
        eventType.EqualsLiteral("compositionend")) {
      WidgetGUIEvent* widgetGUIEvent =
        aEvent->GetInternalNSEvent()->AsGUIEvent();
      if (!widgetGUIEvent || !widgetGUIEvent->widget) {
        return false;
      }
    }
  }

  bool isTrusted;
  nsresult rv = aEvent->GetIsTrusted(&isTrusted);
  NS_ENSURE_SUCCESS(rv, false);
  if (isTrusted) {
    return true;
  }

  // Ignore untrusted mouse event.
  // XXX Why are we handling other untrusted input events?
  if (mouseEvent) {
    return false;
  }

  // Otherwise, we shouldn't handle any input events when we're not an active
  // element of the DOM window.
  return IsActiveInDOMWindow();
}

void
nsEditor::OnFocus(nsIDOMEventTarget* aFocusEventTarget)
{
  InitializeSelection(aFocusEventTarget);
  if (mInlineSpellChecker) {
    mInlineSpellChecker->UpdateCurrentDictionary();
  }
}

NS_IMETHODIMP
nsEditor::GetSuppressDispatchingInputEvent(bool *aSuppressed)
{
  NS_ENSURE_ARG_POINTER(aSuppressed);
  *aSuppressed = !mDispatchInputEvent;
  return NS_OK;
}

NS_IMETHODIMP
nsEditor::SetSuppressDispatchingInputEvent(bool aSuppress)
{
  mDispatchInputEvent = !aSuppress;
  return NS_OK;
}
