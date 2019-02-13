/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * structs that contain the data provided by nsStyleContext, the
 * internal API for computed style data for an element
 */

#ifndef nsStyleStruct_h___
#define nsStyleStruct_h___

#include "mozilla/Attributes.h"
#include "mozilla/CSSVariableValues.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsFont.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsChangeHint.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsCSSValue.h"
#include "imgRequestProxy.h"
#include "Orientation.h"
#include "CounterStyleManager.h"

class nsIFrame;
class nsTextFrame;
class nsIURI;
class imgIContainer;

// Includes nsStyleStructID.
#include "nsStyleStructFwd.h"

// Bits for each struct.
// NS_STYLE_INHERIT_BIT defined in nsStyleStructFwd.h
#define NS_STYLE_INHERIT_MASK              0x000ffffff

// Additional bits for nsStyleContext's mBits:
// See nsStyleContext::HasTextDecorationLines
#define NS_STYLE_HAS_TEXT_DECORATION_LINES 0x001000000
// See nsStyleContext::HasPseudoElementData.
#define NS_STYLE_HAS_PSEUDO_ELEMENT_DATA   0x002000000
// See nsStyleContext::RelevantLinkIsVisited
#define NS_STYLE_RELEVANT_LINK_VISITED     0x004000000
// See nsStyleContext::IsStyleIfVisited
#define NS_STYLE_IS_STYLE_IF_VISITED       0x008000000
// See nsStyleContext::UsesGrandancestorStyle
#define NS_STYLE_USES_GRANDANCESTOR_STYLE  0x010000000
// See nsStyleContext::IsShared
#define NS_STYLE_IS_SHARED                 0x020000000
// See nsStyleContext::AssertStructsNotUsedElsewhere
// (This bit is currently only used in #ifdef DEBUG code.)
#define NS_STYLE_IS_GOING_AWAY             0x040000000
// See nsStyleContext::ShouldSuppressLineBreak
#define NS_STYLE_SUPPRESS_LINEBREAK        0x080000000
// See nsStyleContext::IsInDisplayNoneSubtree
#define NS_STYLE_IN_DISPLAY_NONE_SUBTREE   0x100000000
// See nsStyleContext::FindChildWithRules
#define NS_STYLE_INELIGIBLE_FOR_SHARING    0x200000000
// See nsStyleContext::GetPseudoEnum
#define NS_STYLE_CONTEXT_TYPE_SHIFT        34

// Additional bits for nsRuleNode's mDependentBits:
#define NS_RULE_NODE_IS_ANIMATION_RULE      0x01000000
#define NS_RULE_NODE_GC_MARK                0x02000000
#define NS_RULE_NODE_USED_DIRECTLY          0x04000000
#define NS_RULE_NODE_IS_IMPORTANT           0x08000000
#define NS_RULE_NODE_LEVEL_MASK             0xf0000000
#define NS_RULE_NODE_LEVEL_SHIFT            28

// Additional bits for nsRuleNode's mNoneBits:
#define NS_RULE_NODE_HAS_ANIMATION_DATA     0x80000000

// The lifetime of these objects is managed by the presshell's arena.

struct nsStyleFont {
  nsStyleFont(const nsFont& aFont, nsPresContext *aPresContext);
  nsStyleFont(const nsStyleFont& aStyleFont);
  explicit nsStyleFont(nsPresContext *aPresContext);
private:
  void Init(nsPresContext *aPresContext);
public:
  ~nsStyleFont(void) {
    MOZ_COUNT_DTOR(nsStyleFont);
  }

  nsChangeHint CalcDifference(const nsStyleFont& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_STYLE_HINT_REFLOW,
                          nsChangeHint_NeutralChange);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }
  static nsChangeHint CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2);

  static nscoord ZoomText(nsPresContext* aPresContext, nscoord aSize);
  static nscoord UnZoomText(nsPresContext* aPresContext, nscoord aSize);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleFont_id, sz);
  }
  void Destroy(nsPresContext* aContext);

  void EnableZoom(nsPresContext* aContext, bool aEnable);

  nsFont  mFont;        // [inherited]
  nscoord mSize;        // [inherited] Our "computed size". Can be different
                        // from mFont.size which is our "actual size" and is
                        // enforced to be >= the user's preferred min-size.
                        // mFont.size should be used for display purposes
                        // while mSize is the value to return in
                        // getComputedStyle() for example.
  uint8_t mGenericID;   // [inherited] generic CSS font family, if any;
                        // value is a kGenericFont_* constant, see nsFont.h.

  // MathML scriptlevel support
  int8_t  mScriptLevel;          // [inherited]
  // MathML  mathvariant support
  uint8_t mMathVariant;          // [inherited]
  // MathML displaystyle support
  uint8_t mMathDisplay;         // [inherited]

  // was mLanguage set based on a lang attribute in the document?
  bool mExplicitLanguage;        // [inherited]

  // should calls to ZoomText() and UnZoomText() be made to the font
  // size on this nsStyleFont?
  bool mAllowZoom;               // [inherited]

  // The value mSize would have had if scriptminsize had never been applied
  nscoord mScriptUnconstrainedSize;
  nscoord mScriptMinSize;        // [inherited] length
  float   mScriptSizeMultiplier; // [inherited]
  nsCOMPtr<nsIAtom> mLanguage;   // [inherited]
};

struct nsStyleGradientStop {
  nsStyleCoord mLocation; // percent, coord, calc, none
  nscolor mColor;
  bool mIsInterpolationHint;

  // Use ==/!= on nsStyleGradient instead of on the gradient stop.
  bool operator==(const nsStyleGradientStop&) const = delete;
  bool operator!=(const nsStyleGradientStop&) const = delete;
};

class nsStyleGradient final {
public:
  nsStyleGradient();
  uint8_t mShape;  // NS_STYLE_GRADIENT_SHAPE_*
  uint8_t mSize;   // NS_STYLE_GRADIENT_SIZE_*;
                   // not used (must be FARTHEST_CORNER) for linear shape
  bool mRepeating;
  bool mLegacySyntax;

  nsStyleCoord mBgPosX; // percent, coord, calc, none
  nsStyleCoord mBgPosY; // percent, coord, calc, none
  nsStyleCoord mAngle;  // none, angle

  nsStyleCoord mRadiusX; // percent, coord, calc, none
  nsStyleCoord mRadiusY; // percent, coord, calc, none

  // stops are in the order specified in the stylesheet
  nsTArray<nsStyleGradientStop> mStops;

  bool operator==(const nsStyleGradient& aOther) const;
  bool operator!=(const nsStyleGradient& aOther) const {
    return !(*this == aOther);
  }

  bool IsOpaque();
  bool HasCalc();
  uint32_t Hash(PLDHashNumber aHash);

  NS_INLINE_DECL_REFCOUNTING(nsStyleGradient)

private:
  // Private destructor, to discourage deletion outside of Release():
  ~nsStyleGradient() {}

  nsStyleGradient(const nsStyleGradient& aOther) = delete;
  nsStyleGradient& operator=(const nsStyleGradient& aOther) = delete;
};

enum nsStyleImageType {
  eStyleImageType_Null,
  eStyleImageType_Image,
  eStyleImageType_Gradient,
  eStyleImageType_Element
};

/**
 * Represents a paintable image of one of the following types.
 * (1) A real image loaded from an external source.
 * (2) A CSS linear or radial gradient.
 * (3) An element within a document, or an <img>, <video>, or <canvas> element
 *     not in a document.
 * (*) Optionally a crop rect can be set to paint a partial (rectangular)
 * region of an image. (Currently, this feature is only supported with an
 * image of type (1)).
 */
struct nsStyleImage {
  nsStyleImage();
  ~nsStyleImage();
  nsStyleImage(const nsStyleImage& aOther);
  nsStyleImage& operator=(const nsStyleImage& aOther);

  void SetNull();
  void SetImageData(imgRequestProxy* aImage);
  void TrackImage(nsPresContext* aContext);
  void UntrackImage(nsPresContext* aContext);
  void SetGradientData(nsStyleGradient* aGradient);
  void SetElementId(const char16_t* aElementId);
  void SetCropRect(nsStyleSides* aCropRect);

  nsStyleImageType GetType() const {
    return mType;
  }
  imgRequestProxy* GetImageData() const {
    MOZ_ASSERT(mType == eStyleImageType_Image, "Data is not an image!");
    MOZ_ASSERT(mImageTracked,
               "Should be tracking any image we're going to use!");
    return mImage;
  }
  nsStyleGradient* GetGradientData() const {
    NS_ASSERTION(mType == eStyleImageType_Gradient, "Data is not a gradient!");
    return mGradient;
  }
  const char16_t* GetElementId() const {
    NS_ASSERTION(mType == eStyleImageType_Element, "Data is not an element!");
    return mElementId;
  }
  nsStyleSides* GetCropRect() const {
    NS_ASSERTION(mType == eStyleImageType_Image,
                 "Only image data can have a crop rect");
    return mCropRect;
  }

  /**
   * Compute the actual crop rect in pixels, using the source image bounds.
   * The computation involves converting percentage unit to pixel unit and
   * clamping each side value to fit in the source image bounds.
   * @param aActualCropRect the computed actual crop rect.
   * @param aIsEntireImage true iff |aActualCropRect| is identical to the
   * source image bounds.
   * @return true iff |aActualCropRect| holds a meaningful value.
   */
  bool ComputeActualCropRect(nsIntRect& aActualCropRect,
                               bool* aIsEntireImage = nullptr) const;

  /**
   * Starts the decoding of a image.
   */
  nsresult StartDecoding() const;
  /**
   * @return true if the item is definitely opaque --- i.e., paints every
   * pixel within its bounds opaquely, and the bounds contains at least a pixel.
   */
  bool IsOpaque() const;
  /**
   * @return true if this image is fully loaded, and its size is calculated;
   * always returns true if |mType| is |eStyleImageType_Gradient| or
   * |eStyleImageType_Element|.
   */
  bool IsComplete() const;
  /**
   * @return true if this image is loaded without error;
   * always returns true if |mType| is |eStyleImageType_Gradient| or
   * |eStyleImageType_Element|.
   */
  bool IsLoaded() const;
  /**
   * @return true if it is 100% confident that this image contains no pixel
   * to draw.
   */
  bool IsEmpty() const {
    // There are some other cases when the image will be empty, for example
    // when the crop rect is empty. However, checking the emptiness of crop
    // rect is non-trivial since each side value can be specified with
    // percentage unit, which can not be evaluated until the source image size
    // is available. Therefore, we currently postpone the evaluation of crop
    // rect until the actual rendering time --- alternatively until GetOpaqueRegion()
    // is called.
    return mType == eStyleImageType_Null;
  }

  bool operator==(const nsStyleImage& aOther) const;
  bool operator!=(const nsStyleImage& aOther) const {
    return !(*this == aOther);
  }

  bool ImageDataEquals(const nsStyleImage& aOther) const
  {
    return GetType() == eStyleImageType_Image &&
           aOther.GetType() == eStyleImageType_Image &&
           GetImageData() == aOther.GetImageData();
  }

  // These methods are used for the caller to caches the sub images created
  // during a border-image paint operation
  inline void SetSubImage(uint8_t aIndex, imgIContainer* aSubImage) const;
  inline imgIContainer* GetSubImage(uint8_t aIndex) const;

private:
  void DoCopy(const nsStyleImage& aOther);

  // Cache for border-image painting.
  nsCOMArray<imgIContainer> mSubImages;

  nsStyleImageType mType;
  union {
    imgRequestProxy* mImage;
    nsStyleGradient* mGradient;
    char16_t* mElementId;
  };
  // This is _currently_ used only in conjunction with eStyleImageType_Image.
  nsAutoPtr<nsStyleSides> mCropRect;
#ifdef DEBUG
  bool mImageTracked;
#endif
};

struct nsStyleColor {
  explicit nsStyleColor(nsPresContext* aPresContext);
  nsStyleColor(const nsStyleColor& aOther);
  ~nsStyleColor(void) {
    MOZ_COUNT_DTOR(nsStyleColor);
  }

  nsChangeHint CalcDifference(const nsStyleColor& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_VISUAL;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants at all.
    return nsChangeHint(0);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleColor_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleColor();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleColor_id, this);
  }

  // Don't add ANY members to this struct!  We can achieve caching in the rule
  // tree (rather than the style tree) by letting color stay by itself! -dwh
  nscolor mColor;                 // [inherited]
};

struct nsStyleBackground {
  nsStyleBackground();
  nsStyleBackground(const nsStyleBackground& aOther);
  ~nsStyleBackground();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleBackground_id, sz);
  }
  void Destroy(nsPresContext* aContext);

  nsChangeHint CalcDifference(const nsStyleBackground& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(nsChangeHint_UpdateEffects,
                          NS_CombineHint(NS_STYLE_HINT_VISUAL,
                                         nsChangeHint_NeutralChange));
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants at all.
    return nsChangeHint(0);
  }

  struct Position;
  friend struct Position;
  struct Position {
    typedef nsStyleCoord::CalcValue PositionCoord;
    PositionCoord mXPosition, mYPosition;

    // Initialize nothing
    Position() {}

    // Sets both mXPosition and mYPosition to the given percent value for the
    // initial property-value (e.g. 0.0f for "0% 0%", or 0.5f for "50% 50%")
    void SetInitialPercentValues(float aPercentVal);

    // Sets both mXPosition and mYPosition to 0 (app units) for the
    // initial property-value as a length with no percentage component.
    void SetInitialZeroValues();

    // True if the effective background image position described by this depends
    // on the size of the corresponding frame.
    bool DependsOnPositioningAreaSize() const {
      return mXPosition.mPercent != 0.0f || mYPosition.mPercent != 0.0f;
    }

    bool operator==(const Position& aOther) const {
      return mXPosition == aOther.mXPosition &&
             mYPosition == aOther.mYPosition;
    }
    bool operator!=(const Position& aOther) const {
      return !(*this == aOther);
    }
  };

  struct Size;
  friend struct Size;
  struct Size {
    struct Dimension : public nsStyleCoord::CalcValue {
      nscoord ResolveLengthPercentage(nscoord aAvailable) const {
        double d = double(mPercent) * double(aAvailable) + double(mLength);
        if (d < 0.0)
          return 0;
        return NSToCoordRoundWithClamp(float(d));
      }
    };
    Dimension mWidth, mHeight;

    nscoord ResolveWidthLengthPercentage(const nsSize& aBgPositioningArea) const {
      MOZ_ASSERT(mWidthType == eLengthPercentage,
                 "resolving non-length/percent dimension!");
      return mWidth.ResolveLengthPercentage(aBgPositioningArea.width);
    }

    nscoord ResolveHeightLengthPercentage(const nsSize& aBgPositioningArea) const {
      MOZ_ASSERT(mHeightType == eLengthPercentage,
                 "resolving non-length/percent dimension!");
      return mHeight.ResolveLengthPercentage(aBgPositioningArea.height);
    }

    // Except for eLengthPercentage, Dimension types which might change
    // how a layer is painted when the corresponding frame's dimensions
    // change *must* precede all dimension types which are agnostic to
    // frame size; see DependsOnDependsOnPositioningAreaSizeSize.
    enum DimensionType {
      // If one of mWidth and mHeight is eContain or eCover, then both are.
      // NOTE: eContain and eCover *must* be equal to NS_STYLE_BG_SIZE_CONTAIN
      // and NS_STYLE_BG_SIZE_COVER (in kBackgroundSizeKTable).
      eContain, eCover,

      eAuto,
      eLengthPercentage,
      eDimensionType_COUNT
    };
    uint8_t mWidthType, mHeightType;

    // True if the effective image size described by this depends on the size of
    // the corresponding frame, when aImage (which must not have null type) is
    // the background image.
    bool DependsOnPositioningAreaSize(const nsStyleImage& aImage) const;

    // Initialize nothing
    Size() {}

    // Initialize to initial values
    void SetInitialValues();

    bool operator==(const Size& aOther) const;
    bool operator!=(const Size& aOther) const {
      return !(*this == aOther);
    }
  };

  struct Repeat;
  friend struct Repeat;
  struct Repeat {
    uint8_t mXRepeat, mYRepeat;

    // Initialize nothing
    Repeat() {}

    // Initialize to initial values
    void SetInitialValues();

    bool operator==(const Repeat& aOther) const {
      return mXRepeat == aOther.mXRepeat &&
             mYRepeat == aOther.mYRepeat;
    }
    bool operator!=(const Repeat& aOther) const {
      return !(*this == aOther);
    }
  };

  struct Layer;
  friend struct Layer;
  struct Layer {
    uint8_t mAttachment;                // [reset] See nsStyleConsts.h
    uint8_t mClip;                      // [reset] See nsStyleConsts.h
    uint8_t mOrigin;                    // [reset] See nsStyleConsts.h
    uint8_t mBlendMode;                 // [reset] See nsStyleConsts.h
    Repeat mRepeat;                     // [reset] See nsStyleConsts.h
    Position mPosition;                 // [reset]
    nsStyleImage mImage;                // [reset]
    Size mSize;                         // [reset]

    // Initializes only mImage
    Layer();
    ~Layer();

    // Register/unregister images with the document. We do this only
    // after the dust has settled in ComputeBackgroundData.
    void TrackImages(nsPresContext* aContext) {
      if (mImage.GetType() == eStyleImageType_Image)
        mImage.TrackImage(aContext);
    }
    void UntrackImages(nsPresContext* aContext) {
      if (mImage.GetType() == eStyleImageType_Image)
        mImage.UntrackImage(aContext);
    }

    void SetInitialValues();

    // True if the rendering of this layer might change when the size
    // of the background positioning area changes.  This is true for any
    // non-solid-color background whose position or size depends on
    // the size of the positioning area.  It's also true for SVG images
    // whose root <svg> node has a viewBox.
    bool RenderingMightDependOnPositioningAreaSizeChange() const;

    // An equality operator that compares the images using URL-equality
    // rather than pointer-equality.
    bool operator==(const Layer& aOther) const;
    bool operator!=(const Layer& aOther) const {
      return !(*this == aOther);
    }
  };

  // The (positive) number of computed values of each property, since
  // the lengths of the lists are independent.
  uint32_t mAttachmentCount,
           mClipCount,
           mOriginCount,
           mRepeatCount,
           mPositionCount,
           mImageCount,
           mSizeCount,
           mBlendModeCount;
  // Layers are stored in an array, matching the top-to-bottom order in
  // which they are specified in CSS.  The number of layers to be used
  // should come from the background-image property.  We create
  // additional |Layer| objects for *any* property, not just
  // background-image.  This means that the bottommost layer that
  // callers in layout care about (which is also the one whose
  // background-clip applies to the background-color) may not be last
  // layer.  In layers below the bottom layer, properties will be
  // uninitialized unless their count, above, indicates that they are
  // present.
  nsAutoTArray<Layer, 1> mLayers;

  const Layer& BottomLayer() const { return mLayers[mImageCount - 1]; }

  #define NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(var_, stylebg_) \
    for (uint32_t var_ = (stylebg_) ? (stylebg_)->mImageCount : 1; var_-- != 0; )
  #define NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT_WITH_RANGE(var_, stylebg_, start_, count_) \
    NS_ASSERTION((int32_t)(start_) >= 0 && (uint32_t)(start_) < ((stylebg_) ? (stylebg_)->mImageCount : 1), "Invalid layer start!"); \
    NS_ASSERTION((count_) > 0 && (count_) <= (start_) + 1, "Invalid layer range!"); \
    for (uint32_t var_ = (start_) + 1; var_-- != (uint32_t)((start_) + 1 - (count_)); )

  nscolor mBackgroundColor;       // [reset]

  // True if this background is completely transparent.
  bool IsTransparent() const;

  // We have to take slower codepaths for fixed background attachment,
  // but we don't want to do that when there's no image.
  // Not inline because it uses an nsCOMPtr<imgIRequest>
  // FIXME: Should be in nsStyleStructInlines.h.
  bool HasFixedBackground() const;
};

// See https://bugzilla.mozilla.org/show_bug.cgi?id=271586#c43 for why
// this is hard to replace with 'currentColor'.
#define BORDER_COLOR_FOREGROUND   0x20
#define OUTLINE_COLOR_INITIAL     0x80
// FOREGROUND | INITIAL(OUTLINE)
#define BORDER_COLOR_SPECIAL      0xA0
#define BORDER_STYLE_MASK         0x1F

#define NS_SPACING_MARGIN   0
#define NS_SPACING_PADDING  1
#define NS_SPACING_BORDER   2


struct nsStyleMargin {
  nsStyleMargin(void);
  nsStyleMargin(const nsStyleMargin& aMargin);
  ~nsStyleMargin(void) {
    MOZ_COUNT_DTOR(nsStyleMargin);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleMargin_id, sz);
  }
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStyleMargin& aOther) const;
  static nsChangeHint MaxDifference() {
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference can return all of the reflow hints sometimes not
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint(0);
  }

  nsStyleSides  mMargin;          // [reset] coord, percent, calc, auto

  bool IsWidthDependent() const { return !mHasCachedMargin; }
  bool GetMargin(nsMargin& aMargin) const
  {
    if (mHasCachedMargin) {
      aMargin = mCachedMargin;
      return true;
    }
    return false;
  }

protected:
  bool          mHasCachedMargin;
  nsMargin      mCachedMargin;
};


struct nsStylePadding {
  nsStylePadding(void);
  nsStylePadding(const nsStylePadding& aPadding);
  ~nsStylePadding(void) {
    MOZ_COUNT_DTOR(nsStylePadding);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStylePadding_id, sz);
  }
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStylePadding& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_SubtractHint(NS_STYLE_HINT_REFLOW,
                           nsChangeHint_ClearDescendantIntrinsics);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference can return nsChangeHint_ClearAncestorIntrinsics as
    // a hint not handled for descendants.  We could (and perhaps
    // should) return nsChangeHint_NeedReflow and
    // nsChangeHint_ReflowChangesSizeOrPosition as always handled for
    // descendants, but since they're always returned in conjunction
    // with nsChangeHint_ClearAncestorIntrinsics (which is not), it
    // won't ever lead to any optimization in
    // nsStyleContext::CalcStyleDifference.
    return nsChangeHint(0);
  }

  nsStyleSides  mPadding;         // [reset] coord, percent, calc

  bool IsWidthDependent() const { return !mHasCachedPadding; }
  bool GetPadding(nsMargin& aPadding) const
  {
    if (mHasCachedPadding) {
      aPadding = mCachedPadding;
      return true;
    }
    return false;
  }

protected:
  bool          mHasCachedPadding;
  nsMargin      mCachedPadding;
};

struct nsBorderColors {
  nsBorderColors* mNext;
  nscolor mColor;

  nsBorderColors() : mNext(nullptr), mColor(NS_RGB(0,0,0)) {}
  explicit nsBorderColors(const nscolor& aColor) : mNext(nullptr), mColor(aColor) {}
  ~nsBorderColors();

  nsBorderColors* Clone() const { return Clone(true); }

  static bool Equal(const nsBorderColors* c1,
                      const nsBorderColors* c2) {
    if (c1 == c2)
      return true;
    while (c1 && c2) {
      if (c1->mColor != c2->mColor)
        return false;
      c1 = c1->mNext;
      c2 = c2->mNext;
    }
    // both should be nullptr if these are equal, otherwise one
    // has more colors than another
    return !c1 && !c2;
  }

private:
  nsBorderColors* Clone(bool aDeep) const;
};

struct nsCSSShadowItem {
  nscoord mXOffset;
  nscoord mYOffset;
  nscoord mRadius;
  nscoord mSpread;

  nscolor      mColor;
  bool mHasColor; // Whether mColor should be used
  bool mInset;

  nsCSSShadowItem() : mHasColor(false) {
    MOZ_COUNT_CTOR(nsCSSShadowItem);
  }
  ~nsCSSShadowItem() {
    MOZ_COUNT_DTOR(nsCSSShadowItem);
  }

  bool operator==(const nsCSSShadowItem& aOther) const {
    return (mXOffset == aOther.mXOffset &&
            mYOffset == aOther.mYOffset &&
            mRadius == aOther.mRadius &&
            mHasColor == aOther.mHasColor &&
            mSpread == aOther.mSpread &&
            mInset == aOther.mInset &&
            (!mHasColor || mColor == aOther.mColor));
  }
  bool operator!=(const nsCSSShadowItem& aOther) const {
    return !(*this == aOther);
  }
};

class nsCSSShadowArray final {
  public:
    void* operator new(size_t aBaseSize, uint32_t aArrayLen) {
      // We can allocate both this nsCSSShadowArray and the
      // actual array in one allocation. The amount of memory to
      // allocate is equal to the class's size + the number of bytes for all
      // but the first array item (because aBaseSize includes one
      // item, see the private declarations)
      return ::operator new(aBaseSize +
                            (aArrayLen - 1) * sizeof(nsCSSShadowItem));
    }

    explicit nsCSSShadowArray(uint32_t aArrayLen) :
      mLength(aArrayLen)
    {
      MOZ_COUNT_CTOR(nsCSSShadowArray);
      for (uint32_t i = 1; i < mLength; ++i) {
        // Make sure we call the constructors of each nsCSSShadowItem
        // (the first one is called for us because we declared it under private)
        new (&mArray[i]) nsCSSShadowItem();
      }
    }

private:
    // Private destructor, to discourage deletion outside of Release():
    ~nsCSSShadowArray() {
      MOZ_COUNT_DTOR(nsCSSShadowArray);
      for (uint32_t i = 1; i < mLength; ++i) {
        mArray[i].~nsCSSShadowItem();
      }
    }

public:
    uint32_t Length() const { return mLength; }
    nsCSSShadowItem* ShadowAt(uint32_t i) {
      MOZ_ASSERT(i < mLength, "Accessing too high an index in the text shadow array!");
      return &mArray[i];
    }
    const nsCSSShadowItem* ShadowAt(uint32_t i) const {
      MOZ_ASSERT(i < mLength, "Accessing too high an index in the text shadow array!");
      return &mArray[i];
    }

    bool HasShadowWithInset(bool aInset) {
      for (uint32_t i = 0; i < mLength; ++i) {
        if (mArray[i].mInset == aInset)
          return true;
      }
      return false;
    }

    bool operator==(const nsCSSShadowArray& aOther) const {
      if (mLength != aOther.Length())
        return false;

      for (uint32_t i = 0; i < mLength; ++i) {
        if (ShadowAt(i) != aOther.ShadowAt(i))
          return false;
      }

      return true;
    }

    NS_INLINE_DECL_REFCOUNTING(nsCSSShadowArray)

  private:
    uint32_t mLength;
    nsCSSShadowItem mArray[1]; // This MUST be the last item
};

// Border widths are rounded to the nearest-below integer number of pixels,
// but values between zero and one device pixels are always rounded up to
// one device pixel.
#define NS_ROUND_BORDER_TO_PIXELS(l,tpp) \
  ((l) == 0) ? 0 : std::max((tpp), (l) / (tpp) * (tpp))
// Outline offset is rounded to the nearest integer number of pixels, but values
// between zero and one device pixels are always rounded up to one device pixel.
// Note that the offset can be negative.
#define NS_ROUND_OFFSET_TO_PIXELS(l,tpp) \
  (((l) == 0) ? 0 : \
    ((l) > 0) ? std::max( (tpp), ((l) + ((tpp) / 2)) / (tpp) * (tpp)) : \
                std::min(-(tpp), ((l) - ((tpp) / 2)) / (tpp) * (tpp)))

// Returns if the given border style type is visible or not
static bool IsVisibleBorderStyle(uint8_t aStyle)
{
  return (aStyle != NS_STYLE_BORDER_STYLE_NONE &&
          aStyle != NS_STYLE_BORDER_STYLE_HIDDEN);
}

struct nsStyleBorder {
  explicit nsStyleBorder(nsPresContext* aContext);
  nsStyleBorder(const nsStyleBorder& aBorder);
  ~nsStyleBorder();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleBorder_id, sz);
  }
  void Destroy(nsPresContext* aContext);

  nsChangeHint CalcDifference(const nsStyleBorder& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_STYLE_HINT_REFLOW,
                          NS_CombineHint(nsChangeHint_BorderStyleNoneChange,
                                         nsChangeHint_NeutralChange));
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  void EnsureBorderColors() {
    if (!mBorderColors) {
      mBorderColors = new nsBorderColors*[4];
      if (mBorderColors)
        for (int32_t i = 0; i < 4; i++)
          mBorderColors[i] = nullptr;
    }
  }

  void ClearBorderColors(mozilla::css::Side aSide) {
    if (mBorderColors && mBorderColors[aSide]) {
      delete mBorderColors[aSide];
      mBorderColors[aSide] = nullptr;
    }
  }

  // Return whether aStyle is a visible style.  Invisible styles cause
  // the relevant computed border width to be 0.
  // Note that this does *not* consider the effects of 'border-image':
  // if border-style is none, but there is a loaded border image,
  // HasVisibleStyle will be false even though there *is* a border.
  bool HasVisibleStyle(mozilla::css::Side aSide) const
  {
    return IsVisibleBorderStyle(GetBorderStyle(aSide));
  }

  // aBorderWidth is in twips
  void SetBorderWidth(mozilla::css::Side aSide, nscoord aBorderWidth)
  {
    nscoord roundedWidth =
      NS_ROUND_BORDER_TO_PIXELS(aBorderWidth, mTwipsPerPixel);
    mBorder.Side(aSide) = roundedWidth;
    if (HasVisibleStyle(aSide))
      mComputedBorder.Side(aSide) = roundedWidth;
  }

  // Get the computed border (plus rounding).  This does consider the
  // effects of 'border-style: none', but does not consider
  // 'border-image'.
  const nsMargin& GetComputedBorder() const
  {
    return mComputedBorder;
  }

  bool HasBorder() const
  {
    return mComputedBorder != nsMargin(0,0,0,0) || !mBorderImageSource.IsEmpty();
  }

  // Get the actual border width for a particular side, in appunits.  Note that
  // this is zero if and only if there is no border to be painted for this
  // side.  That is, this value takes into account the border style and the
  // value is rounded to the nearest device pixel by NS_ROUND_BORDER_TO_PIXELS.
  nscoord GetComputedBorderWidth(mozilla::css::Side aSide) const
  {
    return GetComputedBorder().Side(aSide);
  }

  uint8_t GetBorderStyle(mozilla::css::Side aSide) const
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    return (mBorderStyle[aSide] & BORDER_STYLE_MASK);
  }

  void SetBorderStyle(mozilla::css::Side aSide, uint8_t aStyle)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    mBorderStyle[aSide] &= ~BORDER_STYLE_MASK;
    mBorderStyle[aSide] |= (aStyle & BORDER_STYLE_MASK);
    mComputedBorder.Side(aSide) =
      (HasVisibleStyle(aSide) ? mBorder.Side(aSide) : 0);
  }

  inline bool IsBorderImageLoaded() const
  {
    return mBorderImageSource.IsLoaded();
  }

  // Defined in nsStyleStructInlines.h
  inline nsresult RequestDecode();

  void GetBorderColor(mozilla::css::Side aSide, nscolor& aColor,
                      bool& aForeground) const
  {
    aForeground = false;
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    if ((mBorderStyle[aSide] & BORDER_COLOR_SPECIAL) == 0)
      aColor = mBorderColor[aSide];
    else if (mBorderStyle[aSide] & BORDER_COLOR_FOREGROUND)
      aForeground = true;
    else
      NS_NOTREACHED("OUTLINE_COLOR_INITIAL should not be set here");
  }

  void SetBorderColor(mozilla::css::Side aSide, nscolor aColor)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    mBorderColor[aSide] = aColor;
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
  }

  void TrackImage(nsPresContext* aContext)
  {
    if (mBorderImageSource.GetType() == eStyleImageType_Image) {
      mBorderImageSource.TrackImage(aContext);
    }
  }
  void UntrackImage(nsPresContext* aContext)
  {
    if (mBorderImageSource.GetType() == eStyleImageType_Image) {
      mBorderImageSource.UntrackImage(aContext);
    }
  }

  nsMargin GetImageOutset() const;

  void GetCompositeColors(int32_t aIndex, nsBorderColors** aColors) const
  {
    if (!mBorderColors)
      *aColors = nullptr;
    else
      *aColors = mBorderColors[aIndex];
  }

  void AppendBorderColor(int32_t aIndex, nscolor aColor)
  {
    NS_ASSERTION(aIndex >= 0 && aIndex <= 3, "bad side for composite border color");
    nsBorderColors* colorEntry = new nsBorderColors(aColor);
    if (!mBorderColors[aIndex])
      mBorderColors[aIndex] = colorEntry;
    else {
      nsBorderColors* last = mBorderColors[aIndex];
      while (last->mNext)
        last = last->mNext;
      last->mNext = colorEntry;
    }
    mBorderStyle[aIndex] &= ~BORDER_COLOR_SPECIAL;
  }

  void SetBorderToForeground(mozilla::css::Side aSide)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
    mBorderStyle[aSide] |= BORDER_COLOR_FOREGROUND;
  }

  imgIRequest* GetBorderImageRequest() const
  {
    if (mBorderImageSource.GetType() == eStyleImageType_Image) {
      return mBorderImageSource.GetImageData();
    }
    return nullptr;
  }

public:
  nsBorderColors** mBorderColors;        // [reset] composite (stripe) colors
  nsRefPtr<nsCSSShadowArray> mBoxShadow; // [reset] nullptr for 'none'

public:
  nsStyleCorners mBorderRadius;       // [reset] coord, percent
  nsStyleImage   mBorderImageSource;  // [reset]
  nsStyleSides   mBorderImageSlice;   // [reset] factor, percent
  nsStyleSides   mBorderImageWidth;   // [reset] length, factor, percent, auto
  nsStyleSides   mBorderImageOutset;  // [reset] length, factor

  uint8_t        mBorderImageFill;    // [reset]
  uint8_t        mBorderImageRepeatH; // [reset] see nsStyleConsts.h
  uint8_t        mBorderImageRepeatV; // [reset]
  uint8_t        mFloatEdge;          // [reset]
  uint8_t        mBoxDecorationBreak; // [reset] see nsStyleConsts.h

protected:
  // mComputedBorder holds the CSS2.1 computed border-width values.
  // In particular, these widths take into account the border-style
  // for the relevant side, and the values are rounded to the nearest
  // device pixel (which is not part of the definition of computed
  // values). The presence or absence of a border-image does not
  // affect border-width values.
  nsMargin      mComputedBorder;

  // mBorder holds the nscoord values for the border widths as they
  // would be if all the border-style values were visible (not hidden
  // or none).  This member exists so that when we create structs
  // using the copy constructor during style resolution the new
  // structs will know what the specified values of the border were in
  // case they have more specific rules setting the border style.
  //
  // Note that this isn't quite the CSS specified value, since this
  // has had the enumerated border widths converted to lengths, and
  // all lengths converted to twips.  But it's not quite the computed
  // value either. The values are rounded to the nearest device pixel.
  nsMargin      mBorder;

  uint8_t       mBorderStyle[4];  // [reset] See nsStyleConsts.h
  nscolor       mBorderColor[4];  // [reset] the colors to use for a simple
                                  // border.  not used for -moz-border-colors
private:
  nscoord       mTwipsPerPixel;

  nsStyleBorder& operator=(const nsStyleBorder& aOther) = delete;
};


struct nsStyleOutline {
  explicit nsStyleOutline(nsPresContext* aPresContext);
  nsStyleOutline(const nsStyleOutline& aOutline);
  ~nsStyleOutline(void) {
    MOZ_COUNT_DTOR(nsStyleOutline);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleOutline_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleOutline();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleOutline_id, this);
  }

  void RecalcData(nsPresContext* aContext);
  nsChangeHint CalcDifference(const nsStyleOutline& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_CombineHint(nsChangeHint_UpdateOverflow,
                                         nsChangeHint_SchedulePaint),
                          NS_CombineHint(nsChangeHint_RepaintFrame,
                                         nsChangeHint_NeutralChange));
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants at all.
    return nsChangeHint(0);
  }

  nsStyleCorners  mOutlineRadius; // [reset] coord, percent, calc

  // Note that this is a specified value.  You can get the actual values
  // with GetOutlineWidth.  You cannot get the computed value directly.
  nsStyleCoord  mOutlineWidth;    // [reset] coord, enum (see nsStyleConsts.h)
  nscoord       mOutlineOffset;   // [reset]

  bool GetOutlineWidth(nscoord& aWidth) const
  {
    if (mHasCachedOutline) {
      aWidth = mCachedOutlineWidth;
      return true;
    }
    return false;
  }

  uint8_t GetOutlineStyle(void) const
  {
    return (mOutlineStyle & BORDER_STYLE_MASK);
  }

  void SetOutlineStyle(uint8_t aStyle)
  {
    mOutlineStyle &= ~BORDER_STYLE_MASK;
    mOutlineStyle |= (aStyle & BORDER_STYLE_MASK);
  }

  // false means initial value
  bool GetOutlineColor(nscolor& aColor) const
  {
    if ((mOutlineStyle & BORDER_COLOR_SPECIAL) == 0) {
      aColor = mOutlineColor;
      return true;
    }
    return false;
  }

  void SetOutlineColor(nscolor aColor)
  {
    mOutlineColor = aColor;
    mOutlineStyle &= ~BORDER_COLOR_SPECIAL;
  }

  void SetOutlineInitialColor()
  {
    mOutlineStyle |= OUTLINE_COLOR_INITIAL;
  }

  bool GetOutlineInitialColor() const
  {
    return !!(mOutlineStyle & OUTLINE_COLOR_INITIAL);
  }

protected:
  // This value is the actual value, so it's rounded to the nearest device
  // pixel.
  nscoord       mCachedOutlineWidth;

  nscolor       mOutlineColor;    // [reset]

  bool          mHasCachedOutline;
  uint8_t       mOutlineStyle;    // [reset] See nsStyleConsts.h

  nscoord       mTwipsPerPixel;
};


struct nsStyleList {
  explicit nsStyleList(nsPresContext* aPresContext);
  nsStyleList(const nsStyleList& aStyleList);
  ~nsStyleList(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleList_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleList();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleList_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleList& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_STYLE_HINT_FRAMECHANGE,
                          nsChangeHint_NeutralChange);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  imgRequestProxy* GetListStyleImage() const { return mListStyleImage; }
  void SetListStyleImage(imgRequestProxy* aReq)
  {
    if (mListStyleImage)
      mListStyleImage->UnlockImage();
    mListStyleImage = aReq;
    if (mListStyleImage)
      mListStyleImage->LockImage();
  }

  void GetListStyleType(nsSubstring& aType) const { aType = mListStyleType; }
  mozilla::CounterStyle* GetCounterStyle() const
  {
    return mCounterStyle.get();
  }
  void SetListStyleType(const nsSubstring& aType,
                        mozilla::CounterStyle* aStyle)
  {
    mListStyleType = aType;
    mCounterStyle = aStyle;
  }
  void SetListStyleType(const nsSubstring& aType,
                        nsPresContext* aPresContext)
  {
    SetListStyleType(aType, aPresContext->
                     CounterStyleManager()->BuildCounterStyle(aType));
  }

  uint8_t   mListStylePosition;         // [inherited]
private:
  nsString  mListStyleType;             // [inherited]
  nsRefPtr<mozilla::CounterStyle> mCounterStyle; // [inherited]
  nsRefPtr<imgRequestProxy> mListStyleImage; // [inherited]
  nsStyleList& operator=(const nsStyleList& aOther) = delete;
public:
  nsRect        mImageRegion;           // [inherited] the rect to use within an image
};

// Computed value of the grid-template-columns or grid-columns-rows property
// (but *not* grid-template-areas.)
// http://dev.w3.org/csswg/css-grid/#track-sizing
//
// This represents either:
// * none:
//   mIsSubgrid is false, all three arrays are empty
// * <track-list>:
//   mIsSubgrid is false,
//   mMinTrackSizingFunctions and mMaxTrackSizingFunctions
//   are of identical non-zero size,
//   and mLineNameLists is one element longer than that.
//   (Delimiting N columns requires N+1 lines:
//   one before each track, plus one at the very end.)
//
//   An omitted <line-names> is still represented in mLineNameLists,
//   as an empty sub-array.
//
//   A <track-size> specified as a single <track-breadth> is represented
//   as identical min and max sizing functions.
//
//   The units for nsStyleCoord are:
//   * eStyleUnit_Percent represents a <percentage>
//   * eStyleUnit_FlexFraction represents a <flex> flexible fraction
//   * eStyleUnit_Coord represents a <length>
//   * eStyleUnit_Enumerated represents min-content or max-content
// * subgrid <line-name-list>?:
//   mIsSubgrid is true,
//   mLineNameLists may or may not be empty,
//   mMinTrackSizingFunctions and mMaxTrackSizingFunctions are empty.
struct nsStyleGridTemplate {
  bool mIsSubgrid;
  nsTArray<nsTArray<nsString>> mLineNameLists;
  nsTArray<nsStyleCoord> mMinTrackSizingFunctions;
  nsTArray<nsStyleCoord> mMaxTrackSizingFunctions;

  nsStyleGridTemplate()
    : mIsSubgrid(false)
  {
  }

  inline bool operator!=(const nsStyleGridTemplate& aOther) const {
    return mLineNameLists != aOther.mLineNameLists ||
           mMinTrackSizingFunctions != aOther.mMinTrackSizingFunctions ||
           mMaxTrackSizingFunctions != aOther.mMaxTrackSizingFunctions;
  }
};

struct nsStyleGridLine {
  // http://dev.w3.org/csswg/css-grid/#typedef-grid-line
  // XXXmats we could optimize memory size here
  bool mHasSpan;
  int32_t mInteger;  // 0 means not provided
  nsString mLineName;  // Empty string means not provided.

  // mInteger is clamped to this range:
  static const int32_t kMinLine;
  static const int32_t kMaxLine;

  nsStyleGridLine()
    : mHasSpan(false)
    , mInteger(0)
    // mLineName get its default constructor, the empty string
  {
  }

  nsStyleGridLine(const nsStyleGridLine& aOther)
  {
    (*this) = aOther;
  }

  void operator=(const nsStyleGridLine& aOther)
  {
    mHasSpan = aOther.mHasSpan;
    mInteger = aOther.mInteger;
    mLineName = aOther.mLineName;
  }

  bool operator!=(const nsStyleGridLine& aOther) const
  {
    return mHasSpan != aOther.mHasSpan ||
           mInteger != aOther.mInteger ||
           mLineName != aOther.mLineName;
  }

  void SetToInteger(uint32_t value)
  {
    mHasSpan = false;
    mInteger = value;
    mLineName.Truncate();
  }

  void SetAuto()
  {
    mHasSpan = false;
    mInteger = 0;
    mLineName.Truncate();
  }

  bool IsAuto() const
  {
    bool haveInitialValues =  mInteger == 0 && mLineName.IsEmpty();
    MOZ_ASSERT(!(haveInitialValues && mHasSpan),
               "should not have 'span' when other components are "
               "at their initial values");
    return haveInitialValues;
  }
};

struct nsStylePosition {
  nsStylePosition(void);
  nsStylePosition(const nsStylePosition& aOther);
  ~nsStylePosition(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStylePosition_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStylePosition();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStylePosition_id, this);
  }

  nsChangeHint CalcDifference(const nsStylePosition& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_STYLE_HINT_REFLOW,
                          nsChangeHint(nsChangeHint_RecomputePosition |
                                       nsChangeHint_UpdateParentOverflow |
                                       nsChangeHint_UpdateComputedBSize));
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference can return all of the reflow hints that are
    // sometimes handled for descendants as hints not handled for
    // descendants.
    return nsChangeHint(0);
  }

  // XXXdholbert nsStyleBackground::Position should probably be moved to a
  // different scope, since we're now using it in multiple style structs.
  typedef nsStyleBackground::Position Position;

  Position      mObjectPosition;        // [reset]
  nsStyleSides  mOffset;                // [reset] coord, percent, calc, auto
  nsStyleCoord  mWidth;                 // [reset] coord, percent, enum, calc, auto
  nsStyleCoord  mMinWidth;              // [reset] coord, percent, enum, calc
  nsStyleCoord  mMaxWidth;              // [reset] coord, percent, enum, calc, none
  nsStyleCoord  mHeight;                // [reset] coord, percent, calc, auto
  nsStyleCoord  mMinHeight;             // [reset] coord, percent, calc
  nsStyleCoord  mMaxHeight;             // [reset] coord, percent, calc, none
  nsStyleCoord  mFlexBasis;             // [reset] coord, percent, enum, calc, auto
  nsStyleCoord  mGridAutoColumnsMin;    // [reset] coord, percent, enum, calc, flex
  nsStyleCoord  mGridAutoColumnsMax;    // [reset] coord, percent, enum, calc, flex
  nsStyleCoord  mGridAutoRowsMin;       // [reset] coord, percent, enum, calc, flex
  nsStyleCoord  mGridAutoRowsMax;       // [reset] coord, percent, enum, calc, flex
  uint8_t       mGridAutoFlow;          // [reset] enumerated. See nsStyleConsts.h
  uint8_t       mBoxSizing;             // [reset] see nsStyleConsts.h
  uint8_t       mAlignContent;          // [reset] see nsStyleConsts.h
  uint8_t       mAlignItems;            // [reset] see nsStyleConsts.h
  uint8_t       mAlignSelf;             // [reset] see nsStyleConsts.h
  uint8_t       mFlexDirection;         // [reset] see nsStyleConsts.h
  uint8_t       mFlexWrap;              // [reset] see nsStyleConsts.h
  uint8_t       mJustifyContent;        // [reset] see nsStyleConsts.h
  uint8_t       mObjectFit;             // [reset] see nsStyleConsts.h
  int32_t       mOrder;                 // [reset] integer
  float         mFlexGrow;              // [reset] float
  float         mFlexShrink;            // [reset] float
  nsStyleCoord  mZIndex;                // [reset] integer, auto
  nsStyleGridTemplate mGridTemplateColumns;
  nsStyleGridTemplate mGridTemplateRows;

  // nullptr for 'none'
  nsRefPtr<mozilla::css::GridTemplateAreasValue> mGridTemplateAreas;

  nsStyleGridLine mGridColumnStart;
  nsStyleGridLine mGridColumnEnd;
  nsStyleGridLine mGridRowStart;
  nsStyleGridLine mGridRowEnd;

  // FIXME: Logical-coordinate equivalents to these WidthDepends... and
  // HeightDepends... methods have been introduced (see below); we probably
  // want to work towards removing the physical methods, and using the logical
  // ones in all cases.

  bool WidthDependsOnContainer() const
    {
      return mWidth.GetUnit() == eStyleUnit_Auto ||
        WidthCoordDependsOnContainer(mWidth);
    }

  // NOTE: For a flex item, "min-width:auto" is supposed to behave like
  // "min-content", which does depend on the container, so you might think we'd
  // need a special case for "flex item && min-width:auto" here.  However,
  // we don't actually need that special-case code, because flex items are
  // explicitly supposed to *ignore* their min-width (i.e. behave like it's 0)
  // until the flex container explicitly considers it.  So -- since the flex
  // container doesn't rely on this method, we don't need to worry about
  // special behavior for flex items' "min-width:auto" values here.
  bool MinWidthDependsOnContainer() const
    { return WidthCoordDependsOnContainer(mMinWidth); }
  bool MaxWidthDependsOnContainer() const
    { return WidthCoordDependsOnContainer(mMaxWidth); }

  // Note that these functions count 'auto' as depending on the
  // container since that's the case for absolutely positioned elements.
  // However, some callers do not care about this case and should check
  // for it, since it is the most common case.
  // FIXME: We should probably change the assumption to be the other way
  // around.
  // Consider this as part of moving to the logical-coordinate APIs.
  bool HeightDependsOnContainer() const
    {
      return mHeight.GetUnit() == eStyleUnit_Auto || // CSS 2.1, 10.6.4, item (5)
        HeightCoordDependsOnContainer(mHeight);
    }

  // NOTE: The comment above MinWidthDependsOnContainer about flex items
  // applies here, too.
  bool MinHeightDependsOnContainer() const
    { return HeightCoordDependsOnContainer(mMinHeight); }
  bool MaxHeightDependsOnContainer() const
    { return HeightCoordDependsOnContainer(mMaxHeight); }

  bool OffsetHasPercent(mozilla::css::Side aSide) const
  {
    return mOffset.Get(aSide).HasPercent();
  }

  // Logical-coordinate accessors for width and height properties,
  // given a WritingMode value. The definitions of these methods are
  // found in WritingModes.h (after the WritingMode class is fully
  // declared).
  inline nsStyleCoord& ISize(mozilla::WritingMode aWM);
  inline nsStyleCoord& MinISize(mozilla::WritingMode aWM);
  inline nsStyleCoord& MaxISize(mozilla::WritingMode aWM);
  inline nsStyleCoord& BSize(mozilla::WritingMode aWM);
  inline nsStyleCoord& MinBSize(mozilla::WritingMode aWM);
  inline nsStyleCoord& MaxBSize(mozilla::WritingMode aWM);
  inline const nsStyleCoord& ISize(mozilla::WritingMode aWM) const;
  inline const nsStyleCoord& MinISize(mozilla::WritingMode aWM) const;
  inline const nsStyleCoord& MaxISize(mozilla::WritingMode aWM) const;
  inline const nsStyleCoord& BSize(mozilla::WritingMode aWM) const;
  inline const nsStyleCoord& MinBSize(mozilla::WritingMode aWM) const;
  inline const nsStyleCoord& MaxBSize(mozilla::WritingMode aWM) const;
  inline bool ISizeDependsOnContainer(mozilla::WritingMode aWM) const;
  inline bool MinISizeDependsOnContainer(mozilla::WritingMode aWM) const;
  inline bool MaxISizeDependsOnContainer(mozilla::WritingMode aWM) const;
  inline bool BSizeDependsOnContainer(mozilla::WritingMode aWM) const;
  inline bool MinBSizeDependsOnContainer(mozilla::WritingMode aWM) const;
  inline bool MaxBSizeDependsOnContainer(mozilla::WritingMode aWM) const;

private:
  static bool WidthCoordDependsOnContainer(const nsStyleCoord &aCoord);
  static bool HeightCoordDependsOnContainer(const nsStyleCoord &aCoord)
    { return aCoord.HasPercent(); }
};

struct nsStyleTextOverflowSide {
  nsStyleTextOverflowSide() : mType(NS_STYLE_TEXT_OVERFLOW_CLIP) {}

  bool operator==(const nsStyleTextOverflowSide& aOther) const {
    return mType == aOther.mType &&
           (mType != NS_STYLE_TEXT_OVERFLOW_STRING ||
            mString == aOther.mString);
  }
  bool operator!=(const nsStyleTextOverflowSide& aOther) const {
    return !(*this == aOther);
  }

  nsString mString;
  uint8_t  mType;
};

struct nsStyleTextOverflow {
  nsStyleTextOverflow() : mLogicalDirections(true) {}
  bool operator==(const nsStyleTextOverflow& aOther) const {
    return mLeft == aOther.mLeft && mRight == aOther.mRight;
  }
  bool operator!=(const nsStyleTextOverflow& aOther) const {
    return !(*this == aOther);
  }

  // Returns the value to apply on the left side.
  const nsStyleTextOverflowSide& GetLeft(uint8_t aDirection) const {
    NS_ASSERTION(aDirection == NS_STYLE_DIRECTION_LTR ||
                 aDirection == NS_STYLE_DIRECTION_RTL, "bad direction");
    return !mLogicalDirections || aDirection == NS_STYLE_DIRECTION_LTR ?
             mLeft : mRight;
  }

  // Returns the value to apply on the right side.
  const nsStyleTextOverflowSide& GetRight(uint8_t aDirection) const {
    NS_ASSERTION(aDirection == NS_STYLE_DIRECTION_LTR ||
                 aDirection == NS_STYLE_DIRECTION_RTL, "bad direction");
    return !mLogicalDirections || aDirection == NS_STYLE_DIRECTION_LTR ?
             mRight : mLeft;
  }

  // Returns the first value that was specified.
  const nsStyleTextOverflowSide* GetFirstValue() const {
    return mLogicalDirections ? &mRight : &mLeft;
  }

  // Returns the second value, or null if there was only one value specified.
  const nsStyleTextOverflowSide* GetSecondValue() const {
    return mLogicalDirections ? nullptr : &mRight;
  }

  nsStyleTextOverflowSide mLeft;  // start side when mLogicalDirections is true
  nsStyleTextOverflowSide mRight; // end side when mLogicalDirections is true
  bool mLogicalDirections;  // true when only one value was specified
};

struct nsStyleTextReset {
  nsStyleTextReset(void);
  nsStyleTextReset(const nsStyleTextReset& aOther);
  ~nsStyleTextReset(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleTextReset_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTextReset();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleTextReset_id, this);
  }

  uint8_t GetDecorationStyle() const
  {
    return (mTextDecorationStyle & BORDER_STYLE_MASK);
  }

  void SetDecorationStyle(uint8_t aStyle)
  {
    MOZ_ASSERT((aStyle & BORDER_STYLE_MASK) == aStyle,
               "style doesn't fit");
    mTextDecorationStyle &= ~BORDER_STYLE_MASK;
    mTextDecorationStyle |= (aStyle & BORDER_STYLE_MASK);
  }

  void GetDecorationColor(nscolor& aColor, bool& aForeground) const
  {
    aForeground = false;
    if ((mTextDecorationStyle & BORDER_COLOR_SPECIAL) == 0) {
      aColor = mTextDecorationColor;
    } else if (mTextDecorationStyle & BORDER_COLOR_FOREGROUND) {
      aForeground = true;
    } else {
      NS_NOTREACHED("OUTLINE_COLOR_INITIAL should not be set here");
    }
  }

  void SetDecorationColor(nscolor aColor)
  {
    mTextDecorationColor = aColor;
    mTextDecorationStyle &= ~BORDER_COLOR_SPECIAL;
  }

  void SetDecorationColorToForeground()
  {
    mTextDecorationStyle &= ~BORDER_COLOR_SPECIAL;
    mTextDecorationStyle |= BORDER_COLOR_FOREGROUND;
  }

  nsChangeHint CalcDifference(const nsStyleTextReset& aOther) const;
  static nsChangeHint MaxDifference() {
    return nsChangeHint(
        NS_STYLE_HINT_REFLOW |
        nsChangeHint_UpdateSubtreeOverflow);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  nsStyleCoord  mVerticalAlign;         // [reset] coord, percent, calc, enum (see nsStyleConsts.h)
  nsStyleTextOverflow mTextOverflow;    // [reset] enum, string

  uint8_t mTextDecorationLine;          // [reset] see nsStyleConsts.h
  uint8_t mUnicodeBidi;                 // [reset] see nsStyleConsts.h
protected:
  uint8_t mTextDecorationStyle;         // [reset] see nsStyleConsts.h

  nscolor mTextDecorationColor;         // [reset] the colors to use for a decoration lines, not used at currentColor
};

struct nsStyleText {
  nsStyleText(void);
  nsStyleText(const nsStyleText& aOther);
  ~nsStyleText(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleText_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleText();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleText_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleText& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  uint8_t mTextAlign;                   // [inherited] see nsStyleConsts.h
  uint8_t mTextAlignLast;               // [inherited] see nsStyleConsts.h
  bool mTextAlignTrue : 1;              // [inherited] see nsStyleConsts.h
  bool mTextAlignLastTrue : 1;          // [inherited] see nsStyleConsts.h
  uint8_t mTextTransform;               // [inherited] see nsStyleConsts.h
  uint8_t mWhiteSpace;                  // [inherited] see nsStyleConsts.h
  uint8_t mWordBreak;                   // [inherited] see nsStyleConsts.h
  uint8_t mWordWrap;                    // [inherited] see nsStyleConsts.h
  uint8_t mHyphens;                     // [inherited] see nsStyleConsts.h
  uint8_t mRubyAlign;                   // [inherited] see nsStyleConsts.h
  uint8_t mRubyPosition;                // [inherited] see nsStyleConsts.h
  uint8_t mTextSizeAdjust;              // [inherited] see nsStyleConsts.h
  uint8_t mTextCombineUpright;          // [inherited] see nsStyleConsts.h
  uint8_t mControlCharacterVisibility;  // [inherited] see nsStyleConsts.h
  int32_t mTabSize;                     // [inherited] see nsStyleConsts.h

  nscoord mWordSpacing;                 // [inherited]
  nsStyleCoord  mLetterSpacing;         // [inherited] coord, normal
  nsStyleCoord  mLineHeight;            // [inherited] coord, factor, normal
  nsStyleCoord  mTextIndent;            // [inherited] coord, percent, calc

  nsRefPtr<nsCSSShadowArray> mTextShadow; // [inherited] nullptr in case of a zero-length

  bool WhiteSpaceIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_SPACE;
  }

  bool NewlineIsSignificantStyle() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_LINE;
  }

  bool WhiteSpaceOrNewlineIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_LINE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_SPACE;
  }

  bool TabIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP;
  }

  bool WhiteSpaceCanWrapStyle() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_NORMAL ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_LINE;
  }

  bool WordCanWrapStyle() const {
    return WhiteSpaceCanWrapStyle() &&
           mWordWrap == NS_STYLE_WORDWRAP_BREAK_WORD;
  }

  // These are defined in nsStyleStructInlines.h.
  inline bool HasTextShadow() const;
  inline nsCSSShadowArray* GetTextShadow() const;

  // The aContextFrame argument on each of these is the frame this
  // style struct is for.  If the frame is for SVG text or inside ruby,
  // the return value will be massaged to be something that makes sense
  // for those cases.
  inline bool NewlineIsSignificant(const nsTextFrame* aContextFrame) const;
  inline bool WhiteSpaceCanWrap(const nsIFrame* aContextFrame) const;
  inline bool WordCanWrap(const nsIFrame* aContextFrame) const;
};

struct nsStyleImageOrientation {
  static nsStyleImageOrientation CreateAsAngleAndFlip(double aRadians,
                                                      bool aFlip) {
    uint8_t orientation(0);

    // Compute the final angle value, rounding to the closest quarter turn.
    double roundedAngle = fmod(aRadians, 2 * M_PI);
    if      (roundedAngle < 0.25 * M_PI) orientation = ANGLE_0;
    else if (roundedAngle < 0.75 * M_PI) orientation = ANGLE_90;
    else if (roundedAngle < 1.25 * M_PI) orientation = ANGLE_180;
    else if (roundedAngle < 1.75 * M_PI) orientation = ANGLE_270;
    else                                 orientation = ANGLE_0;

    // Add a bit for 'flip' if needed.
    if (aFlip)
      orientation |= FLIP_MASK;

    return nsStyleImageOrientation(orientation);
  }

  static nsStyleImageOrientation CreateAsFlip() {
    return nsStyleImageOrientation(FLIP_MASK);
  }

  static nsStyleImageOrientation CreateAsFromImage() {
    return nsStyleImageOrientation(FROM_IMAGE_MASK);
  }

  // The default constructor yields 0 degrees of rotation and no flip.
  nsStyleImageOrientation() : mOrientation(0) { }

  bool IsDefault()   const { return mOrientation == 0; }
  bool IsFlipped()   const { return mOrientation & FLIP_MASK; }
  bool IsFromImage() const { return mOrientation & FROM_IMAGE_MASK; }
  bool SwapsWidthAndHeight() const {
    uint8_t angle = mOrientation & ORIENTATION_MASK;
    return (angle == ANGLE_90) || (angle == ANGLE_270);
  }

  mozilla::image::Angle Angle() const {
    switch (mOrientation & ORIENTATION_MASK) {
      case ANGLE_0:   return mozilla::image::Angle::D0;
      case ANGLE_90:  return mozilla::image::Angle::D90;
      case ANGLE_180: return mozilla::image::Angle::D180;
      case ANGLE_270: return mozilla::image::Angle::D270;
      default:
        NS_NOTREACHED("Unexpected angle");
        return mozilla::image::Angle::D0;
    }
  }

  nsStyleCoord AngleAsCoord() const {
    switch (mOrientation & ORIENTATION_MASK) {
      case ANGLE_0:   return nsStyleCoord(0.0f,   eStyleUnit_Degree);
      case ANGLE_90:  return nsStyleCoord(90.0f,  eStyleUnit_Degree);
      case ANGLE_180: return nsStyleCoord(180.0f, eStyleUnit_Degree);
      case ANGLE_270: return nsStyleCoord(270.0f, eStyleUnit_Degree);
      default:
        NS_NOTREACHED("Unexpected angle");
        return nsStyleCoord();
    }
  }

  bool operator==(const nsStyleImageOrientation& aOther) const {
    return aOther.mOrientation == mOrientation;
  }

  bool operator!=(const nsStyleImageOrientation& aOther) const {
    return !(*this == aOther);
  }

protected:
  enum Bits {
    ORIENTATION_MASK = 0x1 | 0x2,  // The bottom two bits are the angle.
    FLIP_MASK        = 0x4,        // Whether the image should be flipped.
    FROM_IMAGE_MASK  = 0x8,        // Whether the image's inherent orientation
  };                               // should be used.

  enum Angles {
    ANGLE_0   = 0,
    ANGLE_90  = 1,
    ANGLE_180 = 2,
    ANGLE_270 = 3,
  };

  explicit nsStyleImageOrientation(uint8_t aOrientation)
    : mOrientation(aOrientation)
  { }

  uint8_t mOrientation;
};

struct nsStyleVisibility {
  explicit nsStyleVisibility(nsPresContext* aPresContext);
  nsStyleVisibility(const nsStyleVisibility& aVisibility);
  ~nsStyleVisibility() {
    MOZ_COUNT_DTOR(nsStyleVisibility);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleVisibility_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleVisibility();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleVisibility_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleVisibility& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  nsStyleImageOrientation mImageOrientation;  // [inherited]
  uint8_t mDirection;                  // [inherited] see nsStyleConsts.h NS_STYLE_DIRECTION_*
  uint8_t mVisible;                    // [inherited]
  uint8_t mPointerEvents;              // [inherited] see nsStyleConsts.h
  uint8_t mWritingMode;                // [inherited] see nsStyleConsts.h
  uint8_t mTextOrientation;            // [inherited] see nsStyleConsts.h

  bool IsVisible() const {
    return (mVisible == NS_STYLE_VISIBILITY_VISIBLE);
  }

  bool IsVisibleOrCollapsed() const {
    return ((mVisible == NS_STYLE_VISIBILITY_VISIBLE) ||
            (mVisible == NS_STYLE_VISIBILITY_COLLAPSE));
  }

  inline uint8_t GetEffectivePointerEvents(nsIFrame* aFrame) const;
};

struct nsTimingFunction {
  enum Type { Function, StepStart, StepEnd };

  explicit nsTimingFunction(int32_t aTimingFunctionType
                              = NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE)
  {
    AssignFromKeyword(aTimingFunctionType);
  }

  nsTimingFunction(float x1, float y1, float x2, float y2)
    : mType(Function)
  {
    mFunc.mX1 = x1;
    mFunc.mY1 = y1;
    mFunc.mX2 = x2;
    mFunc.mY2 = y2;
  }

  nsTimingFunction(Type aType, uint32_t aSteps)
    : mType(aType)
  {
    MOZ_ASSERT(mType == StepStart || mType == StepEnd, "wrong type");
    mSteps = aSteps;
  }

  nsTimingFunction(const nsTimingFunction& aOther)
  {
    *this = aOther;
  }

  Type mType;
  union {
    struct {
      float mX1;
      float mY1;
      float mX2;
      float mY2;
    } mFunc;
    uint32_t mSteps;
  };

  nsTimingFunction&
  operator=(const nsTimingFunction& aOther)
  {
    if (&aOther == this)
      return *this;

    mType = aOther.mType;

    if (mType == Function) {
      mFunc.mX1 = aOther.mFunc.mX1;
      mFunc.mY1 = aOther.mFunc.mY1;
      mFunc.mX2 = aOther.mFunc.mX2;
      mFunc.mY2 = aOther.mFunc.mY2;
    } else {
      mSteps = aOther.mSteps;
    }

    return *this;
  }

  bool operator==(const nsTimingFunction& aOther) const
  {
    if (mType != aOther.mType) {
      return false;
    }
    if (mType == Function) {
      return mFunc.mX1 == aOther.mFunc.mX1 && mFunc.mY1 == aOther.mFunc.mY1 &&
             mFunc.mX2 == aOther.mFunc.mX2 && mFunc.mY2 == aOther.mFunc.mY2;
    }
    return mSteps == aOther.mSteps;
  }

  bool operator!=(const nsTimingFunction& aOther) const
  {
    return !(*this == aOther);
  }

private:
  void AssignFromKeyword(int32_t aTimingFunctionType);
};

namespace mozilla {

struct StyleTransition {
  StyleTransition() { /* leaves uninitialized; see also SetInitialValues */ }
  explicit StyleTransition(const StyleTransition& aCopy);

  void SetInitialValues();

  // Delay and Duration are in milliseconds

  const nsTimingFunction& GetTimingFunction() const { return mTimingFunction; }
  float GetDelay() const { return mDelay; }
  float GetDuration() const { return mDuration; }
  nsCSSProperty GetProperty() const { return mProperty; }
  nsIAtom* GetUnknownProperty() const { return mUnknownProperty; }

  float GetCombinedDuration() const {
    // http://dev.w3.org/csswg/css-transitions/#combined-duration
    return std::max(mDuration, 0.0f) + mDelay;
  }

  void SetTimingFunction(const nsTimingFunction& aTimingFunction)
    { mTimingFunction = aTimingFunction; }
  void SetDelay(float aDelay) { mDelay = aDelay; }
  void SetDuration(float aDuration) { mDuration = aDuration; }
  void SetProperty(nsCSSProperty aProperty)
    {
      NS_ASSERTION(aProperty != eCSSProperty_UNKNOWN, "invalid property");
      mProperty = aProperty;
    }
  void SetUnknownProperty(const nsAString& aUnknownProperty);
  void CopyPropertyFrom(const StyleTransition& aOther)
    {
      mProperty = aOther.mProperty;
      mUnknownProperty = aOther.mUnknownProperty;
    }

  nsTimingFunction& TimingFunctionSlot() { return mTimingFunction; }

  bool operator==(const StyleTransition& aOther) const;
  bool operator!=(const StyleTransition& aOther) const
    { return !(*this == aOther); }

private:
  nsTimingFunction mTimingFunction;
  float mDuration;
  float mDelay;
  nsCSSProperty mProperty;
  nsCOMPtr<nsIAtom> mUnknownProperty; // used when mProperty is
                                      // eCSSProperty_UNKNOWN
};

struct StyleAnimation {
  StyleAnimation() { /* leaves uninitialized; see also SetInitialValues */ }
  explicit StyleAnimation(const StyleAnimation& aCopy);

  void SetInitialValues();

  // Delay and Duration are in milliseconds

  const nsTimingFunction& GetTimingFunction() const { return mTimingFunction; }
  float GetDelay() const { return mDelay; }
  float GetDuration() const { return mDuration; }
  const nsString& GetName() const { return mName; }
  uint8_t GetDirection() const { return mDirection; }
  uint8_t GetFillMode() const { return mFillMode; }
  uint8_t GetPlayState() const { return mPlayState; }
  float GetIterationCount() const { return mIterationCount; }

  void SetTimingFunction(const nsTimingFunction& aTimingFunction)
    { mTimingFunction = aTimingFunction; }
  void SetDelay(float aDelay) { mDelay = aDelay; }
  void SetDuration(float aDuration) { mDuration = aDuration; }
  void SetName(const nsSubstring& aName) { mName = aName; }
  void SetDirection(uint8_t aDirection) { mDirection = aDirection; }
  void SetFillMode(uint8_t aFillMode) { mFillMode = aFillMode; }
  void SetPlayState(uint8_t aPlayState) { mPlayState = aPlayState; }
  void SetIterationCount(float aIterationCount)
    { mIterationCount = aIterationCount; }

  nsTimingFunction& TimingFunctionSlot() { return mTimingFunction; }

  bool operator==(const StyleAnimation& aOther) const;
  bool operator!=(const StyleAnimation& aOther) const
    { return !(*this == aOther); }

private:
  nsTimingFunction mTimingFunction;
  float mDuration;
  float mDelay;
  nsString mName; // empty string for 'none'
  uint8_t mDirection;
  uint8_t mFillMode;
  uint8_t mPlayState;
  float mIterationCount; // mozilla::PositiveInfinity<float>() means infinite
};

} // namespace mozilla

struct nsStyleDisplay {
  nsStyleDisplay();
  nsStyleDisplay(const nsStyleDisplay& aOther);
  ~nsStyleDisplay() {
    MOZ_COUNT_DTOR(nsStyleDisplay);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleDisplay_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleDisplay();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleDisplay_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleDisplay& aOther) const;
  static nsChangeHint MaxDifference() {
    // All the parts of FRAMECHANGE are present in CalcDifference.
    return nsChangeHint(NS_STYLE_HINT_FRAMECHANGE |
                        nsChangeHint_UpdateOpacityLayer |
                        nsChangeHint_UpdateTransformLayer |
                        nsChangeHint_UpdateOverflow |
                        nsChangeHint_UpdatePostTransformOverflow |
                        nsChangeHint_AddOrRemoveTransform |
                        nsChangeHint_NeutralChange);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference can return all of the reflow hints that are
    // sometimes handled for descendants as hints not handled for
    // descendants.
    return nsChangeHint(0);
  }

  // XXXdholbert, XXXkgilbert nsStyleBackground::Position should probably be
  // moved to a different scope, since we're now using it in multiple style
  // structs.
  typedef nsStyleBackground::Position Position;

  // We guarantee that if mBinding is non-null, so are mBinding->GetURI() and
  // mBinding->mOriginPrincipal.
  nsRefPtr<mozilla::css::URLValue> mBinding;    // [reset]
  nsRect  mClip;                // [reset] offsets from upper-left border edge
  float   mOpacity;             // [reset]
  uint8_t mDisplay;             // [reset] see nsStyleConsts.h NS_STYLE_DISPLAY_*
  uint8_t mOriginalDisplay;     // [reset] saved mDisplay for position:absolute/fixed
                                //         and float:left/right; otherwise equal
                                //         to mDisplay
  uint8_t mContain;             // [reset] see nsStyleConsts.h NS_STYLE_CONTAIN_*
  uint8_t mAppearance;          // [reset]
  uint8_t mPosition;            // [reset] see nsStyleConsts.h
  uint8_t mFloats;              // [reset] see nsStyleConsts.h NS_STYLE_FLOAT_*
  uint8_t mOriginalFloats;      // [reset] saved mFloats for position:absolute/fixed;
                                //         otherwise equal to mFloats
  uint8_t mBreakType;           // [reset] see nsStyleConsts.h NS_STYLE_CLEAR_*
  uint8_t mBreakInside;         // [reset] NS_STYLE_PAGE_BREAK_AUTO/AVOID
  bool mBreakBefore;    // [reset]
  bool mBreakAfter;     // [reset]
  uint8_t mOverflowX;           // [reset] see nsStyleConsts.h
  uint8_t mOverflowY;           // [reset] see nsStyleConsts.h
  uint8_t mOverflowClipBox;     // [reset] see nsStyleConsts.h
  uint8_t mResize;              // [reset] see nsStyleConsts.h
  uint8_t mClipFlags;           // [reset] see nsStyleConsts.h
  uint8_t mOrient;              // [reset] see nsStyleConsts.h
  uint8_t mMixBlendMode;        // [reset] see nsStyleConsts.h
  uint8_t mIsolation;           // [reset] see nsStyleConsts.h
  uint8_t mWillChangeBitField;  // [reset] see nsStyleConsts.h. Stores a
                                // bitfield representation of the properties
                                // that are frequently queried. This should
                                // match mWillChange. Also tracks if any of the
                                // properties in the will-change list require
                                // a stacking context.
  nsAutoTArray<nsString, 1> mWillChange;

  uint8_t mTouchAction;         // [reset] see nsStyleConsts.h
  uint8_t mScrollBehavior;      // [reset] see nsStyleConsts.h NS_STYLE_SCROLL_BEHAVIOR_*
  uint8_t mScrollSnapTypeX;     // [reset] see nsStyleConsts.h NS_STYLE_SCROLL_SNAP_TYPE_*
  uint8_t mScrollSnapTypeY;     // [reset] see nsStyleConsts.h NS_STYLE_SCROLL_SNAP_TYPE_*
  nsStyleCoord mScrollSnapPointsX; // [reset]
  nsStyleCoord mScrollSnapPointsY; // [reset]
  Position mScrollSnapDestination; // [reset]
  nsTArray<Position> mScrollSnapCoordinate; // [reset]

  // mSpecifiedTransform is the list of transform functions as
  // specified, or null to indicate there is no transform.  (inherit or
  // initial are replaced by an actual list of transform functions, or
  // null, as appropriate.)
  uint8_t mBackfaceVisibility;
  uint8_t mTransformStyle;
  uint8_t mTransformBox;        // [reset] see nsStyleConsts.h
  nsRefPtr<nsCSSValueSharedList> mSpecifiedTransform; // [reset]
  nsStyleCoord mTransformOrigin[3]; // [reset] percent, coord, calc, 3rd param is coord, calc only
  nsStyleCoord mChildPerspective; // [reset] coord
  nsStyleCoord mPerspectiveOrigin[2]; // [reset] percent, coord, calc

  nsAutoTArray<mozilla::StyleTransition, 1> mTransitions; // [reset]
  // The number of elements in mTransitions that are not from repeating
  // a list due to another property being longer.
  uint32_t mTransitionTimingFunctionCount,
           mTransitionDurationCount,
           mTransitionDelayCount,
           mTransitionPropertyCount;

  nsAutoTArray<mozilla::StyleAnimation, 1> mAnimations; // [reset]
  // The number of elements in mAnimations that are not from repeating
  // a list due to another property being longer.
  uint32_t mAnimationTimingFunctionCount,
           mAnimationDurationCount,
           mAnimationDelayCount,
           mAnimationNameCount,
           mAnimationDirectionCount,
           mAnimationFillModeCount,
           mAnimationPlayStateCount,
           mAnimationIterationCountCount;

  bool IsBlockInsideStyle() const {
    return NS_STYLE_DISPLAY_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_LIST_ITEM == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_CAPTION == mDisplay;
    // Should TABLE_CELL be included here?  They have
    // block frames nested inside of them.
    // (But please audit all callers before changing.)
  }

  bool IsBlockOutsideStyle() const {
    return NS_STYLE_DISPLAY_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_FLEX == mDisplay ||
           NS_STYLE_DISPLAY_GRID == mDisplay ||
           NS_STYLE_DISPLAY_LIST_ITEM == mDisplay ||
           NS_STYLE_DISPLAY_TABLE == mDisplay;
  }

  static bool IsDisplayTypeInlineOutside(uint8_t aDisplay) {
    return NS_STYLE_DISPLAY_INLINE == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_BLOCK == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_TABLE == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_BOX == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_FLEX == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_GRID == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_XUL_GRID == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_STACK == aDisplay ||
           NS_STYLE_DISPLAY_RUBY == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_BASE == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_BASE_CONTAINER == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_TEXT == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_TEXT_CONTAINER == aDisplay ||
           NS_STYLE_DISPLAY_CONTENTS == aDisplay;
  }

  bool IsInlineOutsideStyle() const {
    return IsDisplayTypeInlineOutside(mDisplay);
  }

  bool IsOriginalDisplayInlineOutsideStyle() const {
    return IsDisplayTypeInlineOutside(mOriginalDisplay);
  }

  bool IsInnerTableStyle() const {
    return NS_STYLE_DISPLAY_TABLE_CAPTION == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_CELL == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_ROW == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_ROW_GROUP == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_HEADER_GROUP == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_COLUMN == mDisplay ||
           NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP == mDisplay;
  }

  bool IsFloatingStyle() const {
    return NS_STYLE_FLOAT_NONE != mFloats;
  }

  bool IsAbsolutelyPositionedStyle() const {
    return NS_STYLE_POSITION_ABSOLUTE == mPosition ||
           NS_STYLE_POSITION_FIXED == mPosition;
  }

  bool IsRelativelyPositionedStyle() const {
    return NS_STYLE_POSITION_RELATIVE == mPosition ||
           NS_STYLE_POSITION_STICKY == mPosition;
  }

  static bool IsRubyDisplayType(uint8_t aDisplay) {
    return NS_STYLE_DISPLAY_RUBY == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_BASE == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_BASE_CONTAINER == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_TEXT == aDisplay ||
           NS_STYLE_DISPLAY_RUBY_TEXT_CONTAINER == aDisplay;
  }

  bool IsRubyDisplayType() const {
    return IsRubyDisplayType(mDisplay);
  }

  bool IsFlexOrGridDisplayType() const {
    return NS_STYLE_DISPLAY_FLEX == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_FLEX == mDisplay ||
           NS_STYLE_DISPLAY_GRID == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_GRID == mDisplay;
  }

  bool IsOutOfFlowStyle() const {
    return (IsAbsolutelyPositionedStyle() || IsFloatingStyle());
  }

  bool IsScrollableOverflow() const {
    // mOverflowX and mOverflowY always match when one of them is
    // NS_STYLE_OVERFLOW_VISIBLE or NS_STYLE_OVERFLOW_CLIP.
    return mOverflowX != NS_STYLE_OVERFLOW_VISIBLE &&
           mOverflowX != NS_STYLE_OVERFLOW_CLIP;
  }

  /* Returns whether the element has the -moz-transform property
   * or a related property. */
  bool HasTransformStyle() const {
    return mSpecifiedTransform != nullptr ||
           mTransformStyle == NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D ||
           (mWillChangeBitField & NS_STYLE_WILL_CHANGE_TRANSFORM);
  }

  bool HasPerspectiveStyle() const {
    return mChildPerspective.GetUnit() == eStyleUnit_Coord;
  }

  bool BackfaceIsHidden() const {
    return mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN;
  }

  // These are defined in nsStyleStructInlines.h.

  // The aContextFrame argument on each of these is the frame this
  // style struct is for.  If the frame is for SVG text, the return
  // value will be massaged to be something that makes sense for
  // SVG text.
  inline bool IsBlockInside(const nsIFrame* aContextFrame) const;
  inline bool IsBlockOutside(const nsIFrame* aContextFrame) const;
  inline bool IsInlineOutside(const nsIFrame* aContextFrame) const;
  inline bool IsOriginalDisplayInlineOutside(const nsIFrame* aContextFrame) const;
  inline uint8_t GetDisplay(const nsIFrame* aContextFrame) const;
  inline bool IsFloating(const nsIFrame* aContextFrame) const;
  inline bool IsAbsPosContainingBlock(const nsIFrame* aContextFrame) const;
  inline bool IsRelativelyPositioned(const nsIFrame* aContextFrame) const;
  inline bool IsAbsolutelyPositioned(const nsIFrame* aContextFrame) const;

  // These methods are defined in nsStyleStructInlines.h.

  /**
   * Returns true when the element has the transform property
   * or a related property, and supports CSS transforms.
   * aContextFrame is the frame for which this is the nsStylePosition.
   */
  inline bool HasTransform(const nsIFrame* aContextFrame) const;

  /**
   * Returns true when the element is a containing block for its fixed-pos
   * descendants.
   * aContextFrame is the frame for which this is the nsStylePosition.
   */
  inline bool IsFixedPosContainingBlock(const nsIFrame* aContextFrame) const;
};

struct nsStyleTable {
  nsStyleTable(void);
  nsStyleTable(const nsStyleTable& aOther);
  ~nsStyleTable(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleTable_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTable();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleTable_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleTable& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  uint8_t       mLayoutStrategy;// [reset] see nsStyleConsts.h NS_STYLE_TABLE_LAYOUT_*
  int32_t       mSpan;          // [reset] the number of columns spanned by a colgroup or col
};

struct nsStyleTableBorder {
  nsStyleTableBorder();
  nsStyleTableBorder(const nsStyleTableBorder& aOther);
  ~nsStyleTableBorder(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleTableBorder_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTableBorder();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleTableBorder_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleTableBorder& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  nscoord       mBorderSpacingCol;// [inherited]
  nscoord       mBorderSpacingRow;// [inherited]
  uint8_t       mBorderCollapse;// [inherited]
  uint8_t       mCaptionSide;   // [inherited]
  uint8_t       mEmptyCells;    // [inherited]
};

enum nsStyleContentType {
  eStyleContentType_String        = 1,
  eStyleContentType_Image         = 10,
  eStyleContentType_Attr          = 20,
  eStyleContentType_Counter       = 30,
  eStyleContentType_Counters      = 31,
  eStyleContentType_OpenQuote     = 40,
  eStyleContentType_CloseQuote    = 41,
  eStyleContentType_NoOpenQuote   = 42,
  eStyleContentType_NoCloseQuote  = 43,
  eStyleContentType_AltContent    = 50,
  eStyleContentType_Uninitialized
};

struct nsStyleContentData {
  nsStyleContentType  mType;
  union {
    char16_t *mString;
    imgRequestProxy *mImage;
    nsCSSValue::Array* mCounters;
  } mContent;
#ifdef DEBUG
  bool mImageTracked;
#endif

  nsStyleContentData()
    : mType(eStyleContentType_Uninitialized)
#ifdef DEBUG
    , mImageTracked(false)
#endif
  { mContent.mString = nullptr; }

  ~nsStyleContentData();
  nsStyleContentData& operator=(const nsStyleContentData& aOther);
  bool operator==(const nsStyleContentData& aOther) const;

  bool operator!=(const nsStyleContentData& aOther) const {
    return !(*this == aOther);
  }

  void TrackImage(nsPresContext* aContext);
  void UntrackImage(nsPresContext* aContext);

  void SetImage(imgRequestProxy* aRequest)
  {
    MOZ_ASSERT(!mImageTracked,
               "Setting a new image without untracking the old one!");
    MOZ_ASSERT(mType == eStyleContentType_Image, "Wrong type!");
    NS_IF_ADDREF(mContent.mImage = aRequest);
  }
private:
  nsStyleContentData(const nsStyleContentData&); // not to be implemented
};

struct nsStyleCounterData {
  nsString  mCounter;
  int32_t   mValue;
};


#define DELETE_ARRAY_IF(array)  if (array) { delete[] array; array = nullptr; }

struct nsStyleQuotes {
  nsStyleQuotes();
  nsStyleQuotes(const nsStyleQuotes& aQuotes);
  ~nsStyleQuotes();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleQuotes_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleQuotes();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleQuotes_id, this);
  }

  void SetInitial();
  void CopyFrom(const nsStyleQuotes& aSource);

  nsChangeHint CalcDifference(const nsStyleQuotes& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  uint32_t  QuotesCount(void) const { return mQuotesCount; } // [inherited]

  const nsString* OpenQuoteAt(uint32_t aIndex) const
  {
    NS_ASSERTION(aIndex < mQuotesCount, "out of range");
    return mQuotes + (aIndex * 2);
  }
  const nsString* CloseQuoteAt(uint32_t aIndex) const
  {
    NS_ASSERTION(aIndex < mQuotesCount, "out of range");
    return mQuotes + (aIndex * 2 + 1);
  }
  nsresult  GetQuotesAt(uint32_t aIndex, nsString& aOpen, nsString& aClose) const {
    if (aIndex < mQuotesCount) {
      aIndex *= 2;
      aOpen = mQuotes[aIndex];
      aClose = mQuotes[++aIndex];
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult  AllocateQuotes(uint32_t aCount) {
    if (aCount != mQuotesCount) {
      DELETE_ARRAY_IF(mQuotes);
      if (aCount) {
        mQuotes = new nsString[aCount * 2];
        if (! mQuotes) {
          mQuotesCount = 0;
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mQuotesCount = aCount;
    }
    return NS_OK;
  }

  nsresult  SetQuotesAt(uint32_t aIndex, const nsString& aOpen, const nsString& aClose) {
    if (aIndex < mQuotesCount) {
      aIndex *= 2;
      mQuotes[aIndex] = aOpen;
      mQuotes[++aIndex] = aClose;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

protected:
  uint32_t            mQuotesCount;
  nsString*           mQuotes;
};

struct nsStyleContent {
  nsStyleContent(void);
  nsStyleContent(const nsStyleContent& aContent);
  ~nsStyleContent(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleContent_id, sz);
  }
  void Destroy(nsPresContext* aContext);

  nsChangeHint CalcDifference(const nsStyleContent& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  uint32_t  ContentCount(void) const  { return mContentCount; } // [reset]

  const nsStyleContentData& ContentAt(uint32_t aIndex) const {
    NS_ASSERTION(aIndex < mContentCount, "out of range");
    return mContents[aIndex];
  }

  nsStyleContentData& ContentAt(uint32_t aIndex) {
    NS_ASSERTION(aIndex < mContentCount, "out of range");
    return mContents[aIndex];
  }

  nsresult AllocateContents(uint32_t aCount);

  uint32_t  CounterIncrementCount(void) const { return mIncrementCount; }  // [reset]
  const nsStyleCounterData* GetCounterIncrementAt(uint32_t aIndex) const {
    NS_ASSERTION(aIndex < mIncrementCount, "out of range");
    return &mIncrements[aIndex];
  }

  nsresult  AllocateCounterIncrements(uint32_t aCount) {
    if (aCount != mIncrementCount) {
      DELETE_ARRAY_IF(mIncrements);
      if (aCount) {
        mIncrements = new nsStyleCounterData[aCount];
        if (! mIncrements) {
          mIncrementCount = 0;
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mIncrementCount = aCount;
    }
    return NS_OK;
  }

  nsresult  SetCounterIncrementAt(uint32_t aIndex, const nsString& aCounter, int32_t aIncrement) {
    if (aIndex < mIncrementCount) {
      mIncrements[aIndex].mCounter = aCounter;
      mIncrements[aIndex].mValue = aIncrement;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  uint32_t  CounterResetCount(void) const { return mResetCount; }  // [reset]
  const nsStyleCounterData* GetCounterResetAt(uint32_t aIndex) const {
    NS_ASSERTION(aIndex < mResetCount, "out of range");
    return &mResets[aIndex];
  }

  nsresult  AllocateCounterResets(uint32_t aCount) {
    if (aCount != mResetCount) {
      DELETE_ARRAY_IF(mResets);
      if (aCount) {
        mResets = new nsStyleCounterData[aCount];
        if (! mResets) {
          mResetCount = 0;
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mResetCount = aCount;
    }
    return NS_OK;
  }

  nsresult  SetCounterResetAt(uint32_t aIndex, const nsString& aCounter, int32_t aValue) {
    if (aIndex < mResetCount) {
      mResets[aIndex].mCounter = aCounter;
      mResets[aIndex].mValue = aValue;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsStyleCoord  mMarkerOffset;  // [reset] coord, auto

protected:
  nsStyleContentData* mContents;
  nsStyleCounterData* mIncrements;
  nsStyleCounterData* mResets;

  uint32_t            mContentCount;
  uint32_t            mIncrementCount;
  uint32_t            mResetCount;
};

struct nsStyleUIReset {
  nsStyleUIReset(void);
  nsStyleUIReset(const nsStyleUIReset& aOther);
  ~nsStyleUIReset(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleUIReset_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleUIReset();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleUIReset_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleUIReset& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  uint8_t   mUserSelect;      // [reset] (selection-style)
  uint8_t   mForceBrokenImageIcon; // [reset]  (0 if not forcing, otherwise forcing)
  uint8_t   mIMEMode;         // [reset]
  uint8_t   mWindowShadow;    // [reset]
};

struct nsCursorImage {
  bool mHaveHotspot;
  float mHotspotX, mHotspotY;

  nsCursorImage();
  nsCursorImage(const nsCursorImage& aOther);
  ~nsCursorImage();

  nsCursorImage& operator=(const nsCursorImage& aOther);
  /*
   * We hide mImage and force access through the getter and setter so that we
   * can lock the images we use. Cursor images are likely to be small, so we
   * don't care about discarding them. See bug 512260.
   * */
  void SetImage(imgIRequest *aImage) {
    if (mImage)
      mImage->UnlockImage();
    mImage = aImage;
    if (mImage)
      mImage->LockImage();
  }
  imgIRequest* GetImage() const {
    return mImage;
  }

private:
  nsCOMPtr<imgIRequest> mImage;
};

struct nsStyleUserInterface {
  nsStyleUserInterface(void);
  nsStyleUserInterface(const nsStyleUserInterface& aOther);
  ~nsStyleUserInterface(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleUserInterface_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleUserInterface();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleUserInterface_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleUserInterface& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_STYLE_HINT_FRAMECHANGE,
                          NS_CombineHint(nsChangeHint_UpdateCursor,
                                         nsChangeHint_NeutralChange));
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  uint8_t   mUserInput;       // [inherited]
  uint8_t   mUserModify;      // [inherited] (modify-content)
  uint8_t   mUserFocus;       // [inherited] (auto-select)
  uint8_t   mWindowDragging;  // [inherited]

  uint8_t   mCursor;          // [inherited] See nsStyleConsts.h

  uint32_t mCursorArrayLength;
  nsCursorImage *mCursorArray;// [inherited] The specified URL values
                              //   and coordinates.  Takes precedence over
                              //   mCursor.  Zero-length array is represented
                              //   by null pointer.

  // Does not free mCursorArray; the caller is responsible for calling
  // |delete [] mCursorArray| first if it is needed.
  void CopyCursorArrayFrom(const nsStyleUserInterface& aSource);
};

struct nsStyleXUL {
  nsStyleXUL();
  nsStyleXUL(const nsStyleXUL& aSource);
  ~nsStyleXUL();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleXUL_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleXUL();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleXUL_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleXUL& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_STYLE_HINT_FRAMECHANGE;
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  float         mBoxFlex;               // [reset] see nsStyleConsts.h
  uint32_t      mBoxOrdinal;            // [reset] see nsStyleConsts.h
  uint8_t       mBoxAlign;              // [reset] see nsStyleConsts.h
  uint8_t       mBoxDirection;          // [reset] see nsStyleConsts.h
  uint8_t       mBoxOrient;             // [reset] see nsStyleConsts.h
  uint8_t       mBoxPack;               // [reset] see nsStyleConsts.h
  bool          mStretchStack;          // [reset] see nsStyleConsts.h
};

struct nsStyleColumn {
  explicit nsStyleColumn(nsPresContext* aPresContext);
  nsStyleColumn(const nsStyleColumn& aSource);
  ~nsStyleColumn();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleColumn_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleColumn();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleColumn_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleColumn& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_STYLE_HINT_FRAMECHANGE,
                          nsChangeHint_NeutralChange);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  /**
   * This is the maximum number of columns we can process. It's used in both
   * nsColumnSetFrame and nsRuleNode.
   */
  static const uint32_t kMaxColumnCount;

  uint32_t     mColumnCount; // [reset] see nsStyleConsts.h
  nsStyleCoord mColumnWidth; // [reset] coord, auto
  nsStyleCoord mColumnGap;   // [reset] coord, normal

  nscolor      mColumnRuleColor;  // [reset]
  uint8_t      mColumnRuleStyle;  // [reset]
  uint8_t      mColumnFill;  // [reset] see nsStyleConsts.h

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=271586#c43 for why
  // this is hard to replace with 'currentColor'.
  bool mColumnRuleColorIsForeground;

  void SetColumnRuleWidth(nscoord aWidth) {
    mColumnRuleWidth = NS_ROUND_BORDER_TO_PIXELS(aWidth, mTwipsPerPixel);
  }

  nscoord GetComputedColumnRuleWidth() const {
    return (IsVisibleBorderStyle(mColumnRuleStyle) ? mColumnRuleWidth : 0);
  }

protected:
  nscoord mColumnRuleWidth;  // [reset] coord
  nscoord mTwipsPerPixel;
};

enum nsStyleSVGPaintType {
  eStyleSVGPaintType_None = 1,
  eStyleSVGPaintType_Color,
  eStyleSVGPaintType_Server,
  eStyleSVGPaintType_ContextFill,
  eStyleSVGPaintType_ContextStroke
};

enum nsStyleSVGOpacitySource {
  eStyleSVGOpacitySource_Normal,
  eStyleSVGOpacitySource_ContextFillOpacity,
  eStyleSVGOpacitySource_ContextStrokeOpacity
};

struct nsStyleSVGPaint
{
  union {
    nscolor mColor;
    nsIURI *mPaintServer;
  } mPaint;
  nsStyleSVGPaintType mType;
  nscolor mFallbackColor;

  nsStyleSVGPaint() : mType(nsStyleSVGPaintType(0)) { mPaint.mPaintServer = nullptr; }
  ~nsStyleSVGPaint();
  void SetType(nsStyleSVGPaintType aType);
  nsStyleSVGPaint& operator=(const nsStyleSVGPaint& aOther);
  bool operator==(const nsStyleSVGPaint& aOther) const;

  bool operator!=(const nsStyleSVGPaint& aOther) const {
    return !(*this == aOther);
  }
};

struct nsStyleSVG {
  nsStyleSVG();
  nsStyleSVG(const nsStyleSVG& aSource);
  ~nsStyleSVG();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleSVG_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleSVG();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleSVG_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleSVG& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(NS_CombineHint(nsChangeHint_UpdateEffects,
             NS_CombineHint(nsChangeHint_NeedReflow, nsChangeHint_NeedDirtyReflow)), // XXX remove nsChangeHint_NeedDirtyReflow: bug 876085
                                         nsChangeHint_RepaintFrame);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns nsChangeHint_NeedReflow as a hint
    // not handled for descendants, and never returns
    // nsChangeHint_ClearAncestorIntrinsics at all.
    return nsChangeHint_NeedReflow;
  }

  nsStyleSVGPaint  mFill;             // [inherited]
  nsStyleSVGPaint  mStroke;           // [inherited]
  nsCOMPtr<nsIURI> mMarkerEnd;        // [inherited]
  nsCOMPtr<nsIURI> mMarkerMid;        // [inherited]
  nsCOMPtr<nsIURI> mMarkerStart;      // [inherited]
  nsStyleCoord    *mStrokeDasharray;  // [inherited] coord, percent, factor

  nsStyleCoord     mStrokeDashoffset; // [inherited] coord, percent, factor
  nsStyleCoord     mStrokeWidth;      // [inherited] coord, percent, factor

  float            mFillOpacity;      // [inherited]
  float            mStrokeMiterlimit; // [inherited]
  float            mStrokeOpacity;    // [inherited]

  uint32_t         mStrokeDasharrayLength;
  uint8_t          mClipRule;         // [inherited]
  uint8_t          mColorInterpolation; // [inherited] see nsStyleConsts.h
  uint8_t          mColorInterpolationFilters; // [inherited] see nsStyleConsts.h
  uint8_t          mFillRule;         // [inherited] see nsStyleConsts.h
  uint8_t          mImageRendering;   // [inherited] see nsStyleConsts.h
  uint8_t          mPaintOrder;       // [inherited] see nsStyleConsts.h
  uint8_t          mShapeRendering;   // [inherited] see nsStyleConsts.h
  uint8_t          mStrokeLinecap;    // [inherited] see nsStyleConsts.h
  uint8_t          mStrokeLinejoin;   // [inherited] see nsStyleConsts.h
  uint8_t          mTextAnchor;       // [inherited] see nsStyleConsts.h
  uint8_t          mTextRendering;    // [inherited] see nsStyleConsts.h

  // In SVG glyphs, whether we inherit fill or stroke opacity from the outer
  // text object.
  // Use 3 bits to avoid signedness problems in MSVC.
  nsStyleSVGOpacitySource mFillOpacitySource    : 3;
  nsStyleSVGOpacitySource mStrokeOpacitySource  : 3;

  // SVG glyph outer object inheritance for other properties
  bool mStrokeDasharrayFromObject   : 1;
  bool mStrokeDashoffsetFromObject  : 1;
  bool mStrokeWidthFromObject       : 1;

  bool HasMarker() const {
    return mMarkerStart || mMarkerMid || mMarkerEnd;
  }

  /**
   * Returns true if the stroke is not "none" and the stroke-opacity is greater
   * than zero. This ignores stroke-widths as that depends on the context.
   */
  bool HasStroke() const {
    return mStroke.mType != eStyleSVGPaintType_None && mStrokeOpacity > 0;
  }

  /**
   * Returns true if the fill is not "none" and the fill-opacity is greater
   * than zero.
   */
  bool HasFill() const {
    return mFill.mType != eStyleSVGPaintType_None && mFillOpacity > 0;
  }
};

class nsStyleBasicShape final {
public:
  enum Type {
    eInset,
    eCircle,
    eEllipse,
    ePolygon
  };

  explicit nsStyleBasicShape(Type type)
    : mType(type),
      mFillRule(NS_STYLE_FILL_RULE_NONZERO)
  {
    mPosition.SetInitialPercentValues(0.5f);
  }

  Type GetShapeType() const { return mType; }

  int32_t GetFillRule() const { return mFillRule; }
  void SetFillRule(int32_t aFillRule)
  {
    NS_ASSERTION(mType == ePolygon, "expected polygon");
    mFillRule = aFillRule;
  }

  typedef nsStyleBackground::Position Position;
  Position& GetPosition() {
    NS_ASSERTION(mType == eCircle || mType == eEllipse,
                 "expected circle or ellipse");
    return mPosition;
  }
  const Position& GetPosition() const {
    NS_ASSERTION(mType == eCircle || mType == eEllipse,
                 "expected circle or ellipse");
    return mPosition;
  }

  bool HasRadius() const {
    NS_ASSERTION(mType == eInset, "expected inset");
    nsStyleCoord zero;
    zero.SetCoordValue(0);
    NS_FOR_CSS_HALF_CORNERS(corner) {
      if (mRadius.Get(corner) != zero) {
        return true;
      }
    }
    return false;
  }
  nsStyleCorners& GetRadius() {
    NS_ASSERTION(mType == eInset, "expected inset");
    return mRadius;
  }
  const nsStyleCorners& GetRadius() const {
    NS_ASSERTION(mType == eInset, "expected inset");
    return mRadius;
  }

  // mCoordinates has coordinates for polygon or radii for
  // ellipse and circle.
  nsTArray<nsStyleCoord>& Coordinates()
  {
    return mCoordinates;
  }

  const nsTArray<nsStyleCoord>& Coordinates() const
  {
    return mCoordinates;
  }

  bool operator==(const nsStyleBasicShape& aOther) const
  {
    return mType == aOther.mType &&
           mFillRule == aOther.mFillRule &&
           mCoordinates == aOther.mCoordinates &&
           mPosition == aOther.mPosition &&
           mRadius == aOther.mRadius;
  }
  bool operator!=(const nsStyleBasicShape& aOther) const {
    return !(*this == aOther);
  }

  NS_INLINE_DECL_REFCOUNTING(nsStyleBasicShape);

private:
  ~nsStyleBasicShape() {}

  Type mType;
  int32_t mFillRule;

  // mCoordinates has coordinates for polygon or radii for
  // ellipse and circle.
  nsTArray<nsStyleCoord> mCoordinates;
  Position mPosition;
  nsStyleCorners mRadius;
};

struct nsStyleClipPath
{
  nsStyleClipPath();
  nsStyleClipPath(const nsStyleClipPath& aSource);
  ~nsStyleClipPath();

  nsStyleClipPath& operator=(const nsStyleClipPath& aOther);

  bool operator==(const nsStyleClipPath& aOther) const;
  bool operator!=(const nsStyleClipPath& aOther) const {
    return !(*this == aOther);
  }

  int32_t GetType() const {
    return mType;
  }

  nsIURI* GetURL() const {
    NS_ASSERTION(mType == NS_STYLE_CLIP_PATH_URL, "wrong clip-path type");
    return mURL;
  }
  void SetURL(nsIURI* aURL);

  nsStyleBasicShape* GetBasicShape() const {
    NS_ASSERTION(mType == NS_STYLE_CLIP_PATH_SHAPE, "wrong clip-path type");
    return mBasicShape;
  }

  void SetBasicShape(nsStyleBasicShape* mBasicShape,
                     uint8_t aSizingBox = NS_STYLE_CLIP_SHAPE_SIZING_NOBOX);

  uint8_t GetSizingBox() const { return mSizingBox; }
  void SetSizingBox(uint8_t aSizingBox);

private:
  void ReleaseRef();
  void* operator new(size_t) = delete;

  int32_t mType; // see NS_STYLE_CLIP_PATH_* constants in nsStyleConsts.h
  union {
    nsStyleBasicShape* mBasicShape;
    nsIURI* mURL;
  };
  uint8_t mSizingBox; // see NS_STYLE_CLIP_SHAPE_SIZING_* constants in nsStyleConsts.h
};

struct nsStyleFilter {
  nsStyleFilter();
  nsStyleFilter(const nsStyleFilter& aSource);
  ~nsStyleFilter();

  nsStyleFilter& operator=(const nsStyleFilter& aOther);

  bool operator==(const nsStyleFilter& aOther) const;
  bool operator!=(const nsStyleFilter& aOther) const {
    return !(*this == aOther);
  }

  int32_t GetType() const {
    return mType;
  }

  const nsStyleCoord& GetFilterParameter() const {
    NS_ASSERTION(mType != NS_STYLE_FILTER_DROP_SHADOW &&
                 mType != NS_STYLE_FILTER_URL &&
                 mType != NS_STYLE_FILTER_NONE, "wrong filter type");
    return mFilterParameter;
  }
  void SetFilterParameter(const nsStyleCoord& aFilterParameter,
                          int32_t aType);

  nsIURI* GetURL() const {
    NS_ASSERTION(mType == NS_STYLE_FILTER_URL, "wrong filter type");
    return mURL;
  }
  void SetURL(nsIURI* aURL);

  nsCSSShadowArray* GetDropShadow() const {
    NS_ASSERTION(mType == NS_STYLE_FILTER_DROP_SHADOW, "wrong filter type");
    return mDropShadow;
  }
  void SetDropShadow(nsCSSShadowArray* aDropShadow);

private:
  void ReleaseRef();

  int32_t mType; // see NS_STYLE_FILTER_* constants in nsStyleConsts.h
  nsStyleCoord mFilterParameter; // coord, percent, factor, angle
  union {
    nsIURI* mURL;
    nsCSSShadowArray* mDropShadow;
  };
};

template<>
struct nsTArray_CopyChooser<nsStyleFilter> {
  typedef nsTArray_CopyWithConstructors<nsStyleFilter> Type;
};

struct nsStyleSVGReset {
  nsStyleSVGReset();
  nsStyleSVGReset(const nsStyleSVGReset& aSource);
  ~nsStyleSVGReset();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleSVGReset_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleSVGReset();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleSVGReset_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleSVGReset& aOther) const;
  static nsChangeHint MaxDifference() {
    return NS_CombineHint(nsChangeHint_UpdateEffects,
            NS_CombineHint(nsChangeHint_UpdateOverflow, NS_STYLE_HINT_REFLOW));
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns the reflow hints that are sometimes
    // handled for descendants as hints not handled for descendants.
    return nsChangeHint_NeedReflow |
           nsChangeHint_ReflowChangesSizeOrPosition |
           nsChangeHint_ClearAncestorIntrinsics;
  }

  bool HasFilters() const {
    return mFilters.Length() > 0;
  }

  bool HasNonScalingStroke() const {
    return mVectorEffect == NS_STYLE_VECTOR_EFFECT_NON_SCALING_STROKE;
  }

  nsStyleClipPath mClipPath;          // [reset]
  nsTArray<nsStyleFilter> mFilters;   // [reset]
  nsCOMPtr<nsIURI> mMask;             // [reset]
  nscolor          mStopColor;        // [reset]
  nscolor          mFloodColor;       // [reset]
  nscolor          mLightingColor;    // [reset]

  float            mStopOpacity;      // [reset]
  float            mFloodOpacity;     // [reset]

  uint8_t          mDominantBaseline; // [reset] see nsStyleConsts.h
  uint8_t          mVectorEffect;     // [reset] see nsStyleConsts.h
  uint8_t          mMaskType;         // [reset] see nsStyleConsts.h
};

struct nsStyleVariables {
  nsStyleVariables();
  nsStyleVariables(const nsStyleVariables& aSource);
  ~nsStyleVariables();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->PresShell()->
      AllocateByObjectID(nsPresArena::nsStyleVariables_id, sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleVariables();
    aContext->PresShell()->
      FreeByObjectID(nsPresArena::nsStyleVariables_id, this);
  }

  nsChangeHint CalcDifference(const nsStyleVariables& aOther) const;
  static nsChangeHint MaxDifference() {
    return nsChangeHint(0);
  }
  static nsChangeHint DifferenceAlwaysHandledForDescendants() {
    // CalcDifference never returns nsChangeHint_NeedReflow or
    // nsChangeHint_ClearAncestorIntrinsics at all.
    return nsChangeHint(0);
  }

  mozilla::CSSVariableValues mVariables;
};

#endif /* nsStyleStruct_h___ */
