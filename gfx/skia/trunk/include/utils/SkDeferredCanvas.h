/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkDeferredCanvas_DEFINED
#define SkDeferredCanvas_DEFINED

#include "SkCanvas.h"
#include "SkPixelRef.h"

class SkDeferredDevice;
class SkImage;
class SkSurface;

/** \class SkDeferredCanvas
    Subclass of SkCanvas that encapsulates an SkPicture or SkGPipe for deferred
    drawing. The main difference between this class and SkPictureRecord (the
    canvas provided by SkPicture) is that this is a full drop-in replacement
    for SkCanvas, while SkPictureRecord only supports draw operations.
    SkDeferredCanvas will transparently trigger the flushing of deferred
    draw operations when an attempt is made to access the pixel data.
*/
class SK_API SkDeferredCanvas : public SkCanvas {
public:
    class SK_API NotificationClient;

    /** Construct a canvas with the specified surface to draw into.
        This factory must be used for newImageSnapshot to work.
        @param surface Specifies a surface for the canvas to draw into.
     */
    static SkDeferredCanvas* Create(SkSurface* surface);

//    static SkDeferredCanvas* Create(SkBaseDevice* device);

    virtual ~SkDeferredCanvas();

    /**
     *  Specify the surface to be used by this canvas. Calling setSurface will
     *  release the previously set surface or device. Takes a reference on the
     *  surface.
     *
     *  @param surface The surface that the canvas will raw into
     *  @return The surface argument, for convenience.
     */
    SkSurface* setSurface(SkSurface* surface);

    /**
     *  Specify a NotificationClient to be used by this canvas. Calling
     *  setNotificationClient will release the previously set
     *  NotificationClient, if any. SkDeferredCanvas does not take ownership
     *  of the notification client.  Therefore user code is resposible
     *  for its destruction.  The notification client must be unregistered
     *  by calling setNotificationClient(NULL) if it is destroyed before
     *  this canvas.
     *  Note: Must be called after the device is set with setDevice.
     *
     *  @param notificationClient interface for dispatching notifications
     *  @return The notificationClient argument, for convenience.
     */
    NotificationClient* setNotificationClient(NotificationClient* notificationClient);

    /**
     *  Enable or disable deferred drawing. When deferral is disabled,
     *  pending draw operations are immediately flushed and from then on,
     *  the SkDeferredCanvas behaves just like a regular SkCanvas.
     *  This method must not be called while the save/restore stack is in use.
     *  @param deferred true/false
     */
    void setDeferredDrawing(bool deferred);

    /**
     *  Returns true if deferred drawing is currenlty enabled.
     */
    bool isDeferredDrawing() const;

    /**
     *  Returns true if the canvas contains a fresh frame.  A frame is
     *  considered fresh when its content do not depend on the contents
     *  of the previous frame. For example, if a canvas is cleared before
     *  drawing each frame, the frames will all be considered fresh.
     *  A frame is defined as the graphics image produced by as a result
     *  of all the canvas draws operation executed between two successive
     *  calls to isFreshFrame.  The result of isFreshFrame is computed
     *  conservatively, so it may report false negatives.
     */
    bool isFreshFrame() const;

    /**
     *  Returns true if the canvas has recorded draw commands that have
     *  not yet been played back.
     */
    bool hasPendingCommands() const;

    /**
     *  Flushes pending draw commands, if any, and returns an image of the
     *  current state of the surface pixels up to this point. Subsequent
     *  changes to the surface (by drawing into its canvas) will not be
     *  reflected in this image.  Will return NULL if the deferred canvas
     *  was not constructed from an SkSurface.
     */
    SkImage* newImageSnapshot();

    /**
     *  Specify the maximum number of bytes to be allocated for the purpose
     *  of recording draw commands to this canvas.  The default limit, is
     *  64MB.
     *  @param maxStorage The maximum number of bytes to be allocated.
     */
    void setMaxRecordingStorage(size_t maxStorage);

    /**
     *  Returns the number of bytes currently allocated for the purpose of
     *  recording draw commands.
     */
    size_t storageAllocatedForRecording() const;

    /**
     * Attempt to reduce the storage allocated for recording by evicting
     * cache resources.
     * @param bytesToFree minimum number of bytes that should be attempted to
     *   be freed.
     * @return number of bytes actually freed.
     */
    size_t freeMemoryIfPossible(size_t bytesToFree);

    /**
     * Specifies the maximum size (in bytes) allowed for a given image to be
     * rendered using the deferred canvas.
     */
    void setBitmapSizeThreshold(size_t sizeThreshold);
    size_t getBitmapSizeThreshold() const { return fBitmapSizeThreshold; }

    /**
     * Executes all pending commands without drawing
     */
    void silentFlush();

    // Overrides of the SkCanvas interface
    virtual bool isDrawingToLayer() const SK_OVERRIDE;
    virtual void clear(SkColor) SK_OVERRIDE;
    virtual void drawPaint(const SkPaint& paint) SK_OVERRIDE;
    virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[],
                            const SkPaint& paint) SK_OVERRIDE;
    virtual void drawOval(const SkRect&, const SkPaint& paint) SK_OVERRIDE;
    virtual void drawRect(const SkRect& rect, const SkPaint& paint) SK_OVERRIDE;
    virtual void drawRRect(const SkRRect&, const SkPaint& paint) SK_OVERRIDE;
    virtual void drawPath(const SkPath& path, const SkPaint& paint)
                          SK_OVERRIDE;
    virtual void drawBitmap(const SkBitmap& bitmap, SkScalar left,
                            SkScalar top, const SkPaint* paint)
                            SK_OVERRIDE;
    virtual void drawBitmapRectToRect(const SkBitmap& bitmap, const SkRect* src,
                                      const SkRect& dst, const SkPaint* paint,
                                      DrawBitmapRectFlags flags) SK_OVERRIDE;

    virtual void drawBitmapMatrix(const SkBitmap& bitmap, const SkMatrix& m,
                                  const SkPaint* paint) SK_OVERRIDE;
    virtual void drawBitmapNine(const SkBitmap& bitmap, const SkIRect& center,
                                const SkRect& dst, const SkPaint* paint)
                                SK_OVERRIDE;
    virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
                            const SkPaint* paint) SK_OVERRIDE;
    virtual void drawVertices(VertexMode vmode, int vertexCount,
                              const SkPoint vertices[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint& paint) SK_OVERRIDE;
    virtual SkDrawFilter* setDrawFilter(SkDrawFilter* filter) SK_OVERRIDE;

protected:
    virtual void willSave() SK_OVERRIDE;
    virtual SaveLayerStrategy willSaveLayer(const SkRect*, const SkPaint*, SaveFlags) SK_OVERRIDE;
    virtual void willRestore() SK_OVERRIDE;

    virtual void didConcat(const SkMatrix&) SK_OVERRIDE;
    virtual void didSetMatrix(const SkMatrix&) SK_OVERRIDE;

    virtual void onDrawDRRect(const SkRRect&, const SkRRect&, const SkPaint&) SK_OVERRIDE;
    virtual void onDrawText(const void* text, size_t byteLength, SkScalar x, SkScalar y,
                            const SkPaint&) SK_OVERRIDE;
    virtual void onDrawPosText(const void* text, size_t byteLength, const SkPoint pos[],
                               const SkPaint&) SK_OVERRIDE;
    virtual void onDrawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[],
                                SkScalar constY, const SkPaint&) SK_OVERRIDE;
    virtual void onDrawTextOnPath(const void* text, size_t byteLength, const SkPath& path,
                                  const SkMatrix* matrix, const SkPaint&) SK_OVERRIDE;

    virtual void onClipRect(const SkRect&, SkRegion::Op, ClipEdgeStyle) SK_OVERRIDE;
    virtual void onClipRRect(const SkRRect&, SkRegion::Op, ClipEdgeStyle) SK_OVERRIDE;
    virtual void onClipPath(const SkPath&, SkRegion::Op, ClipEdgeStyle) SK_OVERRIDE;
    virtual void onClipRegion(const SkRegion&, SkRegion::Op) SK_OVERRIDE;

    virtual void onDrawPicture(const SkPicture* picture) SK_OVERRIDE;

public:
    class NotificationClient {
    public:
        virtual ~NotificationClient() {}

        /**
         *  Called before executing one or several draw commands, which means
         *  once per flush when deferred rendering is enabled.
         */
        virtual void prepareForDraw() {}

        /**
         *  Called after a recording a draw command if additional memory
         *  had to be allocated for recording.
         *  @param newAllocatedStorage same value as would be returned by
         *      storageAllocatedForRecording(), for convenience.
         */
        virtual void storageAllocatedForRecordingChanged(
            size_t newAllocatedStorage) {}

        /**
         *  Called after pending draw commands have been flushed
         */
        virtual void flushedDrawCommands() {}

        /**
         *  Called after pending draw commands have been skipped, meaning
         *  that they were optimized-out because the canvas is cleared
         *  or completely overwritten by the command currently being recorded.
         */
        virtual void skippedPendingDrawCommands() {}
    };

protected:
    virtual SkCanvas* canvasForDrawIter();
    SkDeferredDevice* getDeferredDevice() const;

private:
    SkDeferredCanvas(SkDeferredDevice*);

    void recordedDrawCommand();
    SkCanvas* drawingCanvas() const;
    SkCanvas* immediateCanvas() const;
    bool isFullFrame(const SkRect*, const SkPaint*) const;
    void validate() const;
    void init();

    size_t fBitmapSizeThreshold;
    bool   fDeferredDrawing;

    friend class SkDeferredCanvasTester; // for unit testing
    typedef SkCanvas INHERITED;
};


#endif
