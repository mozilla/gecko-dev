/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTEXT_H
#define GFX_CONTEXT_H

#include "gfxTypes.h"

#include "gfxASurface.h"
#include "gfxPoint.h"
#include "gfxRect.h"
#include "gfxMatrix.h"
#include "gfxPattern.h"
#include "gfxPath.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

#include "mozilla/gfx/2D.h"

typedef struct _cairo cairo_t;
struct GlyphBufferAzure;
template <typename T> class FallibleTArray;

/**
 * This is the main class for doing actual drawing. It is initialized using
 * a surface and can be drawn on. It manages various state information like
 * a current transformation matrix (CTM), a current path, current color,
 * etc.
 *
 * All drawing happens by creating a path and then stroking or filling it.
 * The functions like Rectangle and Arc do not do any drawing themselves.
 * When a path is drawn (stroked or filled), it is filled/stroked with a
 * pattern set by SetPattern, SetColor or SetSource.
 *
 * Note that the gfxContext takes coordinates in device pixels,
 * as opposed to app units.
 */
class gfxContext MOZ_FINAL {
    NS_INLINE_DECL_REFCOUNTING(gfxContext)

public:
    /**
     * Initialize this context from a surface.
     */
    gfxContext(gfxASurface *surface);

    /**
     * Initialize this context from a DrawTarget.
     * Strips any transform from aTarget.
     * aTarget will be flushed in the gfxContext's destructor.
     */
    gfxContext(mozilla::gfx::DrawTarget *aTarget,
               const mozilla::gfx::Point& aDeviceOffset = mozilla::gfx::Point());

    /**
     * Create a new gfxContext wrapping aTarget and preserving aTarget's
     * transform. Note that the transform is moved from aTarget to the resulting
     * gfxContext, aTarget will no longer have its transform.
     */
    static already_AddRefed<gfxContext> ContextForDrawTarget(mozilla::gfx::DrawTarget* aTarget);

    /**
     * Return the surface that this gfxContext was created with
     */
    gfxASurface *OriginalSurface();

    /**
     * Return the current transparency group target, if any, along
     * with its device offsets from the top.  If no group is
     * active, returns the surface the gfxContext was created with,
     * and 0,0 in dx,dy.
     */
    already_AddRefed<gfxASurface> CurrentSurface(gfxFloat *dx, gfxFloat *dy);
    already_AddRefed<gfxASurface> CurrentSurface() {
        return CurrentSurface(nullptr, nullptr);
    }

    /**
     * Return the raw cairo_t object.
     * XXX this should go away at some point.
     */
    cairo_t *GetCairo();

    mozilla::gfx::DrawTarget *GetDrawTarget() { return mDT; }

    /**
     * Returns true if the cairo context is in an error state.
     */
    bool HasError();

    /**
     ** State
     **/
    // XXX document exactly what bits are saved
    void Save();
    void Restore();

    /**
     ** Paths & Drawing
     **/

    /**
     * Stroke the current path using the current settings (such as line
     * width and color).
     * A path is set up using functions such as Line, Rectangle and Arc.
     *
     * Does not consume the current path.
     */
    void Stroke();
    /**
     * Fill the current path according to the current settings.
     *
     * Does not consume the current path.
     */
    void Fill();

    /**
     * Fill the current path according to the current settings and
     * with |aOpacity|.
     *
     * Does not consume the current path.
     */
    void FillWithOpacity(gfxFloat aOpacity);

    /**
     * Forgets the current path.
     */
    void NewPath();

    /**
     * Closes the path, i.e. connects the last drawn point to the first one.
     *
     * Filling a path will implicitly close it.
     */
    void ClosePath();

    /**
     * Copies the current path and returns the copy.
     */
    already_AddRefed<gfxPath> CopyPath();

    /**
     * Appends the given path to the current path.
     */
    void SetPath(gfxPath* path);

    /**
     * Moves the pen to a new point without drawing a line.
     */
    void MoveTo(const gfxPoint& pt);

    /**
     * Creates a new subpath starting at the current point.
     * Equivalent to MoveTo(CurrentPoint()).
     */
    void NewSubPath();

    /**
     * Returns the current point in the current path.
     */
    gfxPoint CurrentPoint();

    /**
     * Draws a line from the current point to pt.
     *
     * @see MoveTo
     */
    void LineTo(const gfxPoint& pt);

    /**
     * Draws a cubic Bézier curve with control points pt1, pt2 and pt3.
     */
    void CurveTo(const gfxPoint& pt1, const gfxPoint& pt2, const gfxPoint& pt3);

    /**
     * Draws a quadratic Bézier curve with control points pt1, pt2 and pt3.
     */
    void QuadraticCurveTo(const gfxPoint& pt1, const gfxPoint& pt2);

    /**
     * Draws a clockwise arc (i.e. a circle segment).
     * @param center The center of the circle
     * @param radius The radius of the circle
     * @param angle1 Starting angle for the segment
     * @param angle2 Ending angle
     */
    void Arc(const gfxPoint& center, gfxFloat radius,
             gfxFloat angle1, gfxFloat angle2);

    /**
     * Draws a counter-clockwise arc (i.e. a circle segment).
     * @param center The center of the circle
     * @param radius The radius of the circle
     * @param angle1 Starting angle for the segment
     * @param angle2 Ending angle
     */

    void NegativeArc(const gfxPoint& center, gfxFloat radius,
                     gfxFloat angle1, gfxFloat angle2);

    // path helpers
    /**
     * Draws a line from start to end.
     */
    void Line(const gfxPoint& start, const gfxPoint& end); // XXX snapToPixels option?

    /**
     * Draws the rectangle given by rect.
     * @param snapToPixels ?
     */
    void Rectangle(const gfxRect& rect, bool snapToPixels = false);
    void SnappedRectangle(const gfxRect& rect) { return Rectangle(rect, true); }

    /**
     * Draw an ellipse at the center corner with the given dimensions.
     * It extends dimensions.width / 2.0 in the horizontal direction
     * from the center, and dimensions.height / 2.0 in the vertical
     * direction.
     */
    void Ellipse(const gfxPoint& center, const gfxSize& dimensions);

    /**
     * Draw a polygon from the given points
     */
    void Polygon(const gfxPoint *points, uint32_t numPoints);

    /*
     * Draw a rounded rectangle, with the given outer rect and
     * corners.  The corners specify the radii of the two axes of an
     * ellipse (the horizontal and vertical directions given by the
     * width and height, respectively).  By default the ellipse is
     * drawn in a clockwise direction; if draw_clockwise is false,
     * then it's drawn counterclockwise.
     */
    void RoundedRectangle(const gfxRect& rect,
                          const gfxCornerSizes& corners,
                          bool draw_clockwise = true);

    /**
     ** Transformation Matrix manipulation
     **/

    /**
     * Adds a translation to the current matrix. This translation takes place
     * before the previously set transformations.
     */
    void Translate(const gfxPoint& pt);

    /**
     * Adds a scale to the current matrix. This scaling takes place before the
     * previously set transformations.
     */
    void Scale(gfxFloat x, gfxFloat y);

    /**
     * Adds a rotation around the origin to the current matrix. This rotation
     * takes place before the previously set transformations.
     *
     * @param angle The angle in radians.
     */
    void Rotate(gfxFloat angle);

    /**
     * Post-multiplies 'other' onto the current CTM, i.e. this
     * matrix's transformation will take place before the previously set
     * transformations.
     */
    void Multiply(const gfxMatrix& other);
    /**
     * As "Multiply", but also nudges any entries in the resulting matrix that
     * are close to an integer to that integer, to correct for
     * compounded rounding errors.
     */
    void MultiplyAndNudgeToIntegers(const gfxMatrix& other);

    /**
     * Replaces the current transformation matrix with matrix.
     */
    void SetMatrix(const gfxMatrix& matrix);

    /**
     * Sets the transformation matrix to the identity matrix.
     */
    void IdentityMatrix();

    /**
     * Returns the current transformation matrix.
     */
    gfxMatrix CurrentMatrix() const;

    /**
     * Snap components of the current matrix that are close to integers
     * to integers. In particular, components that are integral when
     * converted to single precision are set to those integers.
     */
    void NudgeCurrentMatrixToIntegers();

    /**
     * Converts a point from device to user coordinates using the inverse
     * transformation matrix.
     */
    gfxPoint DeviceToUser(const gfxPoint& point) const;

    /**
     * Converts a size from device to user coordinates. This does not apply
     * translation components of the matrix.
     */
    gfxSize DeviceToUser(const gfxSize& size) const;

    /**
     * Converts a rectangle from device to user coordinates; this has the
     * same effect as using DeviceToUser on both the rectangle's point and
     * size.
     */
    gfxRect DeviceToUser(const gfxRect& rect) const;

    /**
     * Converts a point from user to device coordinates using the transformation
     * matrix.
     */
    gfxPoint UserToDevice(const gfxPoint& point) const;

    /**
     * Converts a size from user to device coordinates. This does not apply
     * translation components of the matrix.
     */
    gfxSize UserToDevice(const gfxSize& size) const;

    /**
     * Converts a rectangle from user to device coordinates.  The
     * resulting rectangle is the minimum device-space rectangle that
     * encloses the user-space rectangle given.
     */
    gfxRect UserToDevice(const gfxRect& rect) const;

    /**
     * Takes the given rect and tries to align it to device pixels.  If
     * this succeeds, the method will return true, and the rect will
     * be in device coordinates (already transformed by the CTM).  If it 
     * fails, the method will return false, and the rect will not be
     * changed.
     *
     * If ignoreScale is true, then snapping will take place even if
     * the CTM has a scale applied.  Snapping never takes place if
     * there is a rotation in the CTM.
     */
    bool UserToDevicePixelSnapped(gfxRect& rect, bool ignoreScale = false) const;

    /**
     * Takes the given point and tries to align it to device pixels.  If
     * this succeeds, the method will return true, and the point will
     * be in device coordinates (already transformed by the CTM).  If it 
     * fails, the method will return false, and the point will not be
     * changed.
     *
     * If ignoreScale is true, then snapping will take place even if
     * the CTM has a scale applied.  Snapping never takes place if
     * there is a rotation in the CTM.
     */
    bool UserToDevicePixelSnapped(gfxPoint& pt, bool ignoreScale = false) const;

    /**
     * Attempts to pixel snap the rectangle, add it to the current
     * path, and to set pattern as the current painting source.  This
     * should be used for drawing filled pixel-snapped rectangles (like
     * images), because the CTM at the time of the SetPattern call needs
     * to have a snapped translation, or you get smeared images.
     */
    void PixelSnappedRectangleAndSetPattern(const gfxRect& rect, gfxPattern *pattern);

    /**
     ** Painting sources
     **/

    /**
     * Set a solid color to use for drawing.  This color is in the device color space
     * and is not transformed.
     */
    void SetDeviceColor(const gfxRGBA& c);

    /**
     * Gets the current color.  It's returned in the device color space.
     * returns false if there is something other than a color
     *         set as the current source (pattern, surface, etc)
     */
    bool GetDeviceColor(gfxRGBA& c);

    /**
     * Set a solid color in the sRGB color space to use for drawing.
     * If CMS is not enabled, the color is treated as a device-space color
     * and this call is identical to SetDeviceColor().
     */
    void SetColor(const gfxRGBA& c);

    /**
     * Uses a surface for drawing. This is a shorthand for creating a
     * pattern and setting it.
     *
     * @param offset from the source surface, to use only part of it.
     *        May need to make it negative.
     */
    void SetSource(gfxASurface *surface, const gfxPoint& offset = gfxPoint(0.0, 0.0));

    /**
     * Uses a pattern for drawing.
     */
    void SetPattern(gfxPattern *pattern);

    /**
     * Get the source pattern (solid color, normal pattern, surface, etc)
     */
    already_AddRefed<gfxPattern> GetPattern();

    /**
     ** Painting
     **/
    /**
     * Paints the current source surface/pattern everywhere in the current
     * clip region.
     */
    void Paint(gfxFloat alpha = 1.0);

    /**
     ** Painting with a Mask
     **/
    /**
     * Like Paint, except that it only draws the source where pattern is
     * non-transparent.
     */
    void Mask(gfxPattern *pattern);

    /**
     * Shorthand for creating a pattern and calling the pattern-taking
     * variant of Mask.
     */
    void Mask(gfxASurface *surface, const gfxPoint& offset = gfxPoint(0.0, 0.0));

    void Mask(mozilla::gfx::SourceSurface *surface, const mozilla::gfx::Point& offset = mozilla::gfx::Point());

    /**
     ** Shortcuts
     **/

    /**
     * Creates a new path with a rectangle from 0,0 to size.w,size.h
     * and calls cairo_fill.
     */
    void DrawSurface(gfxASurface *surface, const gfxSize& size);

    /**
     ** Line Properties
     **/

    typedef enum {
        gfxLineSolid,
        gfxLineDashed,
        gfxLineDotted
    } gfxLineType;

    void SetDash(gfxLineType ltype);
    void SetDash(gfxFloat *dashes, int ndash, gfxFloat offset);
    // Return true if dashing is set, false if it's not enabled or the
    // context is in an error state.  |offset| can be nullptr to mean
    // "don't care".
    bool CurrentDash(FallibleTArray<gfxFloat>& dashes, gfxFloat* offset) const;
    // Returns 0.0 if dashing isn't enabled.
    gfxFloat CurrentDashOffset() const;

    /**
     * Sets the line width that's used for line drawing.
     */
    void SetLineWidth(gfxFloat width);

    /**
     * Returns the currently set line width.
     *
     * @see SetLineWidth
     */
    gfxFloat CurrentLineWidth() const;

    enum GraphicsLineCap {
        LINE_CAP_BUTT,
        LINE_CAP_ROUND,
        LINE_CAP_SQUARE
    };
    /**
     * Sets the line caps, i.e. how line endings are drawn.
     */
    void SetLineCap(GraphicsLineCap cap);
    GraphicsLineCap CurrentLineCap() const;

    enum GraphicsLineJoin {
        LINE_JOIN_MITER,
        LINE_JOIN_ROUND,
        LINE_JOIN_BEVEL
    };
    /**
     * Sets the line join, i.e. how the connection between two lines is
     * drawn.
     */
    void SetLineJoin(GraphicsLineJoin join);
    GraphicsLineJoin CurrentLineJoin() const;

    void SetMiterLimit(gfxFloat limit);
    gfxFloat CurrentMiterLimit() const;

    /**
     ** Fill Properties
     **/

    enum FillRule {
        FILL_RULE_WINDING,
        FILL_RULE_EVEN_ODD
    };
    void SetFillRule(FillRule rule);
    FillRule CurrentFillRule() const;

    /**
     ** Operators and Rendering control
     **/

    // define enum for operators (clear, src, dst, etc)
    enum GraphicsOperator {
        OPERATOR_CLEAR,
        OPERATOR_SOURCE,

        OPERATOR_OVER,
        OPERATOR_IN,
        OPERATOR_OUT,
        OPERATOR_ATOP,

        OPERATOR_DEST,
        OPERATOR_DEST_OVER,
        OPERATOR_DEST_IN,
        OPERATOR_DEST_OUT,
        OPERATOR_DEST_ATOP,

        OPERATOR_XOR,
        OPERATOR_ADD,
        OPERATOR_SATURATE,

        OPERATOR_MULTIPLY,
        OPERATOR_SCREEN,
        OPERATOR_OVERLAY,
        OPERATOR_DARKEN,
        OPERATOR_LIGHTEN,
        OPERATOR_COLOR_DODGE,
        OPERATOR_COLOR_BURN,
        OPERATOR_HARD_LIGHT,
        OPERATOR_SOFT_LIGHT,
        OPERATOR_DIFFERENCE,
        OPERATOR_EXCLUSION,
        OPERATOR_HUE,
        OPERATOR_SATURATION,
        OPERATOR_COLOR,
        OPERATOR_LUMINOSITY
    };
    /**
     * Sets the operator used for all further drawing. The operator affects
     * how drawing something will modify the destination. For example, the
     * OVER operator will do alpha blending of source and destination, while
     * SOURCE will replace the destination with the source.
     *
     * Note that if the flag FLAG_SIMPLIFY_OPERATORS is set on this
     * gfxContext, the actual operator set might change for optimization
     * purposes.  Check the comments below around that flag.
     */
    void SetOperator(GraphicsOperator op);
    GraphicsOperator CurrentOperator() const;

    /**
     * MODE_ALIASED means that only pixels whose centers are in the drawn area
     * should be modified, and they should be modified to take the value drawn
     * at the pixel center.
     */
    enum AntialiasMode {
        MODE_ALIASED,
        MODE_COVERAGE
    };
    void SetAntialiasMode(AntialiasMode mode);
    AntialiasMode CurrentAntialiasMode() const;

    /**
     ** Clipping
     **/

    /**
     * Clips all further drawing to the current path.
     * This does not consume the current path.
     */
    void Clip();

    /**
     * Undoes any clipping. Further drawings will only be restricted by the
     * surface dimensions.
     */
    void ResetClip();

    /**
     * Helper functions that will create a rect path and call Clip().
     * Any current path will be destroyed by these functions!
     */
    void Clip(const gfxRect& rect); // will clip to a rect

    /**
     * This will ensure that the surface actually has its clip set.
     * Useful if you are doing native drawing.
     */
    void UpdateSurfaceClip();

    /**
     * This will return the current bounds of the clip region in user
     * space.
     */
    gfxRect GetClipExtents();

    /**
     * Returns true if the given rectangle is fully contained in the current clip. 
     * This is conservative; it may return false even when the given rectangle is 
     * fully contained by the current clip.
     */
    bool ClipContainsRect(const gfxRect& aRect);

    /**
     * Groups
     */
    void PushGroup(gfxContentType content = gfxContentType::COLOR);
    /**
     * Like PushGroup, but if the current surface is gfxContentType::COLOR and
     * content is gfxContentType::COLOR_ALPHA, makes the pushed surface gfxContentType::COLOR
     * instead and copies the contents of the current surface to the pushed
     * surface. This is good for pushing opacity groups, since blending the
     * group back to the current surface with some alpha applied will give
     * the correct results and using an opaque pushed surface gives better
     * quality and performance.
     * This API really only makes sense if you do a PopGroupToSource and
     * immediate Paint with OPERATOR_OVER.
     */
    void PushGroupAndCopyBackground(gfxContentType content = gfxContentType::COLOR);
    already_AddRefed<gfxPattern> PopGroup();
    void PopGroupToSource();

    /**
     ** Hit Testing - check if given point is in the current path
     **/
    bool PointInFill(const gfxPoint& pt);
    bool PointInStroke(const gfxPoint& pt);

    /**
     ** Extents - returns user space extent of current path
     **/
    gfxRect GetUserPathExtent();
    gfxRect GetUserFillExtent();
    gfxRect GetUserStrokeExtent();

    mozilla::gfx::Point GetDeviceOffset() const;

    /**
     ** Flags
     **/

    enum {
        /* If this flag is set, operators other than CLEAR, SOURCE, or
         * OVER will be converted to OVER before being sent to cairo.
         *
         * This is most useful with a printing surface, where
         * operators such as ADD are used to avoid seams for on-screen
         * display, but where such errors aren't noticeable in print.
         * This approach is currently used in border rendering.
         *
         * However, when printing complex renderings such as SVG,
         * care should be taken to clear this flag.
         */
        FLAG_SIMPLIFY_OPERATORS = (1 << 0),
        /**
         * When this flag is set, snapping to device pixels is disabled.
         * It simply never does anything.
         */
        FLAG_DISABLE_SNAPPING = (1 << 1),
        /**
         * Disable copying of backgrounds in PushGroupAndCopyBackground.
         */
        FLAG_DISABLE_COPY_BACKGROUND = (1 << 2)
    };

    void SetFlag(int32_t aFlag) { mFlags |= aFlag; }
    void ClearFlag(int32_t aFlag) { mFlags &= ~aFlag; }
    int32_t GetFlags() const { return mFlags; }

    bool IsCairo() const { return !mDT; }

    // Work out whether cairo will snap inter-glyph spacing to pixels.
    void GetRoundOffsetsToPixels(bool *aRoundX, bool *aRoundY);

#ifdef MOZ_DUMP_PAINTING
    /**
     * Debug functions to encode the current surface as a PNG and export it.
     */

    /**
     * Writes a binary PNG file.
     */
    void WriteAsPNG(const char* aFile);

    /**
     * Write as a PNG encoded Data URL to stdout.
     */
    void DumpAsDataURL();

    /**
     * Copy a PNG encoded Data URL to the clipboard.
     */
    void CopyAsDataURL();
#endif

    static mozilla::gfx::UserDataKey sDontUseAsSourceKey;

private:
    ~gfxContext();

  friend class GeneralPattern;
  friend struct GlyphBufferAzure;

  typedef mozilla::gfx::Matrix Matrix;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Color Color;
  typedef mozilla::gfx::StrokeOptions StrokeOptions;
  typedef mozilla::gfx::Float Float;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::CompositionOp CompositionOp;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::PathBuilder PathBuilder;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  
  struct AzureState {
    AzureState()
      : op(mozilla::gfx::CompositionOp::OP_OVER)
      , opIsClear(false)
      , color(0, 0, 0, 1.0f)
      , clipWasReset(false)
      , fillRule(mozilla::gfx::FillRule::FILL_WINDING)
      , aaMode(mozilla::gfx::AntialiasMode::SUBPIXEL)
      , patternTransformChanged(false)
    {}

    mozilla::gfx::CompositionOp op;
    bool opIsClear;
    Color color;
    nsRefPtr<gfxPattern> pattern;
    nsRefPtr<gfxASurface> sourceSurfCairo;
    mozilla::RefPtr<SourceSurface> sourceSurface;
    mozilla::gfx::Point sourceSurfaceDeviceOffset;
    Matrix surfTransform;
    Matrix transform;
    struct PushedClip {
      mozilla::RefPtr<Path> path;
      Rect rect;
      Matrix transform;
    };
    nsTArray<PushedClip> pushedClips;
    nsTArray<Float> dashPattern;
    bool clipWasReset;
    mozilla::gfx::FillRule fillRule;
    StrokeOptions strokeOptions;
    mozilla::RefPtr<DrawTarget> drawTarget;
    mozilla::RefPtr<DrawTarget> parentTarget;
    mozilla::gfx::AntialiasMode aaMode;
    bool patternTransformChanged;
    Matrix patternTransform;
    // This is used solely for using minimal intermediate surface size.
    mozilla::gfx::Point deviceOffset;
  };

  // This ensures mPath contains a valid path (in user space!)
  void EnsurePath();
  // This ensures mPathBuilder contains a valid PathBuilder (in user space!)
  void EnsurePathBuilder();
  void FillAzure(mozilla::gfx::Float aOpacity);
  void PushClipsToDT(mozilla::gfx::DrawTarget *aDT);
  CompositionOp GetOp();
  void ChangeTransform(const mozilla::gfx::Matrix &aNewMatrix, bool aUpdatePatternTransform = true);
  Rect GetAzureDeviceSpaceClipBounds();
  Matrix GetDeviceTransform() const;
  Matrix GetDTTransform() const;
  void PushNewDT(gfxContentType content);

  bool mPathIsRect;
  bool mTransformChanged;
  Matrix mPathTransform;
  Rect mRect;
  mozilla::RefPtr<PathBuilder> mPathBuilder;
  mozilla::RefPtr<Path> mPath;
  Matrix mTransform;
  nsTArray<AzureState> mStateStack;

  AzureState &CurrentState() { return mStateStack[mStateStack.Length() - 1]; }
  const AzureState &CurrentState() const { return mStateStack[mStateStack.Length() - 1]; }

  cairo_t *mCairo;
  cairo_t *mRefCairo;
  nsRefPtr<gfxASurface> mSurface;
  int32_t mFlags;

  mozilla::RefPtr<DrawTarget> mDT;
  mozilla::RefPtr<DrawTarget> mOriginalDT;
};

/**
 * Sentry helper class for functions with multiple return points that need to
 * call Save() on a gfxContext and have Restore() called automatically on the
 * gfxContext before they return.
 */
class gfxContextAutoSaveRestore
{
public:
  gfxContextAutoSaveRestore() : mContext(nullptr) {}

  gfxContextAutoSaveRestore(gfxContext *aContext) : mContext(aContext) {
    mContext->Save();
  }

  ~gfxContextAutoSaveRestore() {
    Restore();
  }

  void SetContext(gfxContext *aContext) {
    NS_ASSERTION(!mContext, "Not going to call Restore() on some context!!!");
    mContext = aContext;
    mContext->Save();    
  }

  void EnsureSaved(gfxContext *aContext) {
    MOZ_ASSERT(!mContext || mContext == aContext, "wrong context");
    if (!mContext) {
        mContext = aContext;
        mContext->Save();
    }
  }

  void Restore() {
    if (mContext) {
      mContext->Restore();
      mContext = nullptr;
    }
  }

private:
  gfxContext *mContext;
};

/**
 * Sentry helper class for functions with multiple return points that need to
 * back up the current path of a context and have it automatically restored
 * before they return. This class assumes that the transformation matrix will
 * be the same when Save and Restore are called. The calling function must
 * ensure that this is the case or the path will be copied incorrectly.
 */
class gfxContextPathAutoSaveRestore
{
public:
    gfxContextPathAutoSaveRestore() : mContext(nullptr) {}

    gfxContextPathAutoSaveRestore(gfxContext *aContext, bool aSave = true) : mContext(aContext)
    {
        if (aSave)
            Save();       
    }

    ~gfxContextPathAutoSaveRestore()
    {
        Restore();
    }

    void SetContext(gfxContext *aContext, bool aSave = true)
    {
        mContext = aContext;
        if (aSave)
            Save();
    }

    /**
     * If a path is already saved, does nothing. Else copies the current path
     * so that it may be restored.
     */
    void Save()
    {
        if (!mPath && mContext) {
            mPath = mContext->CopyPath();
        }
    }

    /**
     * If no path is saved, does nothing. Else replaces the context's path with
     * a copy of the saved one, and clears the saved path.
     */
    void Restore()
    {
        if (mPath) {
            mContext->SetPath(mPath);
            mPath = nullptr;
        }
    }

private:
    gfxContext *mContext;

    nsRefPtr<gfxPath> mPath;
};

/**
 * Sentry helper class for functions with multiple return points that need to
 * back up the current matrix of a context and have it automatically restored
 * before they return.
 */
class gfxContextMatrixAutoSaveRestore
{
public:
    gfxContextMatrixAutoSaveRestore() :
        mContext(nullptr)
    {
    }

    gfxContextMatrixAutoSaveRestore(gfxContext *aContext) :
        mContext(aContext), mMatrix(aContext->CurrentMatrix())
    {
    }

    ~gfxContextMatrixAutoSaveRestore()
    {
        if (mContext) {
            mContext->SetMatrix(mMatrix);
        }
    }

    void SetContext(gfxContext *aContext)
    {
        NS_ASSERTION(!mContext, "Not going to restore the matrix on some context!");
        mContext = aContext;
        mMatrix = aContext->CurrentMatrix();
    }

    void Restore()
    {
        if (mContext) {
            mContext->SetMatrix(mMatrix);
        }
    }

    const gfxMatrix& Matrix()
    {
        MOZ_ASSERT(mContext, "mMatrix doesn't contain a useful matrix");
        return mMatrix;
    }

private:
    gfxContext *mContext;
    gfxMatrix   mMatrix;
};


class gfxContextAutoDisableSubpixelAntialiasing {
public:
    gfxContextAutoDisableSubpixelAntialiasing(gfxContext *aContext, bool aDisable)
    {
        if (aDisable) {
            if (aContext->IsCairo()) {
                mSurface = aContext->CurrentSurface();
                if (!mSurface) {
                  return;
                }
                mSubpixelAntialiasingEnabled = mSurface->GetSubpixelAntialiasingEnabled();
                mSurface->SetSubpixelAntialiasingEnabled(false);
            } else {
                mDT = aContext->GetDrawTarget();

                mSubpixelAntialiasingEnabled = mDT->GetPermitSubpixelAA();
                mDT->SetPermitSubpixelAA(false);
            }
        }
    }
    ~gfxContextAutoDisableSubpixelAntialiasing()
    {
        if (mSurface) {
            mSurface->SetSubpixelAntialiasingEnabled(mSubpixelAntialiasingEnabled);
        } else if (mDT) {
            mDT->SetPermitSubpixelAA(mSubpixelAntialiasingEnabled);
        }
    }

private:
    nsRefPtr<gfxASurface> mSurface;
    mozilla::RefPtr<mozilla::gfx::DrawTarget> mDT;
    bool mSubpixelAntialiasingEnabled;
};

#endif /* GFX_CONTEXT_H */
