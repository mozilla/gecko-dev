/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_TEXTURE_H_
#define WEBGL_TEXTURE_H_

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "WebGLFramebufferAttachable.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLTypes.h"

namespace mozilla {
class ErrorResult;
class WebGLContext;
struct FloatOrInt;
struct TexImageSource;

namespace dom {
class Element;
class HTMLVideoElement;
class ImageData;
class ArrayBufferViewOrSharedArrayBufferView;
} // namespace dom

namespace layers {
class Image;
} // namespace layers

namespace webgl {
struct DriverUnpackInfo;
struct FormatUsageInfo;
struct PackingInfo;
class TexUnpackBlob;
} // namespace webgl


bool
DoesTargetMatchDimensions(WebGLContext* webgl, TexImageTarget target, uint8_t dims,
                          const char* funcName);


// NOTE: When this class is switched to new DOM bindings, update the (then-slow)
// WrapObject calls in GetParameter and GetFramebufferAttachmentParameter.
class WebGLTexture final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLTexture>
    , public LinkedListElement<WebGLTexture>
{
    // Friends
    friend class WebGLContext;
    friend class WebGLFramebuffer;

    ////////////////////////////////////
    // Members
public:
    const GLuint mGLName;

protected:
    TexTarget mTarget;

    static const uint8_t kMaxFaceCount = 6;
    uint8_t mFaceCount; // 6 for cube maps, 1 otherwise.

    TexMinFilter mMinFilter;
    TexMagFilter mMagFilter;
    TexWrap mWrapS, mWrapT;

    bool mImmutable; // Set by texStorage*
    uint8_t mImmutableLevelCount;

    uint32_t mBaseMipmapLevel; // Set by texParameter (defaults to 0)
    uint32_t mMaxMipmapLevel;  // Set by texParameter (defaults to 1000)
    // You almost certainly don't want to query mMaxMipmapLevel.
    // You almost certainly want MaxEffectiveMipmapLevel().

    GLenum mTexCompareMode;

    // Resolvable optimizations:
    bool mIsResolved;
    FakeBlackType mResolved_FakeBlack;
    const GLint* mResolved_Swizzle; // nullptr means 'default swizzle'.

public:
    class ImageInfo;

    // numLevels = log2(size) + 1
    // numLevels(16k) = log2(16k) + 1 = 14 + 1 = 15
    // numLevels(1M) = log2(1M) + 1 = 19.9 + 1 ~= 21
    // Or we can just max this out to 31, which is the number of unsigned bits in GLsizei.
    static const uint8_t kMaxLevelCount = 31;

    // And in turn, it needs these forwards:
protected:
    // We need to forward these.
    void SetImageInfo(ImageInfo* target, const ImageInfo& newInfo);
    void SetImageInfosAtLevel(uint32_t level, const ImageInfo& newInfo);

public:
    // We store information about the various images that are part of this
    // texture. (cubemap faces, mipmap levels)
    class ImageInfo
    {
        friend void WebGLTexture::SetImageInfo(ImageInfo* target,
                                               const ImageInfo& newInfo);
        friend void WebGLTexture::SetImageInfosAtLevel(uint32_t level,
                                                       const ImageInfo& newInfo);

    public:
        static const ImageInfo kUndefined;

        // This is the "effective internal format" of the texture, an official
        // OpenGL spec concept, see OpenGL ES 3.0.3 spec, section 3.8.3, page
        // 126 and below.
        const webgl::FormatUsageInfo* const mFormat;

        const uint32_t mWidth;
        const uint32_t mHeight;
        const uint32_t mDepth;

    protected:
        bool mIsDataInitialized;

        std::set<WebGLFBAttachPoint*> mAttachPoints;

    public:
        ImageInfo()
            : mFormat(LOCAL_GL_NONE)
            , mWidth(0)
            , mHeight(0)
            , mDepth(0)
            , mIsDataInitialized(false)
        { }

        ImageInfo(const webgl::FormatUsageInfo* format, uint32_t width, uint32_t height,
                  uint32_t depth, bool isDataInitialized)
            : mFormat(format)
            , mWidth(width)
            , mHeight(height)
            , mDepth(depth)
            , mIsDataInitialized(isDataInitialized)
        {
            MOZ_ASSERT(mFormat);
        }

        void Clear();

        ~ImageInfo() {
            if (!IsDefined())
                Clear();
        }

    protected:
        ImageInfo& operator =(const ImageInfo& a);

    public:
        uint32_t PossibleMipmapLevels() const {
            // GLES 3.0.4, 3.8 - Mipmapping: `floor(log2(largest_of_dims)) + 1`
            const uint32_t largest = std::max(std::max(mWidth, mHeight), mDepth);
            MOZ_ASSERT(largest != 0);
            return FloorLog2Size(largest) + 1;
        }

        bool IsPowerOfTwo() const;

        void AddAttachPoint(WebGLFBAttachPoint* attachPoint);
        void RemoveAttachPoint(WebGLFBAttachPoint* attachPoint);
        void OnRespecify() const;

        size_t MemoryUsage() const;

        bool IsDefined() const {
            if (mFormat == LOCAL_GL_NONE) {
                MOZ_ASSERT(!mWidth && !mHeight && !mDepth);
                return false;
            }

            return true;
        }

        bool IsDataInitialized() const { return mIsDataInitialized; }

        void SetIsDataInitialized(bool isDataInitialized, WebGLTexture* tex);
    };

    ImageInfo mImageInfoArr[kMaxLevelCount * kMaxFaceCount];

    ////////////////////////////////////
public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLTexture)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLTexture)

    WebGLTexture(WebGLContext* webgl, GLuint tex);

    void Delete();

    bool HasEverBeenBound() const { return mTarget != LOCAL_GL_NONE; }
    TexTarget Target() const { return mTarget; }

    WebGLContext* GetParentObject() const {
        return mContext;
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

protected:
    ~WebGLTexture() {
        DeleteOnce();
    }

public:
    ////////////////////////////////////
    // GL calls
    bool BindTexture(TexTarget texTarget);
    void GenerateMipmap(TexTarget texTarget);
    JS::Value GetTexParameter(TexTarget texTarget, GLenum pname);
    bool IsTexture() const;
    void TexParameter(TexTarget texTarget, GLenum pname, const FloatOrInt& param);

    ////////////////////////////////////
    // WebGLTextureUpload.cpp

protected:
    void TexOrSubImageBlob(bool isSubImage, const char* funcName, TexImageTarget target,
                           GLint level, GLenum internalFormat, GLint xOffset,
                           GLint yOffset, GLint zOffset,
                           const webgl::PackingInfo& pi,
                           const webgl::TexUnpackBlob* blob);

    bool ValidateTexImageSpecification(const char* funcName, TexImageTarget target,
                                       GLint level, uint32_t width, uint32_t height,
                                       uint32_t depth,
                                       WebGLTexture::ImageInfo** const out_imageInfo);
    bool ValidateTexImageSelection(const char* funcName, TexImageTarget target,
                                   GLint level, GLint xOffset, GLint yOffset,
                                   GLint zOffset, uint32_t width, uint32_t height,
                                   uint32_t depth,
                                   WebGLTexture::ImageInfo** const out_imageInfo);
    bool ValidateCopyTexImageForFeedback(const char* funcName, uint32_t level, GLint layer = 0) const;

    bool ValidateUnpack(const char* funcName, const webgl::TexUnpackBlob* blob,
                        bool isFunc3D, const webgl::PackingInfo& srcPI) const;
public:
    void TexStorage(const char* funcName, TexTarget target, GLsizei levels,
                    GLenum sizedFormat, GLsizei width, GLsizei height, GLsizei depth);
    void TexImage(const char* funcName, TexImageTarget target, GLint level,
                  GLenum internalFormat, GLsizei width, GLsizei height, GLsizei depth,
                  GLint border, const webgl::PackingInfo& pi, const TexImageSource& src);
    void TexSubImage(const char* funcName, TexImageTarget target, GLint level,
                     GLint xOffset, GLint yOffset, GLint zOffset, GLsizei width,
                     GLsizei height, GLsizei depth, const webgl::PackingInfo& pi,
                     const TexImageSource& src);
protected:
    void TexImage(const char* funcName, TexImageTarget target, GLint level,
                  GLenum internalFormat, const webgl::PackingInfo& pi,
                  const webgl::TexUnpackBlob* blob);
    void TexSubImage(const char* funcName, TexImageTarget target, GLint level,
                     GLint xOffset, GLint yOffset, GLint zOffset,
                     const webgl::PackingInfo& pi, const webgl::TexUnpackBlob* blob);
public:
    void CompressedTexImage(const char* funcName, TexImageTarget target, GLint level,
                            GLenum internalFormat, GLsizei width, GLsizei height,
                            GLsizei depth, GLint border, const TexImageSource& src);
    void CompressedTexSubImage(const char* funcName, TexImageTarget target, GLint level,
                               GLint xOffset, GLint yOffset, GLint zOffset, GLsizei width,
                               GLsizei height, GLsizei depth, GLenum sizedUnpackFormat,
                               const TexImageSource& src);

    void CopyTexImage2D(TexImageTarget target, GLint level, GLenum internalFormat,
                        GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    void CopyTexSubImage(const char* funcName, TexImageTarget target, GLint level,
                         GLint xOffset, GLint yOffset, GLint zOffset, GLint x, GLint y,
                         GLsizei width, GLsizei height);

    ////////////////////////////////////

protected:
    void ClampLevelBaseAndMax();

    void PopulateMipChain(uint32_t baseLevel, uint32_t maxLevel);

    bool MaxEffectiveMipmapLevel(uint32_t texUnit, uint32_t* const out) const;

    static uint8_t FaceForTarget(TexImageTarget texImageTarget) {
        GLenum rawTexImageTarget = texImageTarget.get();
        switch (rawTexImageTarget) {
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            return rawTexImageTarget - LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;

        default:
            return 0;
        }
    }

    ImageInfo& ImageInfoAtFace(uint8_t face, uint32_t level) {
        MOZ_ASSERT(face < mFaceCount);
        MOZ_ASSERT(level < kMaxLevelCount);
        size_t pos = (level * mFaceCount) + face;
        return mImageInfoArr[pos];
    }

    const ImageInfo& ImageInfoAtFace(uint8_t face, uint32_t level) const {
        return const_cast<WebGLTexture*>(this)->ImageInfoAtFace(face, level);
    }

public:
    ImageInfo& ImageInfoAt(TexImageTarget texImageTarget, GLint level) {
        auto face = FaceForTarget(texImageTarget);
        return ImageInfoAtFace(face, level);
    }

    const ImageInfo& ImageInfoAt(TexImageTarget texImageTarget, GLint level) const {
        return const_cast<WebGLTexture*>(this)->ImageInfoAt(texImageTarget, level);
    }

    void SetImageInfoAt(TexImageTarget texImageTarget, GLint level,
                        const ImageInfo& val)
    {
        ImageInfo* target = &ImageInfoAt(texImageTarget, level);
        SetImageInfo(target, val);
    }

    const ImageInfo& BaseImageInfo() const {
        if (mBaseMipmapLevel >= kMaxLevelCount)
            return ImageInfo::kUndefined;

        return ImageInfoAtFace(0, mBaseMipmapLevel);
    }

    size_t MemoryUsage() const;

    bool InitializeImageData(const char* funcName, TexImageTarget target, uint32_t level);
protected:
    bool EnsureImageDataInitialized(const char* funcName, TexImageTarget target,
                                    uint32_t level);
    bool EnsureLevelInitialized(const char* funcName, uint32_t level);

    bool CheckFloatTextureFilterParams() const {
        // Without OES_texture_float_linear, only NEAREST and
        // NEAREST_MIMPAMP_NEAREST are supported.
        return mMagFilter == LOCAL_GL_NEAREST &&
               (mMinFilter == LOCAL_GL_NEAREST ||
                mMinFilter == LOCAL_GL_NEAREST_MIPMAP_NEAREST);
    }

    bool AreBothWrapModesClampToEdge() const {
        return mWrapS == LOCAL_GL_CLAMP_TO_EDGE &&
               mWrapT == LOCAL_GL_CLAMP_TO_EDGE;
    }

public:
    bool DoesMinFilterRequireMipmap() const {
        return !(mMinFilter == LOCAL_GL_NEAREST ||
                 mMinFilter == LOCAL_GL_LINEAR);
    }

    void SetGeneratedMipmap();

    void SetCustomMipmap();

    bool AreAllLevel0ImageInfosEqual() const;

    bool IsMipmapComplete(const char* funcName, uint32_t texUnit,
                          bool* const out_initFailed);

    bool IsCubeComplete() const;

    bool IsComplete(const char* funcName, uint32_t texUnit, const char** const out_reason,
                    bool* const out_initFailed);

    bool IsMipmapCubeComplete() const;

    bool IsCubeMap() const { return (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP); }

    // Resolve cache optimizations
protected:
    bool GetFakeBlackType(const char* funcName, uint32_t texUnit,
                          FakeBlackType* const out_fakeBlack);
public:
    bool IsFeedback(WebGLContext* webgl, const char* funcName, uint32_t texUnit,
                    const std::vector<const WebGLFBAttachPoint*>& fbAttachments) const;

    bool ResolveForDraw(const char* funcName, uint32_t texUnit,
                        FakeBlackType* const out_fakeBlack);

    void InvalidateResolveCache() { mIsResolved = false; }
};

inline TexImageTarget
TexImageTargetForTargetAndFace(TexTarget target, uint8_t face)
{
    switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_3D:
        MOZ_ASSERT(face == 0);
        return target.get();
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        MOZ_ASSERT(face < 6);
        return LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
    default:
        MOZ_CRASH("GFX: TexImageTargetForTargetAndFace");
    }
}

already_AddRefed<mozilla::layers::Image>
ImageFromVideo(dom::HTMLVideoElement* elem);

bool
IsTarget3D(TexImageTarget target);

GLenum
DoTexImage(gl::GLContext* gl, TexImageTarget target, GLint level,
           const webgl::DriverUnpackInfo* dui, GLsizei width, GLsizei height,
           GLsizei depth, const void* data);
GLenum
DoTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level, GLint xOffset,
              GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
              GLsizei depth, const webgl::PackingInfo& pi, const void* data);
GLenum
DoCompressedTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level,
                        GLint xOffset, GLint yOffset, GLint zOffset, GLsizei width,
                        GLsizei height, GLsizei depth, GLenum sizedUnpackFormat,
                        GLsizei dataSize, const void* data);

} // namespace mozilla

#endif // WEBGL_TEXTURE_H_
