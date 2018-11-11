/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrOvalOpFactory.h"
#include "GrDrawOpTest.h"
#include "GrGeometryProcessor.h"
#include "GrOpFlushState.h"
#include "GrProcessor.h"
#include "GrResourceProvider.h"
#include "GrShaderCaps.h"
#include "GrStyle.h"
#include "SkRRectPriv.h"
#include "SkStrokeRec.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLGeometryProcessor.h"
#include "glsl/GrGLSLProgramDataManager.h"
#include "glsl/GrGLSLUniformHandler.h"
#include "glsl/GrGLSLUtil.h"
#include "glsl/GrGLSLVarying.h"
#include "glsl/GrGLSLVertexGeoBuilder.h"
#include "ops/GrMeshDrawOp.h"
#include "ops/GrSimpleMeshDrawOpHelper.h"

#include <utility>

namespace {

struct EllipseVertex {
    SkPoint fPos;
    GrColor fColor;
    SkPoint fOffset;
    SkPoint fOuterRadii;
    SkPoint fInnerRadii;
};

struct DIEllipseVertex {
    SkPoint fPos;
    GrColor fColor;
    SkPoint fOuterOffset;
    SkPoint fInnerOffset;
};

static inline bool circle_stays_circle(const SkMatrix& m) { return m.isSimilarity(); }
}

///////////////////////////////////////////////////////////////////////////////

/**
 * The output of this effect is a modulation of the input color and coverage for a circle. It
 * operates in a space normalized by the circle radius (outer radius in the case of a stroke)
 * with origin at the circle center. Three vertex attributes are used:
 *    vec2f : position in device space of the bounding geometry vertices
 *    vec4ub: color
 *    vec4f : (p.xy, outerRad, innerRad)
 *             p is the position in the normalized space.
 *             outerRad is the outerRadius in device space.
 *             innerRad is the innerRadius in normalized space (ignored if not stroking).
 * Additional clip planes are supported for rendering circular arcs. The additional planes are
 * either intersected or unioned together. Up to three planes are supported (an initial plane,
 * a plane intersected with the initial plane, and a plane unioned with the first two). Only two
 * are useful for any given arc, but having all three in one instance allows combining different
 * types of arcs.
 * Round caps for stroking are allowed as well. The caps are specified as two circle center points
 * in the same space as p.xy.
 */

class CircleGeometryProcessor : public GrGeometryProcessor {
public:
    CircleGeometryProcessor(bool stroke, bool clipPlane, bool isectPlane, bool unionPlane,
                            bool roundCaps, const SkMatrix& localMatrix)
            : INHERITED(kCircleGeometryProcessor_ClassID)
            , fLocalMatrix(localMatrix)
            , fStroke(stroke) {
        int cnt = 3;
        if (clipPlane) {
            fInClipPlane = {"inClipPlane", kFloat3_GrVertexAttribType, kHalf3_GrSLType};
            ++cnt;
        }
        if (isectPlane) {
            fInIsectPlane = {"inIsectPlane", kFloat3_GrVertexAttribType, kHalf3_GrSLType};
            ++cnt;
        }
        if (unionPlane) {
            fInUnionPlane = {"inUnionPlane", kFloat3_GrVertexAttribType, kHalf3_GrSLType};
            ++cnt;
        }
        if (roundCaps) {
            SkASSERT(stroke);
            SkASSERT(clipPlane);
            fInRoundCapCenters =
                    {"inRoundCapCenters", kFloat4_GrVertexAttribType, kFloat4_GrSLType};
            ++cnt;
        }
        this->setVertexAttributeCnt(cnt);
    }

    ~CircleGeometryProcessor() override {}

    const char* name() const override { return "CircleEdge"; }

    void getGLSLProcessorKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const override {
        GLSLProcessor::GenKey(*this, caps, b);
    }

    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const override {
        return new GLSLProcessor();
    }

private:
    class GLSLProcessor : public GrGLSLGeometryProcessor {
    public:
        GLSLProcessor() {}

        void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
            const CircleGeometryProcessor& cgp = args.fGP.cast<CircleGeometryProcessor>();
            GrGLSLVertexBuilder* vertBuilder = args.fVertBuilder;
            GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
            GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;
            GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;

            // emit attributes
            varyingHandler->emitAttributes(cgp);
            fragBuilder->codeAppend("float4 circleEdge;");
            varyingHandler->addPassThroughAttribute(cgp.kInCircleEdge, "circleEdge");
            if (cgp.fInClipPlane.isInitialized()) {
                fragBuilder->codeAppend("half3 clipPlane;");
                varyingHandler->addPassThroughAttribute(cgp.fInClipPlane, "clipPlane");
            }
            if (cgp.fInIsectPlane.isInitialized()) {
                fragBuilder->codeAppend("half3 isectPlane;");
                varyingHandler->addPassThroughAttribute(cgp.fInIsectPlane, "isectPlane");
            }
            if (cgp.fInUnionPlane.isInitialized()) {
                SkASSERT(cgp.fInClipPlane.isInitialized());
                fragBuilder->codeAppend("half3 unionPlane;");
                varyingHandler->addPassThroughAttribute(cgp.fInUnionPlane, "unionPlane");
            }
            GrGLSLVarying capRadius(kFloat_GrSLType);
            if (cgp.fInRoundCapCenters.isInitialized()) {
                fragBuilder->codeAppend("float4 roundCapCenters;");
                varyingHandler->addPassThroughAttribute(cgp.fInRoundCapCenters, "roundCapCenters");
                varyingHandler->addVarying("capRadius", &capRadius,
                                           GrGLSLVaryingHandler::Interpolation::kCanBeFlat);
                // This is the cap radius in normalized space where the outer radius is 1 and
                // circledEdge.w is the normalized inner radius.
                vertBuilder->codeAppendf("%s = (1.0 - %s.w) / 2.0;", capRadius.vsOut(),
                                         cgp.kInCircleEdge.name());
            }

            // setup pass through color
            varyingHandler->addPassThroughAttribute(cgp.kInColor, args.fOutputColor);

            // Setup position
            this->writeOutputPosition(vertBuilder, gpArgs, cgp.kInPosition.name());

            // emit transforms
            this->emitTransforms(vertBuilder,
                                 varyingHandler,
                                 uniformHandler,
                                 cgp.kInPosition.asShaderVar(),
                                 cgp.fLocalMatrix,
                                 args.fFPCoordTransformHandler);

            fragBuilder->codeAppend("float d = length(circleEdge.xy);");
            fragBuilder->codeAppend("half distanceToOuterEdge = circleEdge.z * (1.0 - d);");
            fragBuilder->codeAppend("half edgeAlpha = saturate(distanceToOuterEdge);");
            if (cgp.fStroke) {
                fragBuilder->codeAppend(
                        "half distanceToInnerEdge = circleEdge.z * (d - circleEdge.w);");
                fragBuilder->codeAppend("half innerAlpha = saturate(distanceToInnerEdge);");
                fragBuilder->codeAppend("edgeAlpha *= innerAlpha;");
            }

            if (cgp.fInClipPlane.isInitialized()) {
                fragBuilder->codeAppend(
                        "half clip = saturate(circleEdge.z * dot(circleEdge.xy, clipPlane.xy) + "
                        "clipPlane.z);");
                if (cgp.fInIsectPlane.isInitialized()) {
                    fragBuilder->codeAppend(
                            "clip *= saturate(circleEdge.z * dot(circleEdge.xy, isectPlane.xy) + "
                            "isectPlane.z);");
                }
                if (cgp.fInUnionPlane.isInitialized()) {
                    fragBuilder->codeAppend(
                            "clip = saturate(clip + saturate(circleEdge.z * dot(circleEdge.xy, "
                            "unionPlane.xy) + unionPlane.z));");
                }
                fragBuilder->codeAppend("edgeAlpha *= clip;");
                if (cgp.fInRoundCapCenters.isInitialized()) {
                    // We compute coverage of the round caps as circles at the butt caps produced
                    // by the clip planes. The inverse of the clip planes is applied so that there
                    // is no double counting.
                    fragBuilder->codeAppendf(
                            "half dcap1 = circleEdge.z * (%s - length(circleEdge.xy - "
                            "                                         roundCapCenters.xy));"
                            "half dcap2 = circleEdge.z * (%s - length(circleEdge.xy - "
                            "                                         roundCapCenters.zw));"
                            "half capAlpha = (1 - clip) * (max(dcap1, 0) + max(dcap2, 0));"
                            "edgeAlpha = min(edgeAlpha + capAlpha, 1.0);",
                            capRadius.fsIn(), capRadius.fsIn());
                }
            }
            fragBuilder->codeAppendf("%s = half4(edgeAlpha);", args.fOutputCoverage);
        }

        static void GenKey(const GrGeometryProcessor& gp,
                           const GrShaderCaps&,
                           GrProcessorKeyBuilder* b) {
            const CircleGeometryProcessor& cgp = gp.cast<CircleGeometryProcessor>();
            uint16_t key;
            key = cgp.fStroke ? 0x01 : 0x0;
            key |= cgp.fLocalMatrix.hasPerspective() ? 0x02 : 0x0;
            key |= cgp.fInClipPlane.isInitialized() ? 0x04 : 0x0;
            key |= cgp.fInIsectPlane.isInitialized() ? 0x08 : 0x0;
            key |= cgp.fInUnionPlane.isInitialized() ? 0x10 : 0x0;
            key |= cgp.fInRoundCapCenters.isInitialized() ? 0x20 : 0x0;
            b->add32(key);
        }

        void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor& primProc,
                     FPCoordTransformIter&& transformIter) override {
            this->setTransformDataHelper(primProc.cast<CircleGeometryProcessor>().fLocalMatrix,
                                         pdman, &transformIter);
        }

    private:
        typedef GrGLSLGeometryProcessor INHERITED;
    };

    const Attribute& onVertexAttribute(int i) const override {
        return IthInitializedAttribute(i, kInPosition, kInColor, kInCircleEdge, fInClipPlane,
                                       fInIsectPlane, fInUnionPlane, fInRoundCapCenters);
    }

    SkMatrix fLocalMatrix;

    static constexpr Attribute kInPosition =
            {"inPosition", kFloat2_GrVertexAttribType, kFloat2_GrSLType};
    static constexpr Attribute kInColor =
            {"inColor", kUByte4_norm_GrVertexAttribType, kHalf4_GrSLType};
    static constexpr Attribute kInCircleEdge =
            {"inCircleEdge", kFloat4_GrVertexAttribType, kFloat4_GrSLType};

    // Optional attributes.
    Attribute fInClipPlane;
    Attribute fInIsectPlane;
    Attribute fInUnionPlane;
    Attribute fInRoundCapCenters;

    bool fStroke;
    GR_DECLARE_GEOMETRY_PROCESSOR_TEST

    typedef GrGeometryProcessor INHERITED;
};
constexpr GrPrimitiveProcessor::Attribute CircleGeometryProcessor::kInPosition;
constexpr GrPrimitiveProcessor::Attribute CircleGeometryProcessor::kInColor;
constexpr GrPrimitiveProcessor::Attribute CircleGeometryProcessor::kInCircleEdge;

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(CircleGeometryProcessor);

#if GR_TEST_UTILS
sk_sp<GrGeometryProcessor> CircleGeometryProcessor::TestCreate(GrProcessorTestData* d) {
    bool stroke = d->fRandom->nextBool();
    bool roundCaps = stroke ? d->fRandom->nextBool() : false;
    bool clipPlane = d->fRandom->nextBool();
    bool isectPlane = d->fRandom->nextBool();
    bool unionPlane = d->fRandom->nextBool();
    const SkMatrix& matrix = GrTest::TestMatrix(d->fRandom);
    return sk_sp<GrGeometryProcessor>(new CircleGeometryProcessor(stroke, roundCaps, clipPlane,
                                                                  isectPlane, unionPlane, matrix));
}
#endif

class ButtCapDashedCircleGeometryProcessor : public GrGeometryProcessor {
public:
    ButtCapDashedCircleGeometryProcessor(const SkMatrix& localMatrix)
            : INHERITED(kButtCapStrokedCircleGeometryProcessor_ClassID), fLocalMatrix(localMatrix) {
        this->setVertexAttributeCnt(4);
    }

    ~ButtCapDashedCircleGeometryProcessor() override {}

    const char* name() const override { return "ButtCapDashedCircleGeometryProcessor"; }

    void getGLSLProcessorKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const override {
        GLSLProcessor::GenKey(*this, caps, b);
    }

    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const override {
        return new GLSLProcessor();
    }

private:
    class GLSLProcessor : public GrGLSLGeometryProcessor {
    public:
        GLSLProcessor() {}

        void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
            const ButtCapDashedCircleGeometryProcessor& bcscgp =
                    args.fGP.cast<ButtCapDashedCircleGeometryProcessor>();
            GrGLSLVertexBuilder* vertBuilder = args.fVertBuilder;
            GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
            GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;
            GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;

            // emit attributes
            varyingHandler->emitAttributes(bcscgp);
            fragBuilder->codeAppend("float4 circleEdge;");
            varyingHandler->addPassThroughAttribute(bcscgp.kInCircleEdge, "circleEdge");

            fragBuilder->codeAppend("float4 dashParams;");
            varyingHandler->addPassThroughAttribute(
                    bcscgp.kInDashParams, "dashParams",
                    GrGLSLVaryingHandler::Interpolation::kCanBeFlat);
            GrGLSLVarying wrapDashes(kHalf4_GrSLType);
            varyingHandler->addVarying("wrapDashes", &wrapDashes,
                                       GrGLSLVaryingHandler::Interpolation::kCanBeFlat);
            GrGLSLVarying lastIntervalLength(kHalf_GrSLType);
            varyingHandler->addVarying("lastIntervalLength", &lastIntervalLength,
                                       GrGLSLVaryingHandler::Interpolation::kCanBeFlat);
            vertBuilder->codeAppendf("float4 dashParams = %s;", bcscgp.kInDashParams.name());
            // Our fragment shader works in on/off intervals as specified by dashParams.xy:
            //     x = length of on interval, y = length of on + off.
            // There are two other parameters in dashParams.zw:
            //     z = start angle in radians, w = phase offset in radians in range -y/2..y/2.
            // Each interval has a "corresponding" dash which may be shifted partially or
            // fully out of its interval by the phase. So there may be up to two "visual"
            // dashes in an interval.
            // When computing coverage in an interval we look at three dashes. These are the
            // "corresponding" dashes from the current, previous, and next intervals. Any of these
            // may be phase shifted into our interval or even when phase=0 they may be within half a
            // pixel distance of a pixel center in the interval.
            // When in the first interval we need to check the dash from the last interval. And
            // similarly when in the last interval we need to check the dash from the first
            // interval. When 2pi is not perfectly divisible dashParams.y this is a boundary case.
            // We compute the dash begin/end angles in the vertex shader and apply them in the
            // fragment shader when we detect we're in the first/last interval.
            vertBuilder->codeAppend(R"(
                    // The two boundary dash intervals are stored in wrapDashes.xy and .zw and fed
                    // to the fragment shader as a varying.
                    float4 wrapDashes;
                    half lastIntervalLength = mod(6.28318530718, dashParams.y);
                    // We can happen to be perfectly divisible.
                    if (0 == lastIntervalLength) {
                        lastIntervalLength = dashParams.y;
                    }
                    // Let 'l' be the last interval before reaching 2 pi.
                    // Based on the phase determine whether (l-1)th, l-th, or (l+1)th interval's
                    // "corresponding" dash appears in the l-th interval and is closest to the 0-th
                    // interval.
                    half offset = 0;
                    if (-dashParams.w >= lastIntervalLength) {
                         offset = -dashParams.y;
                    } else if (dashParams.w > dashParams.y - lastIntervalLength) {
                         offset = dashParams.y;
                    }
                    wrapDashes.x = -lastIntervalLength + offset - dashParams.w;
                    // The end of this dash may be beyond the 2 pi and therefore clipped. Hence the
                    // min.
                    wrapDashes.y = min(wrapDashes.x + dashParams.x, 0);

                    // Based on the phase determine whether the -1st, 0th, or 1st interval's
                    // "corresponding" dash appears in the 0th interval and is closest to l.
                    offset = 0;
                    if (dashParams.w >= dashParams.x) {
                        offset = dashParams.y;
                    } else if (-dashParams.w > dashParams.y - dashParams.x) {
                        offset = -dashParams.y;
                    }
                    wrapDashes.z = lastIntervalLength + offset - dashParams.w;
                    wrapDashes.w = wrapDashes.z + dashParams.x;
                    // The start of the dash we're considering may be clipped by the start of the
                    // circle.
                    wrapDashes.z = max(wrapDashes.z, lastIntervalLength);
            )");
            vertBuilder->codeAppendf("%s = wrapDashes;", wrapDashes.vsOut());
            vertBuilder->codeAppendf("%s = lastIntervalLength;", lastIntervalLength.vsOut());
            fragBuilder->codeAppendf("half4 wrapDashes = %s;", wrapDashes.fsIn());
            fragBuilder->codeAppendf("half lastIntervalLength = %s;", lastIntervalLength.fsIn());

            // setup pass through color
            varyingHandler->addPassThroughAttribute(
                    bcscgp.kInColor, args.fOutputColor,
                    GrGLSLVaryingHandler::Interpolation::kCanBeFlat);

            // Setup position
            this->writeOutputPosition(vertBuilder, gpArgs, bcscgp.kInPosition.name());

            // emit transforms
            this->emitTransforms(vertBuilder,
                                 varyingHandler,
                                 uniformHandler,
                                 bcscgp.kInPosition.asShaderVar(),
                                 bcscgp.fLocalMatrix,
                                 args.fFPCoordTransformHandler);
            GrShaderVar fnArgs[] = {
                    GrShaderVar("angleToEdge", kFloat_GrSLType),
                    GrShaderVar("diameter", kFloat_GrSLType),
            };
            SkString fnName;
            fragBuilder->emitFunction(kFloat_GrSLType, "coverage_from_dash_edge",
                                      SK_ARRAY_COUNT(fnArgs), fnArgs, R"(
                    float linearDist;
                    angleToEdge = clamp(angleToEdge, -3.1415, 3.1415);
                    linearDist = diameter * sin(angleToEdge / 2);
                    return saturate(linearDist + 0.5);
            )",
                                      &fnName);
            fragBuilder->codeAppend(R"(
                    float d = length(circleEdge.xy) * circleEdge.z;

                    // Compute coverage from outer/inner edges of the stroke.
                    half distanceToOuterEdge = circleEdge.z - d;
                    half edgeAlpha = saturate(distanceToOuterEdge);
                    half distanceToInnerEdge = d - circleEdge.z * circleEdge.w;
                    half innerAlpha = saturate(distanceToInnerEdge);
                    edgeAlpha *= innerAlpha;

                    half angleFromStart = atan(circleEdge.y, circleEdge.x) - dashParams.z;
                    angleFromStart = mod(angleFromStart, 6.28318530718);
                    float x = mod(angleFromStart, dashParams.y);
                    // Convert the radial distance from center to pixel into a diameter.
                    d *= 2;
                    half2 currDash = half2(-dashParams.w, dashParams.x - dashParams.w);
                    half2 nextDash = half2(dashParams.y - dashParams.w,
                                           dashParams.y + dashParams.x - dashParams.w);
                    half2 prevDash = half2(-dashParams.y - dashParams.w,
                                           -dashParams.y + dashParams.x - dashParams.w);
                    half dashAlpha = 0;
                )");
            fragBuilder->codeAppendf(R"(
                    if (angleFromStart - x + dashParams.y >= 6.28318530718) {
                         dashAlpha += %s(x - wrapDashes.z, d) * %s(wrapDashes.w - x, d);
                         currDash.y = min(currDash.y, lastIntervalLength);
                         if (nextDash.x >= lastIntervalLength) {
                             // The next dash is outside the 0..2pi range, throw it away
                             nextDash.xy = half2(1000);
                         } else {
                             // Clip the end of the next dash to the end of the circle
                             nextDash.y = min(nextDash.y, lastIntervalLength);
                         }
                    }
            )", fnName.c_str(), fnName.c_str());
            fragBuilder->codeAppendf(R"(
                    if (angleFromStart - x - dashParams.y < -0.01) {
                         dashAlpha += %s(x - wrapDashes.x, d) * %s(wrapDashes.y - x, d);
                         currDash.x = max(currDash.x, 0);
                         if (prevDash.y <= 0) {
                             // The previous dash is outside the 0..2pi range, throw it away
                             prevDash.xy = half2(1000);
                         } else {
                             // Clip the start previous dash to the start of the circle
                             prevDash.x = max(prevDash.x, 0);
                         }
                    }
            )", fnName.c_str(), fnName.c_str());
            fragBuilder->codeAppendf(R"(
                    dashAlpha += %s(x - currDash.x, d) * %s(currDash.y - x, d);
                    dashAlpha += %s(x - nextDash.x, d) * %s(nextDash.y - x, d);
                    dashAlpha += %s(x - prevDash.x, d) * %s(prevDash.y - x, d);
                    dashAlpha = min(dashAlpha, 1);
                    edgeAlpha *= dashAlpha;
            )", fnName.c_str(), fnName.c_str(), fnName.c_str(), fnName.c_str(), fnName.c_str(),
                fnName.c_str());
            fragBuilder->codeAppendf("%s = half4(edgeAlpha);", args.fOutputCoverage);
        }

        static void GenKey(const GrGeometryProcessor& gp,
                           const GrShaderCaps&,
                           GrProcessorKeyBuilder* b) {
            const ButtCapDashedCircleGeometryProcessor& bcscgp =
                    gp.cast<ButtCapDashedCircleGeometryProcessor>();
            b->add32(bcscgp.fLocalMatrix.hasPerspective());
        }

        void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor& primProc,
                     FPCoordTransformIter&& transformIter) override {
            this->setTransformDataHelper(
                    primProc.cast<ButtCapDashedCircleGeometryProcessor>().fLocalMatrix, pdman,
                    &transformIter);
        }

    private:
        typedef GrGLSLGeometryProcessor INHERITED;
    };

    const Attribute& onVertexAttribute(int i) const override {
        return IthAttribute(i, kInPosition, kInColor, kInCircleEdge, kInDashParams);
    }

    SkMatrix fLocalMatrix;
    static constexpr Attribute kInPosition =
            {"inPosition", kFloat2_GrVertexAttribType, kFloat2_GrSLType};
    static constexpr Attribute kInColor =
            {"inColor", kUByte4_norm_GrVertexAttribType, kHalf4_GrSLType};
    static constexpr Attribute kInCircleEdge =
            {"inCircleEdge", kFloat4_GrVertexAttribType, kFloat4_GrSLType};
    static constexpr Attribute kInDashParams =
            {"inDashParams", kFloat4_GrVertexAttribType, kFloat4_GrSLType};

    GR_DECLARE_GEOMETRY_PROCESSOR_TEST

    typedef GrGeometryProcessor INHERITED;
};
constexpr GrPrimitiveProcessor::Attribute ButtCapDashedCircleGeometryProcessor::kInPosition;
constexpr GrPrimitiveProcessor::Attribute ButtCapDashedCircleGeometryProcessor::kInColor;
constexpr GrPrimitiveProcessor::Attribute ButtCapDashedCircleGeometryProcessor::kInCircleEdge;
constexpr GrPrimitiveProcessor::Attribute ButtCapDashedCircleGeometryProcessor::kInDashParams;

#if GR_TEST_UTILS
sk_sp<GrGeometryProcessor> ButtCapDashedCircleGeometryProcessor::TestCreate(GrProcessorTestData* d) {
    const SkMatrix& matrix = GrTest::TestMatrix(d->fRandom);
    return sk_sp<GrGeometryProcessor>(new ButtCapDashedCircleGeometryProcessor(matrix));
}
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * The output of this effect is a modulation of the input color and coverage for an axis-aligned
 * ellipse, specified as a 2D offset from center, and the reciprocals of the outer and inner radii,
 * in both x and y directions.
 *
 * We are using an implicit function of x^2/a^2 + y^2/b^2 - 1 = 0.
 */

class EllipseGeometryProcessor : public GrGeometryProcessor {
public:
    EllipseGeometryProcessor(bool stroke, const SkMatrix& localMatrix)
    : INHERITED(kEllipseGeometryProcessor_ClassID)
    , fLocalMatrix(localMatrix) {
        this->setVertexAttributeCnt(4);
        fStroke = stroke;
    }

    ~EllipseGeometryProcessor() override {}

    const char* name() const override { return "EllipseEdge"; }

    void getGLSLProcessorKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const override {
        GLSLProcessor::GenKey(*this, caps, b);
    }

    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const override {
        return new GLSLProcessor();
    }

private:
    class GLSLProcessor : public GrGLSLGeometryProcessor {
    public:
        GLSLProcessor() {}

        void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
            const EllipseGeometryProcessor& egp = args.fGP.cast<EllipseGeometryProcessor>();
            GrGLSLVertexBuilder* vertBuilder = args.fVertBuilder;
            GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
            GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;

            // emit attributes
            varyingHandler->emitAttributes(egp);

            GrGLSLVarying ellipseOffsets(kHalf2_GrSLType);
            varyingHandler->addVarying("EllipseOffsets", &ellipseOffsets);
            vertBuilder->codeAppendf("%s = %s;", ellipseOffsets.vsOut(),
                                     egp.kInEllipseOffset.name());

            GrGLSLVarying ellipseRadii(kHalf4_GrSLType);
            varyingHandler->addVarying("EllipseRadii", &ellipseRadii);
            vertBuilder->codeAppendf("%s = %s;", ellipseRadii.vsOut(), egp.kInEllipseRadii.name());

            GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
            // setup pass through color
            varyingHandler->addPassThroughAttribute(egp.kInColor, args.fOutputColor);

            // Setup position
            this->writeOutputPosition(vertBuilder, gpArgs, egp.kInPosition.name());

            // emit transforms
            this->emitTransforms(vertBuilder,
                                 varyingHandler,
                                 uniformHandler,
                                 egp.kInPosition.asShaderVar(),
                                 egp.fLocalMatrix,
                                 args.fFPCoordTransformHandler);
            // For stroked ellipses, we use the full ellipse equation (x^2/a^2 + y^2/b^2 = 1)
            // to compute both the edges because we need two separate test equations for
            // the single offset.
            // For filled ellipses we can use a unit circle equation (x^2 + y^2 = 1), and warp
            // the distance by the gradient, non-uniformly scaled by the inverse of the
            // ellipse size.

            // for outer curve
            fragBuilder->codeAppendf("half2 offset = %s;", ellipseOffsets.fsIn());
            if (egp.fStroke) {
                fragBuilder->codeAppendf("offset *= %s.xy;", ellipseRadii.fsIn());
            }
            fragBuilder->codeAppend("half test = dot(offset, offset) - 1.0;");
            fragBuilder->codeAppendf("half2 grad = 2.0*offset*%s.xy;", ellipseRadii.fsIn());
            fragBuilder->codeAppend("half grad_dot = dot(grad, grad);");

            // avoid calling inversesqrt on zero.
            fragBuilder->codeAppend("grad_dot = max(grad_dot, 1.0e-4);");
            fragBuilder->codeAppend("half invlen = inversesqrt(grad_dot);");
            fragBuilder->codeAppend("half edgeAlpha = saturate(0.5-test*invlen);");

            // for inner curve
            if (egp.fStroke) {
                fragBuilder->codeAppendf("offset = %s*%s.zw;", ellipseOffsets.fsIn(),
                                         ellipseRadii.fsIn());
                fragBuilder->codeAppend("test = dot(offset, offset) - 1.0;");
                fragBuilder->codeAppendf("grad = 2.0*offset*%s.zw;", ellipseRadii.fsIn());
                fragBuilder->codeAppend("invlen = inversesqrt(dot(grad, grad));");
                fragBuilder->codeAppend("edgeAlpha *= saturate(0.5+test*invlen);");
            }

            fragBuilder->codeAppendf("%s = half4(edgeAlpha);", args.fOutputCoverage);
        }

        static void GenKey(const GrGeometryProcessor& gp,
                           const GrShaderCaps&,
                           GrProcessorKeyBuilder* b) {
            const EllipseGeometryProcessor& egp = gp.cast<EllipseGeometryProcessor>();
            uint16_t key = egp.fStroke ? 0x1 : 0x0;
            key |= egp.fLocalMatrix.hasPerspective() ? 0x2 : 0x0;
            b->add32(key);
        }

        void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor& primProc,
                     FPCoordTransformIter&& transformIter) override {
            const EllipseGeometryProcessor& egp = primProc.cast<EllipseGeometryProcessor>();
            this->setTransformDataHelper(egp.fLocalMatrix, pdman, &transformIter);
        }

    private:
        typedef GrGLSLGeometryProcessor INHERITED;
    };

    const Attribute& onVertexAttribute(int i) const override {
        return IthAttribute(i, kInPosition, kInColor, kInEllipseOffset, kInEllipseRadii);
    }

    static constexpr Attribute kInPosition =
            {"inPosition", kFloat2_GrVertexAttribType, kFloat2_GrSLType};
    static constexpr Attribute kInColor =
            {"inColor", kUByte4_norm_GrVertexAttribType, kHalf4_GrSLType};
    static constexpr Attribute kInEllipseOffset =
            {"inEllipseOffset", kFloat2_GrVertexAttribType, kHalf2_GrSLType};
    static constexpr Attribute kInEllipseRadii =
            {"inEllipseRadii", kFloat4_GrVertexAttribType, kHalf4_GrSLType};

    SkMatrix fLocalMatrix;
    bool fStroke;

    GR_DECLARE_GEOMETRY_PROCESSOR_TEST

    typedef GrGeometryProcessor INHERITED;
};
constexpr GrPrimitiveProcessor::Attribute EllipseGeometryProcessor::kInPosition;
constexpr GrPrimitiveProcessor::Attribute EllipseGeometryProcessor::kInColor;
constexpr GrPrimitiveProcessor::Attribute EllipseGeometryProcessor::kInEllipseOffset;
constexpr GrPrimitiveProcessor::Attribute EllipseGeometryProcessor::kInEllipseRadii;

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(EllipseGeometryProcessor);

#if GR_TEST_UTILS
sk_sp<GrGeometryProcessor> EllipseGeometryProcessor::TestCreate(GrProcessorTestData* d) {
    return sk_sp<GrGeometryProcessor>(
            new EllipseGeometryProcessor(d->fRandom->nextBool(), GrTest::TestMatrix(d->fRandom)));
}
#endif

///////////////////////////////////////////////////////////////////////////////

/**
 * The output of this effect is a modulation of the input color and coverage for an ellipse,
 * specified as a 2D offset from center for both the outer and inner paths (if stroked). The
 * implict equation used is for a unit circle (x^2 + y^2 - 1 = 0) and the edge corrected by
 * using differentials.
 *
 * The result is device-independent and can be used with any affine matrix.
 */

enum class DIEllipseStyle { kStroke = 0, kHairline, kFill };

class DIEllipseGeometryProcessor : public GrGeometryProcessor {
public:
    DIEllipseGeometryProcessor(const SkMatrix& viewMatrix, DIEllipseStyle style)
            : INHERITED(kDIEllipseGeometryProcessor_ClassID)
            , fViewMatrix(viewMatrix) {
        fStyle = style;
        this->setVertexAttributeCnt(4);
    }

    ~DIEllipseGeometryProcessor() override {}

    const char* name() const override { return "DIEllipseEdge"; }

    void getGLSLProcessorKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const override {
        GLSLProcessor::GenKey(*this, caps, b);
    }

    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const override {
        return new GLSLProcessor();
    }

private:
    class GLSLProcessor : public GrGLSLGeometryProcessor {
    public:
        GLSLProcessor() : fViewMatrix(SkMatrix::InvalidMatrix()) {}

        void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
            const DIEllipseGeometryProcessor& diegp = args.fGP.cast<DIEllipseGeometryProcessor>();
            GrGLSLVertexBuilder* vertBuilder = args.fVertBuilder;
            GrGLSLVaryingHandler* varyingHandler = args.fVaryingHandler;
            GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;

            // emit attributes
            varyingHandler->emitAttributes(diegp);

            GrGLSLVarying offsets0(kHalf2_GrSLType);
            varyingHandler->addVarying("EllipseOffsets0", &offsets0);
            vertBuilder->codeAppendf("%s = %s;", offsets0.vsOut(), diegp.kInEllipseOffsets0.name());

            GrGLSLVarying offsets1(kHalf2_GrSLType);
            varyingHandler->addVarying("EllipseOffsets1", &offsets1);
            vertBuilder->codeAppendf("%s = %s;", offsets1.vsOut(), diegp.kInEllipseOffsets1.name());

            GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
            varyingHandler->addPassThroughAttribute(diegp.kInColor, args.fOutputColor);

            // Setup position
            this->writeOutputPosition(vertBuilder,
                                      uniformHandler,
                                      gpArgs,
                                      diegp.kInPosition.name(),
                                      diegp.fViewMatrix,
                                      &fViewMatrixUniform);

            // emit transforms
            this->emitTransforms(vertBuilder,
                                 varyingHandler,
                                 uniformHandler,
                                 diegp.kInPosition.asShaderVar(),
                                 args.fFPCoordTransformHandler);

            // for outer curve
            fragBuilder->codeAppendf("half2 scaledOffset = %s.xy;", offsets0.fsIn());
            fragBuilder->codeAppend("half test = dot(scaledOffset, scaledOffset) - 1.0;");
            fragBuilder->codeAppendf("half2 duvdx = dFdx(%s);", offsets0.fsIn());
            fragBuilder->codeAppendf("half2 duvdy = dFdy(%s);", offsets0.fsIn());
            fragBuilder->codeAppendf(
                    "half2 grad = half2(2.0*%s.x*duvdx.x + 2.0*%s.y*duvdx.y,"
                    "                  2.0*%s.x*duvdy.x + 2.0*%s.y*duvdy.y);",
                    offsets0.fsIn(), offsets0.fsIn(), offsets0.fsIn(), offsets0.fsIn());

            fragBuilder->codeAppend("half grad_dot = dot(grad, grad);");
            // avoid calling inversesqrt on zero.
            fragBuilder->codeAppend("grad_dot = max(grad_dot, 1.0e-4);");
            fragBuilder->codeAppend("half invlen = inversesqrt(grad_dot);");
            if (DIEllipseStyle::kHairline == diegp.fStyle) {
                // can probably do this with one step
                fragBuilder->codeAppend("half edgeAlpha = saturate(1.0-test*invlen);");
                fragBuilder->codeAppend("edgeAlpha *= saturate(1.0+test*invlen);");
            } else {
                fragBuilder->codeAppend("half edgeAlpha = saturate(0.5-test*invlen);");
            }

            // for inner curve
            if (DIEllipseStyle::kStroke == diegp.fStyle) {
                fragBuilder->codeAppendf("scaledOffset = %s.xy;", offsets1.fsIn());
                fragBuilder->codeAppend("test = dot(scaledOffset, scaledOffset) - 1.0;");
                fragBuilder->codeAppendf("duvdx = dFdx(%s);", offsets1.fsIn());
                fragBuilder->codeAppendf("duvdy = dFdy(%s);", offsets1.fsIn());
                fragBuilder->codeAppendf(
                        "grad = half2(2.0*%s.x*duvdx.x + 2.0*%s.y*duvdx.y,"
                        "             2.0*%s.x*duvdy.x + 2.0*%s.y*duvdy.y);",
                        offsets1.fsIn(), offsets1.fsIn(), offsets1.fsIn(), offsets1.fsIn());
                fragBuilder->codeAppend("invlen = inversesqrt(dot(grad, grad));");
                fragBuilder->codeAppend("edgeAlpha *= saturate(0.5+test*invlen);");
            }

            fragBuilder->codeAppendf("%s = half4(edgeAlpha);", args.fOutputCoverage);
        }

        static void GenKey(const GrGeometryProcessor& gp,
                           const GrShaderCaps&,
                           GrProcessorKeyBuilder* b) {
            const DIEllipseGeometryProcessor& diegp = gp.cast<DIEllipseGeometryProcessor>();
            uint16_t key = static_cast<uint16_t>(diegp.fStyle);
            key |= ComputePosKey(diegp.fViewMatrix) << 10;
            b->add32(key);
        }

        void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor& gp,
                     FPCoordTransformIter&& transformIter) override {
            const DIEllipseGeometryProcessor& diegp = gp.cast<DIEllipseGeometryProcessor>();

            if (!diegp.fViewMatrix.isIdentity() && !fViewMatrix.cheapEqualTo(diegp.fViewMatrix)) {
                fViewMatrix = diegp.fViewMatrix;
                float viewMatrix[3 * 3];
                GrGLSLGetMatrix<3>(viewMatrix, fViewMatrix);
                pdman.setMatrix3f(fViewMatrixUniform, viewMatrix);
            }
            this->setTransformDataHelper(SkMatrix::I(), pdman, &transformIter);
        }

    private:
        SkMatrix fViewMatrix;
        UniformHandle fViewMatrixUniform;

        typedef GrGLSLGeometryProcessor INHERITED;
    };

    const Attribute& onVertexAttribute(int i) const override {
        return IthAttribute(i, kInPosition, kInColor, kInEllipseOffsets0, kInEllipseOffsets1);
    }

    static constexpr Attribute kInPosition =
            {"inPosition", kFloat2_GrVertexAttribType, kFloat2_GrSLType};
    static constexpr Attribute kInColor =
            {"inColor", kUByte4_norm_GrVertexAttribType, kHalf4_GrSLType};
    static constexpr Attribute kInEllipseOffsets0 = {"inEllipseOffsets0",
                                                     kFloat2_GrVertexAttribType,
                                                     kHalf2_GrSLType};
    static constexpr Attribute kInEllipseOffsets1 = {"inEllipseOffsets1",
                                                     kFloat2_GrVertexAttribType,
                                                     kHalf2_GrSLType};

    SkMatrix fViewMatrix;
    DIEllipseStyle fStyle;

    GR_DECLARE_GEOMETRY_PROCESSOR_TEST

    typedef GrGeometryProcessor INHERITED;
};
constexpr GrPrimitiveProcessor::Attribute DIEllipseGeometryProcessor::kInPosition;
constexpr GrPrimitiveProcessor::Attribute DIEllipseGeometryProcessor::kInColor;
constexpr GrPrimitiveProcessor::Attribute DIEllipseGeometryProcessor::kInEllipseOffsets0;
constexpr GrPrimitiveProcessor::Attribute DIEllipseGeometryProcessor::kInEllipseOffsets1;

GR_DEFINE_GEOMETRY_PROCESSOR_TEST(DIEllipseGeometryProcessor);

#if GR_TEST_UTILS
sk_sp<GrGeometryProcessor> DIEllipseGeometryProcessor::TestCreate(GrProcessorTestData* d) {
    return sk_sp<GrGeometryProcessor>(new DIEllipseGeometryProcessor(
            GrTest::TestMatrix(d->fRandom), (DIEllipseStyle)(d->fRandom->nextRangeU(0, 2))));
}
#endif

///////////////////////////////////////////////////////////////////////////////

// We have two possible cases for geometry for a circle:

// In the case of a normal fill, we draw geometry for the circle as an octagon.
static const uint16_t gFillCircleIndices[] = {
        // enter the octagon
        // clang-format off
        0, 1, 8, 1, 2, 8,
        2, 3, 8, 3, 4, 8,
        4, 5, 8, 5, 6, 8,
        6, 7, 8, 7, 0, 8
        // clang-format on
};

// For stroked circles, we use two nested octagons.
static const uint16_t gStrokeCircleIndices[] = {
        // enter the octagon
        // clang-format off
        0, 1,  9, 0, 9,   8,
        1, 2, 10, 1, 10,  9,
        2, 3, 11, 2, 11, 10,
        3, 4, 12, 3, 12, 11,
        4, 5, 13, 4, 13, 12,
        5, 6, 14, 5, 14, 13,
        6, 7, 15, 6, 15, 14,
        7, 0,  8, 7,  8, 15,
        // clang-format on
};


static const int kIndicesPerFillCircle = SK_ARRAY_COUNT(gFillCircleIndices);
static const int kIndicesPerStrokeCircle = SK_ARRAY_COUNT(gStrokeCircleIndices);
static const int kVertsPerStrokeCircle = 16;
static const int kVertsPerFillCircle = 9;

static int circle_type_to_vert_count(bool stroked) {
    return stroked ? kVertsPerStrokeCircle : kVertsPerFillCircle;
}

static int circle_type_to_index_count(bool stroked) {
    return stroked ? kIndicesPerStrokeCircle : kIndicesPerFillCircle;
}

static const uint16_t* circle_type_to_indices(bool stroked) {
    return stroked ? gStrokeCircleIndices : gFillCircleIndices;
}

///////////////////////////////////////////////////////////////////////////////

class CircleOp final : public GrMeshDrawOp {
private:
    using Helper = GrSimpleMeshDrawOpHelper;

public:
    DEFINE_OP_CLASS_ID

    /** Optional extra params to render a partial arc rather than a full circle. */
    struct ArcParams {
        SkScalar fStartAngleRadians;
        SkScalar fSweepAngleRadians;
        bool fUseCenter;
    };

    static std::unique_ptr<GrDrawOp> Make(GrContext* context,
                                          GrPaint&& paint,
                                          const SkMatrix& viewMatrix,
                                          SkPoint center,
                                          SkScalar radius,
                                          const GrStyle& style,
                                          const ArcParams* arcParams = nullptr) {
        SkASSERT(circle_stays_circle(viewMatrix));
        if (style.hasPathEffect()) {
            return nullptr;
        }
        const SkStrokeRec& stroke = style.strokeRec();
        SkStrokeRec::Style recStyle = stroke.getStyle();
        if (arcParams) {
            // Arc support depends on the style.
            switch (recStyle) {
                case SkStrokeRec::kStrokeAndFill_Style:
                    // This produces a strange result that this op doesn't implement.
                    return nullptr;
                case SkStrokeRec::kFill_Style:
                    // This supports all fills.
                    break;
                case SkStrokeRec::kStroke_Style:
                    // Strokes that don't use the center point are supported with butt and round
                    // caps.
                    if (arcParams->fUseCenter || stroke.getCap() == SkPaint::kSquare_Cap) {
                        return nullptr;
                    }
                    break;
                case SkStrokeRec::kHairline_Style:
                    // Hairline only supports butt cap. Round caps could be emulated by slightly
                    // extending the angle range if we ever care to.
                    if (arcParams->fUseCenter || stroke.getCap() != SkPaint::kButt_Cap) {
                        return nullptr;
                    }
                    break;
            }
        }
        return Helper::FactoryHelper<CircleOp>(context, std::move(paint), viewMatrix, center,
                                               radius, style, arcParams);
    }

    CircleOp(const Helper::MakeArgs& helperArgs, GrColor color, const SkMatrix& viewMatrix,
             SkPoint center, SkScalar radius, const GrStyle& style, const ArcParams* arcParams)
            : GrMeshDrawOp(ClassID()), fHelper(helperArgs, GrAAType::kCoverage) {
        const SkStrokeRec& stroke = style.strokeRec();
        SkStrokeRec::Style recStyle = stroke.getStyle();

        fRoundCaps = false;

        viewMatrix.mapPoints(&center, 1);
        radius = viewMatrix.mapRadius(radius);
        SkScalar strokeWidth = viewMatrix.mapRadius(stroke.getWidth());

        bool isStrokeOnly =
                SkStrokeRec::kStroke_Style == recStyle || SkStrokeRec::kHairline_Style == recStyle;
        bool hasStroke = isStrokeOnly || SkStrokeRec::kStrokeAndFill_Style == recStyle;

        SkScalar innerRadius = -SK_ScalarHalf;
        SkScalar outerRadius = radius;
        SkScalar halfWidth = 0;
        if (hasStroke) {
            if (SkScalarNearlyZero(strokeWidth)) {
                halfWidth = SK_ScalarHalf;
            } else {
                halfWidth = SkScalarHalf(strokeWidth);
            }

            outerRadius += halfWidth;
            if (isStrokeOnly) {
                innerRadius = radius - halfWidth;
            }
        }

        // The radii are outset for two reasons. First, it allows the shader to simply perform
        // simpler computation because the computed alpha is zero, rather than 50%, at the radius.
        // Second, the outer radius is used to compute the verts of the bounding box that is
        // rendered and the outset ensures the box will cover all partially covered by the circle.
        outerRadius += SK_ScalarHalf;
        innerRadius -= SK_ScalarHalf;
        bool stroked = isStrokeOnly && innerRadius > 0.0f;
        fViewMatrixIfUsingLocalCoords = viewMatrix;

        // This makes every point fully inside the intersection plane.
        static constexpr SkScalar kUnusedIsectPlane[] = {0.f, 0.f, 1.f};
        // This makes every point fully outside the union plane.
        static constexpr SkScalar kUnusedUnionPlane[] = {0.f, 0.f, 0.f};
        static constexpr SkPoint kUnusedRoundCaps[] = {{1e10f, 1e10f}, {1e10f, 1e10f}};
        SkRect devBounds = SkRect::MakeLTRB(center.fX - outerRadius, center.fY - outerRadius,
                                            center.fX + outerRadius, center.fY + outerRadius);
        if (arcParams) {
            // The shader operates in a space where the circle is translated to be centered at the
            // origin. Here we compute points on the unit circle at the starting and ending angles.
            SkPoint startPoint, stopPoint;
            startPoint.fY = SkScalarSinCos(arcParams->fStartAngleRadians, &startPoint.fX);
            SkScalar endAngle = arcParams->fStartAngleRadians + arcParams->fSweepAngleRadians;
            stopPoint.fY = SkScalarSinCos(endAngle, &stopPoint.fX);

            // Adjust the start and end points based on the view matrix (to handle rotated arcs)
            startPoint = viewMatrix.mapVector(startPoint.fX, startPoint.fY);
            stopPoint = viewMatrix.mapVector(stopPoint.fX, stopPoint.fY);
            startPoint.normalize();
            stopPoint.normalize();

            // If the matrix included scale (on one axis) we need to swap our start and end points
            if ((viewMatrix.getScaleX() < 0) != (viewMatrix.getScaleY() < 0)) {
                using std::swap;
                swap(startPoint, stopPoint);
            }

            fRoundCaps = style.strokeRec().getWidth() > 0 &&
                         style.strokeRec().getCap() == SkPaint::kRound_Cap;
            SkPoint roundCaps[2];
            if (fRoundCaps) {
                // Compute the cap center points in the normalized space.
                SkScalar midRadius = (innerRadius + outerRadius) / (2 * outerRadius);
                roundCaps[0] = startPoint * midRadius;
                roundCaps[1] = stopPoint * midRadius;
            } else {
                roundCaps[0] = kUnusedRoundCaps[0];
                roundCaps[1] = kUnusedRoundCaps[1];
            }

            // Like a fill without useCenter, butt-cap stroke can be implemented by clipping against
            // radial lines. We treat round caps the same way, but tack coverage of circles at the
            // center of the butts.
            // However, in both cases we have to be careful about the half-circle.
            // case. In that case the two radial lines are equal and so that edge gets clipped
            // twice. Since the shared edge goes through the center we fall back on the !useCenter
            // case.
            auto absSweep = SkScalarAbs(arcParams->fSweepAngleRadians);
            bool useCenter = (arcParams->fUseCenter || isStrokeOnly) &&
                             !SkScalarNearlyEqual(absSweep, SK_ScalarPI);
            if (useCenter) {
                SkVector norm0 = {startPoint.fY, -startPoint.fX};
                SkVector norm1 = {stopPoint.fY, -stopPoint.fX};
                // This ensures that norm0 is always the clockwise plane, and norm1 is CCW.
                if (arcParams->fSweepAngleRadians < 0) {
                    std::swap(norm0, norm1);
                }
                norm0.negate();
                fClipPlane = true;
                if (absSweep > SK_ScalarPI) {
                    fCircles.emplace_back(Circle{
                            color,
                            innerRadius,
                            outerRadius,
                            {norm0.fX, norm0.fY, 0.5f},
                            {kUnusedIsectPlane[0], kUnusedIsectPlane[1], kUnusedIsectPlane[2]},
                            {norm1.fX, norm1.fY, 0.5f},
                            {roundCaps[0], roundCaps[1]},
                            devBounds,
                            stroked});
                    fClipPlaneIsect = false;
                    fClipPlaneUnion = true;
                } else {
                    fCircles.emplace_back(Circle{
                            color,
                            innerRadius,
                            outerRadius,
                            {norm0.fX, norm0.fY, 0.5f},
                            {norm1.fX, norm1.fY, 0.5f},
                            {kUnusedUnionPlane[0], kUnusedUnionPlane[1], kUnusedUnionPlane[2]},
                            {roundCaps[0], roundCaps[1]},
                            devBounds,
                            stroked});
                    fClipPlaneIsect = true;
                    fClipPlaneUnion = false;
                }
            } else {
                // We clip to a secant of the original circle.
                startPoint.scale(radius);
                stopPoint.scale(radius);
                SkVector norm = {startPoint.fY - stopPoint.fY, stopPoint.fX - startPoint.fX};
                norm.normalize();
                if (arcParams->fSweepAngleRadians > 0) {
                    norm.negate();
                }
                SkScalar d = -norm.dot(startPoint) + 0.5f;

                fCircles.emplace_back(
                        Circle{color,
                               innerRadius,
                               outerRadius,
                               {norm.fX, norm.fY, d},
                               {kUnusedIsectPlane[0], kUnusedIsectPlane[1], kUnusedIsectPlane[2]},
                               {kUnusedUnionPlane[0], kUnusedUnionPlane[1], kUnusedUnionPlane[2]},
                               {roundCaps[0], roundCaps[1]},
                               devBounds,
                               stroked});
                fClipPlane = true;
                fClipPlaneIsect = false;
                fClipPlaneUnion = false;
            }
        } else {
            fCircles.emplace_back(
                    Circle{color,
                           innerRadius,
                           outerRadius,
                           {kUnusedIsectPlane[0], kUnusedIsectPlane[1], kUnusedIsectPlane[2]},
                           {kUnusedIsectPlane[0], kUnusedIsectPlane[1], kUnusedIsectPlane[2]},
                           {kUnusedUnionPlane[0], kUnusedUnionPlane[1], kUnusedUnionPlane[2]},
                           {kUnusedRoundCaps[0], kUnusedRoundCaps[1]},
                           devBounds,
                           stroked});
            fClipPlane = false;
            fClipPlaneIsect = false;
            fClipPlaneUnion = false;
        }
        // Use the original radius and stroke radius for the bounds so that it does not include the
        // AA bloat.
        radius += halfWidth;
        this->setBounds(
                {center.fX - radius, center.fY - radius, center.fX + radius, center.fY + radius},
                HasAABloat::kYes, IsZeroArea::kNo);
        fVertCount = circle_type_to_vert_count(stroked);
        fIndexCount = circle_type_to_index_count(stroked);
        fAllFill = !stroked;
    }

    const char* name() const override { return "CircleOp"; }

    void visitProxies(const VisitProxyFunc& func) const override {
        fHelper.visitProxies(func);
    }

    SkString dumpInfo() const override {
        SkString string;
        for (int i = 0; i < fCircles.count(); ++i) {
            string.appendf(
                    "Color: 0x%08x Rect [L: %.2f, T: %.2f, R: %.2f, B: %.2f],"
                    "InnerRad: %.2f, OuterRad: %.2f\n",
                    fCircles[i].fColor, fCircles[i].fDevBounds.fLeft, fCircles[i].fDevBounds.fTop,
                    fCircles[i].fDevBounds.fRight, fCircles[i].fDevBounds.fBottom,
                    fCircles[i].fInnerRadius, fCircles[i].fOuterRadius);
        }
        string += fHelper.dumpInfo();
        string += INHERITED::dumpInfo();
        return string;
    }

    RequiresDstTexture finalize(const GrCaps& caps, const GrAppliedClip* clip) override {
        GrColor* color = &fCircles.front().fColor;
        return fHelper.xpRequiresDstTexture(caps, clip, GrProcessorAnalysisCoverage::kSingleChannel,
                                            color);
    }

    FixedFunctionFlags fixedFunctionFlags() const override { return fHelper.fixedFunctionFlags(); }

private:
    void onPrepareDraws(Target* target) override {
        SkMatrix localMatrix;
        if (!fViewMatrixIfUsingLocalCoords.invert(&localMatrix)) {
            return;
        }

        // Setup geometry processor
        sk_sp<GrGeometryProcessor> gp(new CircleGeometryProcessor(
                !fAllFill, fClipPlane, fClipPlaneIsect, fClipPlaneUnion, fRoundCaps, localMatrix));

        struct CircleVertex {
            SkPoint fPos;
            GrColor fColor;
            SkPoint fOffset;
            SkScalar fOuterRadius;
            SkScalar fInnerRadius;
            // These planes may or may not be present in the vertex buffer.
            SkScalar fHalfPlanes[3][3];
        };

        int numPlanes = (int)fClipPlane + fClipPlaneIsect + fClipPlaneUnion;
        auto vertexCapCenters = [numPlanes](CircleVertex* v) {
            return (void*)(v->fHalfPlanes + numPlanes);
        };
        size_t vertexStride = sizeof(CircleVertex) - (fClipPlane ? 0 : 3 * sizeof(SkScalar)) -
                              (fClipPlaneIsect ? 0 : 3 * sizeof(SkScalar)) -
                              (fClipPlaneUnion ? 0 : 3 * sizeof(SkScalar)) +
                              (fRoundCaps ? 2 * sizeof(SkPoint) : 0);
        SkASSERT(vertexStride == gp->debugOnly_vertexStride());

        const GrBuffer* vertexBuffer;
        int firstVertex;
        char* vertices = (char*)target->makeVertexSpace(vertexStride, fVertCount, &vertexBuffer,
                                                        &firstVertex);
        if (!vertices) {
            SkDebugf("Could not allocate vertices\n");
            return;
        }

        const GrBuffer* indexBuffer = nullptr;
        int firstIndex = 0;
        uint16_t* indices = target->makeIndexSpace(fIndexCount, &indexBuffer, &firstIndex);
        if (!indices) {
            SkDebugf("Could not allocate indices\n");
            return;
        }

        int currStartVertex = 0;
        for (const auto& circle : fCircles) {
            SkScalar innerRadius = circle.fInnerRadius;
            SkScalar outerRadius = circle.fOuterRadius;
            GrColor color = circle.fColor;
            const SkRect& bounds = circle.fDevBounds;

            CircleVertex* v0 = reinterpret_cast<CircleVertex*>(vertices + 0 * vertexStride);
            CircleVertex* v1 = reinterpret_cast<CircleVertex*>(vertices + 1 * vertexStride);
            CircleVertex* v2 = reinterpret_cast<CircleVertex*>(vertices + 2 * vertexStride);
            CircleVertex* v3 = reinterpret_cast<CircleVertex*>(vertices + 3 * vertexStride);
            CircleVertex* v4 = reinterpret_cast<CircleVertex*>(vertices + 4 * vertexStride);
            CircleVertex* v5 = reinterpret_cast<CircleVertex*>(vertices + 5 * vertexStride);
            CircleVertex* v6 = reinterpret_cast<CircleVertex*>(vertices + 6 * vertexStride);
            CircleVertex* v7 = reinterpret_cast<CircleVertex*>(vertices + 7 * vertexStride);

            // The inner radius in the vertex data must be specified in normalized space.
            innerRadius = innerRadius / outerRadius;

            SkPoint center = SkPoint::Make(bounds.centerX(), bounds.centerY());
            SkScalar halfWidth = 0.5f * bounds.width();
            SkScalar octOffset = 0.41421356237f;  // sqrt(2) - 1

            SkVector geoClipPlane = { 0, 0 };
            SkScalar offsetClipDist = SK_Scalar1;
            if (!circle.fStroked && fClipPlane && fClipPlaneIsect &&
                    (circle.fClipPlane[0] * circle.fIsectPlane[0] +
                     circle.fClipPlane[1] * circle.fIsectPlane[1]) < 0.0f) {
                // Acute arc. Clip the vertices to the perpendicular half-plane. We've constructed
                // fClipPlane to be clockwise, and fISectPlane to be CCW, so we can can rotate them
                // each 90 degrees to point "out", then average them. We back off by 1/2 pixel so
                // the AA can extend just past the center of the circle.
                geoClipPlane.set(circle.fClipPlane[1] - circle.fIsectPlane[1],
                                 circle.fIsectPlane[0] - circle.fClipPlane[0]);
                SkAssertResult(geoClipPlane.normalize());
                offsetClipDist = 0.5f / halfWidth;
            }

            auto clipOffset = [geoClipPlane, offsetClipDist](const SkPoint& p) {
                // This clips the normalized offset to the half-plane we computed above. Then we
                // compute the vertex position from this.
                SkScalar dist = SkTMin(p.dot(geoClipPlane) + offsetClipDist, 0.0f);
                return p - geoClipPlane * dist;
            };

            v0->fOffset = clipOffset(SkPoint::Make(-octOffset, -1));
            v0->fPos = center + v0->fOffset * halfWidth;
            v0->fColor = color;
            v0->fOuterRadius = outerRadius;
            v0->fInnerRadius = innerRadius;

            v1->fOffset = clipOffset(SkPoint::Make(octOffset, -1));
            v1->fPos = center + v1->fOffset * halfWidth;
            v1->fColor = color;
            v1->fOuterRadius = outerRadius;
            v1->fInnerRadius = innerRadius;

            v2->fOffset = clipOffset(SkPoint::Make(1, -octOffset));
            v2->fPos = center + v2->fOffset * halfWidth;
            v2->fColor = color;
            v2->fOuterRadius = outerRadius;
            v2->fInnerRadius = innerRadius;

            v3->fOffset = clipOffset(SkPoint::Make(1, octOffset));
            v3->fPos = center + v3->fOffset * halfWidth;
            v3->fColor = color;
            v3->fOuterRadius = outerRadius;
            v3->fInnerRadius = innerRadius;

            v4->fOffset = clipOffset(SkPoint::Make(octOffset, 1));
            v4->fPos = center + v4->fOffset * halfWidth;
            v4->fColor = color;
            v4->fOuterRadius = outerRadius;
            v4->fInnerRadius = innerRadius;

            v5->fOffset = clipOffset(SkPoint::Make(-octOffset, 1));
            v5->fPos = center + v5->fOffset * halfWidth;
            v5->fColor = color;
            v5->fOuterRadius = outerRadius;
            v5->fInnerRadius = innerRadius;

            v6->fOffset = clipOffset(SkPoint::Make(-1, octOffset));
            v6->fPos = center + v6->fOffset * halfWidth;
            v6->fColor = color;
            v6->fOuterRadius = outerRadius;
            v6->fInnerRadius = innerRadius;

            v7->fOffset = clipOffset(SkPoint::Make(-1, -octOffset));
            v7->fPos = center + v7->fOffset * halfWidth;
            v7->fColor = color;
            v7->fOuterRadius = outerRadius;
            v7->fInnerRadius = innerRadius;

            if (fClipPlane) {
                memcpy(v0->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                memcpy(v1->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                memcpy(v2->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                memcpy(v3->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                memcpy(v4->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                memcpy(v5->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                memcpy(v6->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                memcpy(v7->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
            }
            int unionIdx = 1;
            if (fClipPlaneIsect) {
                memcpy(v0->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                memcpy(v1->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                memcpy(v2->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                memcpy(v3->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                memcpy(v4->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                memcpy(v5->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                memcpy(v6->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                memcpy(v7->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                unionIdx = 2;
            }
            if (fClipPlaneUnion) {
                memcpy(v0->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                memcpy(v1->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                memcpy(v2->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                memcpy(v3->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                memcpy(v4->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                memcpy(v5->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                memcpy(v6->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                memcpy(v7->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
            }
            if (fRoundCaps) {
                memcpy(vertexCapCenters(v0), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                memcpy(vertexCapCenters(v1), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                memcpy(vertexCapCenters(v2), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                memcpy(vertexCapCenters(v3), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                memcpy(vertexCapCenters(v4), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                memcpy(vertexCapCenters(v5), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                memcpy(vertexCapCenters(v6), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                memcpy(vertexCapCenters(v7), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
            }

            if (circle.fStroked) {
                // compute the inner ring
                CircleVertex* v0 = reinterpret_cast<CircleVertex*>(vertices + 8 * vertexStride);
                CircleVertex* v1 = reinterpret_cast<CircleVertex*>(vertices + 9 * vertexStride);
                CircleVertex* v2 = reinterpret_cast<CircleVertex*>(vertices + 10 * vertexStride);
                CircleVertex* v3 = reinterpret_cast<CircleVertex*>(vertices + 11 * vertexStride);
                CircleVertex* v4 = reinterpret_cast<CircleVertex*>(vertices + 12 * vertexStride);
                CircleVertex* v5 = reinterpret_cast<CircleVertex*>(vertices + 13 * vertexStride);
                CircleVertex* v6 = reinterpret_cast<CircleVertex*>(vertices + 14 * vertexStride);
                CircleVertex* v7 = reinterpret_cast<CircleVertex*>(vertices + 15 * vertexStride);

                // cosine and sine of pi/8
                SkScalar c = 0.923579533f;
                SkScalar s = 0.382683432f;
                SkScalar r = circle.fInnerRadius;

                v0->fPos = center + SkPoint::Make(-s * r, -c * r);
                v0->fColor = color;
                v0->fOffset = SkPoint::Make(-s * innerRadius, -c * innerRadius);
                v0->fOuterRadius = outerRadius;
                v0->fInnerRadius = innerRadius;

                v1->fPos = center + SkPoint::Make(s * r, -c * r);
                v1->fColor = color;
                v1->fOffset = SkPoint::Make(s * innerRadius, -c * innerRadius);
                v1->fOuterRadius = outerRadius;
                v1->fInnerRadius = innerRadius;

                v2->fPos = center + SkPoint::Make(c * r, -s * r);
                v2->fColor = color;
                v2->fOffset = SkPoint::Make(c * innerRadius, -s * innerRadius);
                v2->fOuterRadius = outerRadius;
                v2->fInnerRadius = innerRadius;

                v3->fPos = center + SkPoint::Make(c * r, s * r);
                v3->fColor = color;
                v3->fOffset = SkPoint::Make(c * innerRadius, s * innerRadius);
                v3->fOuterRadius = outerRadius;
                v3->fInnerRadius = innerRadius;

                v4->fPos = center + SkPoint::Make(s * r, c * r);
                v4->fColor = color;
                v4->fOffset = SkPoint::Make(s * innerRadius, c * innerRadius);
                v4->fOuterRadius = outerRadius;
                v4->fInnerRadius = innerRadius;

                v5->fPos = center + SkPoint::Make(-s * r, c * r);
                v5->fColor = color;
                v5->fOffset = SkPoint::Make(-s * innerRadius, c * innerRadius);
                v5->fOuterRadius = outerRadius;
                v5->fInnerRadius = innerRadius;

                v6->fPos = center + SkPoint::Make(-c * r, s * r);
                v6->fColor = color;
                v6->fOffset = SkPoint::Make(-c * innerRadius, s * innerRadius);
                v6->fOuterRadius = outerRadius;
                v6->fInnerRadius = innerRadius;

                v7->fPos = center + SkPoint::Make(-c * r, -s * r);
                v7->fColor = color;
                v7->fOffset = SkPoint::Make(-c * innerRadius, -s * innerRadius);
                v7->fOuterRadius = outerRadius;
                v7->fInnerRadius = innerRadius;

                if (fClipPlane) {
                    memcpy(v0->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                    memcpy(v1->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                    memcpy(v2->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                    memcpy(v3->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                    memcpy(v4->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                    memcpy(v5->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                    memcpy(v6->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                    memcpy(v7->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                }
                int unionIdx = 1;
                if (fClipPlaneIsect) {
                    memcpy(v0->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    memcpy(v1->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    memcpy(v2->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    memcpy(v3->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    memcpy(v4->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    memcpy(v5->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    memcpy(v6->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    memcpy(v7->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    unionIdx = 2;
                }
                if (fClipPlaneUnion) {
                    memcpy(v0->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                    memcpy(v1->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                    memcpy(v2->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                    memcpy(v3->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                    memcpy(v4->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                    memcpy(v5->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                    memcpy(v6->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                    memcpy(v7->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                }
                if (fRoundCaps) {
                    memcpy(vertexCapCenters(v0), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                    memcpy(vertexCapCenters(v1), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                    memcpy(vertexCapCenters(v2), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                    memcpy(vertexCapCenters(v3), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                    memcpy(vertexCapCenters(v4), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                    memcpy(vertexCapCenters(v5), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                    memcpy(vertexCapCenters(v6), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                    memcpy(vertexCapCenters(v7), circle.fRoundCapCenters, 2 * sizeof(SkPoint));
                }
            } else {
                // filled
                CircleVertex* v8 = reinterpret_cast<CircleVertex*>(vertices + 8 * vertexStride);
                v8->fPos = center;
                v8->fColor = color;
                v8->fOffset = SkPoint::Make(0, 0);
                v8->fOuterRadius = outerRadius;
                v8->fInnerRadius = innerRadius;
                if (fClipPlane) {
                    memcpy(v8->fHalfPlanes[0], circle.fClipPlane, 3 * sizeof(SkScalar));
                }
                int unionIdx = 1;
                if (fClipPlaneIsect) {
                    memcpy(v8->fHalfPlanes[1], circle.fIsectPlane, 3 * sizeof(SkScalar));
                    unionIdx = 2;
                }
                if (fClipPlaneUnion) {
                    memcpy(v8->fHalfPlanes[unionIdx], circle.fUnionPlane, 3 * sizeof(SkScalar));
                }
                SkASSERT(!fRoundCaps);
            }

            const uint16_t* primIndices = circle_type_to_indices(circle.fStroked);
            const int primIndexCount = circle_type_to_index_count(circle.fStroked);
            for (int i = 0; i < primIndexCount; ++i) {
                *indices++ = primIndices[i] + currStartVertex;
            }

            currStartVertex += circle_type_to_vert_count(circle.fStroked);
            vertices += circle_type_to_vert_count(circle.fStroked) * vertexStride;
        }

        GrMesh* mesh = target->allocMesh(GrPrimitiveType::kTriangles);
        mesh->setIndexed(indexBuffer, fIndexCount, firstIndex, 0, fVertCount - 1,
                         GrPrimitiveRestart::kNo);
        mesh->setVertexData(vertexBuffer, firstVertex);
        auto pipe = fHelper.makePipeline(target);
        target->draw(std::move(gp), pipe.fPipeline, pipe.fFixedDynamicState, mesh);
    }

    CombineResult onCombineIfPossible(GrOp* t, const GrCaps& caps) override {
        CircleOp* that = t->cast<CircleOp>();

        // can only represent 65535 unique vertices with 16-bit indices
        if (fVertCount + that->fVertCount > 65536) {
            return CombineResult::kCannotCombine;
        }

        if (!fHelper.isCompatible(that->fHelper, caps, this->bounds(), that->bounds())) {
            return CombineResult::kCannotCombine;
        }

        if (fHelper.usesLocalCoords() &&
            !fViewMatrixIfUsingLocalCoords.cheapEqualTo(that->fViewMatrixIfUsingLocalCoords)) {
            return CombineResult::kCannotCombine;
        }

        // Because we've set up the ops that don't use the planes with noop values
        // we can just accumulate used planes by later ops.
        fClipPlane |= that->fClipPlane;
        fClipPlaneIsect |= that->fClipPlaneIsect;
        fClipPlaneUnion |= that->fClipPlaneUnion;
        fRoundCaps |= that->fRoundCaps;

        fCircles.push_back_n(that->fCircles.count(), that->fCircles.begin());
        this->joinBounds(*that);
        fVertCount += that->fVertCount;
        fIndexCount += that->fIndexCount;
        fAllFill = fAllFill && that->fAllFill;
        return CombineResult::kMerged;
    }

    struct Circle {
        GrColor fColor;
        SkScalar fInnerRadius;
        SkScalar fOuterRadius;
        SkScalar fClipPlane[3];
        SkScalar fIsectPlane[3];
        SkScalar fUnionPlane[3];
        SkPoint fRoundCapCenters[2];
        SkRect fDevBounds;
        bool fStroked;
    };

    SkMatrix fViewMatrixIfUsingLocalCoords;
    Helper fHelper;
    SkSTArray<1, Circle, true> fCircles;
    int fVertCount;
    int fIndexCount;
    bool fAllFill;
    bool fClipPlane;
    bool fClipPlaneIsect;
    bool fClipPlaneUnion;
    bool fRoundCaps;

    typedef GrMeshDrawOp INHERITED;
};

class ButtCapDashedCircleOp final : public GrMeshDrawOp {
private:
    using Helper = GrSimpleMeshDrawOpHelper;

public:
    DEFINE_OP_CLASS_ID

    static std::unique_ptr<GrDrawOp> Make(GrContext* context,
                                          GrPaint&& paint,
                                          const SkMatrix& viewMatrix,
                                          SkPoint center,
                                          SkScalar radius,
                                          SkScalar strokeWidth,
                                          SkScalar startAngle,
                                          SkScalar onAngle,
                                          SkScalar offAngle,
                                          SkScalar phaseAngle) {
        SkASSERT(circle_stays_circle(viewMatrix));
        SkASSERT(strokeWidth < 2 * radius);
        return Helper::FactoryHelper<ButtCapDashedCircleOp>(context, std::move(paint), viewMatrix,
                                                            center, radius, strokeWidth, startAngle,
                                                            onAngle, offAngle, phaseAngle);
    }

    ButtCapDashedCircleOp(const Helper::MakeArgs& helperArgs, GrColor color,
                          const SkMatrix& viewMatrix, SkPoint center, SkScalar radius,
                          SkScalar strokeWidth, SkScalar startAngle, SkScalar onAngle,
                          SkScalar offAngle, SkScalar phaseAngle)
            : GrMeshDrawOp(ClassID()), fHelper(helperArgs, GrAAType::kCoverage) {
        SkASSERT(circle_stays_circle(viewMatrix));
        viewMatrix.mapPoints(&center, 1);
        radius = viewMatrix.mapRadius(radius);
        strokeWidth = viewMatrix.mapRadius(strokeWidth);

        // Determine the angle where the circle starts in device space and whether its orientation
        // has been reversed.
        SkVector start;
        bool reflection;
        if (!startAngle) {
            start = {1, 0};
        } else {
            start.fY = SkScalarSinCos(startAngle, &start.fX);
        }
        viewMatrix.mapVectors(&start, 1);
        startAngle = SkScalarATan2(start.fY, start.fX);
        reflection = (viewMatrix.getScaleX() * viewMatrix.getScaleY() -
                      viewMatrix.getSkewX() * viewMatrix.getSkewY()) < 0;

        auto totalAngle = onAngle + offAngle;
        phaseAngle = SkScalarMod(phaseAngle + totalAngle / 2, totalAngle) - totalAngle / 2;

        SkScalar halfWidth = 0;
        if (SkScalarNearlyZero(strokeWidth)) {
            halfWidth = SK_ScalarHalf;
        } else {
            halfWidth = SkScalarHalf(strokeWidth);
        }

        SkScalar outerRadius = radius + halfWidth;
        SkScalar innerRadius = radius - halfWidth;

        // The radii are outset for two reasons. First, it allows the shader to simply perform
        // simpler computation because the computed alpha is zero, rather than 50%, at the radius.
        // Second, the outer radius is used to compute the verts of the bounding box that is
        // rendered and the outset ensures the box will cover all partially covered by the circle.
        outerRadius += SK_ScalarHalf;
        innerRadius -= SK_ScalarHalf;
        fViewMatrixIfUsingLocalCoords = viewMatrix;

        SkRect devBounds = SkRect::MakeLTRB(center.fX - outerRadius, center.fY - outerRadius,
                                            center.fX + outerRadius, center.fY + outerRadius);

        // We store whether there is a reflection as a negative total angle.
        if (reflection) {
            totalAngle = -totalAngle;
        }
        fCircles.push_back(Circle{
            color,
            outerRadius,
            innerRadius,
            onAngle,
            totalAngle,
            startAngle,
            phaseAngle,
            devBounds
        });
        // Use the original radius and stroke radius for the bounds so that it does not include the
        // AA bloat.
        radius += halfWidth;
        this->setBounds(
                {center.fX - radius, center.fY - radius, center.fX + radius, center.fY + radius},
                HasAABloat::kYes, IsZeroArea::kNo);
        fVertCount = circle_type_to_vert_count(true);
        fIndexCount = circle_type_to_index_count(true);
    }

    const char* name() const override { return "ButtCappedDashedCircleOp"; }

    void visitProxies(const VisitProxyFunc& func) const override { fHelper.visitProxies(func); }

    SkString dumpInfo() const override {
        SkString string;
        for (int i = 0; i < fCircles.count(); ++i) {
            string.appendf(
                    "Color: 0x%08x Rect [L: %.2f, T: %.2f, R: %.2f, B: %.2f],"
                    "InnerRad: %.2f, OuterRad: %.2f, OnAngle: %.2f, TotalAngle: %.2f, "
                    "Phase: %.2f\n",
                    fCircles[i].fColor, fCircles[i].fDevBounds.fLeft, fCircles[i].fDevBounds.fTop,
                    fCircles[i].fDevBounds.fRight, fCircles[i].fDevBounds.fBottom,
                    fCircles[i].fInnerRadius, fCircles[i].fOuterRadius, fCircles[i].fOnAngle,
                    fCircles[i].fTotalAngle, fCircles[i].fPhaseAngle);
        }
        string += fHelper.dumpInfo();
        string += INHERITED::dumpInfo();
        return string;
    }

    RequiresDstTexture finalize(const GrCaps& caps, const GrAppliedClip* clip) override {
        GrColor* color = &fCircles.front().fColor;
        return fHelper.xpRequiresDstTexture(caps, clip, GrProcessorAnalysisCoverage::kSingleChannel,
                                            color);
    }

    FixedFunctionFlags fixedFunctionFlags() const override { return fHelper.fixedFunctionFlags(); }

private:
    void onPrepareDraws(Target* target) override {
        SkMatrix localMatrix;
        if (!fViewMatrixIfUsingLocalCoords.invert(&localMatrix)) {
            return;
        }

        // Setup geometry processor
        sk_sp<GrGeometryProcessor> gp(new ButtCapDashedCircleGeometryProcessor(localMatrix));

        struct CircleVertex {
            SkPoint fPos;
            GrColor fColor;
            SkPoint fOffset;
            SkScalar fOuterRadius;
            SkScalar fInnerRadius;
            SkScalar fOnAngle;
            SkScalar fTotalAngle;
            SkScalar fStartAngle;
            SkScalar fPhaseAngle;
        };

        static constexpr size_t kVertexStride = sizeof(CircleVertex);
        SkASSERT(kVertexStride == gp->debugOnly_vertexStride());

        const GrBuffer* vertexBuffer;
        int firstVertex;
        char* vertices = (char*)target->makeVertexSpace(kVertexStride, fVertCount, &vertexBuffer,
                                                        &firstVertex);
        if (!vertices) {
            SkDebugf("Could not allocate vertices\n");
            return;
        }

        const GrBuffer* indexBuffer = nullptr;
        int firstIndex = 0;
        uint16_t* indices = target->makeIndexSpace(fIndexCount, &indexBuffer, &firstIndex);
        if (!indices) {
            SkDebugf("Could not allocate indices\n");
            return;
        }

        int currStartVertex = 0;
        for (const auto& circle : fCircles) {
            // The inner radius in the vertex data must be specified in normalized space so that
            // length() can be called with smaller values to avoid precision issues with half
            // floats.
            auto normInnerRadius = circle.fInnerRadius / circle.fOuterRadius;
            const SkRect& bounds = circle.fDevBounds;
            bool reflect = false;
            SkScalar totalAngle = circle.fTotalAngle;
            if (totalAngle < 0) {
                reflect = true;
                totalAngle = -totalAngle;
            }

            // The bounding geometry for the circle is composed of an outer bounding octagon and
            // an inner bounded octagon.

            // Initializes the attributes that are the same at each vertex. Also applies reflection.
            auto init_const_attrs_and_reflect = [&](CircleVertex* v) {
                v->fColor = circle.fColor;
                v->fOuterRadius = circle.fOuterRadius;
                v->fInnerRadius = normInnerRadius;
                v->fOnAngle = circle.fOnAngle;
                v->fTotalAngle = totalAngle;
                v->fStartAngle = circle.fStartAngle;
                v->fPhaseAngle = circle.fPhaseAngle;
                if (reflect) {
                    v->fStartAngle = -v->fStartAngle;
                    v->fOffset.fY = -v->fOffset.fY;
                }
            };

            // Compute the vertices of the outer octagon.
            SkPoint center = SkPoint::Make(bounds.centerX(), bounds.centerY());
            SkScalar halfWidth = 0.5f * bounds.width();
            auto init_outer_vertex = [&](int idx, SkScalar x, SkScalar y) {
                CircleVertex* v = reinterpret_cast<CircleVertex*>(vertices + idx * kVertexStride);
                v->fPos = center + SkPoint{x * halfWidth, y * halfWidth};
                v->fOffset = {x, y};
                init_const_attrs_and_reflect(v);
            };
            static constexpr SkScalar kOctOffset = 0.41421356237f;  // sqrt(2) - 1
            init_outer_vertex(0, -kOctOffset, -1);
            init_outer_vertex(1, kOctOffset, -1);
            init_outer_vertex(2, 1, -kOctOffset);
            init_outer_vertex(3, 1, kOctOffset);
            init_outer_vertex(4, kOctOffset, 1);
            init_outer_vertex(5, -kOctOffset, 1);
            init_outer_vertex(6, -1, kOctOffset);
            init_outer_vertex(7, -1, -kOctOffset);

            // Compute the vertices of the inner octagon.
            auto init_inner_vertex = [&](int idx, SkScalar x, SkScalar y) {
                CircleVertex* v =
                        reinterpret_cast<CircleVertex*>(vertices + (idx + 8) * kVertexStride);
                v->fPos = center + SkPoint{x * circle.fInnerRadius, y * circle.fInnerRadius};
                v->fOffset = {x * normInnerRadius, y * normInnerRadius};
                init_const_attrs_and_reflect(v);
            };

            // cosine and sine of pi/8
            static constexpr SkScalar kCos = 0.923579533f;
            static constexpr SkScalar kSin = 0.382683432f;

            init_inner_vertex(0, -kSin, -kCos);
            init_inner_vertex(1,  kSin, -kCos);
            init_inner_vertex(2,  kCos, -kSin);
            init_inner_vertex(3,  kCos,  kSin);
            init_inner_vertex(4,  kSin,  kCos);
            init_inner_vertex(5, -kSin,  kCos);
            init_inner_vertex(6, -kCos,  kSin);
            init_inner_vertex(7, -kCos, -kSin);

            const uint16_t* primIndices = circle_type_to_indices(true);
            const int primIndexCount = circle_type_to_index_count(true);
            for (int i = 0; i < primIndexCount; ++i) {
                *indices++ = primIndices[i] + currStartVertex;
            }

            currStartVertex += circle_type_to_vert_count(true);
            vertices += circle_type_to_vert_count(true) * kVertexStride;
        }

        GrMesh* mesh = target->allocMesh(GrPrimitiveType::kTriangles);
        mesh->setIndexed(indexBuffer, fIndexCount, firstIndex, 0, fVertCount - 1,
                         GrPrimitiveRestart::kNo);
        mesh->setVertexData(vertexBuffer, firstVertex);
        auto pipe = fHelper.makePipeline(target);
        target->draw(std::move(gp), pipe.fPipeline, pipe.fFixedDynamicState, mesh);
    }

    CombineResult onCombineIfPossible(GrOp* t, const GrCaps& caps) override {
        ButtCapDashedCircleOp* that = t->cast<ButtCapDashedCircleOp>();

        // can only represent 65535 unique vertices with 16-bit indices
        if (fVertCount + that->fVertCount > 65536) {
            return CombineResult::kCannotCombine;
        }

        if (!fHelper.isCompatible(that->fHelper, caps, this->bounds(), that->bounds())) {
            return CombineResult::kCannotCombine;
        }

        if (fHelper.usesLocalCoords() &&
            !fViewMatrixIfUsingLocalCoords.cheapEqualTo(that->fViewMatrixIfUsingLocalCoords)) {
            return CombineResult::kCannotCombine;
        }

        fCircles.push_back_n(that->fCircles.count(), that->fCircles.begin());
        this->joinBounds(*that);
        fVertCount += that->fVertCount;
        fIndexCount += that->fIndexCount;
        return CombineResult::kMerged;
    }

    struct Circle {
        GrColor fColor;
        SkScalar fOuterRadius;
        SkScalar fInnerRadius;
        SkScalar fOnAngle;
        SkScalar fTotalAngle;
        SkScalar fStartAngle;
        SkScalar fPhaseAngle;
        SkRect fDevBounds;
    };

    SkMatrix fViewMatrixIfUsingLocalCoords;
    Helper fHelper;
    SkSTArray<1, Circle, true> fCircles;
    int fVertCount;
    int fIndexCount;

    typedef GrMeshDrawOp INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class EllipseOp : public GrMeshDrawOp {
private:
    using Helper = GrSimpleMeshDrawOpHelper;

    struct DeviceSpaceParams {
        SkPoint fCenter;
        SkScalar fXRadius;
        SkScalar fYRadius;
        SkScalar fInnerXRadius;
        SkScalar fInnerYRadius;
    };

public:
    DEFINE_OP_CLASS_ID

    static std::unique_ptr<GrDrawOp> Make(GrContext* context,
                                          GrPaint&& paint,
                                          const SkMatrix& viewMatrix,
                                          const SkRect& ellipse,
                                          const SkStrokeRec& stroke) {
        DeviceSpaceParams params;
        // do any matrix crunching before we reset the draw state for device coords
        params.fCenter = SkPoint::Make(ellipse.centerX(), ellipse.centerY());
        viewMatrix.mapPoints(&params.fCenter, 1);
        SkScalar ellipseXRadius = SkScalarHalf(ellipse.width());
        SkScalar ellipseYRadius = SkScalarHalf(ellipse.height());
        params.fXRadius = SkScalarAbs(viewMatrix[SkMatrix::kMScaleX] * ellipseXRadius +
                                      viewMatrix[SkMatrix::kMSkewX] * ellipseYRadius);
        params.fYRadius = SkScalarAbs(viewMatrix[SkMatrix::kMSkewY] * ellipseXRadius +
                                      viewMatrix[SkMatrix::kMScaleY] * ellipseYRadius);

        // do (potentially) anisotropic mapping of stroke
        SkVector scaledStroke;
        SkScalar strokeWidth = stroke.getWidth();
        scaledStroke.fX = SkScalarAbs(
                strokeWidth * (viewMatrix[SkMatrix::kMScaleX] + viewMatrix[SkMatrix::kMSkewY]));
        scaledStroke.fY = SkScalarAbs(
                strokeWidth * (viewMatrix[SkMatrix::kMSkewX] + viewMatrix[SkMatrix::kMScaleY]));

        SkStrokeRec::Style style = stroke.getStyle();
        bool isStrokeOnly =
                SkStrokeRec::kStroke_Style == style || SkStrokeRec::kHairline_Style == style;
        bool hasStroke = isStrokeOnly || SkStrokeRec::kStrokeAndFill_Style == style;

        params.fInnerXRadius = 0;
        params.fInnerYRadius = 0;
        if (hasStroke) {
            if (SkScalarNearlyZero(scaledStroke.length())) {
                scaledStroke.set(SK_ScalarHalf, SK_ScalarHalf);
            } else {
                scaledStroke.scale(SK_ScalarHalf);
            }

            // we only handle thick strokes for near-circular ellipses
            if (scaledStroke.length() > SK_ScalarHalf &&
                (0.5f * params.fXRadius > params.fYRadius ||
                 0.5f * params.fYRadius > params.fXRadius)) {
                return nullptr;
            }

            // we don't handle it if curvature of the stroke is less than curvature of the ellipse
            if (scaledStroke.fX * (params.fXRadius * params.fYRadius) <
                        (scaledStroke.fY * scaledStroke.fY) * params.fXRadius ||
                scaledStroke.fY * (params.fXRadius * params.fXRadius) <
                        (scaledStroke.fX * scaledStroke.fX) * params.fYRadius) {
                return nullptr;
            }

            // this is legit only if scale & translation (which should be the case at the moment)
            if (isStrokeOnly) {
                params.fInnerXRadius = params.fXRadius - scaledStroke.fX;
                params.fInnerYRadius = params.fYRadius - scaledStroke.fY;
            }

            params.fXRadius += scaledStroke.fX;
            params.fYRadius += scaledStroke.fY;
        }
        return Helper::FactoryHelper<EllipseOp>(context, std::move(paint), viewMatrix,
                                                params, stroke);
    }

    EllipseOp(const Helper::MakeArgs& helperArgs, GrColor color, const SkMatrix& viewMatrix,
              const DeviceSpaceParams& params, const SkStrokeRec& stroke)
            : INHERITED(ClassID()), fHelper(helperArgs, GrAAType::kCoverage) {
        SkStrokeRec::Style style = stroke.getStyle();
        bool isStrokeOnly =
                SkStrokeRec::kStroke_Style == style || SkStrokeRec::kHairline_Style == style;

        fEllipses.emplace_back(Ellipse{color, params.fXRadius, params.fYRadius,
                                       params.fInnerXRadius, params.fInnerYRadius,
                                       SkRect::MakeLTRB(params.fCenter.fX - params.fXRadius,
                                                        params.fCenter.fY - params.fYRadius,
                                                        params.fCenter.fX + params.fXRadius,
                                                        params.fCenter.fY + params.fYRadius)});

        this->setBounds(fEllipses.back().fDevBounds, HasAABloat::kYes, IsZeroArea::kNo);

        // Outset bounds to include half-pixel width antialiasing.
        fEllipses[0].fDevBounds.outset(SK_ScalarHalf, SK_ScalarHalf);

        fStroked = isStrokeOnly && params.fInnerXRadius > 0 && params.fInnerYRadius > 0;
        fViewMatrixIfUsingLocalCoords = viewMatrix;
    }

    const char* name() const override { return "EllipseOp"; }

    void visitProxies(const VisitProxyFunc& func) const override {
        fHelper.visitProxies(func);
    }

    SkString dumpInfo() const override {
        SkString string;
        string.appendf("Stroked: %d\n", fStroked);
        for (const auto& geo : fEllipses) {
            string.appendf(
                    "Color: 0x%08x Rect [L: %.2f, T: %.2f, R: %.2f, B: %.2f], "
                    "XRad: %.2f, YRad: %.2f, InnerXRad: %.2f, InnerYRad: %.2f\n",
                    geo.fColor, geo.fDevBounds.fLeft, geo.fDevBounds.fTop, geo.fDevBounds.fRight,
                    geo.fDevBounds.fBottom, geo.fXRadius, geo.fYRadius, geo.fInnerXRadius,
                    geo.fInnerYRadius);
        }
        string += fHelper.dumpInfo();
        string += INHERITED::dumpInfo();
        return string;
    }

    RequiresDstTexture finalize(const GrCaps& caps, const GrAppliedClip* clip) override {
        GrColor* color = &fEllipses.front().fColor;
        return fHelper.xpRequiresDstTexture(caps, clip, GrProcessorAnalysisCoverage::kSingleChannel,
                                            color);
    }

    FixedFunctionFlags fixedFunctionFlags() const override { return fHelper.fixedFunctionFlags(); }

private:
    void onPrepareDraws(Target* target) override {
        SkMatrix localMatrix;
        if (!fViewMatrixIfUsingLocalCoords.invert(&localMatrix)) {
            return;
        }

        // Setup geometry processor
        sk_sp<GrGeometryProcessor> gp(new EllipseGeometryProcessor(fStroked, localMatrix));

        SkASSERT(sizeof(EllipseVertex) == gp->debugOnly_vertexStride());
        QuadHelper helper(target, sizeof(EllipseVertex), fEllipses.count());
        EllipseVertex* verts = reinterpret_cast<EllipseVertex*>(helper.vertices());
        if (!verts) {
            return;
        }

        for (const auto& ellipse : fEllipses) {
            GrColor color = ellipse.fColor;
            SkScalar xRadius = ellipse.fXRadius;
            SkScalar yRadius = ellipse.fYRadius;

            // Compute the reciprocals of the radii here to save time in the shader
            SkScalar xRadRecip = SkScalarInvert(xRadius);
            SkScalar yRadRecip = SkScalarInvert(yRadius);
            SkScalar xInnerRadRecip = SkScalarInvert(ellipse.fInnerXRadius);
            SkScalar yInnerRadRecip = SkScalarInvert(ellipse.fInnerYRadius);
            SkScalar xMaxOffset = xRadius + SK_ScalarHalf;
            SkScalar yMaxOffset = yRadius + SK_ScalarHalf;

            if (!fStroked) {
                // For filled ellipses we map a unit circle in the vertex attributes rather than
                // computing an ellipse and modifying that distance, so we normalize to 1
                xMaxOffset /= xRadius;
                yMaxOffset /= yRadius;
            }

            // The inner radius in the vertex data must be specified in normalized space.
            verts[0].fPos = SkPoint::Make(ellipse.fDevBounds.fLeft, ellipse.fDevBounds.fTop);
            verts[0].fColor = color;
            verts[0].fOffset = SkPoint::Make(-xMaxOffset, -yMaxOffset);
            verts[0].fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
            verts[0].fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);

            verts[1].fPos = SkPoint::Make(ellipse.fDevBounds.fLeft, ellipse.fDevBounds.fBottom);
            verts[1].fColor = color;
            verts[1].fOffset = SkPoint::Make(-xMaxOffset, yMaxOffset);
            verts[1].fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
            verts[1].fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);

            verts[2].fPos = SkPoint::Make(ellipse.fDevBounds.fRight, ellipse.fDevBounds.fTop);
            verts[2].fColor = color;
            verts[2].fOffset = SkPoint::Make(xMaxOffset, -yMaxOffset);
            verts[2].fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
            verts[2].fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);

            verts[3].fPos = SkPoint::Make(ellipse.fDevBounds.fRight, ellipse.fDevBounds.fBottom);
            verts[3].fColor = color;
            verts[3].fOffset = SkPoint::Make(xMaxOffset, yMaxOffset);
            verts[3].fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
            verts[3].fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);

            verts += kVerticesPerQuad;
        }
        auto pipe = fHelper.makePipeline(target);
        helper.recordDraw(target, std::move(gp), pipe.fPipeline, pipe.fFixedDynamicState);
    }

    CombineResult onCombineIfPossible(GrOp* t, const GrCaps& caps) override {
        EllipseOp* that = t->cast<EllipseOp>();

        if (!fHelper.isCompatible(that->fHelper, caps, this->bounds(), that->bounds())) {
            return CombineResult::kCannotCombine;
        }

        if (fStroked != that->fStroked) {
            return CombineResult::kCannotCombine;
        }

        if (fHelper.usesLocalCoords() &&
            !fViewMatrixIfUsingLocalCoords.cheapEqualTo(that->fViewMatrixIfUsingLocalCoords)) {
            return CombineResult::kCannotCombine;
        }

        fEllipses.push_back_n(that->fEllipses.count(), that->fEllipses.begin());
        this->joinBounds(*that);
        return CombineResult::kMerged;
    }

    struct Ellipse {
        GrColor fColor;
        SkScalar fXRadius;
        SkScalar fYRadius;
        SkScalar fInnerXRadius;
        SkScalar fInnerYRadius;
        SkRect fDevBounds;
    };

    SkMatrix fViewMatrixIfUsingLocalCoords;
    Helper fHelper;
    bool fStroked;
    SkSTArray<1, Ellipse, true> fEllipses;

    typedef GrMeshDrawOp INHERITED;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

class DIEllipseOp : public GrMeshDrawOp {
private:
    using Helper = GrSimpleMeshDrawOpHelper;

    struct DeviceSpaceParams {
        SkPoint fCenter;
        SkScalar fXRadius;
        SkScalar fYRadius;
        SkScalar fInnerXRadius;
        SkScalar fInnerYRadius;
        DIEllipseStyle fStyle;
    };

public:
    DEFINE_OP_CLASS_ID

    static std::unique_ptr<GrDrawOp> Make(GrContext* context,
                                          GrPaint&& paint,
                                          const SkMatrix& viewMatrix,
                                          const SkRect& ellipse,
                                          const SkStrokeRec& stroke) {
        DeviceSpaceParams params;
        params.fCenter = SkPoint::Make(ellipse.centerX(), ellipse.centerY());
        params.fXRadius = SkScalarHalf(ellipse.width());
        params.fYRadius = SkScalarHalf(ellipse.height());

        SkStrokeRec::Style style = stroke.getStyle();
        params.fStyle = (SkStrokeRec::kStroke_Style == style)
                                ? DIEllipseStyle::kStroke
                                : (SkStrokeRec::kHairline_Style == style)
                                          ? DIEllipseStyle::kHairline
                                          : DIEllipseStyle::kFill;

        params.fInnerXRadius = 0;
        params.fInnerYRadius = 0;
        if (SkStrokeRec::kFill_Style != style && SkStrokeRec::kHairline_Style != style) {
            SkScalar strokeWidth = stroke.getWidth();

            if (SkScalarNearlyZero(strokeWidth)) {
                strokeWidth = SK_ScalarHalf;
            } else {
                strokeWidth *= SK_ScalarHalf;
            }

            // we only handle thick strokes for near-circular ellipses
            if (strokeWidth > SK_ScalarHalf &&
                (SK_ScalarHalf * params.fXRadius > params.fYRadius ||
                 SK_ScalarHalf * params.fYRadius > params.fXRadius)) {
                return nullptr;
            }

            // we don't handle it if curvature of the stroke is less than curvature of the ellipse
            if (strokeWidth * (params.fYRadius * params.fYRadius) <
                (strokeWidth * strokeWidth) * params.fXRadius) {
                return nullptr;
            }
            if (strokeWidth * (params.fXRadius * params.fXRadius) <
                (strokeWidth * strokeWidth) * params.fYRadius) {
                return nullptr;
            }

            // set inner radius (if needed)
            if (SkStrokeRec::kStroke_Style == style) {
                params.fInnerXRadius = params.fXRadius - strokeWidth;
                params.fInnerYRadius = params.fYRadius - strokeWidth;
            }

            params.fXRadius += strokeWidth;
            params.fYRadius += strokeWidth;
        }
        if (DIEllipseStyle::kStroke == params.fStyle &&
            (params.fInnerXRadius <= 0 || params.fInnerYRadius <= 0)) {
            params.fStyle = DIEllipseStyle::kFill;
        }
        return Helper::FactoryHelper<DIEllipseOp>(context, std::move(paint), params, viewMatrix);
    }

    DIEllipseOp(Helper::MakeArgs& helperArgs, GrColor color, const DeviceSpaceParams& params,
                const SkMatrix& viewMatrix)
            : INHERITED(ClassID()), fHelper(helperArgs, GrAAType::kCoverage) {
        // This expands the outer rect so that after CTM we end up with a half-pixel border
        SkScalar a = viewMatrix[SkMatrix::kMScaleX];
        SkScalar b = viewMatrix[SkMatrix::kMSkewX];
        SkScalar c = viewMatrix[SkMatrix::kMSkewY];
        SkScalar d = viewMatrix[SkMatrix::kMScaleY];
        SkScalar geoDx = SK_ScalarHalf / SkScalarSqrt(a * a + c * c);
        SkScalar geoDy = SK_ScalarHalf / SkScalarSqrt(b * b + d * d);

        fEllipses.emplace_back(
                Ellipse{viewMatrix, color, params.fXRadius, params.fYRadius, params.fInnerXRadius,
                        params.fInnerYRadius, geoDx, geoDy, params.fStyle,
                        SkRect::MakeLTRB(params.fCenter.fX - params.fXRadius - geoDx,
                                         params.fCenter.fY - params.fYRadius - geoDy,
                                         params.fCenter.fX + params.fXRadius + geoDx,
                                         params.fCenter.fY + params.fYRadius + geoDy)});
        this->setTransformedBounds(fEllipses[0].fBounds, viewMatrix, HasAABloat::kYes,
                                   IsZeroArea::kNo);
    }

    const char* name() const override { return "DIEllipseOp"; }

    void visitProxies(const VisitProxyFunc& func) const override {
        fHelper.visitProxies(func);
    }

    SkString dumpInfo() const override {
        SkString string;
        for (const auto& geo : fEllipses) {
            string.appendf(
                    "Color: 0x%08x Rect [L: %.2f, T: %.2f, R: %.2f, B: %.2f], XRad: %.2f, "
                    "YRad: %.2f, InnerXRad: %.2f, InnerYRad: %.2f, GeoDX: %.2f, "
                    "GeoDY: %.2f\n",
                    geo.fColor, geo.fBounds.fLeft, geo.fBounds.fTop, geo.fBounds.fRight,
                    geo.fBounds.fBottom, geo.fXRadius, geo.fYRadius, geo.fInnerXRadius,
                    geo.fInnerYRadius, geo.fGeoDx, geo.fGeoDy);
        }
        string += fHelper.dumpInfo();
        string += INHERITED::dumpInfo();
        return string;
    }

    RequiresDstTexture finalize(const GrCaps& caps, const GrAppliedClip* clip) override {
        GrColor* color = &fEllipses.front().fColor;
        return fHelper.xpRequiresDstTexture(caps, clip, GrProcessorAnalysisCoverage::kSingleChannel,
                                            color);
    }

    FixedFunctionFlags fixedFunctionFlags() const override { return fHelper.fixedFunctionFlags(); }

private:
    void onPrepareDraws(Target* target) override {
        // Setup geometry processor
        sk_sp<GrGeometryProcessor> gp(
                new DIEllipseGeometryProcessor(this->viewMatrix(), this->style()));

        SkASSERT(sizeof(DIEllipseVertex) == gp->debugOnly_vertexStride());
        QuadHelper helper(target, sizeof(DIEllipseVertex), fEllipses.count());
        DIEllipseVertex* verts = reinterpret_cast<DIEllipseVertex*>(helper.vertices());
        if (!verts) {
            return;
        }

        for (const auto& ellipse : fEllipses) {
            GrColor color = ellipse.fColor;
            SkScalar xRadius = ellipse.fXRadius;
            SkScalar yRadius = ellipse.fYRadius;

            const SkRect& bounds = ellipse.fBounds;

            // This adjusts the "radius" to include the half-pixel border
            SkScalar offsetDx = ellipse.fGeoDx / xRadius;
            SkScalar offsetDy = ellipse.fGeoDy / yRadius;

            verts[0].fPos = SkPoint::Make(bounds.fLeft, bounds.fTop);
            verts[0].fColor = color;
            verts[0].fOuterOffset = SkPoint::Make(-1.0f - offsetDx, -1.0f - offsetDy);
            verts[0].fInnerOffset = SkPoint::Make(0.0f, 0.0f);

            verts[1].fPos = SkPoint::Make(bounds.fLeft, bounds.fBottom);
            verts[1].fColor = color;
            verts[1].fOuterOffset = SkPoint::Make(-1.0f - offsetDx, 1.0f + offsetDy);
            verts[1].fInnerOffset = SkPoint::Make(0.0f, 0.0f);

            verts[2].fPos = SkPoint::Make(bounds.fRight, bounds.fTop);
            verts[2].fColor = color;
            verts[2].fOuterOffset = SkPoint::Make(1.0f + offsetDx, -1.0f - offsetDy);
            verts[2].fInnerOffset = SkPoint::Make(0.0f, 0.0f);

            verts[3].fPos = SkPoint::Make(bounds.fRight, bounds.fBottom);
            verts[3].fColor = color;
            verts[3].fOuterOffset = SkPoint::Make(1.0f + offsetDx, 1.0f + offsetDy);
            verts[3].fInnerOffset = SkPoint::Make(0.0f, 0.0f);

            if (DIEllipseStyle::kStroke == this->style()) {
                SkScalar innerRatioX = xRadius / ellipse.fInnerXRadius;
                SkScalar innerRatioY = yRadius / ellipse.fInnerYRadius;

                verts[0].fInnerOffset = SkPoint::Make(-innerRatioX - offsetDx,
                                                      -innerRatioY - offsetDy);
                verts[1].fInnerOffset = SkPoint::Make(-innerRatioX - offsetDx,
                                                      innerRatioY + offsetDy);
                verts[2].fInnerOffset = SkPoint::Make(innerRatioX + offsetDx,
                                                      -innerRatioY - offsetDy);
                verts[3].fInnerOffset = SkPoint::Make(innerRatioX + offsetDx,
                                                      innerRatioY + offsetDy);
            }

            verts += kVerticesPerQuad;
        }
        auto pipe = fHelper.makePipeline(target);
        helper.recordDraw(target, std::move(gp), pipe.fPipeline, pipe.fFixedDynamicState);
    }

    CombineResult onCombineIfPossible(GrOp* t, const GrCaps& caps) override {
        DIEllipseOp* that = t->cast<DIEllipseOp>();
        if (!fHelper.isCompatible(that->fHelper, caps, this->bounds(), that->bounds())) {
            return CombineResult::kCannotCombine;
        }

        if (this->style() != that->style()) {
            return CombineResult::kCannotCombine;
        }

        // TODO rewrite to allow positioning on CPU
        if (!this->viewMatrix().cheapEqualTo(that->viewMatrix())) {
            return CombineResult::kCannotCombine;
        }

        fEllipses.push_back_n(that->fEllipses.count(), that->fEllipses.begin());
        this->joinBounds(*that);
        return CombineResult::kMerged;
    }

    const SkMatrix& viewMatrix() const { return fEllipses[0].fViewMatrix; }
    DIEllipseStyle style() const { return fEllipses[0].fStyle; }

    struct Ellipse {
        SkMatrix fViewMatrix;
        GrColor fColor;
        SkScalar fXRadius;
        SkScalar fYRadius;
        SkScalar fInnerXRadius;
        SkScalar fInnerYRadius;
        SkScalar fGeoDx;
        SkScalar fGeoDy;
        DIEllipseStyle fStyle;
        SkRect fBounds;
    };

    Helper fHelper;
    SkSTArray<1, Ellipse, true> fEllipses;

    typedef GrMeshDrawOp INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

// We have three possible cases for geometry for a roundrect.
//
// In the case of a normal fill or a stroke, we draw the roundrect as a 9-patch:
//    ____________
//   |_|________|_|
//   | |        | |
//   | |        | |
//   | |        | |
//   |_|________|_|
//   |_|________|_|
//
// For strokes, we don't draw the center quad.
//
// For circular roundrects, in the case where the stroke width is greater than twice
// the corner radius (overstroke), we add additional geometry to mark out the rectangle
// in the center. The shared vertices are duplicated so we can set a different outer radius
// for the fill calculation.
//    ____________
//   |_|________|_|
//   | |\ ____ /| |
//   | | |    | | |
//   | | |____| | |
//   |_|/______\|_|
//   |_|________|_|
//
// We don't draw the center quad from the fill rect in this case.
//
// For filled rrects that need to provide a distance vector we resuse the overstroke
// geometry but make the inner rect degenerate (either a point or a horizontal or
// vertical line).

static const uint16_t gOverstrokeRRectIndices[] = {
        // clang-format off
        // overstroke quads
        // we place this at the beginning so that we can skip these indices when rendering normally
        16, 17, 19, 16, 19, 18,
        19, 17, 23, 19, 23, 21,
        21, 23, 22, 21, 22, 20,
        22, 16, 18, 22, 18, 20,

        // corners
        0, 1, 5, 0, 5, 4,
        2, 3, 7, 2, 7, 6,
        8, 9, 13, 8, 13, 12,
        10, 11, 15, 10, 15, 14,

        // edges
        1, 2, 6, 1, 6, 5,
        4, 5, 9, 4, 9, 8,
        6, 7, 11, 6, 11, 10,
        9, 10, 14, 9, 14, 13,

        // center
        // we place this at the end so that we can ignore these indices when not rendering as filled
        5, 6, 10, 5, 10, 9,
        // clang-format on
};

// fill and standard stroke indices skip the overstroke "ring"
static const uint16_t* gStandardRRectIndices = gOverstrokeRRectIndices + 6 * 4;

// overstroke count is arraysize minus the center indices
static const int kIndicesPerOverstrokeRRect = SK_ARRAY_COUNT(gOverstrokeRRectIndices) - 6;
// fill count skips overstroke indices and includes center
static const int kIndicesPerFillRRect = kIndicesPerOverstrokeRRect - 6 * 4 + 6;
// stroke count is fill count minus center indices
static const int kIndicesPerStrokeRRect = kIndicesPerFillRRect - 6;
static const int kVertsPerStandardRRect = 16;
static const int kVertsPerOverstrokeRRect = 24;

enum RRectType {
    kFill_RRectType,
    kStroke_RRectType,
    kOverstroke_RRectType,
};

static int rrect_type_to_vert_count(RRectType type) {
    switch (type) {
        case kFill_RRectType:
        case kStroke_RRectType:
            return kVertsPerStandardRRect;
        case kOverstroke_RRectType:
            return kVertsPerOverstrokeRRect;
    }
    SK_ABORT("Invalid type");
    return 0;
}

static int rrect_type_to_index_count(RRectType type) {
    switch (type) {
        case kFill_RRectType:
            return kIndicesPerFillRRect;
        case kStroke_RRectType:
            return kIndicesPerStrokeRRect;
        case kOverstroke_RRectType:
            return kIndicesPerOverstrokeRRect;
    }
    SK_ABORT("Invalid type");
    return 0;
}

static const uint16_t* rrect_type_to_indices(RRectType type) {
    switch (type) {
        case kFill_RRectType:
        case kStroke_RRectType:
            return gStandardRRectIndices;
        case kOverstroke_RRectType:
            return gOverstrokeRRectIndices;
    }
    SK_ABORT("Invalid type");
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

// For distance computations in the interior of filled rrects we:
//
//   add a interior degenerate (point or line) rect
//   each vertex of that rect gets -outerRad as its radius
//      this makes the computation of the distance to the outer edge be negative
//      negative values are caught and then handled differently in the GP's onEmitCode
//   each vertex is also given the normalized x & y distance from the interior rect's edge
//      the GP takes the min of those depths +1 to get the normalized distance to the outer edge

class CircularRRectOp : public GrMeshDrawOp {
private:
    using Helper = GrSimpleMeshDrawOpHelper;

public:
    DEFINE_OP_CLASS_ID

    // A devStrokeWidth <= 0 indicates a fill only. If devStrokeWidth > 0 then strokeOnly indicates
    // whether the rrect is only stroked or stroked and filled.
    static std::unique_ptr<GrDrawOp> Make(GrContext* context,
                                          GrPaint&& paint,
                                          const SkMatrix& viewMatrix,
                                          const SkRect& devRect,
                                          float devRadius,
                                          float devStrokeWidth,
                                          bool strokeOnly) {
        return Helper::FactoryHelper<CircularRRectOp>(context, std::move(paint), viewMatrix,
                                                      devRect, devRadius,
                                                      devStrokeWidth, strokeOnly);
    }
    CircularRRectOp(Helper::MakeArgs& helperArgs, GrColor color, const SkMatrix& viewMatrix,
                    const SkRect& devRect, float devRadius, float devStrokeWidth, bool strokeOnly)
            : INHERITED(ClassID())
            , fViewMatrixIfUsingLocalCoords(viewMatrix)
            , fHelper(helperArgs, GrAAType::kCoverage) {
        SkRect bounds = devRect;
        SkASSERT(!(devStrokeWidth <= 0 && strokeOnly));
        SkScalar innerRadius = 0.0f;
        SkScalar outerRadius = devRadius;
        SkScalar halfWidth = 0;
        RRectType type = kFill_RRectType;
        if (devStrokeWidth > 0) {
            if (SkScalarNearlyZero(devStrokeWidth)) {
                halfWidth = SK_ScalarHalf;
            } else {
                halfWidth = SkScalarHalf(devStrokeWidth);
            }

            if (strokeOnly) {
                // Outset stroke by 1/4 pixel
                devStrokeWidth += 0.25f;
                // If stroke is greater than width or height, this is still a fill
                // Otherwise we compute stroke params
                if (devStrokeWidth <= devRect.width() && devStrokeWidth <= devRect.height()) {
                    innerRadius = devRadius - halfWidth;
                    type = (innerRadius >= 0) ? kStroke_RRectType : kOverstroke_RRectType;
                }
            }
            outerRadius += halfWidth;
            bounds.outset(halfWidth, halfWidth);
        }

        // The radii are outset for two reasons. First, it allows the shader to simply perform
        // simpler computation because the computed alpha is zero, rather than 50%, at the radius.
        // Second, the outer radius is used to compute the verts of the bounding box that is
        // rendered and the outset ensures the box will cover all partially covered by the rrect
        // corners.
        outerRadius += SK_ScalarHalf;
        innerRadius -= SK_ScalarHalf;

        this->setBounds(bounds, HasAABloat::kYes, IsZeroArea::kNo);

        // Expand the rect for aa to generate correct vertices.
        bounds.outset(SK_ScalarHalf, SK_ScalarHalf);

        fRRects.emplace_back(RRect{color, innerRadius, outerRadius, bounds, type});
        fVertCount = rrect_type_to_vert_count(type);
        fIndexCount = rrect_type_to_index_count(type);
        fAllFill = (kFill_RRectType == type);
    }

    const char* name() const override { return "CircularRRectOp"; }

    void visitProxies(const VisitProxyFunc& func) const override {
        fHelper.visitProxies(func);
    }

    SkString dumpInfo() const override {
        SkString string;
        for (int i = 0; i < fRRects.count(); ++i) {
            string.appendf(
                    "Color: 0x%08x Rect [L: %.2f, T: %.2f, R: %.2f, B: %.2f],"
                    "InnerRad: %.2f, OuterRad: %.2f\n",
                    fRRects[i].fColor, fRRects[i].fDevBounds.fLeft, fRRects[i].fDevBounds.fTop,
                    fRRects[i].fDevBounds.fRight, fRRects[i].fDevBounds.fBottom,
                    fRRects[i].fInnerRadius, fRRects[i].fOuterRadius);
        }
        string += fHelper.dumpInfo();
        string += INHERITED::dumpInfo();
        return string;
    }

    RequiresDstTexture finalize(const GrCaps& caps, const GrAppliedClip* clip) override {
        GrColor* color = &fRRects.front().fColor;
        return fHelper.xpRequiresDstTexture(caps, clip, GrProcessorAnalysisCoverage::kSingleChannel,
                                            color);
    }

    FixedFunctionFlags fixedFunctionFlags() const override { return fHelper.fixedFunctionFlags(); }

private:
    struct CircleVertex {
        SkPoint fPos;
        GrColor fColor;
        SkPoint fOffset;
        SkScalar fOuterRadius;
        SkScalar fInnerRadius;
        // No half plane, we don't use it here.
    };

    static void FillInOverstrokeVerts(CircleVertex** verts, const SkRect& bounds, SkScalar smInset,
                                      SkScalar bigInset, SkScalar xOffset, SkScalar outerRadius,
                                      SkScalar innerRadius, GrColor color) {
        SkASSERT(smInset < bigInset);

        // TL
        (*verts)->fPos = SkPoint::Make(bounds.fLeft + smInset, bounds.fTop + smInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(xOffset, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;

        // TR
        (*verts)->fPos = SkPoint::Make(bounds.fRight - smInset, bounds.fTop + smInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(xOffset, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;

        (*verts)->fPos = SkPoint::Make(bounds.fLeft + bigInset, bounds.fTop + bigInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(0, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;

        (*verts)->fPos = SkPoint::Make(bounds.fRight - bigInset, bounds.fTop + bigInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(0, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;

        (*verts)->fPos = SkPoint::Make(bounds.fLeft + bigInset, bounds.fBottom - bigInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(0, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;

        (*verts)->fPos = SkPoint::Make(bounds.fRight - bigInset, bounds.fBottom - bigInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(0, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;

        // BL
        (*verts)->fPos = SkPoint::Make(bounds.fLeft + smInset, bounds.fBottom - smInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(xOffset, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;

        // BR
        (*verts)->fPos = SkPoint::Make(bounds.fRight - smInset, bounds.fBottom - smInset);
        (*verts)->fColor = color;
        (*verts)->fOffset = SkPoint::Make(xOffset, 0);
        (*verts)->fOuterRadius = outerRadius;
        (*verts)->fInnerRadius = innerRadius;
        (*verts)++;
    }

    void onPrepareDraws(Target* target) override {
        // Invert the view matrix as a local matrix (if any other processors require coords).
        SkMatrix localMatrix;
        if (!fViewMatrixIfUsingLocalCoords.invert(&localMatrix)) {
            return;
        }

        // Setup geometry processor
        sk_sp<GrGeometryProcessor> gp(
                new CircleGeometryProcessor(!fAllFill, false, false, false, false, localMatrix));

        SkASSERT(sizeof(CircleVertex) == gp->debugOnly_vertexStride());

        const GrBuffer* vertexBuffer;
        int firstVertex;

        CircleVertex* verts = (CircleVertex*)target->makeVertexSpace(
                sizeof(CircleVertex), fVertCount, &vertexBuffer, &firstVertex);
        if (!verts) {
            SkDebugf("Could not allocate vertices\n");
            return;
        }

        const GrBuffer* indexBuffer = nullptr;
        int firstIndex = 0;
        uint16_t* indices = target->makeIndexSpace(fIndexCount, &indexBuffer, &firstIndex);
        if (!indices) {
            SkDebugf("Could not allocate indices\n");
            return;
        }

        int currStartVertex = 0;
        for (const auto& rrect : fRRects) {
            GrColor color = rrect.fColor;
            SkScalar outerRadius = rrect.fOuterRadius;
            const SkRect& bounds = rrect.fDevBounds;

            SkScalar yCoords[4] = {bounds.fTop, bounds.fTop + outerRadius,
                                   bounds.fBottom - outerRadius, bounds.fBottom};

            SkScalar yOuterRadii[4] = {-1, 0, 0, 1};
            // The inner radius in the vertex data must be specified in normalized space.
            // For fills, specifying -1/outerRadius guarantees an alpha of 1.0 at the inner radius.
            SkScalar innerRadius = rrect.fType != kFill_RRectType
                                           ? rrect.fInnerRadius / rrect.fOuterRadius
                                           : -1.0f / rrect.fOuterRadius;
            for (int i = 0; i < 4; ++i) {
                verts->fPos = SkPoint::Make(bounds.fLeft, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(-1, yOuterRadii[i]);
                verts->fOuterRadius = outerRadius;
                verts->fInnerRadius = innerRadius;
                verts++;

                verts->fPos = SkPoint::Make(bounds.fLeft + outerRadius, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(0, yOuterRadii[i]);
                verts->fOuterRadius = outerRadius;
                verts->fInnerRadius = innerRadius;
                verts++;

                verts->fPos = SkPoint::Make(bounds.fRight - outerRadius, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(0, yOuterRadii[i]);
                verts->fOuterRadius = outerRadius;
                verts->fInnerRadius = innerRadius;
                verts++;

                verts->fPos = SkPoint::Make(bounds.fRight, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(1, yOuterRadii[i]);
                verts->fOuterRadius = outerRadius;
                verts->fInnerRadius = innerRadius;
                verts++;
            }
            // Add the additional vertices for overstroked rrects.
            // Effectively this is an additional stroked rrect, with its
            // outer radius = outerRadius - innerRadius, and inner radius = 0.
            // This will give us correct AA in the center and the correct
            // distance to the outer edge.
            //
            // Also, the outer offset is a constant vector pointing to the right, which
            // guarantees that the distance value along the outer rectangle is constant.
            if (kOverstroke_RRectType == rrect.fType) {
                SkASSERT(rrect.fInnerRadius <= 0.0f);

                SkScalar overstrokeOuterRadius = outerRadius - rrect.fInnerRadius;
                // this is the normalized distance from the outer rectangle of this
                // geometry to the outer edge
                SkScalar maxOffset = -rrect.fInnerRadius / overstrokeOuterRadius;

                FillInOverstrokeVerts(&verts, bounds, outerRadius, overstrokeOuterRadius, maxOffset,
                                      overstrokeOuterRadius, 0.0f, rrect.fColor);
            }

            const uint16_t* primIndices = rrect_type_to_indices(rrect.fType);
            const int primIndexCount = rrect_type_to_index_count(rrect.fType);
            for (int i = 0; i < primIndexCount; ++i) {
                *indices++ = primIndices[i] + currStartVertex;
            }

            currStartVertex += rrect_type_to_vert_count(rrect.fType);
        }

        GrMesh* mesh = target->allocMesh(GrPrimitiveType::kTriangles);
        mesh->setIndexed(indexBuffer, fIndexCount, firstIndex, 0, fVertCount - 1,
                         GrPrimitiveRestart::kNo);
        mesh->setVertexData(vertexBuffer, firstVertex);
        auto pipe = fHelper.makePipeline(target);
        target->draw(std::move(gp), pipe.fPipeline, pipe.fFixedDynamicState, mesh);
    }

    CombineResult onCombineIfPossible(GrOp* t, const GrCaps& caps) override {
        CircularRRectOp* that = t->cast<CircularRRectOp>();

        // can only represent 65535 unique vertices with 16-bit indices
        if (fVertCount + that->fVertCount > 65536) {
            return CombineResult::kCannotCombine;
        }

        if (!fHelper.isCompatible(that->fHelper, caps, this->bounds(), that->bounds())) {
            return CombineResult::kCannotCombine;
        }

        if (fHelper.usesLocalCoords() &&
            !fViewMatrixIfUsingLocalCoords.cheapEqualTo(that->fViewMatrixIfUsingLocalCoords)) {
            return CombineResult::kCannotCombine;
        }

        fRRects.push_back_n(that->fRRects.count(), that->fRRects.begin());
        this->joinBounds(*that);
        fVertCount += that->fVertCount;
        fIndexCount += that->fIndexCount;
        fAllFill = fAllFill && that->fAllFill;
        return CombineResult::kMerged;
    }

    struct RRect {
        GrColor fColor;
        SkScalar fInnerRadius;
        SkScalar fOuterRadius;
        SkRect fDevBounds;
        RRectType fType;
    };

    SkMatrix fViewMatrixIfUsingLocalCoords;
    Helper fHelper;
    int fVertCount;
    int fIndexCount;
    bool fAllFill;
    SkSTArray<1, RRect, true> fRRects;

    typedef GrMeshDrawOp INHERITED;
};

static const int kNumRRectsInIndexBuffer = 256;

GR_DECLARE_STATIC_UNIQUE_KEY(gStrokeRRectOnlyIndexBufferKey);
GR_DECLARE_STATIC_UNIQUE_KEY(gRRectOnlyIndexBufferKey);
static sk_sp<const GrBuffer> get_rrect_index_buffer(RRectType type,
                                                    GrResourceProvider* resourceProvider) {
    GR_DEFINE_STATIC_UNIQUE_KEY(gStrokeRRectOnlyIndexBufferKey);
    GR_DEFINE_STATIC_UNIQUE_KEY(gRRectOnlyIndexBufferKey);
    switch (type) {
        case kFill_RRectType:
            return resourceProvider->findOrCreatePatternedIndexBuffer(
                    gStandardRRectIndices, kIndicesPerFillRRect, kNumRRectsInIndexBuffer,
                    kVertsPerStandardRRect, gRRectOnlyIndexBufferKey);
        case kStroke_RRectType:
            return resourceProvider->findOrCreatePatternedIndexBuffer(
                    gStandardRRectIndices, kIndicesPerStrokeRRect, kNumRRectsInIndexBuffer,
                    kVertsPerStandardRRect, gStrokeRRectOnlyIndexBufferKey);
        default:
            SkASSERT(false);
            return nullptr;
    };
}

class EllipticalRRectOp : public GrMeshDrawOp {
private:
    using Helper = GrSimpleMeshDrawOpHelper;

public:
    DEFINE_OP_CLASS_ID

    // If devStrokeWidths values are <= 0 indicates then fill only. Otherwise, strokeOnly indicates
    // whether the rrect is only stroked or stroked and filled.
    static std::unique_ptr<GrDrawOp> Make(GrContext* context,
                                          GrPaint&& paint,
                                          const SkMatrix& viewMatrix,
                                          const SkRect& devRect,
                                          float devXRadius,
                                          float devYRadius,
                                          SkVector devStrokeWidths,
                                          bool strokeOnly) {
        SkASSERT(devXRadius > 0.5);
        SkASSERT(devYRadius > 0.5);
        SkASSERT((devStrokeWidths.fX > 0) == (devStrokeWidths.fY > 0));
        SkASSERT(!(strokeOnly && devStrokeWidths.fX <= 0));
        if (devStrokeWidths.fX > 0) {
            if (SkScalarNearlyZero(devStrokeWidths.length())) {
                devStrokeWidths.set(SK_ScalarHalf, SK_ScalarHalf);
            } else {
                devStrokeWidths.scale(SK_ScalarHalf);
            }

            // we only handle thick strokes for near-circular ellipses
            if (devStrokeWidths.length() > SK_ScalarHalf &&
                (SK_ScalarHalf * devXRadius > devYRadius ||
                 SK_ScalarHalf * devYRadius > devXRadius)) {
                return nullptr;
            }

            // we don't handle it if curvature of the stroke is less than curvature of the ellipse
            if (devStrokeWidths.fX * (devYRadius * devYRadius) <
                (devStrokeWidths.fY * devStrokeWidths.fY) * devXRadius) {
                return nullptr;
            }
            if (devStrokeWidths.fY * (devXRadius * devXRadius) <
                (devStrokeWidths.fX * devStrokeWidths.fX) * devYRadius) {
                return nullptr;
            }
        }
        return Helper::FactoryHelper<EllipticalRRectOp>(context, std::move(paint),
                                                        viewMatrix, devRect,
                                                        devXRadius, devYRadius, devStrokeWidths,
                                                        strokeOnly);
    }

    EllipticalRRectOp(Helper::MakeArgs helperArgs, GrColor color, const SkMatrix& viewMatrix,
                      const SkRect& devRect, float devXRadius, float devYRadius,
                      SkVector devStrokeHalfWidths, bool strokeOnly)
            : INHERITED(ClassID()), fHelper(helperArgs, GrAAType::kCoverage) {
        SkScalar innerXRadius = 0.0f;
        SkScalar innerYRadius = 0.0f;
        SkRect bounds = devRect;
        bool stroked = false;
        if (devStrokeHalfWidths.fX > 0) {
            // this is legit only if scale & translation (which should be the case at the moment)
            if (strokeOnly) {
                innerXRadius = devXRadius - devStrokeHalfWidths.fX;
                innerYRadius = devYRadius - devStrokeHalfWidths.fY;
                stroked = (innerXRadius >= 0 && innerYRadius >= 0);
            }

            devXRadius += devStrokeHalfWidths.fX;
            devYRadius += devStrokeHalfWidths.fY;
            bounds.outset(devStrokeHalfWidths.fX, devStrokeHalfWidths.fY);
        }

        fStroked = stroked;
        fViewMatrixIfUsingLocalCoords = viewMatrix;
        this->setBounds(bounds, HasAABloat::kYes, IsZeroArea::kNo);
        // Expand the rect for aa in order to generate the correct vertices.
        bounds.outset(SK_ScalarHalf, SK_ScalarHalf);
        fRRects.emplace_back(
                RRect{color, devXRadius, devYRadius, innerXRadius, innerYRadius, bounds});
    }

    const char* name() const override { return "EllipticalRRectOp"; }

    void visitProxies(const VisitProxyFunc& func) const override {
        fHelper.visitProxies(func);
    }

    SkString dumpInfo() const override {
        SkString string;
        string.appendf("Stroked: %d\n", fStroked);
        for (const auto& geo : fRRects) {
            string.appendf(
                    "Color: 0x%08x Rect [L: %.2f, T: %.2f, R: %.2f, B: %.2f], "
                    "XRad: %.2f, YRad: %.2f, InnerXRad: %.2f, InnerYRad: %.2f\n",
                    geo.fColor, geo.fDevBounds.fLeft, geo.fDevBounds.fTop, geo.fDevBounds.fRight,
                    geo.fDevBounds.fBottom, geo.fXRadius, geo.fYRadius, geo.fInnerXRadius,
                    geo.fInnerYRadius);
        }
        string += fHelper.dumpInfo();
        string += INHERITED::dumpInfo();
        return string;
    }

    RequiresDstTexture finalize(const GrCaps& caps, const GrAppliedClip* clip) override {
        GrColor* color = &fRRects.front().fColor;
        return fHelper.xpRequiresDstTexture(caps, clip, GrProcessorAnalysisCoverage::kSingleChannel,
                                            color);
    }

    FixedFunctionFlags fixedFunctionFlags() const override { return fHelper.fixedFunctionFlags(); }

private:
    void onPrepareDraws(Target* target) override {
        SkMatrix localMatrix;
        if (!fViewMatrixIfUsingLocalCoords.invert(&localMatrix)) {
            return;
        }

        // Setup geometry processor
        sk_sp<GrGeometryProcessor> gp(new EllipseGeometryProcessor(fStroked, localMatrix));

        SkASSERT(sizeof(EllipseVertex) == gp->debugOnly_vertexStride());

        // drop out the middle quad if we're stroked
        int indicesPerInstance = fStroked ? kIndicesPerStrokeRRect : kIndicesPerFillRRect;
        sk_sp<const GrBuffer> indexBuffer = get_rrect_index_buffer(
                fStroked ? kStroke_RRectType : kFill_RRectType, target->resourceProvider());

        PatternHelper helper(target, GrPrimitiveType::kTriangles, sizeof(EllipseVertex),
                             indexBuffer.get(), kVertsPerStandardRRect, indicesPerInstance,
                             fRRects.count());
        EllipseVertex* verts = reinterpret_cast<EllipseVertex*>(helper.vertices());
        if (!verts || !indexBuffer) {
            SkDebugf("Could not allocate vertices\n");
            return;
        }

        for (const auto& rrect : fRRects) {
            GrColor color = rrect.fColor;
            // Compute the reciprocals of the radii here to save time in the shader
            SkScalar xRadRecip = SkScalarInvert(rrect.fXRadius);
            SkScalar yRadRecip = SkScalarInvert(rrect.fYRadius);
            SkScalar xInnerRadRecip = SkScalarInvert(rrect.fInnerXRadius);
            SkScalar yInnerRadRecip = SkScalarInvert(rrect.fInnerYRadius);

            // Extend the radii out half a pixel to antialias.
            SkScalar xOuterRadius = rrect.fXRadius + SK_ScalarHalf;
            SkScalar yOuterRadius = rrect.fYRadius + SK_ScalarHalf;

            SkScalar xMaxOffset = xOuterRadius;
            SkScalar yMaxOffset = yOuterRadius;
            if (!fStroked) {
                // For filled rrects we map a unit circle in the vertex attributes rather than
                // computing an ellipse and modifying that distance, so we normalize to 1.
                xMaxOffset /= rrect.fXRadius;
                yMaxOffset /= rrect.fYRadius;
            }

            const SkRect& bounds = rrect.fDevBounds;

            SkScalar yCoords[4] = {bounds.fTop, bounds.fTop + yOuterRadius,
                                   bounds.fBottom - yOuterRadius, bounds.fBottom};
            SkScalar yOuterOffsets[4] = {yMaxOffset,
                                         SK_ScalarNearlyZero,  // we're using inversesqrt() in
                                                               // shader, so can't be exactly 0
                                         SK_ScalarNearlyZero, yMaxOffset};

            for (int i = 0; i < 4; ++i) {
                verts->fPos = SkPoint::Make(bounds.fLeft, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(xMaxOffset, yOuterOffsets[i]);
                verts->fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
                verts->fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);
                verts++;

                verts->fPos = SkPoint::Make(bounds.fLeft + xOuterRadius, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(SK_ScalarNearlyZero, yOuterOffsets[i]);
                verts->fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
                verts->fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);
                verts++;

                verts->fPos = SkPoint::Make(bounds.fRight - xOuterRadius, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(SK_ScalarNearlyZero, yOuterOffsets[i]);
                verts->fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
                verts->fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);
                verts++;

                verts->fPos = SkPoint::Make(bounds.fRight, yCoords[i]);
                verts->fColor = color;
                verts->fOffset = SkPoint::Make(xMaxOffset, yOuterOffsets[i]);
                verts->fOuterRadii = SkPoint::Make(xRadRecip, yRadRecip);
                verts->fInnerRadii = SkPoint::Make(xInnerRadRecip, yInnerRadRecip);
                verts++;
            }
        }
        auto pipe = fHelper.makePipeline(target);
        helper.recordDraw(target, std::move(gp), pipe.fPipeline, pipe.fFixedDynamicState);
    }

    CombineResult onCombineIfPossible(GrOp* t, const GrCaps& caps) override {
        EllipticalRRectOp* that = t->cast<EllipticalRRectOp>();

        if (!fHelper.isCompatible(that->fHelper, caps, this->bounds(), that->bounds())) {
            return CombineResult::kCannotCombine;
        }

        if (fStroked != that->fStroked) {
            return CombineResult::kCannotCombine;
        }

        if (fHelper.usesLocalCoords() &&
            !fViewMatrixIfUsingLocalCoords.cheapEqualTo(that->fViewMatrixIfUsingLocalCoords)) {
            return CombineResult::kCannotCombine;
        }

        fRRects.push_back_n(that->fRRects.count(), that->fRRects.begin());
        this->joinBounds(*that);
        return CombineResult::kMerged;
    }

    struct RRect {
        GrColor fColor;
        SkScalar fXRadius;
        SkScalar fYRadius;
        SkScalar fInnerXRadius;
        SkScalar fInnerYRadius;
        SkRect fDevBounds;
    };

    SkMatrix fViewMatrixIfUsingLocalCoords;
    Helper fHelper;
    bool fStroked;
    SkSTArray<1, RRect, true> fRRects;

    typedef GrMeshDrawOp INHERITED;
};

static std::unique_ptr<GrDrawOp> make_rrect_op(GrContext* context,
                                               GrPaint&& paint,
                                               const SkMatrix& viewMatrix,
                                               const SkRRect& rrect,
                                               const SkStrokeRec& stroke) {
    SkASSERT(viewMatrix.rectStaysRect());
    SkASSERT(rrect.isSimple());
    SkASSERT(!rrect.isOval());

    // RRect ops only handle simple, but not too simple, rrects.
    // Do any matrix crunching before we reset the draw state for device coords.
    const SkRect& rrectBounds = rrect.getBounds();
    SkRect bounds;
    viewMatrix.mapRect(&bounds, rrectBounds);

    SkVector radii = SkRRectPriv::GetSimpleRadii(rrect);
    SkScalar xRadius = SkScalarAbs(viewMatrix[SkMatrix::kMScaleX] * radii.fX +
                                   viewMatrix[SkMatrix::kMSkewY] * radii.fY);
    SkScalar yRadius = SkScalarAbs(viewMatrix[SkMatrix::kMSkewX] * radii.fX +
                                   viewMatrix[SkMatrix::kMScaleY] * radii.fY);

    SkStrokeRec::Style style = stroke.getStyle();

    // Do (potentially) anisotropic mapping of stroke. Use -1s to indicate fill-only draws.
    SkVector scaledStroke = {-1, -1};
    SkScalar strokeWidth = stroke.getWidth();

    bool isStrokeOnly =
            SkStrokeRec::kStroke_Style == style || SkStrokeRec::kHairline_Style == style;
    bool hasStroke = isStrokeOnly || SkStrokeRec::kStrokeAndFill_Style == style;

    bool isCircular = (xRadius == yRadius);
    if (hasStroke) {
        if (SkStrokeRec::kHairline_Style == style) {
            scaledStroke.set(1, 1);
        } else {
            scaledStroke.fX = SkScalarAbs(
                    strokeWidth * (viewMatrix[SkMatrix::kMScaleX] + viewMatrix[SkMatrix::kMSkewY]));
            scaledStroke.fY = SkScalarAbs(
                    strokeWidth * (viewMatrix[SkMatrix::kMSkewX] + viewMatrix[SkMatrix::kMScaleY]));
        }

        isCircular = isCircular && scaledStroke.fX == scaledStroke.fY;
        // for non-circular rrects, if half of strokewidth is greater than radius,
        // we don't handle that right now
        if (!isCircular && (SK_ScalarHalf * scaledStroke.fX > xRadius ||
                            SK_ScalarHalf * scaledStroke.fY > yRadius)) {
            return nullptr;
        }
    }

    // The way the effect interpolates the offset-to-ellipse/circle-center attribute only works on
    // the interior of the rrect if the radii are >= 0.5. Otherwise, the inner rect of the nine-
    // patch will have fractional coverage. This only matters when the interior is actually filled.
    // We could consider falling back to rect rendering here, since a tiny radius is
    // indistinguishable from a square corner.
    if (!isStrokeOnly && (SK_ScalarHalf > xRadius || SK_ScalarHalf > yRadius)) {
        return nullptr;
    }

    // if the corners are circles, use the circle renderer
    if (isCircular) {
        return CircularRRectOp::Make(context, std::move(paint), viewMatrix, bounds, xRadius,
                                     scaledStroke.fX, isStrokeOnly);
        // otherwise we use the ellipse renderer
    } else {
        return EllipticalRRectOp::Make(context, std::move(paint), viewMatrix, bounds,
                                       xRadius, yRadius, scaledStroke, isStrokeOnly);
    }
}

std::unique_ptr<GrDrawOp> GrOvalOpFactory::MakeRRectOp(GrContext* context,
                                                       GrPaint&& paint,
                                                       const SkMatrix& viewMatrix,
                                                       const SkRRect& rrect,
                                                       const SkStrokeRec& stroke,
                                                       const GrShaderCaps* shaderCaps) {
    if (rrect.isOval()) {
        return MakeOvalOp(context, std::move(paint), viewMatrix, rrect.getBounds(),
                          GrStyle(stroke, nullptr), shaderCaps);
    }

    if (!viewMatrix.rectStaysRect() || !rrect.isSimple()) {
        return nullptr;
    }

    return make_rrect_op(context, std::move(paint), viewMatrix, rrect, stroke);
}

///////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrDrawOp> GrOvalOpFactory::MakeOvalOp(GrContext* context,
                                                      GrPaint&& paint,
                                                      const SkMatrix& viewMatrix,
                                                      const SkRect& oval,
                                                      const GrStyle& style,
                                                      const GrShaderCaps* shaderCaps) {
    // we can draw circles
    SkScalar width = oval.width();
    if (width > SK_ScalarNearlyZero && SkScalarNearlyEqual(width, oval.height()) &&
        circle_stays_circle(viewMatrix)) {
        auto r = width / 2.f;
        SkPoint center = {oval.centerX(), oval.centerY()};
        if (style.hasNonDashPathEffect()) {
            return nullptr;
        } else if (style.isDashed()) {
            if (style.strokeRec().getCap() != SkPaint::kButt_Cap ||
                style.dashIntervalCnt() != 2 || style.strokeRec().getWidth() >= width) {
                return nullptr;
            }
            auto onInterval = style.dashIntervals()[0];
            auto offInterval = style.dashIntervals()[1];
            if (offInterval == 0) {
                GrStyle strokeStyle(style.strokeRec(), nullptr);
                return MakeOvalOp(context, std::move(paint), viewMatrix, oval,
                                  strokeStyle, shaderCaps);
            } else if (onInterval == 0) {
                // There is nothing to draw but we have no way to indicate that here.
                return nullptr;
            }
            auto angularOnInterval = onInterval / r;
            auto angularOffInterval = offInterval / r;
            auto phaseAngle = style.dashPhase() / r;
            // Currently this function doesn't accept ovals with different start angles, though
            // it could.
            static const SkScalar kStartAngle = 0.f;
            return ButtCapDashedCircleOp::Make(context, std::move(paint), viewMatrix, center, r,
                                               style.strokeRec().getWidth(), kStartAngle,
                                               angularOnInterval, angularOffInterval, phaseAngle);
        }
        return CircleOp::Make(context, std::move(paint), viewMatrix, center, r, style);
    }

    if (style.pathEffect()) {
        return nullptr;
    }

    // prefer the device space ellipse op for batchability
    if (viewMatrix.rectStaysRect()) {
        return EllipseOp::Make(context, std::move(paint), viewMatrix, oval, style.strokeRec());
    }

    // Otherwise, if we have shader derivative support, render as device-independent
    if (shaderCaps->shaderDerivativeSupport()) {
        SkScalar a = viewMatrix[SkMatrix::kMScaleX];
        SkScalar b = viewMatrix[SkMatrix::kMSkewX];
        SkScalar c = viewMatrix[SkMatrix::kMSkewY];
        SkScalar d = viewMatrix[SkMatrix::kMScaleY];
        // Check for near-degenerate matrix
        if (a*a + c*c > SK_ScalarNearlyZero && b*b + d*d > SK_ScalarNearlyZero) {
            return DIEllipseOp::Make(context, std::move(paint), viewMatrix, oval,
                                     style.strokeRec());
        }
    }

    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrDrawOp> GrOvalOpFactory::MakeArcOp(GrContext* context,
                                                     GrPaint&& paint,
                                                     const SkMatrix& viewMatrix,
                                                     const SkRect& oval, SkScalar startAngle,
                                                     SkScalar sweepAngle, bool useCenter,
                                                     const GrStyle& style,
                                                     const GrShaderCaps* shaderCaps) {
    SkASSERT(!oval.isEmpty());
    SkASSERT(sweepAngle);
    SkScalar width = oval.width();
    if (SkScalarAbs(sweepAngle) >= 360.f) {
        return nullptr;
    }
    if (!SkScalarNearlyEqual(width, oval.height()) || !circle_stays_circle(viewMatrix)) {
        return nullptr;
    }
    SkPoint center = {oval.centerX(), oval.centerY()};
    CircleOp::ArcParams arcParams = {SkDegreesToRadians(startAngle), SkDegreesToRadians(sweepAngle),
                                     useCenter};
    return CircleOp::Make(context, std::move(paint), viewMatrix,
                          center, width / 2.f, style, &arcParams);
}

///////////////////////////////////////////////////////////////////////////////

#if GR_TEST_UTILS

GR_DRAW_OP_TEST_DEFINE(CircleOp) {
    do {
        SkScalar rotate = random->nextSScalar1() * 360.f;
        SkScalar translateX = random->nextSScalar1() * 1000.f;
        SkScalar translateY = random->nextSScalar1() * 1000.f;
        SkScalar scale;
        do {
            scale = random->nextSScalar1() * 100.f;
        } while (scale == 0);
        SkMatrix viewMatrix;
        viewMatrix.setRotate(rotate);
        viewMatrix.postTranslate(translateX, translateY);
        viewMatrix.postScale(scale, scale);
        SkRect circle = GrTest::TestSquare(random);
        SkPoint center = {circle.centerX(), circle.centerY()};
        SkScalar radius = circle.width() / 2.f;
        SkStrokeRec stroke = GrTest::TestStrokeRec(random);
        CircleOp::ArcParams arcParamsTmp;
        const CircleOp::ArcParams* arcParams = nullptr;
        if (random->nextBool()) {
            arcParamsTmp.fStartAngleRadians = random->nextSScalar1() * SK_ScalarPI * 2;
            arcParamsTmp.fSweepAngleRadians = random->nextSScalar1() * SK_ScalarPI * 2 - .01f;
            arcParamsTmp.fUseCenter = random->nextBool();
            arcParams = &arcParamsTmp;
        }
        std::unique_ptr<GrDrawOp> op = CircleOp::Make(context, std::move(paint), viewMatrix,
                                                      center, radius,
                                                      GrStyle(stroke, nullptr), arcParams);
        if (op) {
            return op;
        }
    } while (true);
}

GR_DRAW_OP_TEST_DEFINE(ButtCapDashedCircleOp) {
    SkScalar rotate = random->nextSScalar1() * 360.f;
    SkScalar translateX = random->nextSScalar1() * 1000.f;
    SkScalar translateY = random->nextSScalar1() * 1000.f;
    SkScalar scale;
    do {
        scale = random->nextSScalar1() * 100.f;
    } while (scale == 0);
    SkMatrix viewMatrix;
    viewMatrix.setRotate(rotate);
    viewMatrix.postTranslate(translateX, translateY);
    viewMatrix.postScale(scale, scale);
    SkRect circle = GrTest::TestSquare(random);
    SkPoint center = {circle.centerX(), circle.centerY()};
    SkScalar radius = circle.width() / 2.f;
    SkScalar strokeWidth = random->nextRangeScalar(0.001f * radius, 1.8f * radius);
    SkScalar onAngle = random->nextRangeScalar(0.01f, 1000.f);
    SkScalar offAngle = random->nextRangeScalar(0.01f, 1000.f);
    SkScalar startAngle = random->nextRangeScalar(-1000.f, 1000.f);
    SkScalar phase = random->nextRangeScalar(-1000.f, 1000.f);
    return ButtCapDashedCircleOp::Make(context, std::move(paint), viewMatrix,
                                       center, radius, strokeWidth,
                                       startAngle, onAngle, offAngle, phase);
}

GR_DRAW_OP_TEST_DEFINE(EllipseOp) {
    SkMatrix viewMatrix = GrTest::TestMatrixRectStaysRect(random);
    SkRect ellipse = GrTest::TestSquare(random);
    return EllipseOp::Make(context, std::move(paint), viewMatrix, ellipse,
                           GrTest::TestStrokeRec(random));
}

GR_DRAW_OP_TEST_DEFINE(DIEllipseOp) {
    SkMatrix viewMatrix = GrTest::TestMatrix(random);
    SkRect ellipse = GrTest::TestSquare(random);
    return DIEllipseOp::Make(context, std::move(paint), viewMatrix, ellipse,
                             GrTest::TestStrokeRec(random));
}

GR_DRAW_OP_TEST_DEFINE(RRectOp) {
    SkMatrix viewMatrix = GrTest::TestMatrixRectStaysRect(random);
    const SkRRect& rrect = GrTest::TestRRectSimple(random);
    return make_rrect_op(context, std::move(paint), viewMatrix, rrect,
                         GrTest::TestStrokeRec(random));
}

#endif
