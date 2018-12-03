/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EditorBase_h
#define mozilla_EditorBase_h

#include "mozilla/Assertions.h"          // for MOZ_ASSERT, etc.
#include "mozilla/EditAction.h"          // for EditAction and EditSubAction
#include "mozilla/EditorDOMPoint.h"      // for EditorDOMPoint
#include "mozilla/Maybe.h"               // for Maybe
#include "mozilla/OwningNonNull.h"       // for OwningNonNull
#include "mozilla/PresShell.h"           // for PresShell
#include "mozilla/RangeBoundary.h"       // for RawRangeBoundary, RangeBoundary
#include "mozilla/SelectionState.h"      // for RangeUpdater, etc.
#include "mozilla/StyleSheet.h"          // for StyleSheet
#include "mozilla/TransactionManager.h"  // for TransactionManager
#include "mozilla/WeakPtr.h"             // for WeakPtr
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/Text.h"
#include "nsCOMPtr.h"  // for already_AddRefed, nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"             // for nsIDocument
#include "nsIContentInlines.h"       // for nsINode::IsEditable()
#include "nsIEditor.h"               // for nsIEditor, etc.
#include "nsIObserver.h"             // for NS_DECL_NSIOBSERVER, etc.
#include "nsIPlaintextEditor.h"      // for nsIPlaintextEditor, etc.
#include "nsISelectionController.h"  // for nsISelectionController constants
#include "nsISelectionListener.h"    // for nsISelectionListener
#include "nsISupportsImpl.h"         // for EditorBase::Release, etc.
#include "nsIWeakReferenceUtils.h"   // for nsWeakPtr
#include "nsLiteralString.h"         // for NS_LITERAL_STRING
#include "nsString.h"                // for nsCString
#include "nsTArray.h"                // for nsTArray and nsAutoTArray
#include "nsWeakReference.h"         // for nsSupportsWeakReference
#include "nscore.h"                  // for nsresult, nsAString, etc.

class mozInlineSpellChecker;
class nsAtom;
class nsIContent;
class nsIDocumentStateListener;
class nsIEditActionListener;
class nsIEditorObserver;
class nsINode;
class nsIPresShell;
class nsISupports;
class nsITransaction;
class nsITransactionListener;
class nsIWidget;
class nsRange;

namespace mozilla {
class AutoSelectionRestorer;
class AutoTopLevelEditSubActionNotifier;
class AutoTransactionBatch;
class AutoTransactionsConserveSelection;
class AutoUpdateViewBatch;
class ChangeAttributeTransaction;
class CompositionTransaction;
class CreateElementTransaction;
class CSSEditUtils;
class DeleteNodeTransaction;
class DeleteRangeTransaction;
class DeleteTextTransaction;
class EditAggregateTransaction;
class EditorEventListener;
class EditTransactionBase;
class ErrorResult;
class HTMLEditor;
class HTMLEditUtils;
class IMEContentObserver;
class InsertNodeTransaction;
class InsertTextTransaction;
class JoinNodeTransaction;
class PlaceholderTransaction;
class SplitNodeResult;
class SplitNodeTransaction;
class TextComposition;
class TextEditor;
class TextEditRules;
class TextInputListener;
class TextServicesDocument;
class TypeInState;
class WSRunObject;

namespace dom {
class DataTransfer;
class DragEvent;
class Element;
class EventTarget;
class Text;
}  // namespace dom

namespace widget {
struct IMEState;
}  // namespace widget

/**
 * CachedWeakPtr stores a pointer to a class which inherits nsIWeakReference.
 * If the instance of the class has already been destroyed, this returns
 * nullptr.  Otherwise, returns cached pointer.
 * If class T inherits nsISupports a lot, specify Base explicitly for avoiding
 * ambiguous conversion to nsISupports.
 */
template <class T, class Base = nsISupports>
class CachedWeakPtr final {
 public:
  CachedWeakPtr<T, Base>() : mCache(nullptr) {}
  explicit CachedWeakPtr<T, Base>(T* aObject) {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aObject));
    mCache = aObject;
  }
  explicit CachedWeakPtr<T, Base>(const nsCOMPtr<T>& aOther) {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aOther.get()));
    mCache = aOther;
  }
  explicit CachedWeakPtr<T, Base>(already_AddRefed<T>& aOther) {
    RefPtr<T> other = aOther;
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(other.get()));
    mCache = other;
  }

  CachedWeakPtr<T, Base>& operator=(T* aObject) {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aObject));
    mCache = aObject;
    return *this;
  }
  CachedWeakPtr<T, Base>& operator=(const nsCOMPtr<T>& aOther) {
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(aOther.get()));
    mCache = aOther;
    return *this;
  }
  CachedWeakPtr<T, Base>& operator=(already_AddRefed<T>& aOther) {
    RefPtr<T> other = aOther;
    mWeakPtr = do_GetWeakReference(static_cast<Base*>(other.get()));
    mCache = other;
    return *this;
  }

  bool IsAlive() const { return mWeakPtr && mWeakPtr->IsAlive(); }

  explicit operator bool() const { return mWeakPtr; }
  operator T*() const { return get(); }
  T* get() const {
    if (mCache && !mWeakPtr->IsAlive()) {
      const_cast<CachedWeakPtr<T, Base>*>(this)->mCache = nullptr;
    }
    return mCache;
  }

 private:
  nsWeakPtr mWeakPtr;
  T* MOZ_NON_OWNING_REF mCache;
};

#define kMOZEditorBogusNodeAttrAtom nsGkAtoms::mozeditorbogusnode
#define kMOZEditorBogusNodeValue NS_LITERAL_STRING("TRUE")

/**
 * SplitAtEdges is for EditorBase::SplitNodeDeepWithTransaction(),
 * HTMLEditor::InsertNodeAtPoint()
 */
enum class SplitAtEdges {
  // EditorBase::SplitNodeDeepWithTransaction() won't split container element
  // nodes at their edges.  I.e., when split point is start or end of
  // container, it won't be split.
  eDoNotCreateEmptyContainer,
  // EditorBase::SplitNodeDeepWithTransaction() always splits containers even
  // if the split point is at edge of a container.  E.g., if split point is
  // start of an inline element, empty inline element is created as a new left
  // node.
  eAllowToCreateEmptyContainer,
};

/**
 * Implementation of an editor object.  it will be the controller/focal point
 * for the main editor services. i.e. the GUIManager, publishing, transaction
 * manager, event interfaces. the idea for the event interfaces is to have them
 * delegate the actual commands to the editor independent of the XPFE
 * implementation.
 */
class EditorBase : public nsIEditor,
                   public nsISelectionListener,
                   public nsSupportsWeakReference {
 public:
  /****************************************************************************
   * NOTE: DO NOT MAKE YOUR NEW METHODS PUBLIC IF they are called by other
   *       classes under libeditor except EditorEventListener and
   *       HTMLEditorEventListener because each public method which may fire
   *       eEditorInput event will need to instantiate new stack class for
   *       managing input type value of eEditorInput and cache some objects
   *       for smarter handling.  In other words, when you add new root
   *       method to edit the DOM tree, you can make your new method public.
   ****************************************************************************/

  typedef dom::Element Element;
  typedef dom::Selection Selection;
  typedef dom::Text Text;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(EditorBase, nsIEditor)

  // nsIEditor methods
  NS_DECL_NSIEDITOR

  // nsISelectionListener method
  NS_DECL_NSISELECTIONLISTENER

  /**
   * The default constructor. This should suffice. the setting of the
   * interfaces is done after the construction of the editor class.
   */
  EditorBase();

  /**
   * Init is to tell the implementation of nsIEditor to begin its services
   * @param aDoc          The dom document interface being observed
   * @param aRoot         This is the root of the editable section of this
   *                      document. If it is null then we get root
   *                      from document body.
   * @param aSelCon       this should be used to get the selection location
   *                      (will be null for HTML editors)
   * @param aFlags        A bitmask of flags for specifying the behavior
   *                      of the editor.
   */
  virtual nsresult Init(nsIDocument& doc, Element* aRoot,
                        nsISelectionController* aSelCon, uint32_t aFlags,
                        const nsAString& aInitialValue);

  /**
   * PostCreate should be called after Init, and is the time that the editor
   * tells its documentStateObservers that the document has been created.
   */
  nsresult PostCreate();

  /**
   * PreDestroy is called before the editor goes away, and gives the editor a
   * chance to tell its documentStateObservers that the document is going away.
   * @param aDestroyingFrames set to true when the frames being edited
   * are being destroyed (so there is no need to modify any nsISelections,
   * nor is it safe to do so)
   */
  virtual void PreDestroy(bool aDestroyingFrames);

  bool IsInitialized() const { return !!mDocument; }
  bool Destroyed() const { return mDidPreDestroy; }

  nsIDocument* GetDocument() const { return mDocument; }

  nsIPresShell* GetPresShell() const {
    return mDocument ? mDocument->GetShell() : nullptr;
  }
  nsPresContext* GetPresContext() const {
    nsIPresShell* presShell = GetPresShell();
    return presShell ? presShell->GetPresContext() : nullptr;
  }

  already_AddRefed<nsIWidget> GetWidget();

  nsISelectionController* GetSelectionController() const {
    if (mSelectionController) {
      return mSelectionController;
    }
    if (!mDocument) {
      return nullptr;
    }
    nsIPresShell* presShell = mDocument->GetShell();
    if (!presShell) {
      return nullptr;
    }
    nsISelectionController* sc = static_cast<PresShell*>(presShell);
    return sc;
  }

  nsresult GetSelection(SelectionType aSelectionType,
                        Selection** aSelection) const;

  Selection* GetSelection(
      SelectionType aSelectionType = SelectionType::eNormal) const {
    if (aSelectionType == SelectionType::eNormal &&
        IsEditActionDataAvailable()) {
      return SelectionRefPtr().get();
    }
    nsISelectionController* sc = GetSelectionController();
    if (!sc) {
      return nullptr;
    }
    Selection* selection = sc->GetSelection(ToRawSelectionType(aSelectionType));
    return selection;
  }

  /**
   * Fast non-refcounting editor root element accessor
   */
  Element* GetRoot() const { return mRootElement; }

  /**
   * Set or unset TextInputListener.  If setting non-nullptr when the editor
   * already has a TextInputListener, this will crash in debug build.
   */
  void SetTextInputListener(TextInputListener* aTextInputListener);

  /**
   * Set or unset IMEContentObserver.  If setting non-nullptr when the editor
   * already has an IMEContentObserver, this will crash in debug build.
   */
  void SetIMEContentObserver(IMEContentObserver* aIMEContentObserver);

  /**
   * Returns current composition.
   */
  TextComposition* GetComposition() const;

  /**
   * Get preferred IME status of current widget.
   */
  virtual nsresult GetPreferredIMEState(widget::IMEState* aState);

  /**
   * Returns true if there is composition string and not fixed.
   */
  bool IsIMEComposing() const;

  /**
   * Commit composition if there is.
   * Note that when there is a composition, this requests to commit composition
   * to native IME.  Therefore, when there is composition, this can do anything.
   * For example, the editor instance, the widget or the process itself may
   * be destroyed.
   */
  nsresult CommitComposition();

  /**
   * ToggleTextDirection() toggles text-direction of the root element.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult ToggleTextDirection();

  /**
   * SwitchTextDirectionTo() sets the text-direction of the root element to
   * LTR or RTL.
   */
  enum class TextDirection {
    eLTR,
    eRTL,
  };
  MOZ_CAN_RUN_SCRIPT
  void SwitchTextDirectionTo(TextDirection aTextDirection);

  /**
   * Finalizes selection and caret for the editor.
   */
  nsresult FinalizeSelection();

  /**
   * Returns true if selection is in an editable element and both the range
   * start and the range end are editable.  E.g., even if the selection range
   * includes non-editable elements, returns true when one of common ancestors
   * of the range start and the range end is editable.  Otherwise, false.
   */
  bool IsSelectionEditable();

  /**
   * Returns number of undo or redo items.
   */
  size_t NumberOfUndoItems() const {
    return mTransactionManager ? mTransactionManager->NumberOfUndoItems() : 0;
  }
  size_t NumberOfRedoItems() const {
    return mTransactionManager ? mTransactionManager->NumberOfRedoItems() : 0;
  }

  /**
   * Returns number of maximum undo/redo transactions.
   */
  int32_t NumberOfMaximumTransactions() const {
    return mTransactionManager
               ? mTransactionManager->NumberOfMaximumTransactions()
               : 0;
  }

  /**
   * Returns true if this editor can store transactions for undo/redo.
   */
  bool IsUndoRedoEnabled() const {
    return mTransactionManager &&
           mTransactionManager->NumberOfMaximumTransactions();
  }

  /**
   * Return true if it's possible to undo/redo right now.
   */
  bool CanUndo() const {
    return IsUndoRedoEnabled() && NumberOfUndoItems() > 0;
  }
  bool CanRedo() const {
    return IsUndoRedoEnabled() && NumberOfRedoItems() > 0;
  }

  /**
   * Enables or disables undo/redo feature.  Returns true if it succeeded,
   * otherwise, e.g., we're undoing or redoing, returns false.
   */
  bool EnableUndoRedo(int32_t aMaxTransactionCount = -1) {
    if (!mTransactionManager) {
      mTransactionManager = new TransactionManager();
    }
    return mTransactionManager->EnableUndoRedo(aMaxTransactionCount);
  }
  bool DisableUndoRedo() {
    if (!mTransactionManager) {
      return true;
    }
    return mTransactionManager->DisableUndoRedo();
  }
  bool ClearUndoRedo() {
    if (!mTransactionManager) {
      return true;
    }
    return mTransactionManager->ClearUndoRedo();
  }

  /**
   * Adds or removes transaction listener to or from the transaction manager.
   * Note that TransactionManager does not check if the listener is in the
   * array.  So, caller of AddTransactionListener() needs to manage if it's
   * already been registered to the transaction manager.
   */
  bool AddTransactionListener(nsITransactionListener& aListener) {
    if (!mTransactionManager) {
      return false;
    }
    return mTransactionManager->AddTransactionListener(aListener);
  }
  bool RemoveTransactionListener(nsITransactionListener& aListener) {
    if (!mTransactionManager) {
      return false;
    }
    return mTransactionManager->RemoveTransactionListener(aListener);
  }

  virtual nsresult HandleKeyPressEvent(WidgetKeyboardEvent* aKeyboardEvent);

  virtual dom::EventTarget* GetDOMEventTarget() = 0;

  /**
   * Accessor methods to flags.
   */
  uint32_t Flags() const { return mFlags; }

  nsresult AddFlags(uint32_t aFlags) {
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = (kOldFlags | aFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags);  // virtual call and may be expensive.
  }
  nsresult RemoveFlags(uint32_t aFlags) {
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = (kOldFlags & ~aFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags);  // virtual call and may be expensive.
  }
  nsresult AddAndRemoveFlags(uint32_t aAddingFlags, uint32_t aRemovingFlags) {
    MOZ_ASSERT(!(aAddingFlags & aRemovingFlags),
               "Same flags are specified both adding and removing");
    const uint32_t kOldFlags = Flags();
    const uint32_t kNewFlags = ((kOldFlags | aAddingFlags) & ~aRemovingFlags);
    if (kNewFlags == kOldFlags) {
      return NS_OK;
    }
    return SetFlags(kNewFlags);  // virtual call and may be expensive.
  }

  bool IsPlaintextEditor() const {
    return (mFlags & nsIPlaintextEditor::eEditorPlaintextMask) != 0;
  }

  bool IsSingleLineEditor() const {
    return (mFlags & nsIPlaintextEditor::eEditorSingleLineMask) != 0;
  }

  bool IsPasswordEditor() const {
    return (mFlags & nsIPlaintextEditor::eEditorPasswordMask) != 0;
  }

  // FYI: Both IsRightToLeft() and IsLeftToRight() may return false if
  //      the editor inherits the content node's direction.
  bool IsRightToLeft() const {
    return (mFlags & nsIPlaintextEditor::eEditorRightToLeft) != 0;
  }
  bool IsLeftToRight() const {
    return (mFlags & nsIPlaintextEditor::eEditorLeftToRight) != 0;
  }

  bool IsReadonly() const {
    return (mFlags & nsIPlaintextEditor::eEditorReadonlyMask) != 0;
  }

  bool IsDisabled() const {
    return (mFlags & nsIPlaintextEditor::eEditorDisabledMask) != 0;
  }

  bool IsInputFiltered() const {
    return (mFlags & nsIPlaintextEditor::eEditorFilterInputMask) != 0;
  }

  bool IsMailEditor() const {
    return (mFlags & nsIPlaintextEditor::eEditorMailMask) != 0;
  }

  bool IsWrapHackEnabled() const {
    return (mFlags & nsIPlaintextEditor::eEditorEnableWrapHackMask) != 0;
  }

  bool IsFormWidget() const {
    return (mFlags & nsIPlaintextEditor::eEditorWidgetMask) != 0;
  }

  bool NoCSS() const {
    return (mFlags & nsIPlaintextEditor::eEditorNoCSSMask) != 0;
  }

  bool IsInteractionAllowed() const {
    return (mFlags & nsIPlaintextEditor::eEditorAllowInteraction) != 0;
  }

  bool DontEchoPassword() const {
    return (mFlags & nsIPlaintextEditor::eEditorDontEchoPassword) != 0;
  }

  bool ShouldSkipSpellCheck() const {
    return (mFlags & nsIPlaintextEditor::eEditorSkipSpellCheck) != 0;
  }

  bool IsTabbable() const {
    return IsSingleLineEditor() || IsPasswordEditor() || IsFormWidget() ||
           IsInteractionAllowed();
  }

  bool HasIndependentSelection() const { return !!mSelectionController; }

  bool IsModifiable() const { return !IsReadonly(); }

  /**
   * IsInEditSubAction() return true while the instance is handling an edit
   * sub-action.  Otherwise, false.
   */
  bool IsInEditSubAction() const { return mIsInEditSubAction; }

  /**
   * SuppressDispatchingInputEvent() suppresses or unsuppresses dispatching
   * "input" event.
   */
  void SuppressDispatchingInputEvent(bool aSuppress) {
    mDispatchInputEvent = !aSuppress;
  }

  /**
   * IsSuppressingDispatchingInputEvent() returns true if the editor stops
   * dispatching input event.  Otherwise, false.
   */
  bool IsSuppressingDispatchingInputEvent() const {
    return !mDispatchInputEvent;
  }

  /**
   * Returns true if markNodeDirty() has any effect.  Returns false if
   * markNodeDirty() is a no-op.
   */
  bool OutputsMozDirty() const {
    // Return true for Composer (!IsInteractionAllowed()) or mail
    // (IsMailEditor()), but false for webpages.
    return !IsInteractionAllowed() || IsMailEditor();
  }

  /**
   * Get the focused content, if we're focused.  Returns null otherwise.
   */
  virtual nsIContent* GetFocusedContent();

  /**
   * Get the focused content for the argument of some IMEStateManager's
   * methods.
   */
  virtual already_AddRefed<nsIContent> GetFocusedContentForIME();

  /**
   * Whether the aGUIEvent should be handled by this editor or not.  When this
   * returns false, The aGUIEvent shouldn't be handled on this editor,
   * i.e., The aGUIEvent should be handled by another inner editor or ancestor
   * elements.
   */
  virtual bool IsAcceptableInputEvent(WidgetGUIEvent* aGUIEvent);

  /**
   * FindSelectionRoot() returns a selection root of this editor when aNode
   * gets focus.  aNode must be a content node or a document node.  When the
   * target isn't a part of this editor, returns nullptr.  If this is for
   * designMode, you should set the document node to aNode except that an
   * element in the document has focus.
   */
  virtual Element* FindSelectionRoot(nsINode* aNode) const;

  /**
   * This method has to be called by EditorEventListener::Focus.
   * All actions that have to be done when the editor is focused needs to be
   * added here.
   */
  void OnFocus(dom::EventTarget* aFocusEventTarget);

  /** Resyncs spellchecking state (enabled/disabled).  This should be called
   * when anything that affects spellchecking state changes, such as the
   * spellcheck attribute value.
   */
  void SyncRealTimeSpell();

  /**
   * This method re-initializes the selection and caret state that are for
   * current editor state. When editor session is destroyed, it always reset
   * selection state even if this has no focus.  So if destroying editor,
   * we have to call this method for focused editor to set selection state.
   */
  void ReinitializeSelection(Element& aElement);

 protected:  // AutoEditActionDataSetter, this shouldn't be accessed by friends.
  /**
   * AutoEditActionDataSetter grabs some necessary objects for handling any
   * edit actions and store the edit action what we're handling.  When this is
   * created, its pointer is set to the mEditActionData, and this guarantees
   * the lifetime of grabbing objects until it's destroyed.
   */
  class MOZ_STACK_CLASS AutoEditActionDataSetter final {
   public:
    AutoEditActionDataSetter(const EditorBase& aEditorBase,
                             EditAction aEditAction);
    ~AutoEditActionDataSetter();

    void UpdateEditAction(EditAction aEditAction) { mEditAction = aEditAction; }

    bool CanHandle() const { return mSelection && mEditorBase.IsInitialized(); }

    const RefPtr<Selection>& SelectionRefPtr() const { return mSelection; }
    EditAction GetEditAction() const { return mEditAction; }

    void SetTopLevelEditSubAction(EditSubAction aEditSubAction,
                                  EDirection aDirection = eNone) {
      mTopLevelEditSubAction = aEditSubAction;
      switch (mTopLevelEditSubAction) {
        case EditSubAction::eInsertNode:
        case EditSubAction::eCreateNode:
        case EditSubAction::eSplitNode:
        case EditSubAction::eInsertText:
        case EditSubAction::eInsertTextComingFromIME:
        case EditSubAction::eSetTextProperty:
        case EditSubAction::eRemoveTextProperty:
        case EditSubAction::eRemoveAllTextProperties:
        case EditSubAction::eSetText:
        case EditSubAction::eInsertLineBreak:
        case EditSubAction::eInsertParagraphSeparator:
        case EditSubAction::eCreateOrChangeList:
        case EditSubAction::eIndent:
        case EditSubAction::eOutdent:
        case EditSubAction::eSetOrClearAlignment:
        case EditSubAction::eCreateOrRemoveBlock:
        case EditSubAction::eRemoveList:
        case EditSubAction::eCreateOrChangeDefinitionList:
        case EditSubAction::eInsertElement:
        case EditSubAction::eInsertQuotation:
        case EditSubAction::ePasteHTMLContent:
        case EditSubAction::eInsertHTMLSource:
        case EditSubAction::eSetPositionToAbsolute:
        case EditSubAction::eSetPositionToStatic:
        case EditSubAction::eDecreaseZIndex:
        case EditSubAction::eIncreaseZIndex:
          MOZ_ASSERT(aDirection == eNext);
          mDirectionOfTopLevelEditSubAction = eNext;
          break;
        case EditSubAction::eJoinNodes:
        case EditSubAction::eDeleteText:
          MOZ_ASSERT(aDirection == ePrevious);
          mDirectionOfTopLevelEditSubAction = ePrevious;
          break;
        case EditSubAction::eUndo:
        case EditSubAction::eRedo:
        case EditSubAction::eComputeTextToOutput:
        case EditSubAction::eCreateBogusNode:
        case EditSubAction::eNone:
          MOZ_ASSERT(aDirection == eNone);
          mDirectionOfTopLevelEditSubAction = eNone;
          break;
        case EditSubAction::eReplaceHeadWithHTMLSource:
          // NOTE: Not used with AutoTopLevelEditSubActionNotifier.
          mDirectionOfTopLevelEditSubAction = eNone;
          break;
        case EditSubAction::eDeleteNode:
        case EditSubAction::eDeleteSelectedContent:
          // Unfortunately, eDeleteNode and eDeleteSelectedContent is used with
          // any direction.  We might have specific sub-action for each
          // direction, but there are some points referencing
          // eDeleteSelectedContent so that we should keep storing direction
          // as-is for now.
          mDirectionOfTopLevelEditSubAction = aDirection;
          break;
      }
    }
    EditSubAction GetTopLevelEditSubAction() const {
      MOZ_ASSERT(CanHandle());
      return mTopLevelEditSubAction;
    }
    EDirection GetDirectionOfTopLevelEditSubAction() const {
      return mDirectionOfTopLevelEditSubAction;
    }

    SelectionState& SavedSelectionRef() {
      return mParentData ? mParentData->SavedSelectionRef() : mSavedSelection;
    }
    const SelectionState& SavedSelectionRef() const {
      return mParentData ? mParentData->SavedSelectionRef() : mSavedSelection;
    }

    RangeUpdater& RangeUpdaterRef() {
      return mParentData ? mParentData->RangeUpdaterRef() : mRangeUpdater;
    }
    const RangeUpdater& RangeUpdaterRef() const {
      return mParentData ? mParentData->RangeUpdaterRef() : mRangeUpdater;
    }

   private:
    EditorBase& mEditorBase;
    RefPtr<Selection> mSelection;
    // EditAction may be nested, for example, a command may be executed
    // from mutation event listener which is run while editor changes
    // the DOM tree.  In such case, we need to handle edit action separately.
    AutoEditActionDataSetter* mParentData;

    // Cached selection for AutoSelectionRestorer.
    SelectionState mSavedSelection;

    // Utility class object for maintaining preserved ranges.
    RangeUpdater mRangeUpdater;

    EditAction mEditAction;
    EditSubAction mTopLevelEditSubAction;
    EDirection mDirectionOfTopLevelEditSubAction;

    AutoEditActionDataSetter() = delete;
    AutoEditActionDataSetter(const AutoEditActionDataSetter& aOther) = delete;
  };

 protected:  // May be called by friends.
  /****************************************************************************
   * Some classes like TextEditRules, HTMLEditRules, WSRunObject which are
   * part of handling edit actions are allowed to call the following protected
   * methods.  However, those methods won't prepare caches of some objects
   * which are necessary for them.  So, if you want some following methods
   * to do that for you, you need to create a wrapper method in public scope
   * and call it.
   ****************************************************************************/

  bool IsEditActionDataAvailable() const {
    return mEditActionData && mEditActionData->CanHandle();
  }

  /**
   * SelectionRefPtr() returns cached Selection.  This is pretty faster than
   * EditorBase::GetSelection() if available.
   * Note that this never returns nullptr unless public methods ignore
   * result of AutoEditActionDataSetter::CanHandle() and keep handling edit
   * action but any methods should stop handling edit action if it returns
   * false.
   */
  const RefPtr<Selection>& SelectionRefPtr() const {
    MOZ_ASSERT(mEditActionData);
    return mEditActionData->SelectionRefPtr();
  }

  /**
   * GetEditAction() returns EditAction which is being handled.  If some
   * edit actions are nested, this returns the innermost edit action.
   */
  EditAction GetEditAction() const {
    return mEditActionData ? mEditActionData->GetEditAction()
                           : EditAction::eNone;
  }

  /**
   * GetTopLevelEditSubAction() returns the top level edit sub-action.
   * For example, if selected content is being replaced with inserted text,
   * while removing selected content, the top level edit sub-action may be
   * EditSubAction::eDeleteSelectedContent.  However, while inserting new
   * text, the top level edit sub-action may be EditSubAction::eInsertText.
   * So, this result means what we are doing right now unless you're looking
   * for a case which the method is called via mutation event listener or
   * selectionchange event listener which are fired while handling the edit
   * sub-action.
   */
  EditSubAction GetTopLevelEditSubAction() const {
    return mEditActionData ? mEditActionData->GetTopLevelEditSubAction()
                           : EditSubAction::eNone;
  }

  /**
   * GetDirectionOfTopLevelEditSubAction() returns direction which user
   * intended for doing the edit sub-action.
   */
  EDirection GetDirectionOfTopLevelEditSubAction() const {
    return mEditActionData
               ? mEditActionData->GetDirectionOfTopLevelEditSubAction()
               : eNone;
  }

  /**
   * SavedSelection() returns reference to saved selection which are
   * stored by AutoSelectionRestorer.
   */
  SelectionState& SavedSelectionRef() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->SavedSelectionRef();
  }
  const SelectionState& SavedSelectionRef() const {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->SavedSelectionRef();
  }

  RangeUpdater& RangeUpdaterRef() {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->RangeUpdaterRef();
  }
  const RangeUpdater& RangeUpdaterRef() const {
    MOZ_ASSERT(IsEditActionDataAvailable());
    return mEditActionData->RangeUpdaterRef();
  }

  /**
   * InsertTextWithTransaction() inserts aStringToInsert to aPointToInsert or
   * better insertion point around it.  If aPointToInsert isn't in a text node,
   * this method looks for the nearest point in a text node with
   * FindBetterInsertionPoint().  If there is no text node, this creates
   * new text node and put aStringToInsert to it.
   *
   * @param aDocument       The document of this editor.
   * @param aStringToInsert The string to insert.
   * @param aPointToInser   The point to insert aStringToInsert.
   *                        Must be valid DOM point.
   * @param aPointAfterInsertedString
   *                        The point after inserted aStringToInsert.
   *                        So, when this method actually inserts string,
   *                        this is set to a point in the text node.
   *                        Otherwise, this may be set to aPointToInsert.
   * @return                When this succeeds to insert the string or
   *                        does nothing during composition, returns NS_OK.
   *                        Otherwise, an error code.
   */
  virtual nsresult InsertTextWithTransaction(
      nsIDocument& aDocument, const nsAString& aStringToInsert,
      const EditorRawDOMPoint& aPointToInsert,
      EditorRawDOMPoint* aPointAfterInsertedString = nullptr);

  /**
   * InsertTextIntoTextNodeWithTransaction() inserts aStringToInsert into
   * aOffset of aTextNode with transaction.
   *
   * @param aStringToInsert     String to be inserted.
   * @param aTextNode           Text node to contain aStringToInsert.
   * @param aOffset             Offset at insertion point in aTextNode.
   * @param aSuppressIME        true if it's not a part of IME composition.
   *                            E.g., adjusting whitespaces during composition.
   *                            false, otherwise.
   */
  nsresult InsertTextIntoTextNodeWithTransaction(
      const nsAString& aStringToInsert, Text& aTextNode, int32_t aOffset,
      bool aSuppressIME = false);

  nsresult SetTextImpl(const nsAString& aString, Text& aTextNode);

  /**
   * DeleteNodeWithTransaction() removes aNode from the DOM tree.
   *
   * @param aNode       The node which will be removed form the DOM tree.
   */
  nsresult DeleteNodeWithTransaction(nsINode& aNode);

  /**
   * InsertNodeWithTransaction() inserts aContentToInsert before the child
   * specified by aPointToInsert.
   *
   * @param aContentToInsert    The node to be inserted.
   * @param aPointToInsert      The insertion point of aContentToInsert.
   *                            If this refers end of the container, the
   *                            transaction will append the node to the
   *                            container.  Otherwise, will insert the node
   *                            before child node referred by this.
   */
  template <typename PT, typename CT>
  nsresult InsertNodeWithTransaction(
      nsIContent& aContentToInsert,
      const EditorDOMPointBase<PT, CT>& aPointToInsert);

  /**
   * ReplaceContainerWithTransaction() creates new element whose name is
   * aTagName, moves all children in aOldContainer to the new element, then,
   * removes aOldContainer from the DOM tree.
   *
   * @param aOldContainer       The element node which should be replaced
   *                            with new element.
   * @param aTagName            The name of new element node.
   */
  already_AddRefed<Element> ReplaceContainerWithTransaction(
      Element& aOldContainer, nsAtom& aTagName) {
    return ReplaceContainerWithTransactionInternal(
        aOldContainer, aTagName, *nsGkAtoms::_empty, EmptyString(), false);
  }

  /**
   * ReplaceContainerAndCloneAttributesWithTransaction() creates new element
   * whose name is aTagName, copies all attributes from aOldContainer to the
   * new element, moves all children in aOldContainer to the new element, then,
   * removes aOldContainer from the DOM tree.
   *
   * @param aOldContainer       The element node which should be replaced
   *                            with new element.
   * @param aTagName            The name of new element node.
   */
  already_AddRefed<Element> ReplaceContainerAndCloneAttributesWithTransaction(
      Element& aOldContainer, nsAtom& aTagName) {
    return ReplaceContainerWithTransactionInternal(
        aOldContainer, aTagName, *nsGkAtoms::_empty, EmptyString(), true);
  }

  /**
   * ReplaceContainerWithTransaction() creates new element whose name is
   * aTagName, sets aAttributes of the new element to aAttributeValue, moves
   * all children in aOldContainer to the new element, then, removes
   * aOldContainer from the DOM tree.
   *
   * @param aOldContainer       The element node which should be replaced
   *                            with new element.
   * @param aTagName            The name of new element node.
   * @param aAttribute          Attribute name to be set to the new element.
   * @param aAttributeValue     Attribute value to be set to aAttribute.
   */
  already_AddRefed<Element> ReplaceContainerWithTransaction(
      Element& aOldContainer, nsAtom& aTagName, nsAtom& aAttribute,
      const nsAString& aAttributeValue) {
    return ReplaceContainerWithTransactionInternal(
        aOldContainer, aTagName, aAttribute, aAttributeValue, false);
  }

  /**
   * CloneAttributesWithTransaction() clones all attributes from
   * aSourceElement to aDestElement after removing all attributes in
   * aDestElement.
   */
  void CloneAttributesWithTransaction(Element& aDestElement,
                                      Element& aSourceElement);

  /**
   * RemoveContainerWithTransaction() removes aElement from the DOM tree and
   * moves all its children to the parent of aElement.
   *
   * @param aElement            The element to be removed.
   */
  nsresult RemoveContainerWithTransaction(Element& aElement);

  /**
   * InsertContainerWithTransaction() creates new element whose name is
   * aTagName, moves aContent into the new element, then, inserts the new
   * element into where aContent was.
   * Note that this method does not check if aContent is valid child of
   * the new element.  So, callers need to guarantee it.
   *
   * @param aContent            The content which will be wrapped with new
   *                            element.
   * @param aTagName            Element name of new element which will wrap
   *                            aContent and be inserted into where aContent
   *                            was.
   * @return                    The new element.
   */
  already_AddRefed<Element> InsertContainerWithTransaction(nsIContent& aContent,
                                                           nsAtom& aTagName) {
    return InsertContainerWithTransactionInternal(
        aContent, aTagName, *nsGkAtoms::_empty, EmptyString());
  }

  /**
   * InsertContainerWithTransaction() creates new element whose name is
   * aTagName, sets its aAttribute to aAttributeValue, moves aContent into the
   * new element, then, inserts the new element into where aContent was.
   * Note that this method does not check if aContent is valid child of
   * the new element.  So, callers need to guarantee it.
   *
   * @param aContent            The content which will be wrapped with new
   *                            element.
   * @param aTagName            Element name of new element which will wrap
   *                            aContent and be inserted into where aContent
   *                            was.
   * @param aAttribute          Attribute which should be set to the new
   *                            element.
   * @param aAttributeValue     Value to be set to aAttribute.
   * @return                    The new element.
   */
  already_AddRefed<Element> InsertContainerWithTransaction(
      nsIContent& aContent, nsAtom& aTagName, nsAtom& aAttribute,
      const nsAString& aAttributeValue) {
    return InsertContainerWithTransactionInternal(aContent, aTagName,
                                                  aAttribute, aAttributeValue);
  }

  /**
   * SplitNodeWithTransaction() creates a transaction to create a new node
   * (left node) identical to an existing node (right node), and split the
   * contents between the same point in both nodes, then, execute the
   * transaction.
   *
   * @param aStartOfRightNode   The point to split.  Its container will be
   *                            the right node, i.e., become the new node's
   *                            next sibling.  And the point will be start
   *                            of the right node.
   * @param aError              If succeed, returns no error.  Otherwise, an
   *                            error.
   */
  template <typename PT, typename CT>
  already_AddRefed<nsIContent> SplitNodeWithTransaction(
      const EditorDOMPointBase<PT, CT>& aStartOfRightNode,
      ErrorResult& aResult);

  /**
   * JoinNodesWithTransaction() joins aLeftNode and aRightNode.  Content of
   * aLeftNode will be merged into aRightNode.  Actual implemenation of this
   * method is JoinNodesImpl().  So, see its explanation for the detail.
   *
   * @param aLeftNode   Will be removed from the DOM tree.
   * @param aRightNode  The node which will be new container of the content of
   *                    aLeftNode.
   */
  nsresult JoinNodesWithTransaction(nsINode& aLeftNode, nsINode& aRightNode);

  /**
   * MoveNodeWithTransaction() moves aContent to aPointToInsert.
   *
   * @param aContent        The node to be moved.
   */
  template <typename PT, typename CT>
  nsresult MoveNodeWithTransaction(
      nsIContent& aContent, const EditorDOMPointBase<PT, CT>& aPointToInsert);

  /**
   * MoveNodeToEndWithTransaction() moves aContent to end of aNewContainer.
   *
   * @param aContent        The node to be moved.
   * @param aNewContainer   The new container which will contain aContent as
   *                        its last child.
   */
  nsresult MoveNodeToEndWithTransaction(nsIContent& aContent,
                                        nsINode& aNewContainer) {
    EditorRawDOMPoint pointToInsert;
    pointToInsert.SetToEndOf(&aNewContainer);
    return MoveNodeWithTransaction(aContent, pointToInsert);
  }

  /**
   * MoveAllChildren() moves all children of aContainer to before
   * aPointToInsert.GetChild().
   * See explanation of MoveChildren() for the detail of the behavior.
   *
   * @param aContainer          The container node whose all children should
   *                            be moved.
   * @param aPointToInsert      The insertion point.  The container must not
   *                            be a data node like a text node.
   * @param aError              The result.  If this succeeds to move children,
   *                            returns NS_OK.  Otherwise, an error.
   */
  void MoveAllChildren(nsINode& aContainer,
                       const EditorRawDOMPoint& aPointToInsert,
                       ErrorResult& aError);

  /**
   * MovePreviousSiblings() moves all siblings before aChild (i.e., aChild
   * won't be moved) to before aPointToInsert.GetChild().
   * See explanation of MoveChildren() for the detail of the behavior.
   *
   * @param aChild              The node which is next sibling of the last
   *                            node to be moved.
   * @param aPointToInsert      The insertion point.  The container must not
   *                            be a data node like a text node.
   * @param aError              The result.  If this succeeds to move children,
   *                            returns NS_OK.  Otherwise, an error.
   */
  void MovePreviousSiblings(nsIContent& aChild,
                            const EditorRawDOMPoint& aPointToInsert,
                            ErrorResult& aError);

  /**
   * MoveChildren() moves all children between aFirstChild and aLastChild to
   * before aPointToInsert.GetChild().
   * If some children are moved to different container while this method
   * moves other children, they are just ignored.
   * If the child node referred by aPointToInsert is moved to different
   * container while this method moves children, returns error.
   *
   * @param aFirstChild         The first child which should be moved to
   *                            aPointToInsert.
   * @param aLastChild          The last child which should be moved.  This
   *                            must be a sibling of aFirstChild and it should
   *                            be positioned after aFirstChild in the DOM tree
   *                            order.
   * @param aPointToInsert      The insertion point.  The container must not
   *                            be a data node like a text node.
   * @param aError              The result.  If this succeeds to move children,
   *                            returns NS_OK.  Otherwise, an error.
   */
  void MoveChildren(nsIContent& aFirstChild, nsIContent& aLastChild,
                    const EditorRawDOMPoint& aPointToInsert,
                    ErrorResult& aError);

  /**
   * CloneAttributeWithTransaction() copies aAttribute of aSourceElement to
   * aDestElement.  If aSourceElement doesn't have aAttribute, this removes
   * aAttribute from aDestElement.
   *
   * @param aAttribute          Attribute name to be cloned.
   * @param aDestElement        Element node which will be set aAttribute or
   *                            whose aAttribute will be removed.
   * @param aSourceElement      Element node which provides the value of
   *                            aAttribute in aDestElement.
   */
  nsresult CloneAttributeWithTransaction(nsAtom& aAttribute,
                                         Element& aDestElement,
                                         Element& aSourceElement);

  /**
   * RemoveAttributeWithTransaction() removes aAttribute from aElement.
   *
   * @param aElement        Element node which will lose aAttribute.
   * @param aAttribute      Attribute name to be removed from aElement.
   */
  nsresult RemoveAttributeWithTransaction(Element& aElement,
                                          nsAtom& aAttribute);

  virtual nsresult RemoveAttributeOrEquivalent(Element* aElement,
                                               nsAtom* aAttribute,
                                               bool aSuppressTransaction) = 0;

  /**
   * SetAttributeWithTransaction() sets aAttribute of aElement to aValue.
   *
   * @param aElement        Element node which will have aAttribute.
   * @param aAttribute      Attribute name to be set.
   * @param aValue          Attribute value be set to aAttribute.
   */
  nsresult SetAttributeWithTransaction(Element& aElement, nsAtom& aAttribute,
                                       const nsAString& aValue);

  virtual nsresult SetAttributeOrEquivalent(Element* aElement,
                                            nsAtom* aAttribute,
                                            const nsAString& aValue,
                                            bool aSuppressTransaction) = 0;

  /**
   * Method to replace certain CreateElementNS() calls.
   *
   * @param aTag        Tag you want.
   */
  already_AddRefed<Element> CreateHTMLContent(const nsAtom* aTag);

  /**
   * Creates text node which is marked as "maybe modified frequently".
   */
  static already_AddRefed<nsTextNode> CreateTextNode(nsIDocument& aDocument,
                                                     const nsAString& aData);

  /**
   * Create an element node whose name is aTag at before aPointToInsert.  When
   * this succeed to create an element node, this sets aPointToInsert to the
   * new element because the relation of child and offset may be broken.
   * If the caller needs to collapse the selection to next to the new element
   * node, it should call |aPointToInsert.AdvanceOffset()| after calling this.
   *
   * @param aTag            The element name to create.
   * @param aPointToInsert  The insertion point of new element.  If this refers
   *                        end of the container or after, the transaction
   *                        will append the element to the container.
   *                        Otherwise, will insert the element before the
   *                        child node referred by this.
   * @return                The created new element node.
   */
  template <typename PT, typename CT>
  already_AddRefed<Element> CreateNodeWithTransaction(
      nsAtom& aTag, const EditorDOMPointBase<PT, CT>& aPointToInsert);

  /**
   * Create an aggregate transaction for delete selection.  The result may
   * include DeleteNodeTransactions and/or DeleteTextTransactions as its
   * children.
   *
   * @param aAction             The action caused removing the selection.
   * @param aRemovingNode       The node to be removed.
   * @param aOffset             The start offset of the range in aRemovingNode.
   * @param aLength             The length of the range in aRemovingNode.
   * @return                    If it can remove the selection, returns an
   *                            aggregate transaction which has some
   *                            DeleteNodeTransactions and/or
   *                            DeleteTextTransactions as its children.
   */
  already_AddRefed<EditAggregateTransaction> CreateTxnForDeleteSelection(
      EDirection aAction, nsINode** aNode, int32_t* aOffset, int32_t* aLength);

  /**
   * Create a transaction for removing the nodes and/or text in aRange.
   *
   * @param aRangeToDelete      The range to be removed.
   * @param aAction             The action caused removing the range.
   * @param aRemovingNode       The node to be removed.
   * @param aOffset             The start offset of the range in aRemovingNode.
   * @param aLength             The length of the range in aRemovingNode.
   * @return                    The transaction to remove the range.  Its type
   *                            is DeleteNodeTransaction or
   *                            DeleteTextTransaction.
   */
  already_AddRefed<EditTransactionBase> CreateTxnForDeleteRange(
      nsRange* aRangeToDelete, EDirection aAction, nsINode** aRemovingNode,
      int32_t* aOffset, int32_t* aLength);

  /**
   * DeleteTextWithTransaction() removes text in the range from aCharData.
   *
   * @param aCharData           The data node which should be modified.
   * @param aOffset             Start offset of removing text in aCharData.
   * @param aLength             Length of removing text.
   */
  nsresult DeleteTextWithTransaction(dom::CharacterData& aCharacterData,
                                     uint32_t aOffset, uint32_t aLength);

  /**
   * ReplaceContainerWithTransactionInternal() is implementation of
   * ReplaceContainerWithTransaction() and
   * ReplaceContainerAndCloneAttributesWithTransaction().
   *
   * @param aOldContainer       The element which will be replaced with new
   *                            element.
   * @param aTagName            The name of new element node.
   * @param aAttribute          Attribute name which will be set to the new
   *                            element.  This will be ignored if
   *                            aCloneAllAttributes is set to true.
   * @param aAttributeValue     Attribute value which will be set to
   *                            aAttribute.
   * @param aCloneAllAttributes If true, all attributes of aOldContainer will
   *                            be copied to the new element.
   */
  already_AddRefed<Element> ReplaceContainerWithTransactionInternal(
      Element& aElement, nsAtom& aTagName, nsAtom& aAttribute,
      const nsAString& aAttributeValue, bool aCloneAllAttributes);

  /**
   * InsertContainerWithTransactionInternal() creates new element whose name is
   * aTagName, moves aContent into the new element, then, inserts the new
   * element into where aContent was.  If aAttribute is not nsGkAtoms::_empty,
   * aAttribute of the new element will be set to aAttributeValue.
   *
   * @param aContent            The content which will be wrapped with new
   *                            element.
   * @param aTagName            Element name of new element which will wrap
   *                            aContent and be inserted into where aContent
   *                            was.
   * @param aAttribute          Attribute which should be set to the new
   *                            element.  If this is nsGkAtoms::_empty,
   *                            this does not set any attributes to the new
   *                            element.
   * @param aAttributeValue     Value to be set to aAttribute.
   * @return                    The new element.
   */
  already_AddRefed<Element> InsertContainerWithTransactionInternal(
      nsIContent& aContent, nsAtom& aTagName, nsAtom& aAttribute,
      const nsAString& aAttributeValue);

  /**
   * DoSplitNode() creates a new node (left node) identical to an existing
   * node (right node), and split the contents between the same point in both
   * nodes.
   *
   * @param aStartOfRightNode   The point to split.  Its container will be
   *                            the right node, i.e., become the new node's
   *                            next sibling.  And the point will be start
   *                            of the right node.
   * @param aNewLeftNode        The new node called as left node, so, this
   *                            becomes the container of aPointToSplit's
   *                            previous sibling.
   * @param aError              Must have not already failed.
   *                            If succeed to insert aLeftNode before the
   *                            right node and remove unnecessary contents
   *                            (and collapse selection at end of the left
   *                            node if necessary), returns no error.
   *                            Otherwise, an error.
   */
  void DoSplitNode(const EditorDOMPoint& aStartOfRightNode,
                   nsIContent& aNewLeftNode, ErrorResult& aError);

  /**
   * DoJoinNodes() merges contents in aNodeToJoin to aNodeToKeep and remove
   * aNodeToJoin from the DOM tree.  aNodeToJoin and aNodeToKeep must have
   * same parent, aParent.  Additionally, if one of aNodeToJoin or aNodeToKeep
   * is a text node, the other must be a text node.
   *
   * @param aNodeToKeep   The node that will remain after the join.
   * @param aNodeToJoin   The node that will be joined with aNodeToKeep.
   *                      There is no requirement that the two nodes be of the
   *                      same type.
   * @param aParent       The parent of aNodeToKeep
   */
  nsresult DoJoinNodes(nsINode* aNodeToKeep, nsINode* aNodeToJoin,
                       nsINode* aParent);

  /**
   * SplitNodeDeepWithTransaction() splits aMostAncestorToSplit deeply.
   *
   * @param aMostAncestorToSplit        The most ancestor node which should be
   *                                    split.
   * @param aStartOfDeepestRightNode    The start point of deepest right node.
   *                                    This point must be descendant of
   *                                    aMostAncestorToSplit.
   * @param aSplitAtEdges               Whether the caller allows this to
   *                                    create empty container element when
   *                                    split point is start or end of an
   *                                    element.
   * @return                            SplitPoint() returns split point in
   *                                    aMostAncestorToSplit.  The point must
   *                                    be good to insert something if the
   *                                    caller want to do it.
   */
  template <typename PT, typename CT>
  SplitNodeResult SplitNodeDeepWithTransaction(
      nsIContent& aMostAncestorToSplit,
      const EditorDOMPointBase<PT, CT>& aDeepestStartOfRightNode,
      SplitAtEdges aSplitAtEdges);

  /**
   * JoinNodesDeepWithTransaction() joins aLeftNode and aRightNode "deeply".
   * First, they are joined simply, then, new right node is assumed as the
   * child at length of the left node before joined and new left node is
   * assumed as its previous sibling.  Then, they will be joined again.
   * And then, these steps are repeated.
   *
   * @param aLeftNode   The node which will be removed form the tree.
   * @param aRightNode  The node which will be inserted the contents of
   *                    aLeftNode.
   * @return            The point of the first child of the last right node.
   */
  EditorDOMPoint JoinNodesDeepWithTransaction(nsIContent& aLeftNode,
                                              nsIContent& aRightNode);

  nsresult DoTransactionInternal(nsITransaction* aTxn);

  virtual bool IsBlockNode(nsINode* aNode);

  /**
   * Set outOffset to the offset of aChild in the parent.
   * Returns the parent of aChild.
   */
  static nsINode* GetNodeLocation(nsINode* aChild, int32_t* aOffset);

  /**
   * Get the previous node.
   */
  nsIContent* GetPreviousNode(const EditorRawDOMPoint& aPoint) {
    return GetPreviousNodeInternal(aPoint, false, true, false);
  }
  nsIContent* GetPreviousElementOrText(const EditorRawDOMPoint& aPoint) {
    return GetPreviousNodeInternal(aPoint, false, false, false);
  }
  nsIContent* GetPreviousEditableNode(const EditorRawDOMPoint& aPoint) {
    return GetPreviousNodeInternal(aPoint, true, true, false);
  }
  nsIContent* GetPreviousNodeInBlock(const EditorRawDOMPoint& aPoint) {
    return GetPreviousNodeInternal(aPoint, false, true, true);
  }
  nsIContent* GetPreviousElementOrTextInBlock(const EditorRawDOMPoint& aPoint) {
    return GetPreviousNodeInternal(aPoint, false, false, true);
  }
  nsIContent* GetPreviousEditableNodeInBlock(const EditorRawDOMPoint& aPoint) {
    return GetPreviousNodeInternal(aPoint, true, true, true);
  }
  nsIContent* GetPreviousNode(nsINode& aNode) {
    return GetPreviousNodeInternal(aNode, false, true, false);
  }
  nsIContent* GetPreviousElementOrText(nsINode& aNode) {
    return GetPreviousNodeInternal(aNode, false, false, false);
  }
  nsIContent* GetPreviousEditableNode(nsINode& aNode) {
    return GetPreviousNodeInternal(aNode, true, true, false);
  }
  nsIContent* GetPreviousNodeInBlock(nsINode& aNode) {
    return GetPreviousNodeInternal(aNode, false, true, true);
  }
  nsIContent* GetPreviousElementOrTextInBlock(nsINode& aNode) {
    return GetPreviousNodeInternal(aNode, false, false, true);
  }
  nsIContent* GetPreviousEditableNodeInBlock(nsINode& aNode) {
    return GetPreviousNodeInternal(aNode, true, true, true);
  }

  /**
   * Get the next node.
   *
   * Note that methods taking EditorRawDOMPoint behavior includes the
   * child at offset as search target.  E.g., following code causes infinite
   * loop.
   *
   * EditorRawDOMPoint point(aEditableNode);
   * while (nsIContent* content = GetNextEditableNode(point)) {
   *   // Do something...
   *   point.Set(content);
   * }
   *
   * Following code must be you expected:
   *
   * while (nsIContent* content = GetNextEditableNode(point)) {
   *   // Do something...
   *   DebugOnly<bool> advanced = point.Advanced();
   *   MOZ_ASSERT(advanced);
   *   point.Set(point.GetChild());
   * }
   *
   * On the other hand, the methods taking nsINode behavior must be what
   * you want.  They start to search the result from next node of the given
   * node.
   */
  template <typename PT, typename CT>
  nsIContent* GetNextNode(const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextNodeInternal(aPoint, false, true, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextElementOrText(const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextNodeInternal(aPoint, false, false, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextEditableNode(const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextNodeInternal(aPoint, true, true, false);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextNodeInBlock(const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextNodeInternal(aPoint, false, true, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextElementOrTextInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextNodeInternal(aPoint, false, false, true);
  }
  template <typename PT, typename CT>
  nsIContent* GetNextEditableNodeInBlock(
      const EditorDOMPointBase<PT, CT>& aPoint) {
    return GetNextNodeInternal(aPoint, true, true, true);
  }
  nsIContent* GetNextNode(nsINode& aNode) {
    return GetNextNodeInternal(aNode, false, true, false);
  }
  nsIContent* GetNextElementOrText(nsINode& aNode) {
    return GetNextNodeInternal(aNode, false, false, false);
  }
  nsIContent* GetNextEditableNode(nsINode& aNode) {
    return GetNextNodeInternal(aNode, true, true, false);
  }
  nsIContent* GetNextNodeInBlock(nsINode& aNode) {
    return GetNextNodeInternal(aNode, false, true, true);
  }
  nsIContent* GetNextElementOrTextInBlock(nsINode& aNode) {
    return GetNextNodeInternal(aNode, false, false, true);
  }
  nsIContent* GetNextEditableNodeInBlock(nsINode& aNode) {
    return GetNextNodeInternal(aNode, true, true, true);
  }

  /**
   * Get the rightmost child of aCurrentNode;
   * return nullptr if aCurrentNode has no children.
   */
  nsIContent* GetRightmostChild(nsINode* aCurrentNode,
                                bool bNoBlockCrossing = false);

  /**
   * Get the leftmost child of aCurrentNode;
   * return nullptr if aCurrentNode has no children.
   */
  nsIContent* GetLeftmostChild(nsINode* aCurrentNode,
                               bool bNoBlockCrossing = false);

  /**
   * Returns true if aParent can contain a child of type aTag.
   */
  bool CanContain(nsINode& aParent, nsIContent& aChild) const;
  bool CanContainTag(nsINode& aParent, nsAtom& aTag) const;
  bool TagCanContain(nsAtom& aParentTag, nsIContent& aChild) const;
  virtual bool TagCanContainTag(nsAtom& aParentTag, nsAtom& aChildTag) const;

  /**
   * Returns true if aNode is our root node.
   */
  bool IsRoot(nsINode* inNode) const;
  bool IsEditorRoot(nsINode* aNode) const;

  /**
   * Returns true if aNode is a descendant of our root node.
   */
  bool IsDescendantOfRoot(nsINode* inNode) const;
  bool IsDescendantOfEditorRoot(nsINode* aNode) const;

  /**
   * Returns true if aNode is a container.
   */
  virtual bool IsContainer(nsINode* aNode);

  /**
   * returns true if aNode is an editable node.
   */
  bool IsEditable(nsINode* aNode) {
    if (NS_WARN_IF(!aNode)) {
      return false;
    }

    if (!aNode->IsContent() || IsMozEditorBogusNode(aNode) ||
        !IsModifiableNode(*aNode)) {
      return false;
    }

    switch (aNode->NodeType()) {
      case nsINode::ELEMENT_NODE:
        // In HTML editors, if we're dealing with an element, then ask it
        // whether it's editable.
        return mIsHTMLEditorClass ? aNode->IsEditable() : true;
      case nsINode::TEXT_NODE:
        // Text nodes are considered to be editable by both typed of editors.
        return true;
      default:
        return false;
    }
  }

  /**
   * Returns true if aNode is a usual element node (not bogus node) or
   * a text node.  In other words, returns true if aNode is a usual element
   * node or visible data node.
   */
  bool IsElementOrText(const nsINode& aNode) const {
    if (!aNode.IsContent() || IsMozEditorBogusNode(&aNode)) {
      return false;
    }
    return aNode.NodeType() == nsINode::ELEMENT_NODE ||
           aNode.NodeType() == nsINode::TEXT_NODE;
  }

  /**
   * Returns true if aNode is a MozEditorBogus node.
   */
  bool IsMozEditorBogusNode(const nsINode* aNode) const {
    return aNode && aNode->IsElement() &&
           aNode->AsElement()->AttrValueIs(
               kNameSpaceID_None, kMOZEditorBogusNodeAttrAtom,
               kMOZEditorBogusNodeValue, eCaseMatters);
  }

  /**
   * Counts number of editable child nodes.
   */
  uint32_t CountEditableChildren(nsINode* aNode);

  /**
   * Find the deep first and last children.
   */
  nsINode* GetFirstEditableNode(nsINode* aRoot);

  /**
   * Returns true when inserting text should be a part of current composition.
   */
  bool ShouldHandleIMEComposition() const;

  /**
   * AreNodesSameType() returns true if aNode1 and aNode2 are same type.
   * If the instance is TextEditor, only their names are checked.
   * If the instance is HTMLEditor in CSS mode and both of them are <span>
   * element, their styles are also checked.
   */
  bool AreNodesSameType(nsIContent& aNode1, nsIContent& aNode2) const;

  static bool IsTextNode(nsINode* aNode) {
    return aNode->NodeType() == nsINode::TEXT_NODE;
  }

  /**
   * IsModifiableNode() checks whether the node is editable or not.
   */
  bool IsModifiableNode(const nsINode& aNode) const;

  /**
   * GetNodeAtRangeOffsetPoint() returns the node at this position in a range,
   * assuming that the container is the node itself if it's a text node, or
   * the node's parent otherwise.
   */
  static nsIContent* GetNodeAtRangeOffsetPoint(nsINode* aContainer,
                                               int32_t aOffset) {
    return GetNodeAtRangeOffsetPoint(RawRangeBoundary(aContainer, aOffset));
  }
  static nsIContent* GetNodeAtRangeOffsetPoint(const RawRangeBoundary& aPoint);

  static EditorRawDOMPoint GetStartPoint(const Selection& aSelection);
  static EditorRawDOMPoint GetEndPoint(const Selection& aSelection);

  static nsresult GetEndChildNode(const Selection& aSelection,
                                  nsIContent** aEndNode);

  /**
   * CollapseSelectionToEnd() collapses the selection to the end of the editor.
   */
  nsresult CollapseSelectionToEnd();

  /**
   * Helpers to add a node to the selection.
   * Used by table cell selection methods.
   */
  nsresult CreateRange(nsINode* aStartContainer, int32_t aStartOffset,
                       nsINode* aEndContainer, int32_t aEndOffset,
                       nsRange** aRange);

  static bool IsPreformatted(nsINode* aNode);

  /**
   * AllowsTransactionsToChangeSelection() returns true if editor allows any
   * transactions to change Selection.  Otherwise, transactions shouldn't
   * change Selection.
   */
  inline bool AllowsTransactionsToChangeSelection() const {
    return mAllowsTransactionsToChangeSelection;
  }

  /**
   * MakeThisAllowTransactionsToChangeSelection() with true makes this editor
   * allow transactions to change Selection.  Otherwise, i.e., with false,
   * makes this editor not allow transactions to change Selection.
   */
  inline void MakeThisAllowTransactionsToChangeSelection(bool aAllow) {
    mAllowsTransactionsToChangeSelection = aAllow;
  }

  nsresult HandleInlineSpellCheck(EditSubAction aEditSubAction,
                                  nsINode* previousSelectedNode,
                                  uint32_t previousSelectedOffset,
                                  nsINode* aStartContainer,
                                  uint32_t aStartOffset, nsINode* aEndContainer,
                                  uint32_t aEndOffset);

  /**
   * Likewise, but gets the editor's root instead, which is different for HTML
   * editors.
   */
  virtual Element* GetEditorRoot() const;

  /**
   * Likewise, but gets the text control element instead of the root for
   * plaintext editors.
   */
  Element* GetExposedRoot() const;

  /**
   * Whether the editor is active on the DOM window.  Note that when this
   * returns true but GetFocusedContent() returns null, it means that this
   * editor was focused when the DOM window was active.
   */
  virtual bool IsActiveInDOMWindow();

  /**
   * GetIMESelectionStartOffsetIn() returns the start offset of IME selection in
   * the aTextNode.  If there is no IME selection, returns -1.
   */
  int32_t GetIMESelectionStartOffsetIn(nsINode* aTextNode);

  /**
   * FindBetterInsertionPoint() tries to look for better insertion point which
   * is typically the nearest text node and offset in it.
   *
   * @param aPoint      Insertion point which the callers found.
   * @return            Better insertion point if there is.  If not returns
   *                    same point as aPoint.
   */
  EditorRawDOMPoint FindBetterInsertionPoint(const EditorRawDOMPoint& aPoint);

  /**
   * HideCaret() hides caret with nsCaret::AddForceHide() or may show carent
   * with nsCaret::RemoveForceHide().  This does NOT set visibility of
   * nsCaret.  Therefore, this is stateless.
   */
  void HideCaret(bool aHide);

 protected:  // Called by helper classes.
  /**
   * OnStartToHandleTopLevelEditSubAction() is called when
   * GetTopLevelEditSubAction() is EditSubAction::eNone and somebody starts to
   * handle aEditSubAction.
   *
   * @param aEditSubAction      Top level edit sub action which will be
   *                            handled soon.
   * @param aDirection          Direction of aEditSubAction.
   */
  virtual void OnStartToHandleTopLevelEditSubAction(
      EditSubAction aEditSubAction, nsIEditor::EDirection aDirection);

  /**
   * OnEndHandlingTopLevelEditSubAction() is called after
   * SetTopLevelEditSubAction() is handled.
   */
  virtual void OnEndHandlingTopLevelEditSubAction();

  /**
   * Routines for managing the preservation of selection across
   * various editor actions.
   */
  bool ArePreservingSelection();
  void PreserveSelectionAcrossActions();
  nsresult RestorePreservedSelection();
  void StopPreservingSelection();

  /**
   * (Begin|End)PlaceholderTransaction() are called by AutoPlaceholderBatch.
   * This set of methods are similar to the (Begin|End)Transaction(), but do
   * not use the transaction managers batching feature.  Instead we use a
   * placeholder transaction to wrap up any further transaction while the
   * batch is open.  The advantage of this is that placeholder transactions
   * can later merge, if needed.  Merging is unavailable between transaction
   * manager batches.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void BeginPlaceholderTransaction(nsAtom* aTransactionName);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void EndPlaceholderTransaction();

  void BeginUpdateViewBatch();
  void EndUpdateViewBatch();

  /**
   * Used by AutoTransactionBatch.  After calling BeginTransactionInternal(),
   * all transactions will be treated as an atomic transaction.  I.e.,
   * two or more transactions are undid once.
   * XXX What's the difference with PlaceholderTransaction? Should we always
   *     use it instead?
   */
  void BeginTransactionInternal();
  void EndTransactionInternal();

 protected:  // Shouldn't be used by friend classes
  /**
   * The default destructor. This should suffice. Should this be pure virtual
   * for someone to derive from the EditorBase later? I don't believe so.
   */
  virtual ~EditorBase();

  /**
   * GetDocumentCharsetInternal() returns charset of the document.
   */
  nsresult GetDocumentCharsetInternal(nsACString& aCharset) const;

  /**
   * SelectAllInternal() should be used instead of SelectAll() in editor
   * because SelectAll() creates AutoEditActionSetter but we should avoid
   * to create it as far as possible.
   */
  virtual nsresult SelectAllInternal();

  nsresult DetermineCurrentDirection();

  MOZ_CAN_RUN_SCRIPT
  void FireInputEvent();

  /**
   * Called after a transaction is done successfully.
   */
  void DoAfterDoTransaction(nsITransaction* aTxn);

  /**
   * Called after a transaction is undone successfully.
   */

  void DoAfterUndoTransaction();

  /**
   * Called after a transaction is redone successfully.
   */
  void DoAfterRedoTransaction();

  /**
   * Tell the doc state listeners that the doc state has changed.
   */
  enum TDocumentListenerNotification {
    eDocumentCreated,
    eDocumentToBeDestroyed,
    eDocumentStateChanged
  };
  nsresult NotifyDocumentListeners(
      TDocumentListenerNotification aNotificationType);

  /**
   * Make the given selection span the entire document.
   */
  virtual nsresult SelectEntireDocument();

  /**
   * Helper method for scrolling the selection into view after
   * an edit operation. aScrollToAnchor should be true if you
   * want to scroll to the point where the selection was started.
   * If false, it attempts to scroll the end of the selection into view.
   *
   * Editor methods *should* call this method instead of the versions
   * in the various selection interfaces, since this version makes sure
   * that the editor's sync/async settings for reflowing, painting, and
   * scrolling match.
   */
  nsresult ScrollSelectionIntoView(bool aScrollToAnchor);

  /**
   * Helper for GetPreviousNodeInternal() and GetNextNodeInternal().
   */
  nsIContent* FindNextLeafNode(nsINode* aCurrentNode, bool aGoForward,
                               bool bNoBlockCrossing);
  nsIContent* FindNode(nsINode* aCurrentNode, bool aGoForward,
                       bool aEditableNode, bool aFindAnyDataNode,
                       bool bNoBlockCrossing);

  /**
   * Get the node immediately previous node of aNode.
   * @param atNode               The node from which we start the search.
   * @param aFindEditableNode    If true, only return an editable node.
   * @param aFindAnyDataNode     If true, may return invisible data node
   *                             like Comment.
   * @param aNoBlockCrossing     If true, don't move across "block" nodes,
   *                             whatever that means.
   * @return                     The node that occurs before aNode in
   *                             the tree, skipping non-editable nodes if
   *                             aFindEditableNode is true.  If there is no
   *                             previous node, returns nullptr.
   */
  nsIContent* GetPreviousNodeInternal(nsINode& aNode, bool aFindEditableNode,
                                      bool aFindAnyDataNode,
                                      bool aNoBlockCrossing);

  /**
   * And another version that takes a point in DOM tree rather than a node.
   */
  nsIContent* GetPreviousNodeInternal(const EditorRawDOMPoint& aPoint,
                                      bool aFindEditableNode,
                                      bool aFindAnyDataNode,
                                      bool aNoBlockCrossing);

  /**
   * Get the node immediately next node of aNode.
   * @param aNode                The node from which we start the search.
   * @param aFindEditableNode    If true, only return an editable node.
   * @param aFindAnyDataNode     If true, may return invisible data node
   *                             like Comment.
   * @param aNoBlockCrossing     If true, don't move across "block" nodes,
   *                             whatever that means.
   * @return                     The node that occurs after aNode in the
   *                             tree, skipping non-editable nodes if
   *                             aFindEditableNode is true.  If there is no
   *                             next node, returns nullptr.
   */
  nsIContent* GetNextNodeInternal(nsINode& aNode, bool aFindEditableNode,
                                  bool aFindAnyDataNode, bool bNoBlockCrossing);

  /**
   * And another version that takes a point in DOM tree rather than a node.
   */
  nsIContent* GetNextNodeInternal(const EditorRawDOMPoint& aPoint,
                                  bool aFindEditableNode, bool aFindAnyDataNode,
                                  bool aNoBlockCrossing);

  virtual nsresult InstallEventListeners();
  virtual void CreateEventListeners();
  virtual void RemoveEventListeners();

  /**
   * Get the input event target. This might return null.
   */
  virtual already_AddRefed<Element> GetInputEventTargetElement() = 0;

  /**
   * Return true if spellchecking should be enabled for this editor.
   */
  bool GetDesiredSpellCheckState();

  bool CanEnableSpellCheck() {
    // Check for password/readonly/disabled, which are not spellchecked
    // regardless of DOM. Also, check to see if spell check should be skipped
    // or not.
    return !IsPasswordEditor() && !IsReadonly() && !IsDisabled() &&
           !ShouldSkipSpellCheck();
  }

  /**
   * InitializeSelectionAncestorLimit() is called by InitializeSelection().
   * When this is called, each implementation has to call
   * Selection::SetAncestorLimiter() with aAnotherLimit.
   *
   * @param aAncestorLimit      New ancestor limit of Selection.  This always
   *                            has parent node.  So, it's always safe to
   *                            call SetAncestorLimit() with this node.
   */
  virtual void InitializeSelectionAncestorLimit(nsIContent& aAncestorLimit);

  /**
   * Return the offset of aChild in aParent.  Asserts fatally if parent or
   * child is null, or parent is not child's parent.
   * FYI: aChild must not be being removed from aParent.  In such case, these
   *      methods may return wrong index if aChild doesn't have previous
   *      sibling or next sibling.
   */
  static int32_t GetChildOffset(nsINode* aChild, nsINode* aParent);

  /**
   * Creates a range with just the supplied node and appends that to the
   * selection.
   */
  nsresult AppendNodeToSelectionAsRange(nsINode* aNode);

  /**
   * When you are using AppendNodeToSelectionAsRange(), call this first to
   * start a new selection.
   */
  nsresult ClearSelection();

  /**
   * Initializes selection and caret for the editor.  If aEventTarget isn't
   * a host of the editor, i.e., the editor doesn't get focus, this does
   * nothing.
   */
  nsresult InitializeSelection(dom::EventTarget* aFocusEventTarget);

  enum NotificationForEditorObservers {
    eNotifyEditorObserversOfEnd,
    eNotifyEditorObserversOfBefore,
    eNotifyEditorObserversOfCancel
  };
  MOZ_CAN_RUN_SCRIPT
  void NotifyEditorObservers(NotificationForEditorObservers aNotification);

 private:
  nsCOMPtr<nsISelectionController> mSelectionController;
  nsCOMPtr<nsIDocument> mDocument;

  AutoEditActionDataSetter* mEditActionData;

  /**
   * SetTextDirectionTo() sets text-direction of the root element.
   * Should use SwitchTextDirectionTo() or ToggleTextDirection() instead.
   * This is a helper class of them.
   */
  nsresult SetTextDirectionTo(TextDirection aTextDirection);

 protected:  // helper classes which may be used by friends
  /**
   * Stack based helper class for calling EditorBase::EndTransactionInternal().
   */
  class MOZ_RAII AutoTransactionBatch final {
   public:
    explicit AutoTransactionBatch(
        EditorBase& aEditorBase MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mEditorBase(aEditorBase) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      mEditorBase->BeginTransactionInternal();
    }

    ~AutoTransactionBatch() { mEditorBase->EndTransactionInternal(); }

   protected:
    OwningNonNull<EditorBase> mEditorBase;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /**
   * Stack based helper class for batching a collection of transactions inside
   * a placeholder transaction.
   */
  class MOZ_RAII AutoPlaceholderBatch final {
   public:
    explicit AutoPlaceholderBatch(
        EditorBase& aEditorBase MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mEditorBase(aEditorBase) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      mEditorBase->BeginPlaceholderTransaction(nullptr);
    }

    AutoPlaceholderBatch(EditorBase& aEditorBase,
                         nsAtom& aTransactionName
                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mEditorBase(aEditorBase) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      mEditorBase->BeginPlaceholderTransaction(&aTransactionName);
    }

    ~AutoPlaceholderBatch() { mEditorBase->EndPlaceholderTransaction(); }

   protected:
    OwningNonNull<EditorBase> mEditorBase;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /**
   * Stack based helper class for saving/restoring selection.  Note that this
   * assumes that the nodes involved are still around afterwords!
   */
  class MOZ_RAII AutoSelectionRestorer final {
   public:
    /**
     * Constructor responsible for remembering all state needed to restore
     * aSelection.
     */
    explicit AutoSelectionRestorer(
        EditorBase& aEditorBase MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    /**
     * Destructor restores mSelection to its former state
     */
    ~AutoSelectionRestorer();

    /**
     * Abort() cancels to restore the selection.
     */
    void Abort();

   protected:
    EditorBase* mEditorBase;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /**
   * AutoTopLevelEditSubActionNotifier notifies editor of start to handle
   * top level edit sub-action and end handling top level edit sub-action.
   */
  class MOZ_RAII AutoTopLevelEditSubActionNotifier final {
   public:
    AutoTopLevelEditSubActionNotifier(
        EditorBase& aEditorBase, EditSubAction aEditSubAction,
        nsIEditor::EDirection aDirection MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mEditorBase(aEditorBase), mDoNothing(false) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      // The top level edit sub action has already be set if this is nested call
      // XXX Looks like that this is not aware of unexpected nested edit action
      //     handling via selectionchange event listener or mutation event
      //     listener.
      if (!mEditorBase.GetTopLevelEditSubAction()) {
        mEditorBase.OnStartToHandleTopLevelEditSubAction(aEditSubAction,
                                                         aDirection);
      } else {
        mDoNothing = true;  // nested calls will end up here
      }
    }

    ~AutoTopLevelEditSubActionNotifier() {
      if (!mDoNothing) {
        mEditorBase.OnEndHandlingTopLevelEditSubAction();
      }
    }

   protected:
    EditorBase& mEditorBase;
    bool mDoNothing;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /**
   * Stack based helper class for turning off active selection adjustment
   * by low level transactions
   */
  class MOZ_RAII AutoTransactionsConserveSelection final {
   public:
    explicit AutoTransactionsConserveSelection(
        EditorBase& aEditorBase MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mEditorBase(aEditorBase),
          mAllowedTransactionsToChangeSelection(
              aEditorBase.AllowsTransactionsToChangeSelection()) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      mEditorBase.MakeThisAllowTransactionsToChangeSelection(false);
    }

    ~AutoTransactionsConserveSelection() {
      mEditorBase.MakeThisAllowTransactionsToChangeSelection(
          mAllowedTransactionsToChangeSelection);
    }

   protected:
    EditorBase& mEditorBase;
    bool mAllowedTransactionsToChangeSelection;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  /***************************************************************************
   * stack based helper class for batching reflow and paint requests.
   */
  class MOZ_RAII AutoUpdateViewBatch final {
   public:
    explicit AutoUpdateViewBatch(
        EditorBase& aEditorBase MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : mEditorBase(aEditorBase) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      mEditorBase.BeginUpdateViewBatch();
    }

    ~AutoUpdateViewBatch() { mEditorBase.EndUpdateViewBatch(); }

   protected:
    EditorBase& mEditorBase;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

 protected:
  enum Tristate { eTriUnset, eTriFalse, eTriTrue };

  // MIME type of the doc we are editing.
  nsCString mContentMIMEType;

  RefPtr<mozInlineSpellChecker> mInlineSpellChecker;
  // Reference to text services document for mInlineSpellChecker.
  RefPtr<TextServicesDocument> mTextServicesDocument;

  RefPtr<TransactionManager> mTransactionManager;
  // Cached root node.
  RefPtr<Element> mRootElement;
  // The form field as an event receiver.
  nsCOMPtr<dom::EventTarget> mEventTarget;
  RefPtr<EditorEventListener> mEventListener;
  // Strong reference to placeholder for begin/end batch purposes.
  RefPtr<PlaceholderTransaction> mPlaceholderTransaction;
  // Name of placeholder transaction.
  nsAtom* mPlaceholderName;
  // Saved selection state for placeholder transaction batching.
  mozilla::Maybe<SelectionState> mSelState;
  // IME composition this is not null between compositionstart and
  // compositionend.
  RefPtr<TextComposition> mComposition;

  RefPtr<TextEditRules> mRules;

  RefPtr<TextInputListener> mTextInputListener;

  RefPtr<IMEContentObserver> mIMEContentObserver;

  // Listens to all low level actions on the doc.
  // Edit action listener is currently used by highlighter of the findbar and
  // the spellchecker.  So, we should reserve only 2 items.
  typedef AutoTArray<OwningNonNull<nsIEditActionListener>, 2>
      AutoActionListenerArray;
  AutoActionListenerArray mActionListeners;
  // Just notify once per high level change.
  // Editor observer is used only by legacy addons for Thunderbird and
  // BlueGriffon.  So, we don't need to reserve the space for them.
  typedef AutoTArray<OwningNonNull<nsIEditorObserver>, 0>
      AutoEditorObserverArray;
  AutoEditorObserverArray mEditorObservers;
  // Listen to overall doc state (dirty or not, just created, etc.).
  // Document state listener is currently used by EditingSession and
  // BlueGriffon so that reserving only one is enough (although, this is not
  // necessary for TextEditor).
  typedef AutoTArray<OwningNonNull<nsIDocumentStateListener>, 1>
      AutoDocumentStateListenerArray;
  AutoDocumentStateListenerArray mDocStateListeners;

  // Number of modifications (for undo/redo stack).
  uint32_t mModCount;
  // Behavior flags. See nsIPlaintextEditor.idl for the flags we use.
  uint32_t mFlags;

  int32_t mUpdateCount;

  // Nesting count for batching.
  int32_t mPlaceholderBatch;

  // -1 = not initialized
  int8_t mDocDirtyState;
  // A Tristate value.
  uint8_t mSpellcheckCheckboxState;

  // If false, transactions should not change Selection even after modifying
  // the DOM tree.
  bool mAllowsTransactionsToChangeSelection;
  // Whether PreDestroy has been called.
  bool mDidPreDestroy;
  // Whether PostCreate has been called.
  bool mDidPostCreate;
  bool mDispatchInputEvent;
  // True while the instance is handling an edit sub-action.
  bool mIsInEditSubAction;
  // Whether caret is hidden forcibly.
  bool mHidingCaret;
  // Whether spellchecker dictionary is initialized after focused.
  bool mSpellCheckerDictionaryUpdated;
  // Whether we are an HTML editor class.
  bool mIsHTMLEditorClass;

  friend class CompositionTransaction;
  friend class CreateElementTransaction;
  friend class CSSEditUtils;
  friend class DeleteNodeTransaction;
  friend class DeleteRangeTransaction;
  friend class DeleteTextTransaction;
  friend class HTMLEditRules;
  friend class HTMLEditUtils;
  friend class InsertNodeTransaction;
  friend class InsertTextTransaction;
  friend class JoinNodeTransaction;
  friend class SplitNodeTransaction;
  friend class TextEditRules;
  friend class TypeInState;
  friend class WSRunObject;
  friend class nsIEditor;
};

}  // namespace mozilla

mozilla::EditorBase* nsIEditor::AsEditorBase() {
  return static_cast<mozilla::EditorBase*>(this);
}

const mozilla::EditorBase* nsIEditor::AsEditorBase() const {
  return static_cast<const mozilla::EditorBase*>(this);
}

#endif  // #ifndef mozilla_EditorBase_h
