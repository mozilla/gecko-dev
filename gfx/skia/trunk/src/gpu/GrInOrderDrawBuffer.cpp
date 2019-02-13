/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrInOrderDrawBuffer.h"

#include "GrBufferAllocPool.h"
#include "GrDrawTargetCaps.h"
#include "GrTextStrike.h"
#include "GrGpu.h"
#include "GrIndexBuffer.h"
#include "GrPath.h"
#include "GrPathRange.h"
#include "GrRenderTarget.h"
#include "GrTemplates.h"
#include "GrTexture.h"
#include "GrVertexBuffer.h"

GrInOrderDrawBuffer::GrInOrderDrawBuffer(GrGpu* gpu,
                                         GrVertexBufferAllocPool* vertexPool,
                                         GrIndexBufferAllocPool* indexPool)
    : GrDrawTarget(gpu->getContext())
    , fDstGpu(gpu)
    , fClipSet(true)
    , fClipProxyState(kUnknown_ClipProxyState)
    , fVertexPool(*vertexPool)
    , fIndexPool(*indexPool)
    , fFlushing(false)
    , fDrawID(0) {

    fDstGpu->ref();
    fCaps.reset(SkRef(fDstGpu->caps()));

    SkASSERT(NULL != vertexPool);
    SkASSERT(NULL != indexPool);

    GeometryPoolState& poolState = fGeoPoolStateStack.push_back();
    poolState.fUsedPoolVertexBytes = 0;
    poolState.fUsedPoolIndexBytes = 0;
#ifdef SK_DEBUG
    poolState.fPoolVertexBuffer = (GrVertexBuffer*)~0;
    poolState.fPoolStartVertex = ~0;
    poolState.fPoolIndexBuffer = (GrIndexBuffer*)~0;
    poolState.fPoolStartIndex = ~0;
#endif
    this->reset();
}

GrInOrderDrawBuffer::~GrInOrderDrawBuffer() {
    this->reset();
    // This must be called by before the GrDrawTarget destructor
    this->releaseGeometry();
    fDstGpu->unref();
}

////////////////////////////////////////////////////////////////////////////////

namespace {
void get_vertex_bounds(const void* vertices,
                       size_t vertexSize,
                       int vertexCount,
                       SkRect* bounds) {
    SkASSERT(vertexSize >= sizeof(SkPoint));
    SkASSERT(vertexCount > 0);
    const SkPoint* point = static_cast<const SkPoint*>(vertices);
    bounds->fLeft = bounds->fRight = point->fX;
    bounds->fTop = bounds->fBottom = point->fY;
    for (int i = 1; i < vertexCount; ++i) {
        point = reinterpret_cast<SkPoint*>(reinterpret_cast<intptr_t>(point) + vertexSize);
        bounds->growToInclude(point->fX, point->fY);
    }
}
}


namespace {

extern const GrVertexAttrib kRectPosColorUVAttribs[] = {
    {kVec2f_GrVertexAttribType,  0,               kPosition_GrVertexAttribBinding},
    {kVec4ub_GrVertexAttribType, sizeof(SkPoint), kColor_GrVertexAttribBinding},
    {kVec2f_GrVertexAttribType,  sizeof(SkPoint)+sizeof(GrColor),
                                                  kLocalCoord_GrVertexAttribBinding},
};

extern const GrVertexAttrib kRectPosUVAttribs[] = {
    {kVec2f_GrVertexAttribType,  0,              kPosition_GrVertexAttribBinding},
    {kVec2f_GrVertexAttribType, sizeof(SkPoint), kLocalCoord_GrVertexAttribBinding},
};

static void set_vertex_attributes(GrDrawState* drawState,
                                  bool hasColor, bool hasUVs,
                                  int* colorOffset, int* localOffset) {
    *colorOffset = -1;
    *localOffset = -1;

    // Using per-vertex colors allows batching across colors. (A lot of rects in a row differing
    // only in color is a common occurrence in tables). However, having per-vertex colors disables
    // blending optimizations because we don't know if the color will be solid or not. These
    // optimizations help determine whether coverage and color can be blended correctly when
    // dual-source blending isn't available. This comes into play when there is coverage. If colors
    // were a stage it could take a hint that every vertex's color will be opaque.
    if (hasColor && hasUVs) {
        *colorOffset = sizeof(SkPoint);
        *localOffset = sizeof(SkPoint) + sizeof(GrColor);
        drawState->setVertexAttribs<kRectPosColorUVAttribs>(3);
    } else if (hasColor) {
        *colorOffset = sizeof(SkPoint);
        drawState->setVertexAttribs<kRectPosColorUVAttribs>(2);
    } else if (hasUVs) {
        *localOffset = sizeof(SkPoint);
        drawState->setVertexAttribs<kRectPosUVAttribs>(2);
    } else {
        drawState->setVertexAttribs<kRectPosUVAttribs>(1);
    }
}

};

enum {
    kTraceCmdBit = 0x80,
    kCmdMask = 0x7f,
};

static uint8_t add_trace_bit(uint8_t cmd) {
    return cmd | kTraceCmdBit;
}

static uint8_t strip_trace_bit(uint8_t cmd) {
    return cmd & kCmdMask;
}

static bool cmd_has_trace_marker(uint8_t cmd) {
    return SkToBool(cmd & kTraceCmdBit);
}

void GrInOrderDrawBuffer::onDrawRect(const SkRect& rect,
                                     const SkMatrix* matrix,
                                     const SkRect* localRect,
                                     const SkMatrix* localMatrix) {
    GrDrawState::AutoColorRestore acr;

    GrDrawState* drawState = this->drawState();

    GrColor color = drawState->getColor();

    int colorOffset, localOffset;
    set_vertex_attributes(drawState,
                   this->caps()->dualSourceBlendingSupport() || drawState->hasSolidCoverage(),
                   NULL != localRect,
                   &colorOffset, &localOffset);
    if (colorOffset >= 0) {
        // We set the draw state's color to white here. This is done so that any batching performed
        // in our subclass's onDraw() won't get a false from GrDrawState::op== due to a color
        // mismatch. TODO: Once vertex layout is owned by GrDrawState it should skip comparing the
        // constant color in its op== when the kColor layout bit is set and then we can remove
        // this.
        acr.set(drawState, 0xFFFFFFFF);
    }

    AutoReleaseGeometry geo(this, 4, 0);
    if (!geo.succeeded()) {
        GrPrintf("Failed to get space for vertices!\n");
        return;
    }

    // Go to device coords to allow batching across matrix changes
    SkMatrix combinedMatrix;
    if (NULL != matrix) {
        combinedMatrix = *matrix;
    } else {
        combinedMatrix.reset();
    }
    combinedMatrix.postConcat(drawState->getViewMatrix());
    // When the caller has provided an explicit source rect for a stage then we don't want to
    // modify that stage's matrix. Otherwise if the effect is generating its source rect from
    // the vertex positions then we have to account for the view matrix change.
    GrDrawState::AutoViewMatrixRestore avmr;
    if (!avmr.setIdentity(drawState)) {
        return;
    }

    size_t vsize = drawState->getVertexSize();

    geo.positions()->setRectFan(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom, vsize);
    combinedMatrix.mapPointsWithStride(geo.positions(), vsize, 4);

    SkRect devBounds;
    // since we already computed the dev verts, set the bounds hint. This will help us avoid
    // unnecessary clipping in our onDraw().
    get_vertex_bounds(geo.vertices(), vsize, 4, &devBounds);

    if (localOffset >= 0) {
        SkPoint* coords = GrTCast<SkPoint*>(GrTCast<intptr_t>(geo.vertices()) + localOffset);
        coords->setRectFan(localRect->fLeft, localRect->fTop,
                           localRect->fRight, localRect->fBottom,
                            vsize);
        if (NULL != localMatrix) {
            localMatrix->mapPointsWithStride(coords, vsize, 4);
        }
    }

    if (colorOffset >= 0) {
        GrColor* vertColor = GrTCast<GrColor*>(GrTCast<intptr_t>(geo.vertices()) + colorOffset);
        for (int i = 0; i < 4; ++i) {
            *vertColor = color;
            vertColor = (GrColor*) ((intptr_t) vertColor + vsize);
        }
    }

    this->setIndexSourceToBuffer(this->getContext()->getQuadIndexBuffer());
    this->drawIndexedInstances(kTriangles_GrPrimitiveType, 1, 4, 6, &devBounds);

    // to ensure that stashing the drawState ptr is valid
    SkASSERT(this->drawState() == drawState);
}

bool GrInOrderDrawBuffer::quickInsideClip(const SkRect& devBounds) {
    if (!this->getDrawState().isClipState()) {
        return true;
    }
    if (kUnknown_ClipProxyState == fClipProxyState) {
        SkIRect rect;
        bool iior;
        this->getClip()->getConservativeBounds(this->getDrawState().getRenderTarget(), &rect, &iior);
        if (iior) {
            // The clip is a rect. We will remember that in fProxyClip. It is common for an edge (or
            // all edges) of the clip to be at the edge of the RT. However, we get that clipping for
            // free via the viewport. We don't want to think that clipping must be enabled in this
            // case. So we extend the clip outward from the edge to avoid these false negatives.
            fClipProxyState = kValid_ClipProxyState;
            fClipProxy = SkRect::Make(rect);

            if (fClipProxy.fLeft <= 0) {
                fClipProxy.fLeft = SK_ScalarMin;
            }
            if (fClipProxy.fTop <= 0) {
                fClipProxy.fTop = SK_ScalarMin;
            }
            if (fClipProxy.fRight >= this->getDrawState().getRenderTarget()->width()) {
                fClipProxy.fRight = SK_ScalarMax;
            }
            if (fClipProxy.fBottom >= this->getDrawState().getRenderTarget()->height()) {
                fClipProxy.fBottom = SK_ScalarMax;
            }
        } else {
            fClipProxyState = kInvalid_ClipProxyState;
        }
    }
    if (kValid_ClipProxyState == fClipProxyState) {
        return fClipProxy.contains(devBounds);
    }
    SkPoint originOffset = {SkIntToScalar(this->getClip()->fOrigin.fX),
                            SkIntToScalar(this->getClip()->fOrigin.fY)};
    SkRect clipSpaceBounds = devBounds;
    clipSpaceBounds.offset(originOffset);
    return this->getClip()->fClipStack->quickContains(clipSpaceBounds);
}

int GrInOrderDrawBuffer::concatInstancedDraw(const DrawInfo& info) {
    SkASSERT(info.isInstanced());

    const GeometrySrcState& geomSrc = this->getGeomSrc();
    const GrDrawState& drawState = this->getDrawState();

    // we only attempt to concat the case when reserved verts are used with a client-specified index
    // buffer. To make this work with client-specified VBs we'd need to know if the VB was updated
    // between draws.
    if (kReserved_GeometrySrcType != geomSrc.fVertexSrc ||
        kBuffer_GeometrySrcType != geomSrc.fIndexSrc) {
        return 0;
    }
    // Check if there is a draw info that is compatible that uses the same VB from the pool and
    // the same IB
    if (kDraw_Cmd != strip_trace_bit(fCmds.back())) {
        return 0;
    }

    DrawRecord* draw = &fDraws.back();
    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    const GrVertexBuffer* vertexBuffer = poolState.fPoolVertexBuffer;

    if (!draw->isInstanced() ||
        draw->verticesPerInstance() != info.verticesPerInstance() ||
        draw->indicesPerInstance() != info.indicesPerInstance() ||
        draw->fVertexBuffer != vertexBuffer ||
        draw->fIndexBuffer != geomSrc.fIndexBuffer) {
        return 0;
    }
    // info does not yet account for the offset from the start of the pool's VB while the previous
    // draw record does.
    int adjustedStartVertex = poolState.fPoolStartVertex + info.startVertex();
    if (draw->startVertex() + draw->vertexCount() != adjustedStartVertex) {
        return 0;
    }

    SkASSERT(poolState.fPoolStartVertex == draw->startVertex() + draw->vertexCount());

    // how many instances can be concat'ed onto draw given the size of the index buffer
    int instancesToConcat = this->indexCountInCurrentSource() / info.indicesPerInstance();
    instancesToConcat -= draw->instanceCount();
    instancesToConcat = SkTMin(instancesToConcat, info.instanceCount());

    // update the amount of reserved vertex data actually referenced in draws
    size_t vertexBytes = instancesToConcat * info.verticesPerInstance() *
                         drawState.getVertexSize();
    poolState.fUsedPoolVertexBytes = SkTMax(poolState.fUsedPoolVertexBytes, vertexBytes);

    draw->adjustInstanceCount(instancesToConcat);

    // update last fGpuCmdMarkers to include any additional trace markers that have been added
    if (this->getActiveTraceMarkers().count() > 0) {
        if (cmd_has_trace_marker(fCmds.back())) {
            fGpuCmdMarkers.back().addSet(this->getActiveTraceMarkers());
        } else {
            fGpuCmdMarkers.push_back(this->getActiveTraceMarkers());
            fCmds.back() = add_trace_bit(fCmds.back());
        }
    }

    return instancesToConcat;
}

class AutoClipReenable {
public:
    AutoClipReenable() : fDrawState(NULL) {}
    ~AutoClipReenable() {
        if (NULL != fDrawState) {
            fDrawState->enableState(GrDrawState::kClip_StateBit);
        }
    }
    void set(GrDrawState* drawState) {
        if (drawState->isClipState()) {
            fDrawState = drawState;
            drawState->disableState(GrDrawState::kClip_StateBit);
        }
    }
private:
    GrDrawState*    fDrawState;
};

void GrInOrderDrawBuffer::onDraw(const DrawInfo& info) {

    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    const GrDrawState& drawState = this->getDrawState();
    AutoClipReenable acr;

    if (drawState.isClipState() &&
        NULL != info.getDevBounds() &&
        this->quickInsideClip(*info.getDevBounds())) {
        acr.set(this->drawState());
    }

    if (this->needsNewClip()) {
       this->recordClip();
    }
    if (this->needsNewState()) {
        this->recordState();
    }

    DrawRecord* draw;
    if (info.isInstanced()) {
        int instancesConcated = this->concatInstancedDraw(info);
        if (info.instanceCount() > instancesConcated) {
            draw = this->recordDraw(info);
            draw->adjustInstanceCount(-instancesConcated);
        } else {
            return;
        }
    } else {
        draw = this->recordDraw(info);
    }

    switch (this->getGeomSrc().fVertexSrc) {
        case kBuffer_GeometrySrcType:
            draw->fVertexBuffer = this->getGeomSrc().fVertexBuffer;
            break;
        case kReserved_GeometrySrcType: // fallthrough
        case kArray_GeometrySrcType: {
            size_t vertexBytes = (info.vertexCount() + info.startVertex()) *
                                 drawState.getVertexSize();
            poolState.fUsedPoolVertexBytes = SkTMax(poolState.fUsedPoolVertexBytes, vertexBytes);
            draw->fVertexBuffer = poolState.fPoolVertexBuffer;
            draw->adjustStartVertex(poolState.fPoolStartVertex);
            break;
        }
        default:
            SkFAIL("unknown geom src type");
    }
    draw->fVertexBuffer->ref();

    if (info.isIndexed()) {
        switch (this->getGeomSrc().fIndexSrc) {
            case kBuffer_GeometrySrcType:
                draw->fIndexBuffer = this->getGeomSrc().fIndexBuffer;
                break;
            case kReserved_GeometrySrcType: // fallthrough
            case kArray_GeometrySrcType: {
                size_t indexBytes = (info.indexCount() + info.startIndex()) * sizeof(uint16_t);
                poolState.fUsedPoolIndexBytes = SkTMax(poolState.fUsedPoolIndexBytes, indexBytes);
                draw->fIndexBuffer = poolState.fPoolIndexBuffer;
                draw->adjustStartIndex(poolState.fPoolStartIndex);
                break;
            }
            default:
                SkFAIL("unknown geom src type");
        }
        draw->fIndexBuffer->ref();
    } else {
        draw->fIndexBuffer = NULL;
    }
}

GrInOrderDrawBuffer::StencilPath::StencilPath() {}
GrInOrderDrawBuffer::DrawPath::DrawPath() {}
GrInOrderDrawBuffer::DrawPaths::DrawPaths() {}
GrInOrderDrawBuffer::DrawPaths::~DrawPaths() {
    if (fTransforms) {
        SkDELETE_ARRAY(fTransforms);
    }
    if (fIndices) {
        SkDELETE_ARRAY(fIndices);
    }
}

void GrInOrderDrawBuffer::onStencilPath(const GrPath* path, SkPath::FillType fill) {
    if (this->needsNewClip()) {
        this->recordClip();
    }
    // Only compare the subset of GrDrawState relevant to path stenciling?
    if (this->needsNewState()) {
        this->recordState();
    }
    StencilPath* sp = this->recordStencilPath();
    sp->fPath.reset(path);
    path->ref();
    sp->fFill = fill;
}

void GrInOrderDrawBuffer::onDrawPath(const GrPath* path,
                                     SkPath::FillType fill, const GrDeviceCoordTexture* dstCopy) {
    if (this->needsNewClip()) {
        this->recordClip();
    }
    // TODO: Only compare the subset of GrDrawState relevant to path covering?
    if (this->needsNewState()) {
        this->recordState();
    }
    DrawPath* cp = this->recordDrawPath();
    cp->fPath.reset(path);
    path->ref();
    cp->fFill = fill;
    if (NULL != dstCopy) {
        cp->fDstCopy = *dstCopy;
    }
}

void GrInOrderDrawBuffer::onDrawPaths(const GrPathRange* pathRange,
                                      const uint32_t indices[], int count,
                                      const float transforms[], PathTransformType transformsType,
                                      SkPath::FillType fill, const GrDeviceCoordTexture* dstCopy) {
    SkASSERT(NULL != pathRange);
    SkASSERT(NULL != indices);
    SkASSERT(NULL != transforms);

    if (this->needsNewClip()) {
        this->recordClip();
    }
    if (this->needsNewState()) {
        this->recordState();
    }
    DrawPaths* dp = this->recordDrawPaths();
    dp->fPathRange.reset(SkRef(pathRange));
    dp->fIndices = SkNEW_ARRAY(uint32_t, count); // TODO: Accomplish this without a malloc
    memcpy(dp->fIndices, indices, sizeof(uint32_t) * count);
    dp->fCount = count;

    const int transformsLength = PathTransformSize(transformsType) * count;
    dp->fTransforms = SkNEW_ARRAY(float, transformsLength);
    memcpy(dp->fTransforms, transforms, sizeof(float) * transformsLength);
    dp->fTransformsType = transformsType;

    dp->fFill = fill;

    if (NULL != dstCopy) {
        dp->fDstCopy = *dstCopy;
    }
}

void GrInOrderDrawBuffer::clear(const SkIRect* rect, GrColor color,
                                bool canIgnoreRect, GrRenderTarget* renderTarget) {
    SkIRect r;
    if (NULL == renderTarget) {
        renderTarget = this->drawState()->getRenderTarget();
        SkASSERT(NULL != renderTarget);
    }
    if (NULL == rect) {
        // We could do something smart and remove previous draws and clears to
        // the current render target. If we get that smart we have to make sure
        // those draws aren't read before this clear (render-to-texture).
        r.setLTRB(0, 0, renderTarget->width(), renderTarget->height());
        rect = &r;
    }
    Clear* clr = this->recordClear();
    GrColorIsPMAssert(color);
    clr->fColor = color;
    clr->fRect = *rect;
    clr->fCanIgnoreRect = canIgnoreRect;
    clr->fRenderTarget = renderTarget;
    renderTarget->ref();
}

void GrInOrderDrawBuffer::discard(GrRenderTarget* renderTarget) {
    if (!this->caps()->discardRenderTargetSupport()) {
        return;
    }
    if (NULL == renderTarget) {
        renderTarget = this->drawState()->getRenderTarget();
        SkASSERT(NULL != renderTarget);
    }
    Clear* clr = this->recordClear();
    clr->fColor = GrColor_ILLEGAL;
    clr->fRenderTarget = renderTarget;
    renderTarget->ref();
}

void GrInOrderDrawBuffer::reset() {
    SkASSERT(1 == fGeoPoolStateStack.count());
    this->resetVertexSource();
    this->resetIndexSource();
        
    DrawAllocator::Iter drawIter(&fDraws);
    while (drawIter.next()) {
        // we always have a VB, but not always an IB
        SkASSERT(NULL != drawIter->fVertexBuffer);
        drawIter->fVertexBuffer->unref();
        SkSafeUnref(drawIter->fIndexBuffer);
    }
    fCmds.reset();
    fDraws.reset();
    fStencilPaths.reset();
    fDrawPath.reset();
    fDrawPaths.reset();
    fStates.reset();
    fClears.reset();
    fVertexPool.reset();
    fIndexPool.reset();
    fClips.reset();
    fCopySurfaces.reset();
    fGpuCmdMarkers.reset();
    fClipSet = true;
}

void GrInOrderDrawBuffer::flush() {
    if (fFlushing) {
        return;
    }

    this->getContext()->getFontCache()->updateTextures();

    SkASSERT(kReserved_GeometrySrcType != this->getGeomSrc().fVertexSrc);
    SkASSERT(kReserved_GeometrySrcType != this->getGeomSrc().fIndexSrc);

    int numCmds = fCmds.count();
    if (0 == numCmds) {
        return;
    }

    GrAutoTRestore<bool> flushRestore(&fFlushing);
    fFlushing = true;

    fVertexPool.unmap();
    fIndexPool.unmap();

    GrDrawTarget::AutoClipRestore acr(fDstGpu);
    AutoGeometryAndStatePush agasp(fDstGpu, kPreserve_ASRInit);

    GrDrawState* prevDrawState = SkRef(fDstGpu->drawState());

    GrClipData clipData;

    StateAllocator::Iter stateIter(&fStates);
    ClipAllocator::Iter clipIter(&fClips);
    ClearAllocator::Iter clearIter(&fClears);
    DrawAllocator::Iter drawIter(&fDraws);
    StencilPathAllocator::Iter stencilPathIter(&fStencilPaths);
    DrawPathAllocator::Iter drawPathIter(&fDrawPath);
    DrawPathsAllocator::Iter drawPathsIter(&fDrawPaths);
    CopySurfaceAllocator::Iter copySurfaceIter(&fCopySurfaces);

    int currCmdMarker   = 0;

    fDstGpu->saveActiveTraceMarkers();
    for (int c = 0; c < numCmds; ++c) {
        GrGpuTraceMarker newMarker("", -1);
        SkString traceString;
        if (cmd_has_trace_marker(fCmds[c])) {
            traceString = fGpuCmdMarkers[currCmdMarker].toString();
            newMarker.fMarker = traceString.c_str();
            fDstGpu->addGpuTraceMarker(&newMarker);
            ++currCmdMarker;
        }
        switch (strip_trace_bit(fCmds[c])) {
            case kDraw_Cmd: {
                SkASSERT(fDstGpu->drawState() != prevDrawState);
                SkAssertResult(drawIter.next());
                fDstGpu->setVertexSourceToBuffer(drawIter->fVertexBuffer);
                if (drawIter->isIndexed()) {
                    fDstGpu->setIndexSourceToBuffer(drawIter->fIndexBuffer);
                }
                fDstGpu->executeDraw(*drawIter);
                break;
            }
            case kStencilPath_Cmd: {
                SkASSERT(fDstGpu->drawState() != prevDrawState);
                SkAssertResult(stencilPathIter.next());
                fDstGpu->stencilPath(stencilPathIter->fPath.get(), stencilPathIter->fFill);
                break;
            }
            case kDrawPath_Cmd: {
                SkASSERT(fDstGpu->drawState() != prevDrawState);
                SkAssertResult(drawPathIter.next());
                fDstGpu->executeDrawPath(drawPathIter->fPath.get(), drawPathIter->fFill,
                                         NULL != drawPathIter->fDstCopy.texture() ?
                                            &drawPathIter->fDstCopy :
                                            NULL);
                break;
            }
            case kDrawPaths_Cmd: {
                SkASSERT(fDstGpu->drawState() != prevDrawState);
                SkAssertResult(drawPathsIter.next());
                const GrDeviceCoordTexture* dstCopy =
                    NULL !=drawPathsIter->fDstCopy.texture() ? &drawPathsIter->fDstCopy : NULL;
                fDstGpu->executeDrawPaths(drawPathsIter->fPathRange.get(),
                                          drawPathsIter->fIndices,
                                          drawPathsIter->fCount,
                                          drawPathsIter->fTransforms,
                                          drawPathsIter->fTransformsType,
                                          drawPathsIter->fFill,
                                          dstCopy);
                break;
            }
            case kSetState_Cmd:
                SkAssertResult(stateIter.next());
                fDstGpu->setDrawState(stateIter.get());
                break;
            case kSetClip_Cmd:
                SkAssertResult(clipIter.next());
                clipData.fClipStack = &clipIter->fStack;
                clipData.fOrigin = clipIter->fOrigin;
                fDstGpu->setClip(&clipData);
                break;
            case kClear_Cmd:
                SkAssertResult(clearIter.next());
                if (GrColor_ILLEGAL == clearIter->fColor) {
                    fDstGpu->discard(clearIter->fRenderTarget);
                } else {
                    fDstGpu->clear(&clearIter->fRect,
                                   clearIter->fColor,
                                   clearIter->fCanIgnoreRect,
                                   clearIter->fRenderTarget);
                }
                break;
            case kCopySurface_Cmd:
                SkAssertResult(copySurfaceIter.next());
                fDstGpu->copySurface(copySurfaceIter->fDst.get(),
                                     copySurfaceIter->fSrc.get(),
                                     copySurfaceIter->fSrcRect,
                                     copySurfaceIter->fDstPoint);
                break;
        }
        if (cmd_has_trace_marker(fCmds[c])) {
            fDstGpu->removeGpuTraceMarker(&newMarker);
        }
    }
    fDstGpu->restoreActiveTraceMarkers();
    // we should have consumed all the states, clips, etc.
    SkASSERT(!stateIter.next());
    SkASSERT(!clipIter.next());
    SkASSERT(!clearIter.next());
    SkASSERT(!drawIter.next());
    SkASSERT(!copySurfaceIter.next());
    SkASSERT(!stencilPathIter.next());
    SkASSERT(!drawPathIter.next());
    SkASSERT(!drawPathsIter.next());

    SkASSERT(fGpuCmdMarkers.count() == currCmdMarker);

    fDstGpu->setDrawState(prevDrawState);
    prevDrawState->unref();
    this->reset();
    ++fDrawID;
}

bool GrInOrderDrawBuffer::onCopySurface(GrSurface* dst,
                                        GrSurface* src,
                                        const SkIRect& srcRect,
                                        const SkIPoint& dstPoint) {
    if (fDstGpu->canCopySurface(dst, src, srcRect, dstPoint)) {
        CopySurface* cs = this->recordCopySurface();
        cs->fDst.reset(SkRef(dst));
        cs->fSrc.reset(SkRef(src));
        cs->fSrcRect = srcRect;
        cs->fDstPoint = dstPoint;
        return true;
    } else {
        return false;
    }
}

bool GrInOrderDrawBuffer::onCanCopySurface(GrSurface* dst,
                                           GrSurface* src,
                                           const SkIRect& srcRect,
                                           const SkIPoint& dstPoint) {
    return fDstGpu->canCopySurface(dst, src, srcRect, dstPoint);
}

void GrInOrderDrawBuffer::initCopySurfaceDstDesc(const GrSurface* src, GrTextureDesc* desc) {
    fDstGpu->initCopySurfaceDstDesc(src, desc);
}

void GrInOrderDrawBuffer::willReserveVertexAndIndexSpace(int vertexCount,
                                                         int indexCount) {
    // We use geometryHints() to know whether to flush the draw buffer. We
    // can't flush if we are inside an unbalanced pushGeometrySource.
    // Moreover, flushing blows away vertex and index data that was
    // previously reserved. So if the vertex or index data is pulled from
    // reserved space and won't be released by this request then we can't
    // flush.
    bool insideGeoPush = fGeoPoolStateStack.count() > 1;

    bool unreleasedVertexSpace =
        !vertexCount &&
        kReserved_GeometrySrcType == this->getGeomSrc().fVertexSrc;

    bool unreleasedIndexSpace =
        !indexCount &&
        kReserved_GeometrySrcType == this->getGeomSrc().fIndexSrc;

    // we don't want to finalize any reserved geom on the target since
    // we don't know that the client has finished writing to it.
    bool targetHasReservedGeom = fDstGpu->hasReservedVerticesOrIndices();

    int vcount = vertexCount;
    int icount = indexCount;

    if (!insideGeoPush &&
        !unreleasedVertexSpace &&
        !unreleasedIndexSpace &&
        !targetHasReservedGeom &&
        this->geometryHints(&vcount, &icount)) {

        this->flush();
    }
}

bool GrInOrderDrawBuffer::geometryHints(int* vertexCount,
                                        int* indexCount) const {
    // we will recommend a flush if the data could fit in a single
    // preallocated buffer but none are left and it can't fit
    // in the current buffer (which may not be prealloced).
    bool flush = false;
    if (NULL != indexCount) {
        int32_t currIndices = fIndexPool.currentBufferIndices();
        if (*indexCount > currIndices &&
            (!fIndexPool.preallocatedBuffersRemaining() &&
             *indexCount <= fIndexPool.preallocatedBufferIndices())) {

            flush = true;
        }
        *indexCount = currIndices;
    }
    if (NULL != vertexCount) {
        size_t vertexSize = this->getDrawState().getVertexSize();
        int32_t currVertices = fVertexPool.currentBufferVertices(vertexSize);
        if (*vertexCount > currVertices &&
            (!fVertexPool.preallocatedBuffersRemaining() &&
             *vertexCount <= fVertexPool.preallocatedBufferVertices(vertexSize))) {

            flush = true;
        }
        *vertexCount = currVertices;
    }
    return flush;
}

bool GrInOrderDrawBuffer::onReserveVertexSpace(size_t vertexSize,
                                               int vertexCount,
                                               void** vertices) {
    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    SkASSERT(vertexCount > 0);
    SkASSERT(NULL != vertices);
    SkASSERT(0 == poolState.fUsedPoolVertexBytes);

    *vertices = fVertexPool.makeSpace(vertexSize,
                                      vertexCount,
                                      &poolState.fPoolVertexBuffer,
                                      &poolState.fPoolStartVertex);
    return NULL != *vertices;
}

bool GrInOrderDrawBuffer::onReserveIndexSpace(int indexCount, void** indices) {
    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    SkASSERT(indexCount > 0);
    SkASSERT(NULL != indices);
    SkASSERT(0 == poolState.fUsedPoolIndexBytes);

    *indices = fIndexPool.makeSpace(indexCount,
                                    &poolState.fPoolIndexBuffer,
                                    &poolState.fPoolStartIndex);
    return NULL != *indices;
}

void GrInOrderDrawBuffer::releaseReservedVertexSpace() {
    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    const GeometrySrcState& geoSrc = this->getGeomSrc();

    // If we get a release vertex space call then our current source should either be reserved
    // or array (which we copied into reserved space).
    SkASSERT(kReserved_GeometrySrcType == geoSrc.fVertexSrc ||
             kArray_GeometrySrcType == geoSrc.fVertexSrc);

    // When the caller reserved vertex buffer space we gave it back a pointer
    // provided by the vertex buffer pool. At each draw we tracked the largest
    // offset into the pool's pointer that was referenced. Now we return to the
    // pool any portion at the tail of the allocation that no draw referenced.
    size_t reservedVertexBytes = geoSrc.fVertexSize * geoSrc.fVertexCount;
    fVertexPool.putBack(reservedVertexBytes -
                        poolState.fUsedPoolVertexBytes);
    poolState.fUsedPoolVertexBytes = 0;
    poolState.fPoolVertexBuffer = NULL;
    poolState.fPoolStartVertex = 0;
}

void GrInOrderDrawBuffer::releaseReservedIndexSpace() {
    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    const GeometrySrcState& geoSrc = this->getGeomSrc();

    // If we get a release index space call then our current source should either be reserved
    // or array (which we copied into reserved space).
    SkASSERT(kReserved_GeometrySrcType == geoSrc.fIndexSrc ||
             kArray_GeometrySrcType == geoSrc.fIndexSrc);

    // Similar to releaseReservedVertexSpace we return any unused portion at
    // the tail
    size_t reservedIndexBytes = sizeof(uint16_t) * geoSrc.fIndexCount;
    fIndexPool.putBack(reservedIndexBytes - poolState.fUsedPoolIndexBytes);
    poolState.fUsedPoolIndexBytes = 0;
    poolState.fPoolIndexBuffer = NULL;
    poolState.fPoolStartIndex = 0;
}

void GrInOrderDrawBuffer::onSetVertexSourceToArray(const void* vertexArray,
                                                   int vertexCount) {

    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    SkASSERT(0 == poolState.fUsedPoolVertexBytes);
#ifdef SK_DEBUG
    bool success =
#endif
    fVertexPool.appendVertices(this->getVertexSize(),
                               vertexCount,
                               vertexArray,
                               &poolState.fPoolVertexBuffer,
                               &poolState.fPoolStartVertex);
    GR_DEBUGASSERT(success);
}

void GrInOrderDrawBuffer::onSetIndexSourceToArray(const void* indexArray,
                                                  int indexCount) {
    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    SkASSERT(0 == poolState.fUsedPoolIndexBytes);
#ifdef SK_DEBUG
    bool success =
#endif
    fIndexPool.appendIndices(indexCount,
                             indexArray,
                             &poolState.fPoolIndexBuffer,
                             &poolState.fPoolStartIndex);
    GR_DEBUGASSERT(success);
}

void GrInOrderDrawBuffer::releaseVertexArray() {
    // When the client provides an array as the vertex source we handled it
    // by copying their array into reserved space.
    this->GrInOrderDrawBuffer::releaseReservedVertexSpace();
}

void GrInOrderDrawBuffer::releaseIndexArray() {
    // When the client provides an array as the index source we handled it
    // by copying their array into reserved space.
    this->GrInOrderDrawBuffer::releaseReservedIndexSpace();
}

void GrInOrderDrawBuffer::geometrySourceWillPush() {
    GeometryPoolState& poolState = fGeoPoolStateStack.push_back();
    poolState.fUsedPoolVertexBytes = 0;
    poolState.fUsedPoolIndexBytes = 0;
#ifdef SK_DEBUG
    poolState.fPoolVertexBuffer = (GrVertexBuffer*)~0;
    poolState.fPoolStartVertex = ~0;
    poolState.fPoolIndexBuffer = (GrIndexBuffer*)~0;
    poolState.fPoolStartIndex = ~0;
#endif
}

void GrInOrderDrawBuffer::geometrySourceWillPop(
                                        const GeometrySrcState& restoredState) {
    SkASSERT(fGeoPoolStateStack.count() > 1);
    fGeoPoolStateStack.pop_back();
    GeometryPoolState& poolState = fGeoPoolStateStack.back();
    // we have to assume that any slack we had in our vertex/index data
    // is now unreleasable because data may have been appended later in the
    // pool.
    if (kReserved_GeometrySrcType == restoredState.fVertexSrc ||
        kArray_GeometrySrcType == restoredState.fVertexSrc) {
        poolState.fUsedPoolVertexBytes = restoredState.fVertexSize * restoredState.fVertexCount;
    }
    if (kReserved_GeometrySrcType == restoredState.fIndexSrc ||
        kArray_GeometrySrcType == restoredState.fIndexSrc) {
        poolState.fUsedPoolIndexBytes = sizeof(uint16_t) *
                                         restoredState.fIndexCount;
    }
}

bool GrInOrderDrawBuffer::needsNewState() const {
    return fStates.empty() || fStates.back() != this->getDrawState();
}

bool GrInOrderDrawBuffer::needsNewClip() const {
    if (this->getDrawState().isClipState()) {
       if (fClipSet &&
           (fClips.empty() ||
            fClips.back().fStack != *this->getClip()->fClipStack ||
            fClips.back().fOrigin != this->getClip()->fOrigin)) {
           return true;
       }
    }
    return false;
}

void GrInOrderDrawBuffer::addToCmdBuffer(uint8_t cmd) {
    SkASSERT(!cmd_has_trace_marker(cmd));
    const GrTraceMarkerSet& activeTraceMarkers = this->getActiveTraceMarkers();
    if (activeTraceMarkers.count() > 0) {
        fCmds.push_back(add_trace_bit(cmd));
        fGpuCmdMarkers.push_back(activeTraceMarkers);
    } else {
        fCmds.push_back(cmd);
    }
}

void GrInOrderDrawBuffer::recordClip() {
    fClips.push_back().fStack = *this->getClip()->fClipStack;
    fClips.back().fOrigin = this->getClip()->fOrigin;
    fClipSet = false;
    this->addToCmdBuffer(kSetClip_Cmd);
}

void GrInOrderDrawBuffer::recordState() {
    fStates.push_back() = this->getDrawState();
    this->addToCmdBuffer(kSetState_Cmd);
}

GrInOrderDrawBuffer::DrawRecord* GrInOrderDrawBuffer::recordDraw(const DrawInfo& info) {
    this->addToCmdBuffer(kDraw_Cmd);
    return &fDraws.push_back(info);
}

GrInOrderDrawBuffer::StencilPath* GrInOrderDrawBuffer::recordStencilPath() {
    this->addToCmdBuffer(kStencilPath_Cmd);
    return &fStencilPaths.push_back();
}

GrInOrderDrawBuffer::DrawPath* GrInOrderDrawBuffer::recordDrawPath() {
    this->addToCmdBuffer(kDrawPath_Cmd);
    return &fDrawPath.push_back();
}

GrInOrderDrawBuffer::DrawPaths* GrInOrderDrawBuffer::recordDrawPaths() {
    this->addToCmdBuffer(kDrawPaths_Cmd);
    return &fDrawPaths.push_back();
}

GrInOrderDrawBuffer::Clear* GrInOrderDrawBuffer::recordClear() {
    this->addToCmdBuffer(kClear_Cmd);
    return &fClears.push_back();
}

GrInOrderDrawBuffer::CopySurface* GrInOrderDrawBuffer::recordCopySurface() {
    this->addToCmdBuffer(kCopySurface_Cmd);
    return &fCopySurfaces.push_back();
}

void GrInOrderDrawBuffer::clipWillBeSet(const GrClipData* newClipData) {
    INHERITED::clipWillBeSet(newClipData);
    fClipSet = true;
    fClipProxyState = kUnknown_ClipProxyState;
}
