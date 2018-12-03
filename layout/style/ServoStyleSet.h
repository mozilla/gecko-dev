/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleSet_h
#define mozilla_ServoStyleSet_h

#include "mozilla/AtomArray.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/EventStates.h"
#include "mozilla/MediaFeatureChange.h"
#include "mozilla/PostTraversalTask.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/SheetType.h"
#include "mozilla/UniquePtr.h"
#include "MainThreadUtils.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsChangeHint.h"
#include "nsCoord.h"
#include "nsAtom.h"
#include "nsIMemoryReporter.h"
#include "nsTArray.h"

namespace mozilla {
namespace css {
class Rule;
}  // namespace css
namespace dom {
class Element;
class ShadowRoot;
}  // namespace dom
class StyleSheet;
struct Keyframe;
class ServoElementSnapshotTable;
class ComputedStyle;
class ServoStyleRuleMap;
class StyleSheet;
}  // namespace mozilla
class gfxFontFeatureValueSet;
class nsIContent;
class nsIDocument;
class nsPresContext;
struct nsTimingFunction;
struct RawServoRuleNode;
struct TreeMatchContext;

namespace mozilla {

// A few flags used to track which kind of stylist state we may need to
// update.
enum class StylistState : uint8_t {
  // The stylist is not dirty, we should do nothing.
  NotDirty = 0,

  // The style sheets have changed, so we need to update the style data.
  StyleSheetsDirty = 1 << 0,

  // Some of the style sheets of the bound elements in binding manager have
  // changed, so we need to tell the binding manager to update style data.
  XBLStyleSheetsDirty = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(StylistState)

// Bitfield type to represent Servo stylesheet origins.
enum class OriginFlags : uint8_t {
  UserAgent = 0x01,
  User = 0x02,
  Author = 0x04,
  All = 0x07,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(OriginFlags)

/**
 * The set of style sheets that apply to a document, backed by a Servo
 * Stylist.  A ServoStyleSet contains StyleSheets.
 */
class ServoStyleSet {
  friend class RestyleManager;
  typedef ServoElementSnapshotTable SnapshotTable;

 public:
  static bool IsInServoTraversal() { return mozilla::IsInServoTraversal(); }

#ifdef DEBUG
  // Used for debug assertions. We make this debug-only to prevent callers from
  // accidentally using it instead of IsInServoTraversal, which is cheaper. We
  // can change this if a use-case arises.
  static bool IsCurrentThreadInServoTraversal();
#endif

  static ServoStyleSet* Current() { return sInServoTraversal; }

  ServoStyleSet();
  ~ServoStyleSet();

  void Init(nsPresContext* aPresContext);
  void BeginShutdown() {}
  void Shutdown();

  // Called when a rules in a stylesheet in this set, or a child sheet of that,
  // are mutated from CSSOM.
  void RuleAdded(StyleSheet&, css::Rule&);
  void RuleRemoved(StyleSheet&, css::Rule&);
  void RuleChanged(StyleSheet& aSheet, css::Rule* aRule);

  // Runs style invalidation due to document state changes.
  void InvalidateStyleForDocumentStateChanges(EventStates aStatesChanged);

  void RecordShadowStyleChange(dom::ShadowRoot&);

  bool StyleSheetsHaveChanged() const { return StylistNeedsUpdate(); }

  nsRestyleHint MediumFeaturesChanged(MediaFeatureChangeReason);

  // Evaluates a given SourceSizeList, returning the optimal viewport width in
  // app units.
  //
  // The SourceSizeList parameter can be null, in which case it will return
  // 100vw.
  inline nscoord EvaluateSourceSizeList(
      const RawServoSourceSizeList* aSourceSizeList) const;

  void AddSizeOfIncludingThis(nsWindowSizes& aSizes) const;
  const RawServoStyleSet* RawSet() const { return mRawSet.get(); }

  bool GetAuthorStyleDisabled() const { return mAuthorStyleDisabled; }

  void SetAuthorStyleDisabled(bool aStyleDisabled);

  // FIXME(emilio): All the callers pass Allow here
  already_AddRefed<ComputedStyle> ResolveStyleFor(
      dom::Element* aElement, LazyComputeBehavior aMayCompute);

  // Get a CopmutedStyle for a text node (which no rules will match).
  //
  // The returned ComputedStyle will have nsCSSAnonBoxes::mozText() as its
  // pseudo.
  //
  // (Perhaps mozText should go away and we shouldn't even create style
  // contexts for such content nodes, when text-combine-upright is not
  // present.  However, not doing any rule matching for them is a first step.)
  already_AddRefed<ComputedStyle> ResolveStyleForText(
      nsIContent* aTextNode, ComputedStyle* aParentContext);

  // Get a ComputedStyle for a first-letter continuation (which no rules will
  // match).
  //
  // The returned ComputedStyle will have
  // nsCSSAnonBoxes::firstLetterContinuation() as its pseudo.
  //
  // (Perhaps nsCSSAnonBoxes::firstLetterContinuation() should go away and we
  // shouldn't even create ComputedStyles for such frames.  However, not doing
  // any rule matching for them is a first step.  And right now we do use this
  // ComputedStyle for some things)
  already_AddRefed<ComputedStyle> ResolveStyleForFirstLetterContinuation(
      ComputedStyle* aParentContext);

  // Get a ComputedStyle for a placeholder frame (which no rules will match).
  //
  // The returned ComputedStyle will have nsCSSAnonBoxes::oofPlaceholder() as
  // its pseudo.
  //
  // (Perhaps nsCSSAnonBoxes::oofPaceholder() should go away and we shouldn't
  // even create ComputedStyle for placeholders.  However, not doing any rule
  // matching for them is a first step.)
  already_AddRefed<ComputedStyle> ResolveStyleForPlaceholder();

  // Get a ComputedStyle for a pseudo-element.  aParentElement must be
  // non-null.  aPseudoID is the CSSPseudoElementType for the
  // pseudo-element.  aPseudoElement must be non-null if the pseudo-element
  // type is one that allows user action pseudo-classes after it or allows
  // style attributes; otherwise, it is ignored.
  already_AddRefed<ComputedStyle> ResolvePseudoElementStyle(
      dom::Element* aOriginatingElement, CSSPseudoElementType aType,
      ComputedStyle* aParentContext, dom::Element* aPseudoElement);

  // Resolves style for a (possibly-pseudo) Element without assuming that the
  // style has been resolved. If the element was unstyled and a new style
  // context was resolved, it is not stored in the DOM. (That is, the element
  // remains unstyled.)
  already_AddRefed<ComputedStyle> ResolveStyleLazily(
      dom::Element* aElement, CSSPseudoElementType aPseudoType,
      StyleRuleInclusion aRules = StyleRuleInclusion::All);

  // Get a ComputedStyle for an anonymous box.  aPseudoTag is the pseudo-tag to
  // use and must be non-null.  It must be an anon box, and must be one that
  // inherits style from the given aParentContext.
  already_AddRefed<ComputedStyle> ResolveInheritingAnonymousBoxStyle(
      nsAtom* aPseudoTag, ComputedStyle* aParentContext);

  // Get a ComputedStyle for an anonymous box that does not inherit style from
  // anything.  aPseudoTag is the pseudo-tag to use and must be non-null.  It
  // must be an anon box, and must be a non-inheriting one.
  already_AddRefed<ComputedStyle> ResolveNonInheritingAnonymousBoxStyle(
      nsAtom* aPseudoTag);

#ifdef MOZ_XUL
  already_AddRefed<ComputedStyle> ResolveXULTreePseudoStyle(
      dom::Element* aParentElement, nsCSSAnonBoxPseudoStaticAtom* aPseudoTag,
      ComputedStyle* aParentContext, const AtomArray& aInputWord);
#endif

  // manage the set of style sheets in the style set
  nsresult AppendStyleSheet(SheetType aType, StyleSheet* aSheet);
  nsresult PrependStyleSheet(SheetType aType, StyleSheet* aSheet);
  nsresult RemoveStyleSheet(SheetType aType, StyleSheet* aSheet);
  nsresult ReplaceSheets(SheetType aType,
                         const nsTArray<RefPtr<StyleSheet>>& aNewSheets);
  nsresult InsertStyleSheetBefore(SheetType aType, StyleSheet* aNewSheet,
                                  StyleSheet* aReferenceSheet);

  int32_t SheetCount(SheetType aType) const;
  StyleSheet* StyleSheetAt(SheetType aType, int32_t aIndex) const;

  void AppendAllNonDocumentAuthorSheets(nsTArray<StyleSheet*>& aArray) const;

  template <typename Func>
  void EnumerateStyleSheetArrays(Func aCallback) const {
    for (const auto& sheetArray : mSheets) {
      aCallback(sheetArray);
    }
  }

  nsresult RemoveDocStyleSheet(StyleSheet* aSheet);
  nsresult AddDocStyleSheet(StyleSheet* aSheet, nsIDocument* aDocument);

  // check whether there is ::before/::after style for an element
  already_AddRefed<ComputedStyle> ProbePseudoElementStyle(
      const dom::Element& aOriginatingElement, CSSPseudoElementType aType,
      ComputedStyle* aParentStyle);

  /**
   * Performs a Servo traversal to compute style for all dirty nodes in the
   * document.
   *
   * This will traverse all of the document's style roots (that is, its document
   * element, and the roots of the document-level native anonymous content).
   *
   * We specify |ForCSSRuleChanges| to try to update all CSS animations
   * when we call this function due to CSS rule changes since @keyframes rules
   * may have changed.
   *
   * Returns true if a post-traversal is required.
   */
  bool StyleDocument(ServoTraversalFlags aFlags);

  /**
   * Eagerly styles a subtree of unstyled nodes that was just appended to the
   * tree. This is used in situations where we need the style immediately and
   * cannot wait for a future batch restyle.
   */
  void StyleNewSubtree(dom::Element* aRoot);

  /**
   * Helper for correctly calling UpdateStylist without paying the cost of an
   * extra function call in the common no-rebuild-needed case.
   */
  void UpdateStylistIfNeeded() {
    if (StylistNeedsUpdate()) {
      UpdateStylist();
    }
  }

  /**
   * Checks whether the rule tree has crossed its threshold for unused nodes,
   * and if so, frees them.
   */
  void MaybeGCRuleTree();

  /**
   * Returns true if the given element may be used as the root of a style
   * traversal. Reasons for false include having an unstyled parent, or having
   * a parent that is display:none.
   *
   * Most traversal callsites don't need to check this, but some do.
   */
  static bool MayTraverseFrom(const dom::Element* aElement);

#ifdef DEBUG
  void AssertTreeIsClean();
#else
  void AssertTreeIsClean() {}
#endif

  /**
   * Clears any cached style data that may depend on all sorts of computed
   * values.
   *
   * Right now this clears the non-inheriting ComputedStyle cache, and resets
   * the default computed values.
   *
   * This does _not_, however, clear the stylist.
   */
  void ClearCachedStyleData();

  /**
   * Notifies the Servo stylesheet that the document's compatibility mode has
   * changed.
   */
  void CompatibilityModeChanged();

  /**
   * Resolve style for the given element, and return it as a
   * ComputedStyle.
   *
   * FIXME(emilio): Is there a point in this after bug 1367904?
   */
  inline already_AddRefed<ComputedStyle> ResolveServoStyle(const dom::Element&);

  bool GetKeyframesForName(const dom::Element&, const ComputedStyle&,
                           nsAtom* aName,
                           const nsTimingFunction& aTimingFunction,
                           nsTArray<Keyframe>& aKeyframes);

  nsTArray<ComputedKeyframeValues> GetComputedKeyframeValuesFor(
      const nsTArray<Keyframe>& aKeyframes, dom::Element* aElement,
      const ComputedStyle* aStyle);

  void GetAnimationValues(
      RawServoDeclarationBlock* aDeclarations, dom::Element* aElement,
      const mozilla::ComputedStyle* aStyle,
      nsTArray<RefPtr<RawServoAnimationValue>>& aAnimationValues);

  bool AppendFontFaceRules(nsTArray<nsFontFaceRuleContainer>& aArray);

  const RawServoCounterStyleRule* CounterStyleRuleForName(nsAtom* aName);

  // Get all the currently-active font feature values set.
  already_AddRefed<gfxFontFeatureValueSet> BuildFontFeatureValueSet();

  already_AddRefed<ComputedStyle> GetBaseContextForElement(
      dom::Element* aElement, const ComputedStyle* aStyle);

  // Get a ComputedStyle that represents |aStyle|, but as though it additionally
  // matched the rules of the newly added |aAnimaitonaValue|.
  //
  // We use this function to temporarily generate a ComputedStyle for
  // calculating the cumulative change hints.
  //
  // This must hold:
  //   The additional rules must be appropriate for the transition
  //   level of the cascade, which is the highest level of the cascade.
  //   (This is the case for one current caller, the cover rule used
  //   for CSS transitions.)
  // Note: |aElement| should be the generated element if it is pseudo.
  already_AddRefed<ComputedStyle> ResolveServoStyleByAddingAnimation(
      dom::Element* aElement, const ComputedStyle* aStyle,
      RawServoAnimationValue* aAnimationValue);
  /**
   * Resolve style for a given declaration block with/without the parent style.
   * If the parent style is not specified, the document default computed values
   * is used.
   */
  already_AddRefed<ComputedStyle> ResolveForDeclarations(
      const ComputedStyle* aParentOrNull,
      RawServoDeclarationBlockBorrowed aDeclarations);

  already_AddRefed<RawServoAnimationValue> ComputeAnimationValue(
      dom::Element* aElement, RawServoDeclarationBlock* aDeclaration,
      const mozilla::ComputedStyle* aStyle);

  void AppendTask(PostTraversalTask aTask) {
    MOZ_ASSERT(IsInServoTraversal());

    // We currently only use PostTraversalTasks while the Servo font metrics
    // mutex is locked.  If we need to use them in other situations during
    // a traversal, we should assert that we've taken appropriate
    // synchronization measures.
    AssertIsMainThreadOrServoFontMetricsLocked();

    mPostTraversalTasks.AppendElement(aTask);
  }

  // Returns true if a restyle of the document is needed due to cloning
  // sheet inners.
  bool EnsureUniqueInnerOnCSSSheets();

  // Called by StyleSheet::EnsureUniqueInner to let us know it cloned
  // its inner.
  void SetNeedsRestyleAfterEnsureUniqueInner() {
    mNeedsRestyleAfterEnsureUniqueInner = true;
  }

  // Returns the style rule map.
  ServoStyleRuleMap* StyleRuleMap();

  /**
   * Returns true if a modification to an an attribute with the specified
   * local name might require us to restyle the element.
   *
   * This function allows us to skip taking a an attribute snapshot when
   * the modified attribute doesn't appear in an attribute selector in
   * a style sheet.
   */
  bool MightHaveAttributeDependency(const dom::Element&,
                                    nsAtom* aAttribute) const;

  /**
   * Returns true if a change in event state on an element might require
   * us to restyle the element.
   *
   * This function allows us to skip taking a state snapshot when
   * the changed state isn't depended upon by any pseudo-class selectors
   * in a style sheet.
   */
  bool HasStateDependency(const dom::Element&, EventStates) const;

  /**
   * Returns true if a change in document state might require us to restyle the
   * document.
   */
  bool HasDocumentStateDependency(EventStates aState) const;

  /**
   * Get a new ComputedStyle that uses the same rules as the given ComputedStyle
   * but has a different parent.
   *
   * aElement is non-null if this is a ComputedStyle for a frame whose mContent
   * is an element and which has no pseudo on its ComputedStyle (so it's the
   * actual style for the element being passed).
   */
  already_AddRefed<ComputedStyle> ReparentComputedStyle(
      ComputedStyle* aComputedStyle, ComputedStyle* aNewParent,
      ComputedStyle* aNewParentIgnoringFirstLine,
      ComputedStyle* aNewLayoutParent, dom::Element* aElement);

 private:
  friend class AutoSetInServoTraversal;
  friend class AutoPrepareTraversal;

  bool ShouldTraverseInParallel() const;

  /**
   * Gets the pending snapshots to handle from the restyle manager.
   */
  const SnapshotTable& Snapshots();

  /**
   * Resolve all DeclarationBlocks attached to mapped
   * presentation attributes cached on the document.
   *
   * Call this before jumping into Servo's style system.
   */
  void ResolveMappedAttrDeclarationBlocks();

  /**
   * Clear our cached mNonInheritingComputedStyles.
   *
   * We do this when we want to make sure those ComputedStyles won't live too
   * long (e.g. when rebuilding all style data or when shutting down the style
   * set).
   */
  void ClearNonInheritingComputedStyles();

  /**
   * Perform processes that we should do before traversing.
   *
   * When aRoot is null, the entire document is pre-traversed.  Otherwise,
   * only the subtree rooted at aRoot is pre-traversed.
   */
  void PreTraverse(ServoTraversalFlags aFlags, dom::Element* aRoot = nullptr);

  // Subset of the pre-traverse steps that involve syncing up data
  void PreTraverseSync();

  /**
   * Records that the contents of style sheets at the specified origin have
   * changed since the last.  Calling this will ensure that the Stylist
   * rebuilds its selector maps.
   */
  void MarkOriginsDirty(OriginFlags aChangedOrigins);

  /**
   * Note that the stylist needs a style flush due to style sheet changes.
   */
  void SetStylistStyleSheetsDirty();

  void SetStylistXBLStyleSheetsDirty();

  bool StylistNeedsUpdate() const {
    return mStylistState != StylistState::NotDirty;
  }

  /**
   * Update the stylist as needed to ensure style data is up-to-date.
   *
   * This should only be called if StylistNeedsUpdate returns true.
   */
  void UpdateStylist();

  already_AddRefed<ComputedStyle> ResolveStyleLazilyInternal(
      dom::Element* aElement, CSSPseudoElementType aPseudoType,
      StyleRuleInclusion aRules = StyleRuleInclusion::All);

  void RunPostTraversalTasks();

  void PrependSheetOfType(SheetType aType, StyleSheet* aSheet);

  void AppendSheetOfType(SheetType aType, StyleSheet* aSheet);

  void InsertSheetOfType(SheetType aType, StyleSheet* aSheet,
                         StyleSheet* aBeforeSheet);

  void RemoveSheetOfType(SheetType aType, StyleSheet* aSheet);

  // The owner document of this style set. Null if this is an XBL style set.
  //
  // TODO(emilio): This should become a DocumentOrShadowRoot, and be owned by it
  // directly instead of the shell, eventually.
  nsIDocument* mDocument;

  const nsPresContext* GetPresContext() const {
    return const_cast<ServoStyleSet*>(this)->GetPresContext();
  }

  /**
   * Return the associated pres context if we're the master style set and we
   * have an associated pres shell.
   */
  nsPresContext* GetPresContext();

  UniquePtr<RawServoStyleSet> mRawSet;
  EnumeratedArray<SheetType, SheetType::Count, nsTArray<RefPtr<StyleSheet>>>
      mSheets;
  bool mAuthorStyleDisabled;
  StylistState mStylistState;
  uint64_t mUserFontSetUpdateGeneration;

  bool mNeedsRestyleAfterEnsureUniqueInner;

  // Stores pointers to our cached ComputedStyles for non-inheriting anonymous
  // boxes.
  EnumeratedArray<nsCSSAnonBoxes::NonInheriting,
                  nsCSSAnonBoxes::NonInheriting::_Count, RefPtr<ComputedStyle>>
      mNonInheritingComputedStyles;

  // Tasks to perform after a traversal, back on the main thread.
  //
  // These are similar to Servo's SequentialTasks, except that they are
  // posted by C++ code running on style worker threads.
  nsTArray<PostTraversalTask> mPostTraversalTasks;

  // Map from raw Servo style rule to Gecko's wrapper object.
  // Constructed lazily when requested by devtools.
  UniquePtr<ServoStyleRuleMap> mStyleRuleMap;
};

class UACacheReporter final : public nsIMemoryReporter {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTER

 private:
  ~UACacheReporter() {}
};

}  // namespace mozilla

#endif  // mozilla_ServoStyleSet_h
