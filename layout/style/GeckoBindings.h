/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* FFI functions for Servo to call into Gecko */

#ifndef mozilla_GeckoBindings_h
#define mozilla_GeckoBindings_h

#include <stdint.h>

#include "mozilla/ServoTypes.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/css/DocumentMatchingFunction.h"
#include "mozilla/css/SheetLoadData.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/ComputedTimingFunction.h"
#include "nsCSSValue.h"
#include "nsStyleStruct.h"

class nsAtom;
class nsIURI;
class nsSimpleContentList;
struct nsFont;

namespace mozilla {
class ComputedStyle;
class SeenPtrs;
class ServoElementSnapshot;
class ServoElementSnapshotTable;
class SharedFontList;
class StyleSheet;
enum class CSSPseudoElementType : uint8_t;
enum class PointerCapabilities : uint8_t;
enum class UpdateAnimationsTasks : uint8_t;
struct FontFamilyName;
struct Keyframe;

namespace css {
class LoaderReusableStyleSheets;
}
}  // namespace mozilla

#ifdef NIGHTLY_BUILD
const bool GECKO_IS_NIGHTLY = true;
#else
const bool GECKO_IS_NIGHTLY = false;
#endif

#define NS_DECL_THREADSAFE_FFI_REFCOUNTING(class_, name_)  \
  void Gecko_AddRef##name_##ArbitraryThread(class_* aPtr); \
  void Gecko_Release##name_##ArbitraryThread(class_* aPtr);
#define NS_IMPL_THREADSAFE_FFI_REFCOUNTING(class_, name_)                      \
  static_assert(class_::HasThreadSafeRefCnt::value,                            \
                "NS_DECL_THREADSAFE_FFI_REFCOUNTING can only be used with "    \
                "classes that have thread-safe refcounting");                  \
  void Gecko_AddRef##name_##ArbitraryThread(class_* aPtr) { NS_ADDREF(aPtr); } \
  void Gecko_Release##name_##ArbitraryThread(class_* aPtr) { NS_RELEASE(aPtr); }

extern "C" {

// Debugging stuff.
void Gecko_Element_DebugListAttributes(RawGeckoElementBorrowed, nsCString*);

void Gecko_Snapshot_DebugListAttributes(const mozilla::ServoElementSnapshot*,
                                        nsCString*);

bool Gecko_IsSignificantChild(RawGeckoNodeBorrowed node,
                              bool whitespace_is_significant);

RawGeckoNodeBorrowedOrNull Gecko_GetLastChild(RawGeckoNodeBorrowed node);
RawGeckoNodeBorrowedOrNull Gecko_GetPreviousSibling(RawGeckoNodeBorrowed node);

RawGeckoNodeBorrowedOrNull Gecko_GetFlattenedTreeParentNode(
    RawGeckoNodeBorrowed node);

RawGeckoElementBorrowedOrNull Gecko_GetBeforeOrAfterPseudo(
    RawGeckoElementBorrowed element, bool is_before);

nsTArray<nsIContent*>* Gecko_GetAnonymousContentForElement(
    RawGeckoElementBorrowed element);

const nsTArray<RefPtr<nsINode>>* Gecko_GetAssignedNodes(
    RawGeckoElementBorrowed element);

void Gecko_DestroyAnonymousContentList(nsTArray<nsIContent*>* anon_content);

void Gecko_ComputedStyle_Init(mozilla::ComputedStyle* context,
                              RawGeckoPresContextBorrowed pres_context,
                              ServoComputedDataBorrowed values,
                              mozilla::CSSPseudoElementType pseudo_type,
                              nsAtom* pseudo_tag);

void Gecko_ComputedStyle_Destroy(mozilla::ComputedStyle* context);

// By default, Servo walks the DOM by traversing the siblings of the DOM-view
// first child. This generally works, but misses anonymous children, which we
// want to traverse during styling. To support these cases, we create an
// optional stack-allocated iterator in aIterator for nodes that need it.
void Gecko_ConstructStyleChildrenIterator(
    RawGeckoElementBorrowed aElement,
    RawGeckoStyleChildrenIteratorBorrowedMut aIterator);

void Gecko_DestroyStyleChildrenIterator(
    RawGeckoStyleChildrenIteratorBorrowedMut aIterator);

RawGeckoNodeBorrowedOrNull Gecko_GetNextStyleChild(
    RawGeckoStyleChildrenIteratorBorrowedMut it);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::SheetLoadDataHolder,
                                   SheetLoadDataHolder);

void Gecko_StyleSheet_FinishAsyncParse(
    mozilla::css::SheetLoadDataHolder* data,
    RawServoStyleSheetContentsStrong sheet_contents,
    StyleUseCountersOwnedOrNull use_counters);

mozilla::StyleSheet* Gecko_LoadStyleSheet(
    mozilla::css::Loader* loader, mozilla::StyleSheet* parent,
    mozilla::css::SheetLoadData* parent_load_data,
    mozilla::css::LoaderReusableStyleSheets* reusable_sheets,
    RawServoCssUrlDataStrong url, RawServoMediaListStrong media_list);

void Gecko_LoadStyleSheetAsync(mozilla::css::SheetLoadDataHolder* parent_data,
                               RawServoCssUrlDataStrong url,
                               RawServoMediaListStrong media_list,
                               RawServoImportRuleStrong import_rule);

// Selector Matching.
uint64_t Gecko_ElementState(RawGeckoElementBorrowed element);
bool Gecko_IsRootElement(RawGeckoElementBorrowed element);

bool Gecko_MatchLang(RawGeckoElementBorrowed element, nsAtom* override_lang,
                     bool has_override_lang, const char16_t* value);

nsAtom* Gecko_GetXMLLangValue(RawGeckoElementBorrowed element);

nsIDocument::DocumentTheme Gecko_GetDocumentLWTheme(
    const nsIDocument* aDocument);

bool Gecko_IsTableBorderNonzero(RawGeckoElementBorrowed element);
bool Gecko_IsBrowserFrame(RawGeckoElementBorrowed element);

// Attributes.
#define SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(prefix_, implementor_)   \
  nsAtom* prefix_##LangValue(implementor_ element);                            \
  bool prefix_##HasAttr(implementor_ element, nsAtom* ns, nsAtom* name);       \
  bool prefix_##AttrEquals(implementor_ element, nsAtom* ns, nsAtom* name,     \
                           nsAtom* str, bool ignoreCase);                      \
  bool prefix_##AttrDashEquals(implementor_ element, nsAtom* ns, nsAtom* name, \
                               nsAtom* str, bool ignore_case);                 \
  bool prefix_##AttrIncludes(implementor_ element, nsAtom* ns, nsAtom* name,   \
                             nsAtom* str, bool ignore_case);                   \
  bool prefix_##AttrHasSubstring(implementor_ element, nsAtom* ns,             \
                                 nsAtom* name, nsAtom* str, bool ignore_case); \
  bool prefix_##AttrHasPrefix(implementor_ element, nsAtom* ns, nsAtom* name,  \
                              nsAtom* str, bool ignore_case);                  \
  bool prefix_##AttrHasSuffix(implementor_ element, nsAtom* ns, nsAtom* name,  \
                              nsAtom* str, bool ignore_case);

bool Gecko_AssertClassAttrValueIsSane(const nsAttrValue*);
const nsAttrValue* Gecko_GetSVGAnimatedClass(RawGeckoElementBorrowed);

SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(Gecko_, RawGeckoElementBorrowed)

SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS(
    Gecko_Snapshot, const mozilla::ServoElementSnapshot*)

#undef SERVO_DECLARE_ELEMENT_ATTR_MATCHING_FUNCTIONS

// Style attributes.
RawServoDeclarationBlockStrongBorrowedOrNull Gecko_GetStyleAttrDeclarationBlock(
    RawGeckoElementBorrowed element);

void Gecko_UnsetDirtyStyleAttr(RawGeckoElementBorrowed element);

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetHTMLPresentationAttrDeclarationBlock(RawGeckoElementBorrowed element);

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetExtraContentStyleDeclarations(RawGeckoElementBorrowed element);

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetUnvisitedLinkAttrDeclarationBlock(RawGeckoElementBorrowed element);

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetVisitedLinkAttrDeclarationBlock(RawGeckoElementBorrowed element);

RawServoDeclarationBlockStrongBorrowedOrNull
Gecko_GetActiveLinkAttrDeclarationBlock(RawGeckoElementBorrowed element);

// Visited handling.

// Returns whether visited styles are enabled for a given document.
bool Gecko_VisitedStylesEnabled(const nsIDocument*);

// Animations
bool Gecko_GetAnimationRule(
    RawGeckoElementBorrowed aElementOrPseudo,
    mozilla::EffectCompositor::CascadeLevel aCascadeLevel,
    RawServoAnimationValueMapBorrowedMut aAnimationValues);

bool Gecko_StyleAnimationsEquals(RawGeckoStyleAnimationListBorrowed,
                                 RawGeckoStyleAnimationListBorrowed);

void Gecko_CopyAnimationNames(RawGeckoStyleAnimationListBorrowedMut aDest,
                              RawGeckoStyleAnimationListBorrowed aSrc);

// This function takes an already addrefed nsAtom
void Gecko_SetAnimationName(mozilla::StyleAnimation* aStyleAnimation,
                            nsAtom* aAtom);

void Gecko_UpdateAnimations(RawGeckoElementBorrowed aElementOrPseudo,
                            ComputedStyleBorrowedOrNull aOldComputedValues,
                            ComputedStyleBorrowedOrNull aComputedValues,
                            mozilla::UpdateAnimationsTasks aTasks);

size_t Gecko_GetAnimationEffectCount(RawGeckoElementBorrowed aElementOrPseudo);
bool Gecko_ElementHasAnimations(RawGeckoElementBorrowed aElementOrPseudo);
bool Gecko_ElementHasCSSAnimations(RawGeckoElementBorrowed aElementOrPseudo);
bool Gecko_ElementHasCSSTransitions(RawGeckoElementBorrowed aElementOrPseudo);
size_t Gecko_ElementTransitions_Length(
    RawGeckoElementBorrowed aElementOrPseudo);

nsCSSPropertyID Gecko_ElementTransitions_PropertyAt(
    RawGeckoElementBorrowed aElementOrPseudo, size_t aIndex);

RawServoAnimationValueBorrowedOrNull Gecko_ElementTransitions_EndValueAt(
    RawGeckoElementBorrowed aElementOrPseudo, size_t aIndex);

double Gecko_GetProgressFromComputedTiming(
    RawGeckoComputedTimingBorrowed aComputedTiming);

double Gecko_GetPositionInSegment(
    RawGeckoAnimationPropertySegmentBorrowed aSegment, double aProgress,
    mozilla::ComputedTimingFunction::BeforeFlag aBeforeFlag);

// Get servo's AnimationValue for |aProperty| from the cached base style
// |aBaseStyles|.
// |aBaseStyles| is nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue>.
// We use RawServoAnimationValueTableBorrowed to avoid exposing
// nsRefPtrHashtable in FFI.
RawServoAnimationValueBorrowedOrNull Gecko_AnimationGetBaseStyle(
    RawServoAnimationValueTableBorrowed aBaseStyles, nsCSSPropertyID aProperty);

void Gecko_StyleTransition_SetUnsupportedProperty(
    mozilla::StyleTransition* aTransition, nsAtom* aAtom);

// Atoms.
nsAtom* Gecko_Atomize(const char* aString, uint32_t aLength);
nsAtom* Gecko_Atomize16(const nsAString* aString);
void Gecko_AddRefAtom(nsAtom* aAtom);
void Gecko_ReleaseAtom(nsAtom* aAtom);

// Font style
void Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src);

void Gecko_nsTArray_FontFamilyName_AppendNamed(
    nsTArray<mozilla::FontFamilyName>* aNames, nsAtom* aName, bool aQuoted);

void Gecko_nsTArray_FontFamilyName_AppendGeneric(
    nsTArray<mozilla::FontFamilyName>* aNames, mozilla::FontFamilyType aType);

// Returns an already-AddRefed SharedFontList with an empty mNames array.
mozilla::SharedFontList* Gecko_SharedFontList_Create();

size_t Gecko_SharedFontList_SizeOfIncludingThis(
    mozilla::SharedFontList* fontlist);

size_t Gecko_SharedFontList_SizeOfIncludingThisIfUnshared(
    mozilla::SharedFontList* fontlist);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::SharedFontList, SharedFontList);

// will not run destructors on dst, give it uninitialized memory
// font_id is LookAndFeel::FontID
void Gecko_nsFont_InitSystem(nsFont* dst, int32_t font_id,
                             const nsStyleFont* font,
                             RawGeckoPresContextBorrowed pres_context);

void Gecko_nsFont_Destroy(nsFont* dst);

// The gfxFontFeatureValueSet returned from this function has zero reference.
gfxFontFeatureValueSet* Gecko_ConstructFontFeatureValueSet();

nsTArray<unsigned int>* Gecko_AppendFeatureValueHashEntry(
    gfxFontFeatureValueSet* value_set, nsAtom* family, uint32_t alternate,
    nsAtom* name);

void Gecko_nsFont_SetFontFeatureValuesLookup(
    nsFont* font, const RawGeckoPresContext* pres_context);

void Gecko_nsFont_ResetFontFeatureValuesLookup(nsFont* font);

// Font variant alternates
void Gecko_ClearAlternateValues(nsFont* font, size_t length);

void Gecko_AppendAlternateValues(nsFont* font, uint32_t alternate_name,
                                 nsAtom* atom);

void Gecko_CopyAlternateValuesFrom(nsFont* dest, const nsFont* src);

// Visibility style
void Gecko_SetImageOrientation(nsStyleVisibility* aVisibility,
                               uint8_t aOrientation, bool aFlip);

void Gecko_SetImageOrientationAsFromImage(nsStyleVisibility* aVisibility);

void Gecko_CopyImageOrientationFrom(nsStyleVisibility* aDst,
                                    const nsStyleVisibility* aSrc);

// Counter style.
// This function takes an already addrefed nsAtom
void Gecko_SetCounterStyleToName(mozilla::CounterStylePtr* ptr, nsAtom* name,
                                 RawGeckoPresContextBorrowed pres_context);

void Gecko_SetCounterStyleToSymbols(mozilla::CounterStylePtr* ptr,
                                    uint8_t symbols_type,
                                    nsACString const* const* symbols,
                                    uint32_t symbols_count);

void Gecko_SetCounterStyleToString(mozilla::CounterStylePtr* ptr,
                                   const nsACString* symbol);

void Gecko_CopyCounterStyle(mozilla::CounterStylePtr* dst,
                            const mozilla::CounterStylePtr* src);

nsAtom* Gecko_CounterStyle_GetName(const mozilla::CounterStylePtr* ptr);

const mozilla::AnonymousCounterStyle* Gecko_CounterStyle_GetAnonymous(
    const mozilla::CounterStylePtr* ptr);

// background-image style.
void Gecko_SetNullImageValue(nsStyleImage* image);
void Gecko_SetGradientImageValue(nsStyleImage* image,
                                 nsStyleGradient* gradient);

void Gecko_SetLayerImageImageValue(nsStyleImage* image,
                                   mozilla::css::URLValue* image_value);

void Gecko_SetImageElement(nsStyleImage* image, nsAtom* atom);
void Gecko_CopyImageValueFrom(nsStyleImage* image, const nsStyleImage* other);
void Gecko_InitializeImageCropRect(nsStyleImage* image);

nsStyleGradient* Gecko_CreateGradient(uint8_t shape, uint8_t size,
                                      bool repeating, bool legacy_syntax,
                                      bool moz_legacy_syntax, uint32_t stops);

const nsStyleImageRequest* Gecko_GetImageRequest(const nsStyleImage* image);
nsAtom* Gecko_GetImageElement(const nsStyleImage* image);
const nsStyleGradient* Gecko_GetGradientImageValue(const nsStyleImage* image);

// list-style-image style.
void Gecko_SetListStyleImageNone(nsStyleList* style_struct);

void Gecko_SetListStyleImageImageValue(nsStyleList* style_struct,
                                       mozilla::css::URLValue* aImageValue);

void Gecko_CopyListStyleImageFrom(nsStyleList* dest, const nsStyleList* src);

// cursor style.
void Gecko_SetCursorArrayLength(nsStyleUI* ui, size_t len);

void Gecko_SetCursorImageValue(nsCursorImage* aCursor,
                               mozilla::css::URLValue* aImageValue);

void Gecko_CopyCursorArrayFrom(nsStyleUI* dest, const nsStyleUI* src);

void Gecko_SetContentDataImageValue(nsStyleContentData* aList,
                                    mozilla::css::URLValue* aImageValue);

nsStyleContentData::CounterFunction* Gecko_SetCounterFunction(
    nsStyleContentData* content_data, mozilla::StyleContentType);

// Dirtiness tracking.
void Gecko_SetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);
void Gecko_UnsetNodeFlags(RawGeckoNodeBorrowed node, uint32_t flags);
void Gecko_NoteDirtyElement(RawGeckoElementBorrowed element);
void Gecko_NoteDirtySubtreeForInvalidation(RawGeckoElementBorrowed element);
void Gecko_NoteAnimationOnlyDirtyElement(RawGeckoElementBorrowed element);

bool Gecko_AnimationNameMayBeReferencedFromStyle(
    RawGeckoPresContextBorrowed pres_context, nsAtom* name);

// Incremental restyle.
mozilla::CSSPseudoElementType Gecko_GetImplementedPseudo(
    RawGeckoElementBorrowed element);

// We'd like to return `nsChangeHint` here, but bindgen bitfield enums don't
// work as return values with the Linux 32-bit ABI at the moment because
// they wrap the value in a struct.
uint32_t Gecko_CalcStyleDifference(ComputedStyleBorrowed old_style,
                                   ComputedStyleBorrowed new_style,
                                   bool* any_style_struct_changed,
                                   bool* reset_only_changed);

// Get an element snapshot for a given element from the table.
const ServoElementSnapshot* Gecko_GetElementSnapshot(
    const mozilla::ServoElementSnapshotTable* table,
    RawGeckoElementBorrowed element);

// Have we seen this pointer before?
bool Gecko_HaveSeenPtr(mozilla::SeenPtrs* table, const void* ptr);

// `array` must be an nsTArray
// If changing this signature, please update the
// friend function declaration in nsTArray.h
void Gecko_EnsureTArrayCapacity(void* array, size_t capacity, size_t elem_size);

// Same here, `array` must be an nsTArray<T>, for some T.
//
// Important note: Only valid for POD types, since destructors won't be run
// otherwise. This is ensured with rust traits for the relevant structs.
void Gecko_ClearPODTArray(void* array, size_t elem_size, size_t elem_align);

void Gecko_ResizeTArrayForStrings(nsTArray<nsString>* array, uint32_t length);

void Gecko_SetStyleGridTemplate(
    mozilla::UniquePtr<nsStyleGridTemplate>* grid_template,
    nsStyleGridTemplate* value);

nsStyleGridTemplate* Gecko_CreateStyleGridTemplate(uint32_t track_sizes,
                                                   uint32_t name_size);

void Gecko_CopyStyleGridTemplateValues(
    mozilla::UniquePtr<nsStyleGridTemplate>* grid_template,
    const nsStyleGridTemplate* other);

mozilla::css::GridTemplateAreasValue* Gecko_NewGridTemplateAreasValue(
    uint32_t areas, uint32_t templates, uint32_t columns);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::GridTemplateAreasValue,
                                   GridTemplateAreasValue);

// Clear the mContents, mCounterIncrements, or mCounterResets field in
// nsStyleContent. This is needed to run the destructors, otherwise we'd
// leak the images, strings, and whatnot.
void Gecko_ClearAndResizeStyleContents(nsStyleContent* content,
                                       uint32_t how_many);

void Gecko_ClearAndResizeCounterIncrements(nsStyleContent* content,
                                           uint32_t how_many);

void Gecko_ClearAndResizeCounterResets(nsStyleContent* content,
                                       uint32_t how_many);

void Gecko_CopyStyleContentsFrom(nsStyleContent* content,
                                 const nsStyleContent* other);

void Gecko_CopyCounterResetsFrom(nsStyleContent* content,
                                 const nsStyleContent* other);

void Gecko_CopyCounterIncrementsFrom(nsStyleContent* content,
                                     const nsStyleContent* other);

void Gecko_EnsureImageLayersLength(nsStyleImageLayers* layers, size_t len,
                                   nsStyleImageLayers::LayerType layer_type);

void Gecko_EnsureStyleAnimationArrayLength(void* array, size_t len);
void Gecko_EnsureStyleTransitionArrayLength(void* array, size_t len);
void Gecko_ClearWillChange(nsStyleDisplay* display, size_t length);
void Gecko_AppendWillChange(nsStyleDisplay* display, nsAtom* atom);
void Gecko_CopyWillChangeFrom(nsStyleDisplay* dest, nsStyleDisplay* src);

// Searches from the beginning of |keyframes| for a Keyframe object with the
// specified offset and timing function. If none is found, a new Keyframe object
// with the specified |offset| and |timingFunction| will be prepended to
// |keyframes|.
//
// @param keyframes  An array of Keyframe objects, sorted by offset.
//                   The first Keyframe in the array, if any, MUST have an
//                   offset greater than or equal to |offset|.
// @param offset  The offset to search for, or, if no suitable Keyframe is
//                found, the offset to use for the created Keyframe.
//                Must be a floating point number in the range [0.0, 1.0].
// @param timingFunction  The timing function to match, or, if no suitable
//                        Keyframe is found, to set on the created Keyframe.
//
// @returns  The matching or created Keyframe.
mozilla::Keyframe* Gecko_GetOrCreateKeyframeAtStart(
    RawGeckoKeyframeListBorrowedMut keyframes, float offset,
    const nsTimingFunction* timingFunction);

// As with Gecko_GetOrCreateKeyframeAtStart except that this method will search
// from the beginning of |keyframes| for a Keyframe with matching timing
// function and an offset of 0.0.
// Furthermore, if a matching Keyframe is not found, a new Keyframe will be
// inserted after the *last* Keyframe in |keyframes| with offset 0.0.
mozilla::Keyframe* Gecko_GetOrCreateInitialKeyframe(
    RawGeckoKeyframeListBorrowedMut keyframes,
    const nsTimingFunction* timingFunction);

// As with Gecko_GetOrCreateKeyframeAtStart except that this method will search
// from the *end* of |keyframes| for a Keyframe with matching timing function
// and an offset of 1.0. If a matching Keyframe is not found, a new Keyframe
// will be appended to the end of |keyframes|.
mozilla::Keyframe* Gecko_GetOrCreateFinalKeyframe(
    RawGeckoKeyframeListBorrowedMut keyframes,
    const nsTimingFunction* timingFunction);

// Appends and returns a new PropertyValuePair to |aProperties| initialized with
// its mProperty member set to |aProperty| and all other members initialized to
// their default values.
mozilla::PropertyValuePair* Gecko_AppendPropertyValuePair(
    RawGeckoPropertyValuePairListBorrowedMut aProperties,
    nsCSSPropertyID aProperty);

// Clean up pointer-based coordinates
void Gecko_ResetStyleCoord(nsStyleUnit* unit, nsStyleUnion* value);

// Set an nsStyleCoord to a computed `calc()` value
void Gecko_SetStyleCoordCalcValue(nsStyleUnit* unit, nsStyleUnion* value,
                                  nsStyleCoord::CalcValue calc);

void Gecko_CopyShapeSourceFrom(mozilla::StyleShapeSource* dst,
                               const mozilla::StyleShapeSource* src);

void Gecko_DestroyShapeSource(mozilla::StyleShapeSource* shape);

void Gecko_NewBasicShape(mozilla::StyleShapeSource* shape,
                         mozilla::StyleBasicShapeType type);

void Gecko_NewShapeImage(mozilla::StyleShapeSource* shape);

void Gecko_StyleShapeSource_SetURLValue(mozilla::StyleShapeSource* shape,
                                        mozilla::css::URLValue* uri);

void Gecko_NewStyleSVGPath(mozilla::StyleShapeSource* shape);

void Gecko_SetStyleMotion(mozilla::UniquePtr<mozilla::StyleMotion>* aMotion,
                          mozilla::StyleMotion* aValue);

mozilla::StyleMotion* Gecko_NewStyleMotion();

void Gecko_CopyStyleMotions(mozilla::UniquePtr<mozilla::StyleMotion>* motion,
                            const mozilla::StyleMotion* other);

void Gecko_ResetFilters(nsStyleEffects* effects, size_t new_len);

void Gecko_CopyFiltersFrom(nsStyleEffects* aSrc, nsStyleEffects* aDest);

void Gecko_nsStyleFilter_SetURLValue(nsStyleFilter* effects,
                                     mozilla::css::URLValue* uri);

void Gecko_nsStyleSVGPaint_CopyFrom(nsStyleSVGPaint* dest,
                                    const nsStyleSVGPaint* src);

void Gecko_nsStyleSVGPaint_SetURLValue(nsStyleSVGPaint* paint,
                                       mozilla::css::URLValue* uri);

void Gecko_nsStyleSVGPaint_Reset(nsStyleSVGPaint* paint);

void Gecko_nsStyleSVG_SetDashArrayLength(nsStyleSVG* svg, uint32_t len);

void Gecko_nsStyleSVG_CopyDashArray(nsStyleSVG* dst, const nsStyleSVG* src);

void Gecko_nsStyleSVG_SetContextPropertiesLength(nsStyleSVG* svg, uint32_t len);

void Gecko_nsStyleSVG_CopyContextProperties(nsStyleSVG* dst,
                                            const nsStyleSVG* src);

mozilla::css::URLValue* Gecko_URLValue_Create(RawServoCssUrlDataStrong url,
                                              mozilla::CORSMode aCORSMode);

size_t Gecko_URLValue_SizeOfIncludingThis(mozilla::css::URLValue* url);

void Gecko_GetComputedURLSpec(const mozilla::css::URLValue* url,
                              nsCString* spec);

void Gecko_GetComputedImageURLSpec(const mozilla::css::URLValue* url,
                                   nsCString* spec);

void Gecko_nsIURI_Debug(nsIURI*, nsCString* spec);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(mozilla::css::URLValue, CSSURLValue);
NS_DECL_THREADSAFE_FFI_REFCOUNTING(RawGeckoURLExtraData, URLExtraData);

void Gecko_FillAllImageLayers(nsStyleImageLayers* layers, uint32_t max_len);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsStyleCoord::Calc, Calc);

nsCSSShadowArray* Gecko_NewCSSShadowArray(uint32_t len);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsCSSShadowArray, CSSShadowArray);

nsCSSValueSharedList* Gecko_NewCSSValueSharedList(uint32_t len);
nsCSSValueSharedList* Gecko_NewNoneTransform();

// Getter for nsCSSValue
nsCSSValueBorrowedMut Gecko_CSSValue_GetArrayItem(
    nsCSSValueBorrowedMut css_value, int32_t index);

// const version of the above function.
nsCSSValueBorrowed Gecko_CSSValue_GetArrayItemConst(
    nsCSSValueBorrowed css_value, int32_t index);

nsCSSKeyword Gecko_CSSValue_GetKeyword(nsCSSValueBorrowed aCSSValue);
float Gecko_CSSValue_GetNumber(nsCSSValueBorrowed css_value);
float Gecko_CSSValue_GetPercentage(nsCSSValueBorrowed css_value);
nsStyleCoord::CalcValue Gecko_CSSValue_GetCalc(nsCSSValueBorrowed aCSSValue);
void Gecko_CSSValue_SetNumber(nsCSSValueBorrowedMut css_value, float number);

void Gecko_CSSValue_SetKeyword(nsCSSValueBorrowedMut css_value,
                               nsCSSKeyword keyword);

void Gecko_CSSValue_SetPercentage(nsCSSValueBorrowedMut css_value,
                                  float percent);

void Gecko_CSSValue_SetPixelLength(nsCSSValueBorrowedMut aCSSValue, float aLen);

void Gecko_CSSValue_SetCalc(nsCSSValueBorrowedMut css_value,
                            nsStyleCoord::CalcValue calc);

void Gecko_CSSValue_SetFunction(nsCSSValueBorrowedMut css_value, int32_t len);

void Gecko_CSSValue_SetString(nsCSSValueBorrowedMut css_value,
                              const uint8_t* string, uint32_t len,
                              nsCSSUnit unit);

void Gecko_CSSValue_SetStringFromAtom(nsCSSValueBorrowedMut css_value,
                                      nsAtom* atom, nsCSSUnit unit);

// Take an addrefed nsAtom and set it to the nsCSSValue
void Gecko_CSSValue_SetAtomIdent(nsCSSValueBorrowedMut css_value, nsAtom* atom);
void Gecko_CSSValue_SetArray(nsCSSValueBorrowedMut css_value, int32_t len);

void Gecko_CSSValue_SetInt(nsCSSValueBorrowedMut css_value, int32_t integer,
                           nsCSSUnit unit);

void Gecko_CSSValue_SetFloat(nsCSSValueBorrowedMut css_value, float value,
                             nsCSSUnit unit);

void Gecko_CSSValue_SetPair(nsCSSValueBorrowedMut css_value,
                            nsCSSValueBorrowed xvalue,
                            nsCSSValueBorrowed yvalue);

void Gecko_CSSValue_SetList(nsCSSValueBorrowedMut css_value, uint32_t len);
void Gecko_CSSValue_SetPairList(nsCSSValueBorrowedMut css_value, uint32_t len);

void Gecko_CSSValue_InitSharedList(nsCSSValueBorrowedMut css_value,
                                   uint32_t len);

void Gecko_CSSValue_Drop(nsCSSValueBorrowedMut css_value);

NS_DECL_THREADSAFE_FFI_REFCOUNTING(nsCSSValueSharedList, CSSValueSharedList);

float Gecko_FontStretch_ToFloat(mozilla::FontStretch aStretch);

void Gecko_FontStretch_SetFloat(mozilla::FontStretch* aStretch,
                                float aFloatValue);

float Gecko_FontSlantStyle_ToFloat(mozilla::FontSlantStyle aStyle);
void Gecko_FontSlantStyle_SetNormal(mozilla::FontSlantStyle*);
void Gecko_FontSlantStyle_SetItalic(mozilla::FontSlantStyle*);

void Gecko_FontSlantStyle_SetOblique(mozilla::FontSlantStyle*,
                                     float angle_degrees);

void Gecko_FontSlantStyle_Get(mozilla::FontSlantStyle, bool* normal,
                              bool* italic, float* oblique_angle);

float Gecko_FontWeight_ToFloat(mozilla::FontWeight aWeight);

void Gecko_FontWeight_SetFloat(mozilla::FontWeight* aWeight, float aFloatValue);

void Gecko_nsStyleFont_SetLang(nsStyleFont* font, nsAtom* atom);

void Gecko_nsStyleFont_CopyLangFrom(nsStyleFont* aFont,
                                    const nsStyleFont* aSource);

void Gecko_nsStyleFont_FixupNoneGeneric(
    nsStyleFont* font, RawGeckoPresContextBorrowed pres_context);

void Gecko_nsStyleFont_PrefillDefaultForGeneric(
    nsStyleFont* font, RawGeckoPresContextBorrowed pres_context,
    uint8_t generic_id);

void Gecko_nsStyleFont_FixupMinFontSize(
    nsStyleFont* font, RawGeckoPresContextBorrowed pres_context);

mozilla::FontSizePrefs Gecko_GetBaseSize(nsAtom* lang);

// XBL related functions.
RawGeckoElementBorrowedOrNull Gecko_GetBindingParent(
    RawGeckoElementBorrowed aElement);

RawServoAuthorStylesBorrowedOrNull Gecko_XBLBinding_GetRawServoStyles(
    RawGeckoXBLBindingBorrowed aXBLBinding);

bool Gecko_XBLBinding_InheritsStyle(RawGeckoXBLBindingBorrowed aXBLBinding);

struct GeckoFontMetrics {
  nscoord mChSize;
  nscoord mXSize;
};

GeckoFontMetrics Gecko_GetFontMetrics(RawGeckoPresContextBorrowed pres_context,
                                      bool is_vertical, const nsStyleFont* font,
                                      nscoord font_size,
                                      bool use_user_font_set);

int32_t Gecko_GetAppUnitsPerPhysicalInch(
    RawGeckoPresContextBorrowed pres_context);

mozilla::StyleSheet* Gecko_StyleSheet_Clone(
    const mozilla::StyleSheet* aSheet,
    const mozilla::StyleSheet* aNewParentSheet);

void Gecko_StyleSheet_AddRef(const mozilla::StyleSheet* aSheet);
void Gecko_StyleSheet_Release(const mozilla::StyleSheet* aSheet);
nsCSSKeyword Gecko_LookupCSSKeyword(const uint8_t* string, uint32_t len);
const char* Gecko_CSSKeywordString(nsCSSKeyword keyword, uint32_t* len);
bool Gecko_IsDocumentBody(RawGeckoElementBorrowed element);

// We use an int32_t here instead of a LookAndFeel::ColorID
// because forward-declaring a nested enum/struct is impossible
nscolor Gecko_GetLookAndFeelSystemColor(
    int32_t color_id, RawGeckoPresContextBorrowed pres_context);

void Gecko_AddPropertyToSet(nsCSSPropertyIDSetBorrowedMut, nsCSSPropertyID);

// Style-struct management.
#define STYLE_STRUCT(name)                                            \
  void Gecko_Construct_Default_nsStyle##name(                         \
      nsStyle##name* ptr, RawGeckoPresContextBorrowed pres_context);  \
  void Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,          \
                                         const nsStyle##name* other); \
  void Gecko_Destroy_nsStyle##name(nsStyle##name* ptr);
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

void Gecko_RegisterProfilerThread(const char* name);
void Gecko_UnregisterProfilerThread();

bool Gecko_DocumentRule_UseForPresentation(
    RawGeckoPresContextBorrowed, const nsACString* aPattern,
    mozilla::css::DocumentMatchingFunction);

// Allocator hinting.
void Gecko_SetJemallocThreadLocalArena(bool enabled);
void Gecko_AddBufferToCrashReport(const void* addr, size_t len);
void Gecko_AnnotateCrashReport(uint32_t key, const char* value_str);

// Pseudo-element flags.
#define CSS_PSEUDO_ELEMENT(name_, value_, flags_) \
  const uint32_t SERVO_CSS_PSEUDO_ELEMENT_FLAGS_##name_ = flags_;
#include "nsCSSPseudoElementList.h"
#undef CSS_PSEUDO_ELEMENT

bool Gecko_ErrorReportingEnabled(const mozilla::StyleSheet* sheet,
                                 const mozilla::css::Loader* loader);

void Gecko_ReportUnexpectedCSSError(const mozilla::StyleSheet* sheet,
                                    const mozilla::css::Loader* loader,
                                    nsIURI* uri, const char* message,
                                    const char* param, uint32_t paramLen,
                                    const char* prefix, const char* prefixParam,
                                    uint32_t prefixParamLen, const char* suffix,
                                    const char* source, uint32_t sourceLen,
                                    uint32_t lineNumber, uint32_t colNumber);

// DOM APIs.
void Gecko_ContentList_AppendAll(nsSimpleContentList* aContentList,
                                 const RawGeckoElement** aElements,
                                 size_t aLength);

// FIXME(emilio): These two below should be a single function that takes a
// `const DocumentOrShadowRoot*`, but that doesn't make MSVC builds happy for a
// reason I haven't really dug into.
const nsTArray<mozilla::dom::Element*>* Gecko_Document_GetElementsWithId(
    const nsIDocument* aDocument, nsAtom* aId);

const nsTArray<mozilla::dom::Element*>* Gecko_ShadowRoot_GetElementsWithId(
    const mozilla::dom::ShadowRoot* aDocument, nsAtom* aId);

// Check the value of the given bool preference. The pref name needs to
// be null-terminated.
bool Gecko_GetBoolPrefValue(const char* pref_name);

// Returns true if we're currently performing the servo traversal.
bool Gecko_IsInServoTraversal();

// Returns true if we're currently on the main thread.
bool Gecko_IsMainThread();

// Media feature helpers.
//
// Defined in nsMediaFeatures.cpp.
mozilla::StyleDisplayMode Gecko_MediaFeatures_GetDisplayMode(nsIDocument*);

uint32_t Gecko_MediaFeatures_GetColorDepth(nsIDocument*);

void Gecko_MediaFeatures_GetDeviceSize(nsIDocument*, nscoord* width,
                                       nscoord* height);

float Gecko_MediaFeatures_GetResolution(nsIDocument*);
bool Gecko_MediaFeatures_PrefersReducedMotion(nsIDocument*);

mozilla::PointerCapabilities Gecko_MediaFeatures_PrimaryPointerCapabilities(
    nsIDocument*);

mozilla::PointerCapabilities Gecko_MediaFeatures_AllPointerCapabilities(
    nsIDocument*);

float Gecko_MediaFeatures_GetDevicePixelRatio(nsIDocument*);

bool Gecko_MediaFeatures_HasSystemMetric(nsIDocument*, nsAtom* metric,
                                         bool is_accessible_from_content);

bool Gecko_MediaFeatures_IsResourceDocument(nsIDocument*);
nsAtom* Gecko_MediaFeatures_GetOperatingSystemVersion(nsIDocument*);

}  // extern "C"

#endif  // mozilla_GeckoBindings_h
