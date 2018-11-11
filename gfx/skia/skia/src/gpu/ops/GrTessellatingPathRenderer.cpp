/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrTessellatingPathRenderer.h"
#include <stdio.h>
#include "GrAuditTrail.h"
#include "GrClip.h"
#include "GrDefaultGeoProcFactory.h"
#include "GrDrawOpTest.h"
#include "GrMesh.h"
#include "GrOpFlushState.h"
#include "GrPathUtils.h"
#include "GrResourceCache.h"
#include "GrResourceProvider.h"
#include "GrShape.h"
#include "GrSimpleMeshDrawOpHelper.h"
#include "GrStyle.h"
#include "GrTessellator.h"
#include "SkGeometry.h"
#include "ops/GrMeshDrawOp.h"

#ifndef GR_AA_TESSELLATOR_MAX_VERB_COUNT
#define GR_AA_TESSELLATOR_MAX_VERB_COUNT 10
#endif

/*
 * This path renderer tessellates the path into triangles using GrTessellator, uploads the
 * triangles to a vertex buffer, and renders them with a single draw call. It can do screenspace
 * antialiasing with a one-pixel coverage ramp.
 */
namespace {

struct TessInfo {
    SkScalar  fTolerance;
    int       fCount;
};

// When the SkPathRef genID changes, invalidate a corresponding GrResource described by key.
class PathInvalidator : public SkPathRef::GenIDChangeListener {
public:
    PathInvalidator(const GrUniqueKey& key, uint32_t contextUniqueID)
            : fMsg(key, contextUniqueID) {}

private:
    GrUniqueKeyInvalidatedMessage fMsg;

    void onChange() override {
        SkMessageBus<GrUniqueKeyInvalidatedMessage>::Post(fMsg);
    }
};

bool cache_match(GrBuffer* vertexBuffer, SkScalar tol, int* actualCount) {
    if (!vertexBuffer) {
        return false;
    }
    const SkData* data = vertexBuffer->getUniqueKey().getCustomData();
    SkASSERT(data);
    const TessInfo* info = static_cast<const TessInfo*>(data->data());
    if (info->fTolerance == 0 || info->fTolerance < 3.0f * tol) {
        *actualCount = info->fCount;
        return true;
    }
    return false;
}

class StaticVertexAllocator : public GrTessellator::VertexAllocator {
public:
    StaticVertexAllocator(size_t stride, GrResourceProvider* resourceProvider, bool canMapVB)
      : VertexAllocator(stride)
      , fResourceProvider(resourceProvider)
      , fCanMapVB(canMapVB)
      , fVertices(nullptr) {
    }
    void* lock(int vertexCount) override {
        size_t size = vertexCount * stride();
        fVertexBuffer.reset(fResourceProvider->createBuffer(
            size, kVertex_GrBufferType, kStatic_GrAccessPattern, GrResourceProvider::Flags::kNone));
        if (!fVertexBuffer.get()) {
            return nullptr;
        }
        if (fCanMapVB) {
            fVertices = fVertexBuffer->map();
        } else {
            fVertices = sk_malloc_throw(vertexCount * stride());
        }
        return fVertices;
    }
    void unlock(int actualCount) override {
        if (fCanMapVB) {
            fVertexBuffer->unmap();
        } else {
            fVertexBuffer->updateData(fVertices, actualCount * stride());
            sk_free(fVertices);
        }
        fVertices = nullptr;
    }
    GrBuffer* vertexBuffer() { return fVertexBuffer.get(); }
private:
    sk_sp<GrBuffer> fVertexBuffer;
    GrResourceProvider* fResourceProvider;
    bool fCanMapVB;
    void* fVertices;
};

class DynamicVertexAllocator : public GrTessellator::VertexAllocator {
public:
    DynamicVertexAllocator(size_t stride, GrMeshDrawOp::Target* target)
            : VertexAllocator(stride)
            , fTarget(target)
            , fVertexBuffer(nullptr)
            , fVertices(nullptr) {}
    void* lock(int vertexCount) override {
        fVertexCount = vertexCount;
        fVertices = fTarget->makeVertexSpace(stride(), vertexCount, &fVertexBuffer, &fFirstVertex);
        return fVertices;
    }
    void unlock(int actualCount) override {
        fTarget->putBackVertices(fVertexCount - actualCount, stride());
        fVertices = nullptr;
    }
    const GrBuffer* vertexBuffer() const { return fVertexBuffer; }
    int firstVertex() const { return fFirstVertex; }
private:
    GrMeshDrawOp::Target* fTarget;
    const GrBuffer* fVertexBuffer;
    int fVertexCount;
    int fFirstVertex;
    void* fVertices;
};

}  // namespace

GrTessellatingPathRenderer::GrTessellatingPathRenderer() {
}

GrPathRenderer::CanDrawPath
GrTessellatingPathRenderer::onCanDrawPath(const CanDrawPathArgs& args) const {
    // This path renderer can draw fill styles, and can do screenspace antialiasing via a
    // one-pixel coverage ramp. It can do convex and concave paths, but we'll leave the convex
    // ones to simpler algorithms. We pass on paths that have styles, though they may come back
    // around after applying the styling information to the geometry to create a filled path. In
    // the non-AA case, We skip paths that don't have a key since the real advantage of this path
    // renderer comes from caching the tessellated geometry. In the AA case, we do not cache, so we
    // accept paths without keys.
    if (!args.fShape->style().isSimpleFill() || args.fShape->knownToBeConvex()) {
        return CanDrawPath::kNo;
    }
    if (GrAAType::kCoverage == args.fAAType) {
        SkPath path;
        args.fShape->asPath(&path);
        if (path.countVerbs() > GR_AA_TESSELLATOR_MAX_VERB_COUNT) {
            return CanDrawPath::kNo;
        }
    } else if (!args.fShape->hasUnstyledKey()) {
        return CanDrawPath::kNo;
    }
    return CanDrawPath::kYes;
}

namespace {

class TessellatingPathOp final : public GrMeshDrawOp {
private:
    using Helper = GrSimpleMeshDrawOpHelperWithStencil;

public:
    DEFINE_OP_CLASS_ID

    static std::unique_ptr<GrDrawOp> Make(GrContext* context,
                                          GrPaint&& paint,
                                          const GrShape& shape,
                                          const SkMatrix& viewMatrix,
                                          SkIRect devClipBounds,
                                          GrAAType aaType,
                                          const GrUserStencilSettings* stencilSettings) {
        return Helper::FactoryHelper<TessellatingPathOp>(context, std::move(paint), shape,
                                                         viewMatrix, devClipBounds,
                                                         aaType, stencilSettings);
    }

    const char* name() const override { return "TessellatingPathOp"; }

    void visitProxies(const VisitProxyFunc& func) const override {
        fHelper.visitProxies(func);
    }

    SkString dumpInfo() const override {
        SkString string;
        string.appendf("Color 0x%08x, aa: %d\n", fColor, fAntiAlias);
        string += fHelper.dumpInfo();
        string += INHERITED::dumpInfo();
        return string;
    }

    TessellatingPathOp(Helper::MakeArgs helperArgs,
                       GrColor color,
                       const GrShape& shape,
                       const SkMatrix& viewMatrix,
                       const SkIRect& devClipBounds,
                       GrAAType aaType,
                       const GrUserStencilSettings* stencilSettings)
            : INHERITED(ClassID())
            , fHelper(helperArgs, aaType, stencilSettings)
            , fColor(color)
            , fShape(shape)
            , fViewMatrix(viewMatrix)
            , fDevClipBounds(devClipBounds)
            , fAntiAlias(GrAAType::kCoverage == aaType) {
        SkRect devBounds;
        viewMatrix.mapRect(&devBounds, shape.bounds());
        if (shape.inverseFilled()) {
            // Because the clip bounds are used to add a contour for inverse fills, they must also
            // include the path bounds.
            devBounds.join(SkRect::Make(fDevClipBounds));
        }
        this->setBounds(devBounds, HasAABloat::kNo, IsZeroArea::kNo);
    }

    FixedFunctionFlags fixedFunctionFlags() const override { return fHelper.fixedFunctionFlags(); }

    RequiresDstTexture finalize(const GrCaps& caps, const GrAppliedClip* clip) override {
        GrProcessorAnalysisCoverage coverage = fAntiAlias
                                                       ? GrProcessorAnalysisCoverage::kSingleChannel
                                                       : GrProcessorAnalysisCoverage::kNone;
        return fHelper.xpRequiresDstTexture(caps, clip, coverage, &fColor);
    }

private:
    SkPath getPath() const {
        SkASSERT(!fShape.style().applies());
        SkPath path;
        fShape.asPath(&path);
        return path;
    }

    void draw(Target* target, sk_sp<const GrGeometryProcessor> gp, size_t vertexStride) {
        SkASSERT(!fAntiAlias);
        GrResourceProvider* rp = target->resourceProvider();
        bool inverseFill = fShape.inverseFilled();
        // construct a cache key from the path's genID and the view matrix
        static const GrUniqueKey::Domain kDomain = GrUniqueKey::GenerateDomain();
        GrUniqueKey key;
        static constexpr int kClipBoundsCnt = sizeof(fDevClipBounds) / sizeof(uint32_t);
        int shapeKeyDataCnt = fShape.unstyledKeySize();
        SkASSERT(shapeKeyDataCnt >= 0);
        GrUniqueKey::Builder builder(&key, kDomain, shapeKeyDataCnt + kClipBoundsCnt, "Path");
        fShape.writeUnstyledKey(&builder[0]);
        // For inverse fills, the tessellation is dependent on clip bounds.
        if (inverseFill) {
            memcpy(&builder[shapeKeyDataCnt], &fDevClipBounds, sizeof(fDevClipBounds));
        } else {
            memset(&builder[shapeKeyDataCnt], 0, sizeof(fDevClipBounds));
        }
        builder.finish();
        sk_sp<GrBuffer> cachedVertexBuffer(rp->findByUniqueKey<GrBuffer>(key));
        int actualCount;
        SkScalar tol = GrPathUtils::kDefaultTolerance;
        tol = GrPathUtils::scaleToleranceToSrc(tol, fViewMatrix, fShape.bounds());
        if (cache_match(cachedVertexBuffer.get(), tol, &actualCount)) {
            this->drawVertices(target, std::move(gp), cachedVertexBuffer.get(), 0, actualCount);
            return;
        }

        SkRect clipBounds = SkRect::Make(fDevClipBounds);

        SkMatrix vmi;
        if (!fViewMatrix.invert(&vmi)) {
            return;
        }
        vmi.mapRect(&clipBounds);
        bool isLinear;
        bool canMapVB = GrCaps::kNone_MapFlags != target->caps().mapBufferFlags();
        StaticVertexAllocator allocator(vertexStride, rp, canMapVB);
        int count = GrTessellator::PathToTriangles(getPath(), tol, clipBounds, &allocator,
                                                   false, GrColor(), false, &isLinear);
        if (count == 0) {
            return;
        }
        this->drawVertices(target, std::move(gp), allocator.vertexBuffer(), 0, count);
        TessInfo info;
        info.fTolerance = isLinear ? 0 : tol;
        info.fCount = count;
        key.setCustomData(SkData::MakeWithCopy(&info, sizeof(info)));
        rp->assignUniqueKeyToResource(key, allocator.vertexBuffer());
        fShape.addGenIDChangeListener(sk_make_sp<PathInvalidator>(key, target->contextUniqueID()));
    }

    void drawAA(Target* target, sk_sp<const GrGeometryProcessor> gp, size_t vertexStride) {
        SkASSERT(fAntiAlias);
        SkPath path = getPath();
        if (path.isEmpty()) {
            return;
        }
        SkRect clipBounds = SkRect::Make(fDevClipBounds);
        path.transform(fViewMatrix);
        SkScalar tol = GrPathUtils::kDefaultTolerance;
        bool isLinear;
        DynamicVertexAllocator allocator(vertexStride, target);
        int count =
                GrTessellator::PathToTriangles(path, tol, clipBounds, &allocator, true, fColor,
                                               fHelper.compatibleWithAlphaAsCoverage(), &isLinear);
        if (count == 0) {
            return;
        }
        this->drawVertices(target, std::move(gp), allocator.vertexBuffer(), allocator.firstVertex(),
                           count);
    }

    void onPrepareDraws(Target* target) override {
        sk_sp<GrGeometryProcessor> gp;
        size_t vertexStride;
        {
            using namespace GrDefaultGeoProcFactory;

            vertexStride = sizeof(SkPoint);  // position

            Color color(fColor);
            LocalCoords::Type localCoordsType = fHelper.usesLocalCoords()
                                                        ? LocalCoords::kUsePosition_Type
                                                        : LocalCoords::kUnused_Type;
            Coverage::Type coverageType;
            if (fAntiAlias) {
                color = Color(Color::kPremulGrColorAttribute_Type);
                vertexStride += sizeof(uint32_t);
                if (fHelper.compatibleWithAlphaAsCoverage()) {
                    coverageType = Coverage::kSolid_Type;
                } else {
                    coverageType = Coverage::kAttribute_Type;
                    vertexStride += 4;
                }
            } else {
                coverageType = Coverage::kSolid_Type;
            }
            if (fAntiAlias) {
                gp = GrDefaultGeoProcFactory::MakeForDeviceSpace(target->caps().shaderCaps(),
                                                                 color, coverageType,
                                                                 localCoordsType, fViewMatrix);
            } else {
                gp = GrDefaultGeoProcFactory::Make(target->caps().shaderCaps(),
                                                   color, coverageType, localCoordsType,
                                                   fViewMatrix);
            }
        }
        if (!gp.get()) {
            return;
        }
        SkASSERT(vertexStride == gp->debugOnly_vertexStride());
        if (fAntiAlias) {
            this->drawAA(target, std::move(gp), vertexStride);
        } else {
            this->draw(target, std::move(gp), vertexStride);
        }
    }

    void drawVertices(Target* target, sk_sp<const GrGeometryProcessor> gp, const GrBuffer* vb,
                      int firstVertex, int count) {
        GrMesh* mesh = target->allocMesh(TESSELLATOR_WIREFRAME ? GrPrimitiveType::kLines
                                                               : GrPrimitiveType::kTriangles);
        mesh->setNonIndexedNonInstanced(count);
        mesh->setVertexData(vb, firstVertex);
        auto pipe = fHelper.makePipeline(target);
        target->draw(std::move(gp), pipe.fPipeline, pipe.fFixedDynamicState, mesh);
    }

    Helper fHelper;
    GrColor                 fColor;
    GrShape                 fShape;
    SkMatrix                fViewMatrix;
    SkIRect                 fDevClipBounds;
    bool                    fAntiAlias;

    typedef GrMeshDrawOp INHERITED;
};

}  // anonymous namespace

bool GrTessellatingPathRenderer::onDrawPath(const DrawPathArgs& args) {
    GR_AUDIT_TRAIL_AUTO_FRAME(args.fRenderTargetContext->auditTrail(),
                              "GrTessellatingPathRenderer::onDrawPath");
    SkIRect clipBoundsI;
    args.fClip->getConservativeBounds(args.fRenderTargetContext->width(),
                                      args.fRenderTargetContext->height(),
                                      &clipBoundsI);
    std::unique_ptr<GrDrawOp> op = TessellatingPathOp::Make(args.fContext,
                                                            std::move(args.fPaint),
                                                            *args.fShape,
                                                            *args.fViewMatrix,
                                                            clipBoundsI,
                                                            args.fAAType,
                                                            args.fUserStencilSettings);
    args.fRenderTargetContext->addDrawOp(*args.fClip, std::move(op));
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if GR_TEST_UTILS

GR_DRAW_OP_TEST_DEFINE(TesselatingPathOp) {
    SkMatrix viewMatrix = GrTest::TestMatrixInvertible(random);
    SkPath path = GrTest::TestPath(random);
    SkIRect devClipBounds = SkIRect::MakeLTRB(
        random->nextU(), random->nextU(), random->nextU(), random->nextU());
    devClipBounds.sort();
    static constexpr GrAAType kAATypes[] = {GrAAType::kNone, GrAAType::kMSAA, GrAAType::kCoverage};
    GrAAType aaType;
    do {
        aaType = kAATypes[random->nextULessThan(SK_ARRAY_COUNT(kAATypes))];
    } while(GrAAType::kMSAA == aaType && GrFSAAType::kUnifiedMSAA != fsaaType);
    GrStyle style;
    do {
        GrTest::TestStyle(random, &style);
    } while (!style.isSimpleFill());
    GrShape shape(path, style);
    return TessellatingPathOp::Make(context, std::move(paint), shape, viewMatrix, devClipBounds,
                                    aaType, GrGetRandomStencil(random, context));
}

#endif
