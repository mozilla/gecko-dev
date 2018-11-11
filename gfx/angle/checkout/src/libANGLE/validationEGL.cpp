//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// validationEGL.cpp: Validation functions for generic EGL entry point parameters

#include "libANGLE/validationEGL.h"

#include "common/utilities.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Device.h"
#include "libANGLE/Display.h"
#include "libANGLE/Image.h"
#include "libANGLE/Stream.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Thread.h"
#include "libANGLE/formatutils.h"

#include <EGL/eglext.h>

namespace egl
{
namespace
{
size_t GetMaximumMipLevel(const gl::Context *context, gl::TextureType type)
{
    const gl::Caps &caps = context->getCaps();

    size_t maxDimension = 0;
    switch (type)
    {
        case gl::TextureType::_2D:
        case gl::TextureType::_2DArray:
        case gl::TextureType::_2DMultisample:
            maxDimension = caps.max2DTextureSize;
            break;
        case gl::TextureType::Rectangle:
            maxDimension = caps.maxRectangleTextureSize;
            break;
        case gl::TextureType::CubeMap:
            maxDimension = caps.maxCubeMapTextureSize;
            break;
        case gl::TextureType::_3D:
            maxDimension = caps.max3DTextureSize;
            break;

        default:
            UNREACHABLE();
    }

    return gl::log2(static_cast<int>(maxDimension));
}

bool TextureHasNonZeroMipLevelsSpecified(const gl::Context *context, const gl::Texture *texture)
{
    size_t maxMip = GetMaximumMipLevel(context, texture->getType());
    for (size_t level = 1; level < maxMip; level++)
    {
        if (texture->getType() == gl::TextureType::CubeMap)
        {
            for (gl::TextureTarget face : gl::AllCubeFaceTextureTargets())
            {
                if (texture->getFormat(face, level).valid())
                {
                    return true;
                }
            }
        }
        else
        {
            if (texture->getFormat(gl::NonCubeTextureTypeToTarget(texture->getType()), level)
                    .valid())
            {
                return true;
            }
        }
    }

    return false;
}

bool CubeTextureHasUnspecifiedLevel0Face(const gl::Texture *texture)
{
    ASSERT(texture->getType() == gl::TextureType::CubeMap);
    for (gl::TextureTarget face : gl::AllCubeFaceTextureTargets())
    {
        if (!texture->getFormat(face, 0).valid())
        {
            return true;
        }
    }

    return false;
}

Error ValidateStreamAttribute(const EGLAttrib attribute,
                              const EGLAttrib value,
                              const DisplayExtensions &extensions)
{
    switch (attribute)
    {
        case EGL_STREAM_STATE_KHR:
        case EGL_PRODUCER_FRAME_KHR:
        case EGL_CONSUMER_FRAME_KHR:
            return EglBadAccess() << "Attempt to initialize readonly parameter";
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            // Technically not in spec but a latency < 0 makes no sense so we check it
            if (value < 0)
            {
                return EglBadParameter() << "Latency must be positive";
            }
            break;
        case EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR:
            if (!extensions.streamConsumerGLTexture)
            {
                return EglBadAttribute() << "Consumer GL extension not enabled";
            }
            // Again not in spec but it should be positive anyways
            if (value < 0)
            {
                return EglBadParameter() << "Timeout must be positive";
            }
            break;
        default:
            return EglBadAttribute() << "Invalid stream attribute";
    }
    return NoError();
}

Error ValidateCreateImageKHRMipLevelCommon(gl::Context *context,
                                           const gl::Texture *texture,
                                           EGLAttrib level)
{
    // Note that the spec EGL_KHR_create_image spec does not explicitly specify an error
    // when the level is outside the base/max level range, but it does mention that the
    // level "must be a part of the complete texture object <buffer>". It can be argued
    // that out-of-range levels are not a part of the complete texture.
    const GLuint effectiveBaseLevel = texture->getTextureState().getEffectiveBaseLevel();
    if (level > 0 &&
        (!texture->isMipmapComplete() || static_cast<GLuint>(level) < effectiveBaseLevel ||
         static_cast<GLuint>(level) > texture->getTextureState().getMipmapMaxLevel()))
    {
        return EglBadParameter() << "texture must be complete if level is non-zero.";
    }

    if (level == 0 && !texture->isMipmapComplete() &&
        TextureHasNonZeroMipLevelsSpecified(context, texture))
    {
        return EglBadParameter() << "if level is zero and the texture is incomplete, it must "
                                    "have no mip levels specified except zero.";
    }

    return NoError();
}

Error ValidateConfigAttribute(const Display *display, EGLAttrib attribute)
{
    switch (attribute)
    {
        case EGL_BUFFER_SIZE:
        case EGL_ALPHA_SIZE:
        case EGL_BLUE_SIZE:
        case EGL_GREEN_SIZE:
        case EGL_RED_SIZE:
        case EGL_DEPTH_SIZE:
        case EGL_STENCIL_SIZE:
        case EGL_CONFIG_CAVEAT:
        case EGL_CONFIG_ID:
        case EGL_LEVEL:
        case EGL_NATIVE_RENDERABLE:
        case EGL_NATIVE_VISUAL_ID:
        case EGL_NATIVE_VISUAL_TYPE:
        case EGL_SAMPLES:
        case EGL_SAMPLE_BUFFERS:
        case EGL_SURFACE_TYPE:
        case EGL_TRANSPARENT_TYPE:
        case EGL_TRANSPARENT_BLUE_VALUE:
        case EGL_TRANSPARENT_GREEN_VALUE:
        case EGL_TRANSPARENT_RED_VALUE:
        case EGL_BIND_TO_TEXTURE_RGB:
        case EGL_BIND_TO_TEXTURE_RGBA:
        case EGL_MIN_SWAP_INTERVAL:
        case EGL_MAX_SWAP_INTERVAL:
        case EGL_LUMINANCE_SIZE:
        case EGL_ALPHA_MASK_SIZE:
        case EGL_COLOR_BUFFER_TYPE:
        case EGL_RENDERABLE_TYPE:
        case EGL_MATCH_NATIVE_PIXMAP:
        case EGL_CONFORMANT:
        case EGL_MAX_PBUFFER_WIDTH:
        case EGL_MAX_PBUFFER_HEIGHT:
        case EGL_MAX_PBUFFER_PIXELS:
            break;

        case EGL_OPTIMAL_SURFACE_ORIENTATION_ANGLE:
            if (!display->getExtensions().surfaceOrientation)
            {
                return EglBadAttribute() << "EGL_ANGLE_surface_orientation is not enabled.";
            }
            break;

        case EGL_COLOR_COMPONENT_TYPE_EXT:
            if (!display->getExtensions().pixelFormatFloat)
            {
                return EglBadAttribute() << "EGL_EXT_pixel_format_float is not enabled.";
            }
            break;

        default:
            return EglBadAttribute() << "Unknown attribute.";
    }

    return NoError();
}

Error ValidateConfigAttributes(const Display *display, const AttributeMap &attributes)
{
    for (const auto &attrib : attributes)
    {
        ANGLE_TRY(ValidateConfigAttribute(display, attrib.first));
    }

    return NoError();
}

Error ValidatePlatformType(const ClientExtensions &clientExtensions, EGLAttrib platformType)
{
    switch (platformType)
    {
        case EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE:
            break;

        case EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE:
        case EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE:
            if (!clientExtensions.platformANGLED3D)
            {
                return EglBadAttribute() << "Direct3D platform is unsupported.";
            }
            break;

        case EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE:
        case EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE:
            if (!clientExtensions.platformANGLEOpenGL)
            {
                return EglBadAttribute() << "OpenGL platform is unsupported.";
            }
            break;

        case EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE:
            if (!clientExtensions.platformANGLENULL)
            {
                return EglBadAttribute() << "Display type EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE "
                                            "requires EGL_ANGLE_platform_angle_null.";
            }
            break;

        case EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE:
            if (!clientExtensions.platformANGLEVulkan)
            {
                return EglBadAttribute() << "Vulkan platform is unsupported.";
            }
            break;

        default:
            return EglBadAttribute() << "Unknown platform type.";
    }

    return NoError();
}

Error ValidateGetPlatformDisplayCommon(EGLenum platform,
                                       void *native_display,
                                       const AttributeMap &attribMap)
{
    const ClientExtensions &clientExtensions = Display::GetClientExtensions();

    switch (platform)
    {
        case EGL_PLATFORM_ANGLE_ANGLE:
            if (!clientExtensions.platformANGLE)
            {
                return EglBadParameter() << "Platform ANGLE extension is not active";
            }
            break;
        case EGL_PLATFORM_DEVICE_EXT:
            if (!clientExtensions.platformDevice)
            {
                return EglBadParameter() << "Platform Device extension is not active";
            }
            break;
        default:
            return EglBadConfig() << "Bad platform type.";
    }

    if (platform == EGL_PLATFORM_ANGLE_ANGLE)
    {
        EGLAttrib platformType       = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
        bool enableAutoTrimSpecified = false;
        bool presentPathSpecified    = false;

        Optional<EGLAttrib> majorVersion;
        Optional<EGLAttrib> minorVersion;
        Optional<EGLAttrib> deviceType;
        Optional<EGLAttrib> eglHandle;

        for (const auto &curAttrib : attribMap)
        {
            const EGLAttrib value = curAttrib.second;

            switch (curAttrib.first)
            {
                case EGL_PLATFORM_ANGLE_TYPE_ANGLE:
                {
                    ANGLE_TRY(ValidatePlatformType(clientExtensions, value));
                    platformType = value;
                    break;
                }

                case EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE:
                    if (value != EGL_DONT_CARE)
                    {
                        majorVersion = value;
                    }
                    break;

                case EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE:
                    if (value != EGL_DONT_CARE)
                    {
                        minorVersion = value;
                    }
                    break;

                case EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE:
                    switch (value)
                    {
                        case EGL_TRUE:
                        case EGL_FALSE:
                            break;
                        default:
                            return EglBadAttribute() << "Invalid automatic trim attribute";
                    }
                    enableAutoTrimSpecified = true;
                    break;

                case EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE:
                    if (!clientExtensions.experimentalPresentPath)
                    {
                        return EglBadAttribute()
                               << "EGL_ANGLE_experimental_present_path extension not active";
                    }

                    switch (value)
                    {
                        case EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE:
                        case EGL_EXPERIMENTAL_PRESENT_PATH_COPY_ANGLE:
                            break;
                        default:
                            return EglBadAttribute()
                                   << "Invalid value for EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE";
                    }
                    presentPathSpecified = true;
                    break;

                case EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE:
                    switch (value)
                    {
                        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE:
                        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE:
                            break;

                        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE:
                        case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE:
                            if (!clientExtensions.platformANGLED3D)
                            {
                                return EglBadAttribute()
                                       << "EGL_ANGLE_platform_angle_d3d is not supported";
                            }
                            break;

                        default:
                            return EglBadAttribute() << "Invalid value for "
                                                        "EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE "
                                                        "attrib";
                    }
                    deviceType = value;
                    break;

                case EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE:
                    if (!clientExtensions.platformANGLE)
                    {
                        return EglBadAttribute() << "EGL_ANGLE_platform_angle extension not active";
                    }
                    if (value != EGL_TRUE && value != EGL_FALSE && value != EGL_DONT_CARE)
                    {
                        return EglBadAttribute() << "EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE "
                                                    "must be EGL_TRUE, EGL_FALSE, or "
                                                    "EGL_DONT_CARE.";
                    }
                    break;

                case EGL_PLATFORM_ANGLE_EGL_HANDLE_ANGLE:
                    if (value != EGL_DONT_CARE)
                    {
                        eglHandle = value;
                    }
                    break;

                case EGL_PLATFORM_ANGLE_CONTEXT_VIRTUALIZATION_ANGLE:
                    if (!clientExtensions.platformANGLEContextVirtualization)
                    {
                        return EglBadAttribute() << "EGL_ANGLE_platform_angle_context_"
                                                    "virtualization extension not active";
                    }

                    switch (value)
                    {
                        case EGL_DONT_CARE:
                        case EGL_FALSE:
                        case EGL_TRUE:
                            break;

                        default:
                            return EglBadAttribute() << "Invalid value for "
                                                        "EGL_PLATFORM_ANGLE_CONTEXT_VIRTUALIZATION_"
                                                        "ANGLE attrib";
                    }
                    break;

                default:
                    break;
            }
        }

        if (!majorVersion.valid() && minorVersion.valid())
        {
            return EglBadAttribute()
                   << "Must specify major version if you specify a minor version.";
        }

        if (deviceType == EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE &&
            platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            return EglBadAttribute() << "EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE requires a "
                                        "device type of EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE.";
        }

        if (enableAutoTrimSpecified && platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            return EglBadAttribute() << "EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE "
                                        "requires a device type of "
                                        "EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE.";
        }

        if (presentPathSpecified && platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
        {
            return EglBadAttribute() << "EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE requires a "
                                        "device type of EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE.";
        }

        if (deviceType.valid())
        {
            switch (deviceType.value())
            {
                case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE:
                case EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE:
                    if (platformType != EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE &&
                        platformType != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE)
                    {
                        return EglBadAttribute()
                               << "This device type requires a "
                                  "platform type of EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE or "
                                  "EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE.";
                    }
                    break;
                default:
                    break;
            }
        }

        if (platformType == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE)
        {
            if ((majorVersion.valid() && majorVersion.value() != 1) ||
                (minorVersion.valid() && minorVersion.value() != 0))
            {
                return EglBadAttribute() << "EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE currently "
                                            "only supports Vulkan 1.0.";
            }
        }

        if (eglHandle.valid() && platformType != EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE &&
            platformType != EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE)
        {
            return EglBadAttribute() << "EGL_PLATFORM_ANGLE_EGL_HANDLE_ANGLE requires a "
                                        "device type of EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE.";
        }
    }
    else if (platform == EGL_PLATFORM_DEVICE_EXT)
    {
        Device *eglDevice = static_cast<Device *>(native_display);
        if (eglDevice == nullptr || !Device::IsValidDevice(eglDevice))
        {
            return EglBadAttribute() << "native_display should be a valid EGL device if "
                                        "platform equals EGL_PLATFORM_DEVICE_EXT";
        }
    }
    else
    {
        UNREACHABLE();
    }

    return NoError();
}

Error ValidateStream(const Display *display, const Stream *stream)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.stream)
    {
        return EglBadAccess() << "Stream extension not active";
    }

    if (stream == EGL_NO_STREAM_KHR || !display->isValidStream(stream))
    {
        return EglBadStream() << "Invalid stream";
    }

    return NoError();
}

Error ValidateLabeledObject(Thread *thread,
                            const Display *display,
                            ObjectType objectType,
                            EGLObjectKHR object,
                            LabeledObject **outLabeledObject)
{
    switch (objectType)
    {
        case ObjectType::Context:
        {
            gl::Context *context = static_cast<gl::Context *>(object);
            ANGLE_TRY(ValidateContext(display, context));
            *outLabeledObject = context;
            break;
        }

        case ObjectType::Display:
        {
            ANGLE_TRY(ValidateDisplay(display));
            if (display != object)
            {
                return EglBadParameter() << "when object type is EGL_OBJECT_DISPLAY_KHR, the "
                                            "object must be the same as the display.";
            }

            *outLabeledObject = static_cast<Display *>(object);
            break;
        }

        case ObjectType::Image:
        {
            Image *image = static_cast<Image *>(object);
            ANGLE_TRY(ValidateImage(display, image));
            *outLabeledObject = image;
            break;
        }

        case ObjectType::Stream:
        {
            Stream *stream = static_cast<Stream *>(object);
            ANGLE_TRY(ValidateStream(display, stream));
            *outLabeledObject = stream;
            break;
        }

        case ObjectType::Surface:
        {
            Surface *surface = static_cast<Surface *>(object);
            ANGLE_TRY(ValidateSurface(display, surface));
            *outLabeledObject = surface;
            break;
        }

        case ObjectType::Sync:
        {
            ANGLE_TRY(ValidateDisplay(display));
            // TODO(geofflang): Implement sync objects. http://anglebug.com/2466
            UNIMPLEMENTED();
            return EglBadDisplay() << "Sync objects are unimplemented.";

            break;
        }

        case ObjectType::Thread:
        {
            *outLabeledObject = thread;
            break;
        }

        default:
            return EglBadParameter() << "unknown object type.";
    }

    return NoError();
}

// This is a common sub-check of Display status that's shared by multiple functions
Error ValidateDisplayPointer(const Display *display)
{
    if (display == EGL_NO_DISPLAY)
    {
        return EglBadDisplay() << "display is EGL_NO_DISPLAY.";
    }

    if (!Display::isValidDisplay(display))
    {
        return EglBadDisplay() << "display is not a valid display.";
    }

    return NoError();
}

}  // namespace

Error ValidateDisplay(const Display *display)
{
    ANGLE_TRY(ValidateDisplayPointer(display));

    if (!display->isInitialized())
    {
        return EglNotInitialized() << "display is not initialized.";
    }

    if (display->isDeviceLost())
    {
        return EglContextLost() << "display had a context loss";
    }

    return NoError();
}

Error ValidateSurface(const Display *display, const Surface *surface)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->isValidSurface(surface))
    {
        return EglBadSurface();
    }

    return NoError();
}

Error ValidateConfig(const Display *display, const Config *config)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->isValidConfig(config))
    {
        return EglBadConfig();
    }

    return NoError();
}

Error ValidateContext(const Display *display, const gl::Context *context)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->isValidContext(context))
    {
        return EglBadContext();
    }

    return NoError();
}

Error ValidateImage(const Display *display, const Image *image)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->isValidImage(image))
    {
        return EglBadParameter() << "image is not valid.";
    }

    return NoError();
}

Error ValidateDevice(const Device *device)
{
    if (device == EGL_NO_DEVICE_EXT)
    {
        return EglBadAccess() << "device is EGL_NO_DEVICE.";
    }

    if (!Device::IsValidDevice(device))
    {
        return EglBadAccess() << "device is not valid.";
    }

    return NoError();
}

const Thread *GetThreadIfValid(const Thread *thread)
{
    // Threads should always be valid
    return thread;
}

const Display *GetDisplayIfValid(const Display *display)
{
    if (ValidateDisplay(display).isError())
    {
        return nullptr;
    }

    return display;
}

const Surface *GetSurfaceIfValid(const Display *display, const Surface *surface)
{
    if (ValidateSurface(display, surface).isError())
    {
        return nullptr;
    }

    return surface;
}

const Image *GetImageIfValid(const Display *display, const Image *image)
{
    if (ValidateImage(display, image).isError())
    {
        return nullptr;
    }

    return image;
}

const Stream *GetStreamIfValid(const Display *display, const Stream *stream)
{
    if (ValidateStream(display, stream).isError())
    {
        return nullptr;
    }

    return stream;
}

const gl::Context *GetContextIfValid(const Display *display, const gl::Context *context)
{
    if (ValidateContext(display, context).isError())
    {
        return nullptr;
    }

    return context;
}

const Device *GetDeviceIfValid(const Device *device)
{
    if (ValidateDevice(device).isError())
    {
        return nullptr;
    }

    return device;
}

LabeledObject *GetLabeledObjectIfValid(Thread *thread,
                                       const Display *display,
                                       ObjectType objectType,
                                       EGLObjectKHR object)
{
    LabeledObject *labeledObject = nullptr;
    if (ValidateLabeledObject(thread, display, objectType, object, &labeledObject).isError())
    {
        return nullptr;
    }

    return labeledObject;
}

Error ValidateInitialize(const Display *display)
{
    return ValidateDisplayPointer(display);
}

Error ValidateTerminate(const Display *display)
{
    return ValidateDisplayPointer(display);
}

Error ValidateCreateContext(Display *display,
                            Config *configuration,
                            gl::Context *shareContext,
                            const AttributeMap &attributes)
{
    ANGLE_TRY(ValidateConfig(display, configuration));

    // Get the requested client version (default is 1) and check it is 2 or 3.
    EGLAttrib clientMajorVersion = 1;
    EGLAttrib clientMinorVersion = 0;
    EGLAttrib contextFlags       = 0;
    bool resetNotification       = false;
    for (AttributeMap::const_iterator attributeIter = attributes.begin();
         attributeIter != attributes.end(); attributeIter++)
    {
        EGLAttrib attribute = attributeIter->first;
        EGLAttrib value     = attributeIter->second;

        switch (attribute)
        {
            case EGL_CONTEXT_CLIENT_VERSION:
                clientMajorVersion = value;
                break;

            case EGL_CONTEXT_MINOR_VERSION:
                clientMinorVersion = value;
                break;

            case EGL_CONTEXT_FLAGS_KHR:
                contextFlags = value;
                break;

            case EGL_CONTEXT_OPENGL_DEBUG:
                break;

            case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR:
                // Only valid for OpenGL (non-ES) contexts
                return EglBadAttribute();

            case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT:
                if (!display->getExtensions().createContextRobustness)
                {
                    return EglBadAttribute();
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute();
                }
                break;

            case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR:
                return EglBadAttribute()
                       << "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR is not"
                       << " valid for GLES with EGL 1.4 and KHR_create_context. Use"
                       << " EXT_create_context_robustness.";
            case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT:
                if (!display->getExtensions().createContextRobustness)
                {
                    return EglBadAttribute();
                }
                if (value == EGL_LOSE_CONTEXT_ON_RESET_EXT)
                {
                    resetNotification = true;
                }
                else if (value != EGL_NO_RESET_NOTIFICATION_EXT)
                {
                    return EglBadAttribute();
                }
                break;

            case EGL_CONTEXT_OPENGL_NO_ERROR_KHR:
                if (!display->getExtensions().createContextNoError)
                {
                    return EglBadAttribute() << "Invalid Context attribute.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "Attribute must be EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE:
                if (!display->getExtensions().createContextWebGLCompatibility)
                {
                    return EglBadAttribute() << "Attribute "
                                                "EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE requires "
                                                "EGL_ANGLE_create_context_webgl_compatibility.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE must be "
                                                "EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM:
                if (!display->getExtensions().createContextBindGeneratesResource)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM requires "
                              "EGL_CHROMIUM_create_context_bind_generates_resource.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM "
                                                "must be EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE:
                if (!display->getExtensions().displayTextureShareGroup)
                {
                    return EglBadAttribute() << "Attribute "
                                                "EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE requires "
                                                "EGL_ANGLE_display_texture_share_group.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE must be "
                                                "EGL_TRUE or EGL_FALSE.";
                }
                if (shareContext &&
                    (shareContext->usingDisplayTextureShareGroup() != (value == EGL_TRUE)))
                {
                    return EglBadAttribute() << "All contexts within a share group must be "
                                                "created with the same value of "
                                                "EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE.";
                }
                break;

            case EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE:
                if (!display->getExtensions().createContextClientArrays)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE requires "
                              "EGL_ANGLE_create_context_client_arrays.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE must "
                                                "be EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE:
                if (!display->getExtensions().programCacheControl)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE "
                              "requires EGL_ANGLE_program_cache_control.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute()
                           << "EGL_CONTEXT_PROGRAM_BINARY_CACHE_ENABLED_ANGLE must "
                              "be EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
                if (!display->getExtensions().robustResourceInitialization)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE "
                              "requires EGL_ANGLE_robust_resource_initialization.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE must be "
                                                "either EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_EXTENSIONS_ENABLED_ANGLE:
                if (!display->getExtensions().createContextExtensionsEnabled)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_EXTENSIONS_ENABLED_ANGLE "
                              "requires EGL_ANGLE_create_context_extensions_enabled.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_EXTENSIONS_ENABLED_ANGLE must be "
                                                "either EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_CONTEXT_PROVOKING_VERTEX_DONT_CARE_MOZ:
                if (!display->getExtensions().provokingVertexDontCare)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_CONTEXT_PROVOKING_VERTEX_DONT_CARE_MOZ "
                              "requires EGL_MOZ_create_context_provoking_vertex_dont_care.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_CONTEXT_PROVOKING_VERTEX_DONT_CARE_MOZ "
                                              "must be either EGL_TRUE or EGL_FALSE.";
                }
                break;

            default:
                return EglBadAttribute() << "Unknown attribute.";
        }
    }

    switch (clientMajorVersion)
    {
        case 1:
            if (clientMinorVersion != 0 && clientMinorVersion != 1)
            {
                return EglBadConfig();
            }
            break;

        case 2:
            if (clientMinorVersion != 0)
            {
                return EglBadConfig();
            }
            break;
        case 3:
            if (clientMinorVersion != 0 && clientMinorVersion != 1)
            {
                return EglBadConfig();
            }
            if (!(configuration->renderableType & EGL_OPENGL_ES3_BIT_KHR))
            {
                return EglBadConfig();
            }
            if (display->getMaxSupportedESVersion() <
                gl::Version(static_cast<GLuint>(clientMajorVersion),
                            static_cast<GLuint>(clientMinorVersion)))
            {
                return EglBadConfig() << "Requested GLES version is not supported.";
            }
            break;
        default:
            return EglBadConfig();
            break;
    }

    // Note: EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR does not apply to ES
    const EGLint validContextFlags =
        (EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR | EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR);
    if ((contextFlags & ~validContextFlags) != 0)
    {
        return EglBadAttribute();
    }

    if (shareContext)
    {
        // Shared context is invalid or is owned by another display
        if (!display->isValidContext(shareContext))
        {
            return EglBadMatch();
        }

        if (shareContext->isResetNotificationEnabled() != resetNotification)
        {
            return EglBadMatch();
        }

        if (shareContext->getClientMajorVersion() != clientMajorVersion ||
            shareContext->getClientMinorVersion() != clientMinorVersion)
        {
            return EglBadContext();
        }
    }

    return NoError();
}

Error ValidateCreateWindowSurface(Display *display,
                                  Config *config,
                                  EGLNativeWindowType window,
                                  const AttributeMap &attributes)
{
    ANGLE_TRY(ValidateConfig(display, config));

    if (!display->isValidNativeWindow(window))
    {
        return EglBadNativeWindow();
    }

    const DisplayExtensions &displayExtensions = display->getExtensions();

    for (AttributeMap::const_iterator attributeIter = attributes.begin();
         attributeIter != attributes.end(); attributeIter++)
    {
        EGLAttrib attribute = attributeIter->first;
        EGLAttrib value     = attributeIter->second;

        switch (attribute)
        {
            case EGL_RENDER_BUFFER:
                switch (value)
                {
                    case EGL_BACK_BUFFER:
                        break;
                    case EGL_SINGLE_BUFFER:
                        return EglBadMatch();  // Rendering directly to front buffer not supported
                    default:
                        return EglBadAttribute();
                }
                break;

            case EGL_POST_SUB_BUFFER_SUPPORTED_NV:
                if (!displayExtensions.postSubBuffer)
                {
                    return EglBadAttribute();
                }
                break;

            case EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE:
                if (!displayExtensions.flexibleSurfaceCompatibility)
                {
                    return EglBadAttribute();
                }
                break;

            case EGL_WIDTH:
            case EGL_HEIGHT:
                if (!displayExtensions.windowFixedSize)
                {
                    return EglBadAttribute();
                }
                if (value < 0)
                {
                    return EglBadParameter();
                }
                break;

            case EGL_FIXED_SIZE_ANGLE:
                if (!displayExtensions.windowFixedSize)
                {
                    return EglBadAttribute();
                }
                break;

            case EGL_SURFACE_ORIENTATION_ANGLE:
                if (!displayExtensions.surfaceOrientation)
                {
                    return EglBadAttribute() << "EGL_ANGLE_surface_orientation is not enabled.";
                }
                break;

            case EGL_VG_COLORSPACE:
                return EglBadMatch();

            case EGL_VG_ALPHA_FORMAT:
                return EglBadMatch();

            case EGL_DIRECT_COMPOSITION_ANGLE:
                if (!displayExtensions.directComposition)
                {
                    return EglBadAttribute();
                }
                break;

            case EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
                if (!display->getExtensions().robustResourceInitialization)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE "
                              "requires EGL_ANGLE_robust_resource_initialization.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE must be "
                                                "either EGL_TRUE or EGL_FALSE.";
                }
                break;

            default:
                return EglBadAttribute();
        }
    }

    if (Display::hasExistingWindowSurface(window))
    {
        return EglBadAlloc();
    }

    return NoError();
}

Error ValidateCreatePbufferSurface(Display *display, Config *config, const AttributeMap &attributes)
{
    ANGLE_TRY(ValidateConfig(display, config));

    const DisplayExtensions &displayExtensions = display->getExtensions();

    for (AttributeMap::const_iterator attributeIter = attributes.begin();
         attributeIter != attributes.end(); attributeIter++)
    {
        EGLAttrib attribute = attributeIter->first;
        EGLAttrib value     = attributeIter->second;

        switch (attribute)
        {
            case EGL_WIDTH:
            case EGL_HEIGHT:
                if (value < 0)
                {
                    return EglBadParameter();
                }
                break;

            case EGL_LARGEST_PBUFFER:
                break;

            case EGL_TEXTURE_FORMAT:
                switch (value)
                {
                    case EGL_NO_TEXTURE:
                    case EGL_TEXTURE_RGB:
                    case EGL_TEXTURE_RGBA:
                        break;
                    default:
                        return EglBadAttribute();
                }
                break;

            case EGL_TEXTURE_TARGET:
                switch (value)
                {
                    case EGL_NO_TEXTURE:
                    case EGL_TEXTURE_2D:
                        break;
                    default:
                        return EglBadAttribute();
                }
                break;

            case EGL_MIPMAP_TEXTURE:
                break;

            case EGL_VG_COLORSPACE:
                break;

            case EGL_VG_ALPHA_FORMAT:
                break;

            case EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE:
                if (!displayExtensions.flexibleSurfaceCompatibility)
                {
                    return EglBadAttribute()
                           << "EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE cannot be used "
                              "without EGL_ANGLE_flexible_surface_compatibility support.";
                }
                break;

            case EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
                if (!display->getExtensions().robustResourceInitialization)
                {
                    return EglBadAttribute()
                           << "Attribute EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE "
                              "requires EGL_ANGLE_robust_resource_initialization.";
                }
                if (value != EGL_TRUE && value != EGL_FALSE)
                {
                    return EglBadAttribute() << "EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE must be "
                                                "either EGL_TRUE or EGL_FALSE.";
                }
                break;

            default:
                return EglBadAttribute();
        }
    }

    if (!(config->surfaceType & EGL_PBUFFER_BIT))
    {
        return EglBadMatch();
    }

    const Caps &caps = display->getCaps();

    EGLAttrib textureFormat = attributes.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE);
    EGLAttrib textureTarget = attributes.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE);

    if ((textureFormat != EGL_NO_TEXTURE && textureTarget == EGL_NO_TEXTURE) ||
        (textureFormat == EGL_NO_TEXTURE && textureTarget != EGL_NO_TEXTURE))
    {
        return EglBadMatch();
    }

    if ((textureFormat == EGL_TEXTURE_RGB && config->bindToTextureRGB != EGL_TRUE) ||
        (textureFormat == EGL_TEXTURE_RGBA && config->bindToTextureRGBA != EGL_TRUE))
    {
        return EglBadAttribute();
    }

    EGLint width  = static_cast<EGLint>(attributes.get(EGL_WIDTH, 0));
    EGLint height = static_cast<EGLint>(attributes.get(EGL_HEIGHT, 0));
    if (textureFormat != EGL_NO_TEXTURE && !caps.textureNPOT &&
        (!gl::isPow2(width) || !gl::isPow2(height)))
    {
        return EglBadMatch();
    }

    return NoError();
}

Error ValidateCreatePbufferFromClientBuffer(Display *display,
                                            EGLenum buftype,
                                            EGLClientBuffer buffer,
                                            Config *config,
                                            const AttributeMap &attributes)
{
    ANGLE_TRY(ValidateConfig(display, config));

    const DisplayExtensions &displayExtensions = display->getExtensions();

    switch (buftype)
    {
        case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE:
            if (!displayExtensions.d3dShareHandleClientBuffer)
            {
                return EglBadParameter();
            }
            if (buffer == nullptr)
            {
                return EglBadParameter();
            }
            break;

        case EGL_D3D_TEXTURE_ANGLE:
            if (!displayExtensions.d3dTextureClientBuffer)
            {
                return EglBadParameter();
            }
            if (buffer == nullptr)
            {
                return EglBadParameter();
            }
            break;

        case EGL_IOSURFACE_ANGLE:
            if (!displayExtensions.iosurfaceClientBuffer)
            {
                return EglBadParameter() << "<buftype> EGL_IOSURFACE_ANGLE requires the "
                                            "EGL_ANGLE_iosurface_client_buffer extension.";
            }
            if (buffer == nullptr)
            {
                return EglBadParameter() << "<buffer> must be non null";
            }
            break;

        default:
            return EglBadParameter();
    }

    for (AttributeMap::const_iterator attributeIter = attributes.begin();
         attributeIter != attributes.end(); attributeIter++)
    {
        EGLAttrib attribute = attributeIter->first;
        EGLAttrib value     = attributeIter->second;

        switch (attribute)
        {
            case EGL_WIDTH:
            case EGL_HEIGHT:
                if (buftype != EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE &&
                    buftype != EGL_D3D_TEXTURE_ANGLE && buftype != EGL_IOSURFACE_ANGLE)
                {
                    return EglBadParameter()
                           << "Width and Height are not supported for thie <buftype>";
                }
                if (value < 0)
                {
                    return EglBadParameter() << "Width and Height must be positive";
                }
                break;

            case EGL_TEXTURE_FORMAT:
                switch (value)
                {
                    case EGL_NO_TEXTURE:
                    case EGL_TEXTURE_RGB:
                    case EGL_TEXTURE_RGBA:
                        break;
                    default:
                        return EglBadAttribute() << "Invalid value for EGL_TEXTURE_FORMAT";
                }
                break;

            case EGL_TEXTURE_TARGET:
                switch (value)
                {
                    case EGL_NO_TEXTURE:
                    case EGL_TEXTURE_2D:
                        break;
                    case EGL_TEXTURE_RECTANGLE_ANGLE:
                        if (buftype != EGL_IOSURFACE_ANGLE)
                        {
                            return EglBadParameter()
                                   << "<buftype> doesn't support rectangle texture targets";
                        }
                        break;

                    default:
                        return EglBadAttribute() << "Invalid value for EGL_TEXTURE_TARGET";
                }
                break;

            case EGL_MIPMAP_TEXTURE:
                break;

            case EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE:
                if (!displayExtensions.flexibleSurfaceCompatibility)
                {
                    return EglBadAttribute()
                           << "EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE cannot be used "
                              "without EGL_ANGLE_flexible_surface_compatibility support.";
                }
                break;

            case EGL_IOSURFACE_PLANE_ANGLE:
                if (buftype != EGL_IOSURFACE_ANGLE)
                {
                    return EglBadAttribute() << "<buftype> doesn't support iosurface plane";
                }
                break;

            case EGL_TEXTURE_TYPE_ANGLE:
                if (buftype != EGL_IOSURFACE_ANGLE)
                {
                    return EglBadAttribute() << "<buftype> doesn't support texture type";
                }
                break;

            case EGL_TEXTURE_INTERNAL_FORMAT_ANGLE:
                if (buftype != EGL_IOSURFACE_ANGLE)
                {
                    return EglBadAttribute() << "<buftype> doesn't support texture internal format";
                }
                break;
            case EGL_GL_COLORSPACE:
                if (buftype != EGL_D3D_TEXTURE_ANGLE)
                {
                    return EglBadAttribute() << "<buftype> doesn't support setting GL colorspace";
                }
                break;
            default:
                return EglBadAttribute();
        }
    }

    EGLAttrib colorspace = attributes.get(EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR);
    if (colorspace != EGL_GL_COLORSPACE_LINEAR && colorspace != EGL_GL_COLORSPACE_SRGB)
    {
        return EglBadAttribute() << "invalid GL colorspace";
    }

    if (!(config->surfaceType & EGL_PBUFFER_BIT))
    {
        return EglBadMatch();
    }

    EGLAttrib textureFormat = attributes.get(EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE);
    EGLAttrib textureTarget = attributes.get(EGL_TEXTURE_TARGET, EGL_NO_TEXTURE);
    if ((textureFormat != EGL_NO_TEXTURE && textureTarget == EGL_NO_TEXTURE) ||
        (textureFormat == EGL_NO_TEXTURE && textureTarget != EGL_NO_TEXTURE))
    {
        return EglBadMatch();
    }
    if ((textureFormat == EGL_TEXTURE_RGB && config->bindToTextureRGB != EGL_TRUE) ||
        (textureFormat == EGL_TEXTURE_RGBA && config->bindToTextureRGBA != EGL_TRUE))
    {
        // TODO(cwallez@chromium.org): For IOSurface pbuffers we require that EGL_TEXTURE_RGBA is
        // set so that eglBindTexImage works. Normally this is only allowed if the config exposes
        // the bindToTextureRGB/RGBA flag. This issue is that enabling this flags means that
        // eglBindTexImage should also work for regular pbuffers which isn't implemented on macOS.
        // Instead of adding the flag we special case the check here to be ignored for IOSurfaces.
        // The TODO is to find a proper solution for this, maybe by implementing eglBindTexImage on
        // OSX?
        if (buftype != EGL_IOSURFACE_ANGLE)
        {
            return EglBadAttribute();
        }
    }

    if (buftype == EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE)
    {
        EGLint width  = static_cast<EGLint>(attributes.get(EGL_WIDTH, 0));
        EGLint height = static_cast<EGLint>(attributes.get(EGL_HEIGHT, 0));

        if (width == 0 || height == 0)
        {
            return EglBadAttribute();
        }

        const Caps &caps = display->getCaps();
        if (textureFormat != EGL_NO_TEXTURE && !caps.textureNPOT &&
            (!gl::isPow2(width) || !gl::isPow2(height)))
        {
            return EglBadMatch();
        }
    }

    if (buftype == EGL_IOSURFACE_ANGLE)
    {
        if (textureTarget != EGL_TEXTURE_RECTANGLE_ANGLE)
        {
            return EglBadAttribute() << "EGL_IOSURFACE requires the EGL_TEXTURE_RECTANGLE target";
        }

        if (textureFormat != EGL_TEXTURE_RGBA)
        {
            return EglBadAttribute() << "EGL_IOSURFACE requires the EGL_TEXTURE_RGBA format";
        }

        if (!attributes.contains(EGL_WIDTH) || !attributes.contains(EGL_HEIGHT) ||
            !attributes.contains(EGL_TEXTURE_FORMAT) ||
            !attributes.contains(EGL_TEXTURE_TYPE_ANGLE) ||
            !attributes.contains(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE) ||
            !attributes.contains(EGL_IOSURFACE_PLANE_ANGLE))
        {
            return EglBadParameter() << "Missing required attribute for EGL_IOSURFACE";
        }
    }

    ANGLE_TRY(display->validateClientBuffer(config, buftype, buffer, attributes));

    return NoError();
}

Error ValidateMakeCurrent(Display *display, Surface *draw, Surface *read, gl::Context *context)
{
    if (context == EGL_NO_CONTEXT && (draw != EGL_NO_SURFACE || read != EGL_NO_SURFACE))
    {
        return EglBadMatch() << "If ctx is EGL_NO_CONTEXT, surfaces must be EGL_NO_SURFACE";
    }

    // If ctx is EGL_NO_CONTEXT and either draw or read are not EGL_NO_SURFACE, an EGL_BAD_MATCH
    // error is generated. EGL_KHR_surfaceless_context allows both surfaces to be EGL_NO_SURFACE.
    if (context != EGL_NO_CONTEXT && (draw == EGL_NO_SURFACE || read == EGL_NO_SURFACE))
    {
        if (display->getExtensions().surfacelessContext)
        {
            if ((draw == EGL_NO_SURFACE) != (read == EGL_NO_SURFACE))
            {
                return EglBadMatch() << "If ctx is not EGL_NOT_CONTEXT, draw or read must "
                                        "both be EGL_NO_SURFACE, or both not";
            }
        }
        else
        {
            return EglBadMatch()
                   << "If ctx is not EGL_NO_CONTEXT, surfaces must not be EGL_NO_SURFACE";
        }
    }

    // If either of draw or read is a valid surface and the other is EGL_NO_SURFACE, an
    // EGL_BAD_MATCH error is generated.
    if ((read == EGL_NO_SURFACE) != (draw == EGL_NO_SURFACE))
    {
        return EglBadMatch()
               << "read and draw must both be valid surfaces, or both be EGL_NO_SURFACE";
    }

    if (display == EGL_NO_DISPLAY || !Display::isValidDisplay(display))
    {
        return EglBadDisplay() << "'dpy' not a valid EGLDisplay handle";
    }

    // EGL 1.5 spec: dpy can be uninitialized if all other parameters are null
    if (!display->isInitialized() &&
        (context != EGL_NO_CONTEXT || draw != EGL_NO_SURFACE || read != EGL_NO_SURFACE))
    {
        return EglNotInitialized() << "'dpy' not initialized";
    }

    if (context != EGL_NO_CONTEXT)
    {
        ANGLE_TRY(ValidateContext(display, context));
    }

    if (display->isInitialized() && display->isDeviceLost())
    {
        return EglContextLost();
    }

    if (draw != EGL_NO_SURFACE)
    {
        ANGLE_TRY(ValidateSurface(display, draw));
    }

    if (read != EGL_NO_SURFACE)
    {
        ANGLE_TRY(ValidateSurface(display, read));
        ANGLE_TRY(ValidateCompatibleConfigs(display, read->getConfig(), read, context->getConfig(),
                                            read->getType()));
    }

    if (draw != read)
    {
        UNIMPLEMENTED();  // FIXME

        if (draw)
        {
            ANGLE_TRY(ValidateCompatibleConfigs(display, draw->getConfig(), draw,
                                                context->getConfig(), draw->getType()));
        }
    }
    return NoError();
}

Error ValidateCompatibleConfigs(const Display *display,
                                const Config *config1,
                                const Surface *surface,
                                const Config *config2,
                                EGLint surfaceType)
{

    if (!surface->flexibleSurfaceCompatibilityRequested())
    {
        // Config compatibility is defined in section 2.2 of the EGL 1.5 spec

        bool colorBufferCompat = config1->colorBufferType == config2->colorBufferType;
        if (!colorBufferCompat)
        {
            return EglBadMatch() << "Color buffer types are not compatible.";
        }

        bool colorCompat =
            config1->redSize == config2->redSize && config1->greenSize == config2->greenSize &&
            config1->blueSize == config2->blueSize && config1->alphaSize == config2->alphaSize &&
            config1->luminanceSize == config2->luminanceSize;
        if (!colorCompat)
        {
            return EglBadMatch() << "Color buffer sizes are not compatible.";
        }

        bool componentTypeCompat = config1->colorComponentType == config2->colorComponentType;
        if (!componentTypeCompat)
        {
            return EglBadMatch() << "Color buffer component types are not compatible.";
        }

        bool dsCompat = config1->depthSize == config2->depthSize &&
                        config1->stencilSize == config2->stencilSize;
        if (!dsCompat)
        {
            return EglBadMatch() << "Depth-stencil buffer types are not compatible.";
        }
    }

    bool surfaceTypeCompat = (config1->surfaceType & config2->surfaceType & surfaceType) != 0;
    if (!surfaceTypeCompat)
    {
        return EglBadMatch() << "Surface types are not compatible.";
    }

    return NoError();
}

Error ValidateCreateImageKHR(const Display *display,
                             gl::Context *context,
                             EGLenum target,
                             EGLClientBuffer buffer,
                             const AttributeMap &attributes)
{
    ANGLE_TRY(ValidateContext(display, context));

    const DisplayExtensions &displayExtensions = display->getExtensions();

    if (!displayExtensions.imageBase && !displayExtensions.image)
    {
        // It is out of spec what happens when calling an extension function when the extension is
        // not available.
        // EGL_BAD_DISPLAY seems like a reasonable error.
        return EglBadDisplay() << "EGL_KHR_image not supported.";
    }

    // TODO(geofflang): Complete validation from EGL_KHR_image_base:
    // If the resource specified by <dpy>, <ctx>, <target>, <buffer> and <attrib_list> is itself an
    // EGLImage sibling, the error EGL_BAD_ACCESS is generated.

    for (AttributeMap::const_iterator attributeIter = attributes.begin();
         attributeIter != attributes.end(); attributeIter++)
    {
        EGLAttrib attribute = attributeIter->first;
        EGLAttrib value     = attributeIter->second;

        switch (attribute)
        {
            case EGL_IMAGE_PRESERVED_KHR:
                switch (value)
                {
                    case EGL_TRUE:
                    case EGL_FALSE:
                        break;

                    default:
                        return EglBadParameter()
                               << "EGL_IMAGE_PRESERVED_KHR must be EGL_TRUE or EGL_FALSE.";
                }
                break;

            case EGL_GL_TEXTURE_LEVEL_KHR:
                if (!displayExtensions.glTexture2DImage &&
                    !displayExtensions.glTextureCubemapImage && !displayExtensions.glTexture3DImage)
                {
                    return EglBadParameter() << "EGL_GL_TEXTURE_LEVEL_KHR cannot be used "
                                                "without KHR_gl_texture_*_image support.";
                }

                if (value < 0)
                {
                    return EglBadParameter() << "EGL_GL_TEXTURE_LEVEL_KHR cannot be negative.";
                }
                break;

            case EGL_GL_TEXTURE_ZOFFSET_KHR:
                if (!displayExtensions.glTexture3DImage)
                {
                    return EglBadParameter() << "EGL_GL_TEXTURE_ZOFFSET_KHR cannot be used "
                                                "without KHR_gl_texture_3D_image support.";
                }
                break;

            default:
                return EglBadParameter()
                       << "invalid attribute: 0x" << std::hex << std::uppercase << attribute;
        }
    }

    switch (target)
    {
        case EGL_GL_TEXTURE_2D_KHR:
        {
            if (!displayExtensions.glTexture2DImage)
            {
                return EglBadParameter() << "KHR_gl_texture_2D_image not supported.";
            }

            if (buffer == 0)
            {
                return EglBadParameter() << "buffer cannot reference a 2D texture with the name 0.";
            }

            const gl::Texture *texture =
                context->getTexture(egl_gl::EGLClientBufferToGLObjectHandle(buffer));
            if (texture == nullptr || texture->getType() != gl::TextureType::_2D)
            {
                return EglBadParameter() << "target is not a 2D texture.";
            }

            if (texture->getBoundSurface() != nullptr)
            {
                return EglBadAccess() << "texture has a surface bound to it.";
            }

            EGLAttrib level = attributes.get(EGL_GL_TEXTURE_LEVEL_KHR, 0);
            if (texture->getWidth(gl::TextureTarget::_2D, static_cast<size_t>(level)) == 0 ||
                texture->getHeight(gl::TextureTarget::_2D, static_cast<size_t>(level)) == 0)
            {
                return EglBadParameter()
                       << "target 2D texture does not have a valid size at specified level.";
            }

            ANGLE_TRY(ValidateCreateImageKHRMipLevelCommon(context, texture, level));
        }
        break;

        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
        case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
        {
            if (!displayExtensions.glTextureCubemapImage)
            {
                return EglBadParameter() << "KHR_gl_texture_cubemap_image not supported.";
            }

            if (buffer == 0)
            {
                return EglBadParameter()
                       << "buffer cannot reference a cubemap texture with the name 0.";
            }

            const gl::Texture *texture =
                context->getTexture(egl_gl::EGLClientBufferToGLObjectHandle(buffer));
            if (texture == nullptr || texture->getType() != gl::TextureType::CubeMap)
            {
                return EglBadParameter() << "target is not a cubemap texture.";
            }

            if (texture->getBoundSurface() != nullptr)
            {
                return EglBadAccess() << "texture has a surface bound to it.";
            }

            EGLAttrib level               = attributes.get(EGL_GL_TEXTURE_LEVEL_KHR, 0);
            gl::TextureTarget cubeMapFace = egl_gl::EGLCubeMapTargetToCubeMapTarget(target);
            if (texture->getWidth(cubeMapFace, static_cast<size_t>(level)) == 0 ||
                texture->getHeight(cubeMapFace, static_cast<size_t>(level)) == 0)
            {
                return EglBadParameter() << "target cubemap texture does not have a valid "
                                            "size at specified level and face.";
            }

            ANGLE_TRY(ValidateCreateImageKHRMipLevelCommon(context, texture, level));

            if (level == 0 && !texture->isMipmapComplete() &&
                CubeTextureHasUnspecifiedLevel0Face(texture))
            {
                return EglBadParameter() << "if level is zero and the texture is incomplete, "
                                            "it must have all of its faces specified at level "
                                            "zero.";
            }
        }
        break;

        case EGL_GL_TEXTURE_3D_KHR:
        {
            if (!displayExtensions.glTexture3DImage)
            {
                return EglBadParameter() << "KHR_gl_texture_3D_image not supported.";
            }

            if (buffer == 0)
            {
                return EglBadParameter() << "buffer cannot reference a 3D texture with the name 0.";
            }

            const gl::Texture *texture =
                context->getTexture(egl_gl::EGLClientBufferToGLObjectHandle(buffer));
            if (texture == nullptr || texture->getType() != gl::TextureType::_3D)
            {
                return EglBadParameter() << "target is not a 3D texture.";
            }

            if (texture->getBoundSurface() != nullptr)
            {
                return EglBadAccess() << "texture has a surface bound to it.";
            }

            EGLAttrib level   = attributes.get(EGL_GL_TEXTURE_LEVEL_KHR, 0);
            EGLAttrib zOffset = attributes.get(EGL_GL_TEXTURE_ZOFFSET_KHR, 0);
            if (texture->getWidth(gl::TextureTarget::_3D, static_cast<size_t>(level)) == 0 ||
                texture->getHeight(gl::TextureTarget::_3D, static_cast<size_t>(level)) == 0 ||
                texture->getDepth(gl::TextureTarget::_3D, static_cast<size_t>(level)) == 0)
            {
                return EglBadParameter()
                       << "target 3D texture does not have a valid size at specified level.";
            }

            if (static_cast<size_t>(zOffset) >=
                texture->getDepth(gl::TextureTarget::_3D, static_cast<size_t>(level)))
            {
                return EglBadParameter() << "target 3D texture does not have enough layers "
                                            "for the specified Z offset at the specified "
                                            "level.";
            }

            ANGLE_TRY(ValidateCreateImageKHRMipLevelCommon(context, texture, level));
        }
        break;

        case EGL_GL_RENDERBUFFER_KHR:
        {
            if (!displayExtensions.glRenderbufferImage)
            {
                return EglBadParameter() << "KHR_gl_renderbuffer_image not supported.";
            }

            if (attributes.contains(EGL_GL_TEXTURE_LEVEL_KHR))
            {
                return EglBadParameter() << "EGL_GL_TEXTURE_LEVEL_KHR cannot be used in "
                                            "conjunction with a renderbuffer target.";
            }

            if (buffer == 0)
            {
                return EglBadParameter()
                       << "buffer cannot reference a renderbuffer with the name 0.";
            }

            const gl::Renderbuffer *renderbuffer =
                context->getRenderbuffer(egl_gl::EGLClientBufferToGLObjectHandle(buffer));
            if (renderbuffer == nullptr)
            {
                return EglBadParameter() << "target is not a renderbuffer.";
            }

            if (renderbuffer->getSamples() > 0)
            {
                return EglBadParameter() << "target renderbuffer cannot be multisampled.";
            }
        }
        break;

        default:
            return EglBadParameter()
                   << "invalid target: 0x" << std::hex << std::uppercase << target;
    }

    if (attributes.contains(EGL_GL_TEXTURE_ZOFFSET_KHR) && target != EGL_GL_TEXTURE_3D_KHR)
    {
        return EglBadParameter()
               << "EGL_GL_TEXTURE_ZOFFSET_KHR must be used with a 3D texture target.";
    }

    return NoError();
}

Error ValidateDestroyImageKHR(const Display *display, const Image *image)
{
    ANGLE_TRY(ValidateImage(display, image));

    if (!display->getExtensions().imageBase && !display->getExtensions().image)
    {
        // It is out of spec what happens when calling an extension function when the extension is
        // not available.
        // EGL_BAD_DISPLAY seems like a reasonable error.
        return EglBadDisplay();
    }

    return NoError();
}

Error ValidateCreateDeviceANGLE(EGLint device_type,
                                void *native_device,
                                const EGLAttrib *attrib_list)
{
    const ClientExtensions &clientExtensions = Display::GetClientExtensions();
    if (!clientExtensions.deviceCreation)
    {
        return EglBadAccess() << "Device creation extension not active";
    }

    if (attrib_list != nullptr && attrib_list[0] != EGL_NONE)
    {
        return EglBadAttribute() << "Invalid attrib_list parameter";
    }

    switch (device_type)
    {
        case EGL_D3D11_DEVICE_ANGLE:
            if (!clientExtensions.deviceCreationD3D11)
            {
                return EglBadAttribute() << "D3D11 device creation extension not active";
            }
            break;
        default:
            return EglBadAttribute() << "Invalid device_type parameter";
    }

    return NoError();
}

Error ValidateReleaseDeviceANGLE(Device *device)
{
    const ClientExtensions &clientExtensions = Display::GetClientExtensions();
    if (!clientExtensions.deviceCreation)
    {
        return EglBadAccess() << "Device creation extension not active";
    }

    if (device == EGL_NO_DEVICE_EXT || !Device::IsValidDevice(device))
    {
        return EglBadDevice() << "Invalid device parameter";
    }

    Display *owningDisplay = device->getOwningDisplay();
    if (owningDisplay != nullptr)
    {
        return EglBadDevice() << "Device must have been created using eglCreateDevice";
    }

    return NoError();
}

Error ValidateCreateStreamKHR(const Display *display, const AttributeMap &attributes)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.stream)
    {
        return EglBadAlloc() << "Stream extension not active";
    }

    for (const auto &attributeIter : attributes)
    {
        EGLAttrib attribute = attributeIter.first;
        EGLAttrib value     = attributeIter.second;

        ANGLE_TRY(ValidateStreamAttribute(attribute, value, displayExtensions));
    }

    return NoError();
}

Error ValidateDestroyStreamKHR(const Display *display, const Stream *stream)
{
    ANGLE_TRY(ValidateStream(display, stream));
    return NoError();
}

Error ValidateStreamAttribKHR(const Display *display,
                              const Stream *stream,
                              EGLint attribute,
                              EGLint value)
{
    ANGLE_TRY(ValidateStream(display, stream));

    if (stream->getState() == EGL_STREAM_STATE_DISCONNECTED_KHR)
    {
        return EglBadState() << "Bad stream state";
    }

    return ValidateStreamAttribute(attribute, value, display->getExtensions());
}

Error ValidateQueryStreamKHR(const Display *display,
                             const Stream *stream,
                             EGLenum attribute,
                             EGLint *value)
{
    ANGLE_TRY(ValidateStream(display, stream));

    switch (attribute)
    {
        case EGL_STREAM_STATE_KHR:
        case EGL_CONSUMER_LATENCY_USEC_KHR:
            break;
        case EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR:
            if (!display->getExtensions().streamConsumerGLTexture)
            {
                return EglBadAttribute() << "Consumer GLTexture extension not active";
            }
            break;
        default:
            return EglBadAttribute() << "Invalid attribute";
    }

    return NoError();
}

Error ValidateQueryStreamu64KHR(const Display *display,
                                const Stream *stream,
                                EGLenum attribute,
                                EGLuint64KHR *value)
{
    ANGLE_TRY(ValidateStream(display, stream));

    switch (attribute)
    {
        case EGL_CONSUMER_FRAME_KHR:
        case EGL_PRODUCER_FRAME_KHR:
            break;
        default:
            return EglBadAttribute() << "Invalid attribute";
    }

    return NoError();
}

Error ValidateStreamConsumerGLTextureExternalKHR(const Display *display,
                                                 gl::Context *context,
                                                 const Stream *stream)
{
    ANGLE_TRY(ValidateDisplay(display));
    ANGLE_TRY(ValidateContext(display, context));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.streamConsumerGLTexture)
    {
        return EglBadAccess() << "Stream consumer extension not active";
    }

    if (!context->getExtensions().eglStreamConsumerExternal)
    {
        return EglBadAccess() << "EGL stream consumer external GL extension not enabled";
    }

    if (stream == EGL_NO_STREAM_KHR || !display->isValidStream(stream))
    {
        return EglBadStream() << "Invalid stream";
    }

    if (stream->getState() != EGL_STREAM_STATE_CREATED_KHR)
    {
        return EglBadState() << "Invalid stream state";
    }

    // Lookup the texture and ensure it is correct
    gl::Texture *texture = context->getGLState().getTargetTexture(gl::TextureType::External);
    if (texture == nullptr || texture->getId() == 0)
    {
        return EglBadAccess() << "No external texture bound";
    }

    return NoError();
}

Error ValidateStreamConsumerAcquireKHR(const Display *display,
                                       gl::Context *context,
                                       const Stream *stream)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.streamConsumerGLTexture)
    {
        return EglBadAccess() << "Stream consumer extension not active";
    }

    if (stream == EGL_NO_STREAM_KHR || !display->isValidStream(stream))
    {
        return EglBadStream() << "Invalid stream";
    }

    if (!context)
    {
        return EglBadAccess() << "No GL context current to calling thread.";
    }

    ANGLE_TRY(ValidateContext(display, context));

    if (!stream->isConsumerBoundToContext(context))
    {
        return EglBadAccess() << "Current GL context not associated with stream consumer";
    }

    if (stream->getConsumerType() != Stream::ConsumerType::GLTextureRGB &&
        stream->getConsumerType() != Stream::ConsumerType::GLTextureYUV)
    {
        return EglBadAccess() << "Invalid stream consumer type";
    }

    // Note: technically EGL_STREAM_STATE_EMPTY_KHR is a valid state when the timeout is non-zero.
    // However, the timeout is effectively ignored since it has no useful functionality with the
    // current producers that are implemented, so we don't allow that state
    if (stream->getState() != EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR &&
        stream->getState() != EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR)
    {
        return EglBadState() << "Invalid stream state";
    }

    return NoError();
}

Error ValidateStreamConsumerReleaseKHR(const Display *display,
                                       gl::Context *context,
                                       const Stream *stream)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.streamConsumerGLTexture)
    {
        return EglBadAccess() << "Stream consumer extension not active";
    }

    if (stream == EGL_NO_STREAM_KHR || !display->isValidStream(stream))
    {
        return EglBadStream() << "Invalid stream";
    }

    if (!context)
    {
        return EglBadAccess() << "No GL context current to calling thread.";
    }

    ANGLE_TRY(ValidateContext(display, context));

    if (!stream->isConsumerBoundToContext(context))
    {
        return EglBadAccess() << "Current GL context not associated with stream consumer";
    }

    if (stream->getConsumerType() != Stream::ConsumerType::GLTextureRGB &&
        stream->getConsumerType() != Stream::ConsumerType::GLTextureYUV)
    {
        return EglBadAccess() << "Invalid stream consumer type";
    }

    if (stream->getState() != EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR &&
        stream->getState() != EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR)
    {
        return EglBadState() << "Invalid stream state";
    }

    return NoError();
}

Error ValidateStreamConsumerGLTextureExternalAttribsNV(const Display *display,
                                                       gl::Context *context,
                                                       const Stream *stream,
                                                       const AttributeMap &attribs)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.streamConsumerGLTexture)
    {
        return EglBadAccess() << "Stream consumer extension not active";
    }

    // Although technically not a requirement in spec, the context needs to be checked for support
    // for external textures or future logic will cause assertations. This extension is also
    // effectively useless without external textures.
    if (!context->getExtensions().eglStreamConsumerExternal)
    {
        return EglBadAccess() << "EGL stream consumer external GL extension not enabled";
    }

    if (stream == EGL_NO_STREAM_KHR || !display->isValidStream(stream))
    {
        return EglBadStream() << "Invalid stream";
    }

    if (!context)
    {
        return EglBadAccess() << "No GL context current to calling thread.";
    }

    ANGLE_TRY(ValidateContext(display, context));

    if (stream->getState() != EGL_STREAM_STATE_CREATED_KHR)
    {
        return EglBadState() << "Invalid stream state";
    }

    const gl::Caps &glCaps = context->getCaps();

    EGLAttrib colorBufferType = EGL_RGB_BUFFER;
    EGLAttrib planeCount      = -1;
    EGLAttrib plane[3];
    for (int i = 0; i < 3; i++)
    {
        plane[i] = -1;
    }
    for (const auto &attributeIter : attribs)
    {
        EGLAttrib attribute = attributeIter.first;
        EGLAttrib value     = attributeIter.second;

        switch (attribute)
        {
            case EGL_COLOR_BUFFER_TYPE:
                if (value != EGL_RGB_BUFFER && value != EGL_YUV_BUFFER_EXT)
                {
                    return EglBadParameter() << "Invalid color buffer type";
                }
                colorBufferType = value;
                break;
            case EGL_YUV_NUMBER_OF_PLANES_EXT:
                // planeCount = -1 is a tag for the default plane count so the value must be checked
                // to be positive here to ensure future logic doesn't break on invalid negative
                // inputs
                if (value < 0)
                {
                    return EglBadMatch() << "Invalid plane count";
                }
                planeCount = value;
                break;
            default:
                if (attribute >= EGL_YUV_PLANE0_TEXTURE_UNIT_NV &&
                    attribute <= EGL_YUV_PLANE2_TEXTURE_UNIT_NV)
                {
                    if ((value < 0 ||
                         value >= static_cast<EGLAttrib>(glCaps.maxCombinedTextureImageUnits)) &&
                        value != EGL_NONE)
                    {
                        return EglBadAccess() << "Invalid texture unit";
                    }
                    plane[attribute - EGL_YUV_PLANE0_TEXTURE_UNIT_NV] = value;
                }
                else
                {
                    return EglBadAttribute() << "Invalid attribute";
                }
        }
    }

    if (colorBufferType == EGL_RGB_BUFFER)
    {
        if (planeCount > 0)
        {
            return EglBadMatch() << "Plane count must be 0 for RGB buffer";
        }
        for (int i = 0; i < 3; i++)
        {
            if (plane[i] != -1)
            {
                return EglBadMatch() << "Planes cannot be specified";
            }
        }

        // Lookup the texture and ensure it is correct
        gl::Texture *texture = context->getGLState().getTargetTexture(gl::TextureType::External);
        if (texture == nullptr || texture->getId() == 0)
        {
            return EglBadAccess() << "No external texture bound";
        }
    }
    else
    {
        if (planeCount == -1)
        {
            planeCount = 2;
        }
        if (planeCount < 1 || planeCount > 3)
        {
            return EglBadMatch() << "Invalid YUV plane count";
        }
        for (EGLAttrib i = planeCount; i < 3; i++)
        {
            if (plane[i] != -1)
            {
                return EglBadMatch() << "Invalid plane specified";
            }
        }

        // Set to ensure no texture is referenced more than once
        std::set<gl::Texture *> textureSet;
        for (EGLAttrib i = 0; i < planeCount; i++)
        {
            if (plane[i] == -1)
            {
                return EglBadMatch() << "Not all planes specified";
            }
            if (plane[i] != EGL_NONE)
            {
                gl::Texture *texture = context->getGLState().getSamplerTexture(
                    static_cast<unsigned int>(plane[i]), gl::TextureType::External);
                if (texture == nullptr || texture->getId() == 0)
                {
                    return EglBadAccess()
                           << "No external texture bound at one or more specified texture units";
                }
                if (textureSet.find(texture) != textureSet.end())
                {
                    return EglBadAccess() << "Multiple planes bound to same texture object";
                }
                textureSet.insert(texture);
            }
        }
    }

    return NoError();
}

Error ValidateCreateStreamProducerD3DTextureANGLE(const Display *display,
                                                  const Stream *stream,
                                                  const AttributeMap &attribs)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.streamProducerD3DTexture)
    {
        return EglBadAccess() << "Stream producer extension not active";
    }

    ANGLE_TRY(ValidateStream(display, stream));

    if (!attribs.isEmpty())
    {
        return EglBadAttribute() << "Invalid attribute";
    }

    if (stream->getState() != EGL_STREAM_STATE_CONNECTING_KHR)
    {
        return EglBadState() << "Stream not in connecting state";
    }

    switch (stream->getConsumerType())
    {
        case Stream::ConsumerType::GLTextureYUV:
            if (stream->getPlaneCount() != 2)
            {
                return EglBadMatch() << "Incompatible stream consumer type";
            }
            break;

        case Stream::ConsumerType::GLTextureRGB:
            if (stream->getPlaneCount() != 1)
            {
                return EglBadMatch() << "Incompatible stream consumer type";
            }
            break;

        default:
            return EglBadMatch() << "Incompatible stream consumer type";
    }

    return NoError();
}

Error ValidateStreamPostD3DTextureANGLE(const Display *display,
                                        const Stream *stream,
                                        void *texture,
                                        const AttributeMap &attribs)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.streamProducerD3DTexture)
    {
        return EglBadAccess() << "Stream producer extension not active";
    }

    ANGLE_TRY(ValidateStream(display, stream));

    for (auto &attributeIter : attribs)
    {
        EGLAttrib attribute = attributeIter.first;
        EGLAttrib value     = attributeIter.second;

        switch (attribute)
        {
            case EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE:
                if (value < 0)
                {
                    return EglBadParameter() << "Invalid subresource index";
                }
                break;
            case EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG:
                if (value < 0)
                {
                    return EglBadParameter() << "Invalid plane offset";
                }
                break;
            default:
                return EglBadAttribute() << "Invalid attribute";
        }
    }

    if (stream->getState() != EGL_STREAM_STATE_EMPTY_KHR &&
        stream->getState() != EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR &&
        stream->getState() != EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR)
    {
        return EglBadState() << "Stream not fully configured";
    }

    if (stream->getProducerType() != Stream::ProducerType::D3D11Texture)
    {
        return EglBadMatch() << "Incompatible stream producer";
    }

    if (texture == nullptr)
    {
        return EglBadParameter() << "Texture is null";
    }

    return stream->validateD3D11Texture(texture, attribs);
}

Error ValidateGetSyncValuesCHROMIUM(const Display *display,
                                    const Surface *surface,
                                    const EGLuint64KHR *ust,
                                    const EGLuint64KHR *msc,
                                    const EGLuint64KHR *sbc)
{
    ANGLE_TRY(ValidateDisplay(display));

    const DisplayExtensions &displayExtensions = display->getExtensions();
    if (!displayExtensions.getSyncValues)
    {
        return EglBadAccess() << "getSyncValues extension not active";
    }

    if (display->isDeviceLost())
    {
        return EglContextLost() << "Context is lost.";
    }

    if (surface == EGL_NO_SURFACE)
    {
        return EglBadSurface() << "getSyncValues surface cannot be EGL_NO_SURFACE";
    }

    if (!surface->directComposition())
    {
        return EglBadSurface() << "getSyncValues surface requires Direct Composition to be enabled";
    }

    if (ust == nullptr)
    {
        return EglBadParameter() << "ust is null";
    }

    if (msc == nullptr)
    {
        return EglBadParameter() << "msc is null";
    }

    if (sbc == nullptr)
    {
        return EglBadParameter() << "sbc is null";
    }

    return NoError();
}

Error ValidateDestroySurface(const Display *display,
                             const Surface *surface,
                             const EGLSurface eglSurface)
{
    ANGLE_TRY(ValidateSurface(display, surface));

    if (eglSurface == EGL_NO_SURFACE)
    {
        return EglBadSurface();
    }

    return NoError();
}

Error ValidateDestroyContext(const Display *display,
                             const gl::Context *glCtx,
                             const EGLContext eglCtx)
{
    ANGLE_TRY(ValidateContext(display, glCtx));

    if (eglCtx == EGL_NO_CONTEXT)
    {
        return EglBadContext();
    }

    return NoError();
}

Error ValidateSwapBuffers(Thread *thread, const Display *display, const Surface *eglSurface)
{
    ANGLE_TRY(ValidateSurface(display, eglSurface));

    if (display->isDeviceLost())
    {
        return EglContextLost();
    }

    if (eglSurface == EGL_NO_SURFACE || !thread->getContext() ||
        thread->getCurrentDrawSurface() != eglSurface)
    {
        return EglBadSurface();
    }

    return NoError();
}

Error ValidateSwapBuffersWithDamageKHR(const Display *display,
                                       const Surface *surface,
                                       EGLint *rects,
                                       EGLint n_rects)
{
    Error error = ValidateSurface(display, surface);
    if (error.isError())
    {
        return error;
    }

    if (!display->getExtensions().swapBuffersWithDamage)
    {
        // It is out of spec what happens when calling an extension function when the extension is
        // not available. EGL_BAD_DISPLAY seems like a reasonable error.
        return EglBadDisplay() << "EGL_KHR_swap_buffers_with_damage is not available.";
    }

    if (surface == EGL_NO_SURFACE)
    {
        return EglBadSurface() << "Swap surface cannot be EGL_NO_SURFACE.";
    }

    if (n_rects < 0)
    {
        return EglBadParameter() << "n_rects cannot be negative.";
    }

    if (n_rects > 0 && rects == nullptr)
    {
        return EglBadParameter() << "n_rects cannot be greater than zero when rects is NULL.";
    }

    // TODO(jmadill): Validate Surface is bound to the thread.

    return NoError();
}

Error ValidateWaitNative(const Display *display, const EGLint engine)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (engine != EGL_CORE_NATIVE_ENGINE)
    {
        return EglBadParameter() << "the 'engine' parameter has an unrecognized value";
    }

    return NoError();
}

Error ValidateCopyBuffers(Display *display, const Surface *surface)
{
    ANGLE_TRY(ValidateSurface(display, surface));

    if (display->testDeviceLost())
    {
        return EglContextLost();
    }

    return NoError();
}

// Validate state for eglBindTexImage. If context is non-null then textureObject will be set to
// surface's texture that will have an image bound to it
Error ValidateBindTexImage(const Display *display,
                           const Surface *surface,
                           const EGLSurface eglSurface,
                           const EGLint buffer,
                           const gl::Context *context,
                           gl::Texture **textureObject)
{
    ANGLE_TRY(ValidateSurface(display, surface));

    if (buffer != EGL_BACK_BUFFER)
    {
        return EglBadParameter();
    }

    if (eglSurface == EGL_NO_SURFACE || surface->getType() == EGL_WINDOW_BIT)
    {
        return EglBadSurface();
    }

    if (surface->getBoundTexture())
    {
        return EglBadAccess();
    }

    if (surface->getTextureFormat() == TextureFormat::NoTexture)
    {
        return EglBadMatch();
    }

    if (context)
    {
        gl::TextureType type = egl_gl::EGLTextureTargetToTextureType(surface->getTextureTarget());
        *textureObject       = context->getTargetTexture(type);
        ASSERT(*textureObject != nullptr);

        if ((*textureObject)->getImmutableFormat())
        {
            return EglBadMatch();
        }
    }

    return NoError();
}

Error ValidateReleaseTexImage(const Display *display,
                              const Surface *surface,
                              const EGLSurface eglSurface,
                              const EGLint buffer)
{
    ANGLE_TRY(ValidateSurface(display, surface));

    if (buffer != EGL_BACK_BUFFER)
    {
        return EglBadParameter();
    }

    if (eglSurface == EGL_NO_SURFACE || surface->getType() == EGL_WINDOW_BIT)
    {
        return EglBadSurface();
    }

    if (surface->getTextureFormat() == TextureFormat::NoTexture)
    {
        return EglBadMatch();
    }

    return NoError();
}

Error ValidateSwapInterval(const Display *display, const Surface *draw_surface)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (draw_surface == nullptr)
    {
        return EglBadSurface();
    }

    return NoError();
}

Error ValidateBindAPI(const EGLenum api)
{
    switch (api)
    {
        case EGL_OPENGL_API:
        case EGL_OPENVG_API:
            return EglBadParameter();  // Not supported by this implementation
        case EGL_OPENGL_ES_API:
            break;
        default:
            return EglBadParameter();
    }

    return NoError();
}

Error ValidatePresentationTimeANDROID(const Display *display,
                                      const Surface *surface,
                                      EGLnsecsANDROID time)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->getExtensions().presentationTime)
    {
        // It is out of spec what happens when calling an extension function when the extension is
        // not available. EGL_BAD_DISPLAY seems like a reasonable error.
        return EglBadDisplay() << "EGL_ANDROID_presentation_time is not available.";
    }

    ANGLE_TRY(ValidateSurface(display, surface));

    return NoError();
}

Error ValidateGetConfigAttrib(const Display *display, const Config *config, EGLint attribute)
{
    ANGLE_TRY(ValidateConfig(display, config));
    ANGLE_TRY(ValidateConfigAttribute(display, static_cast<EGLAttrib>(attribute)));
    return NoError();
}

Error ValidateChooseConfig(const Display *display,
                           const AttributeMap &attribs,
                           EGLint configSize,
                           EGLint *numConfig)
{
    ANGLE_TRY(ValidateDisplay(display));
    ANGLE_TRY(ValidateConfigAttributes(display, attribs));

    if (numConfig == nullptr)
    {
        return EglBadParameter() << "num_config cannot be null.";
    }

    return NoError();
}

Error ValidateGetConfigs(const Display *display, EGLint configSize, EGLint *numConfig)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (numConfig == nullptr)
    {
        return EglBadParameter() << "num_config cannot be null.";
    }

    return NoError();
}

Error ValidateGetPlatformDisplay(EGLenum platform,
                                 void *native_display,
                                 const EGLAttrib *attrib_list)
{
    const auto &attribMap = AttributeMap::CreateFromAttribArray(attrib_list);
    return ValidateGetPlatformDisplayCommon(platform, native_display, attribMap);
}

Error ValidateGetPlatformDisplayEXT(EGLenum platform,
                                    void *native_display,
                                    const EGLint *attrib_list)
{
    const auto &attribMap = AttributeMap::CreateFromIntArray(attrib_list);
    return ValidateGetPlatformDisplayCommon(platform, native_display, attribMap);
}

Error ValidateCreatePlatformWindowSurfaceEXT(const Display *display,
                                             const Config *configuration,
                                             void *nativeWindow,
                                             const AttributeMap &attributes)
{
    if (!Display::GetClientExtensions().platformBase)
    {
        return EglBadAccess() << "EGL_EXT_platform_base not supported";
    }

    ANGLE_TRY(ValidateConfig(display, configuration));

    return EglBadDisplay() << "ValidateCreatePlatformWindowSurfaceEXT unimplemented.";
}

Error ValidateCreatePlatformPixmapSurfaceEXT(const Display *display,
                                             const Config *configuration,
                                             void *nativePixmap,
                                             const AttributeMap &attributes)
{
    if (!Display::GetClientExtensions().platformBase)
    {
        return EglBadAccess() << "EGL_EXT_platform_base not supported";
    }

    ANGLE_TRY(ValidateConfig(display, configuration));

    return EglBadDisplay() << "ValidateCreatePlatformPixmapSurfaceEXT unimplemented.";
}

Error ValidateProgramCacheGetAttribANGLE(const Display *display, EGLenum attrib)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->getExtensions().programCacheControl)
    {
        return EglBadAccess() << "Extension not supported";
    }

    switch (attrib)
    {
        case EGL_PROGRAM_CACHE_KEY_LENGTH_ANGLE:
        case EGL_PROGRAM_CACHE_SIZE_ANGLE:
            break;

        default:
            return EglBadParameter() << "Invalid program cache attribute.";
    }

    return NoError();
}

Error ValidateProgramCacheQueryANGLE(const Display *display,
                                     EGLint index,
                                     void *key,
                                     EGLint *keysize,
                                     void *binary,
                                     EGLint *binarysize)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->getExtensions().programCacheControl)
    {
        return EglBadAccess() << "Extension not supported";
    }

    if (index < 0 || index >= display->programCacheGetAttrib(EGL_PROGRAM_CACHE_SIZE_ANGLE))
    {
        return EglBadParameter() << "Program index out of range.";
    }

    if (keysize == nullptr || binarysize == nullptr)
    {
        return EglBadParameter() << "keysize and binarysize must always be valid pointers.";
    }

    if (binary && *keysize != static_cast<EGLint>(gl::kProgramHashLength))
    {
        return EglBadParameter() << "Invalid program key size.";
    }

    if ((key == nullptr) != (binary == nullptr))
    {
        return EglBadParameter() << "key and binary must both be null or both non-null.";
    }

    return NoError();
}

Error ValidateProgramCachePopulateANGLE(const Display *display,
                                        const void *key,
                                        EGLint keysize,
                                        const void *binary,
                                        EGLint binarysize)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->getExtensions().programCacheControl)
    {
        return EglBadAccess() << "Extension not supported";
    }

    if (keysize != static_cast<EGLint>(gl::kProgramHashLength))
    {
        return EglBadParameter() << "Invalid program key size.";
    }

    if (key == nullptr || binary == nullptr)
    {
        return EglBadParameter() << "null pointer in arguments.";
    }

    // Upper bound for binarysize is arbitrary.
    if (binarysize <= 0 || binarysize > egl::kProgramCacheSizeAbsoluteMax)
    {
        return EglBadParameter() << "binarysize out of valid range.";
    }

    return NoError();
}

Error ValidateProgramCacheResizeANGLE(const Display *display, EGLint limit, EGLenum mode)
{
    ANGLE_TRY(ValidateDisplay(display));

    if (!display->getExtensions().programCacheControl)
    {
        return EglBadAccess() << "Extension not supported";
    }

    if (limit < 0)
    {
        return EglBadParameter() << "limit must be non-negative.";
    }

    switch (mode)
    {
        case EGL_PROGRAM_CACHE_RESIZE_ANGLE:
        case EGL_PROGRAM_CACHE_TRIM_ANGLE:
            break;

        default:
            return EglBadParameter() << "Invalid cache resize mode.";
    }

    return NoError();
}

Error ValidateSurfaceAttrib(const Display *display,
                            const Surface *surface,
                            EGLint attribute,
                            EGLint value)
{
    ANGLE_TRY(ValidateDisplay(display));
    ANGLE_TRY(ValidateSurface(display, surface));

    if (surface == EGL_NO_SURFACE)
    {
        return EglBadSurface() << "Surface cannot be EGL_NO_SURFACE.";
    }

    switch (attribute)
    {
        case EGL_MIPMAP_LEVEL:
            break;

        case EGL_MULTISAMPLE_RESOLVE:
            switch (value)
            {
                case EGL_MULTISAMPLE_RESOLVE_DEFAULT:
                    break;

                case EGL_MULTISAMPLE_RESOLVE_BOX:
                    if ((surface->getConfig()->surfaceType & EGL_MULTISAMPLE_RESOLVE_BOX_BIT) == 0)
                    {
                        return EglBadMatch()
                               << "Surface does not support EGL_MULTISAMPLE_RESOLVE_BOX.";
                    }
                    break;

                default:
                    return EglBadAttribute() << "Invalid multisample resolve type.";
            }
            break;

        case EGL_SWAP_BEHAVIOR:
            switch (value)
            {
                case EGL_BUFFER_PRESERVED:
                    if ((surface->getConfig()->surfaceType & EGL_SWAP_BEHAVIOR_PRESERVED_BIT) == 0)
                    {
                        return EglBadMatch()
                               << "Surface does not support EGL_SWAP_BEHAVIOR_PRESERVED.";
                    }
                    break;

                case EGL_BUFFER_DESTROYED:
                    break;

                default:
                    return EglBadAttribute() << "Invalid swap behaviour.";
            }
            break;

        case EGL_WIDTH:
        case EGL_HEIGHT:
            if (!display->getExtensions().windowFixedSize)
            {
                return EglBadAttribute() << "EGL_WIDTH or EGL_HEIGHT cannot be set without "
                                            "EGL_ANGLE_window_fixed_size support.";
            }
            if (!surface->isFixedSize())
            {
                return EglBadMatch() << "EGL_WIDTH or EGL_HEIGHT cannot be set without "
                                        "EGL_FIXED_SIZE_ANGLE being enabled on the surface.";
            }
            break;

        default:
            return EglBadAttribute() << "Invalid surface attribute.";
    }

    return NoError();
}

Error ValidateQuerySurface(const Display *display,
                           const Surface *surface,
                           EGLint attribute,
                           EGLint *value)
{
    ANGLE_TRY(ValidateDisplay(display));
    ANGLE_TRY(ValidateSurface(display, surface));

    if (surface == EGL_NO_SURFACE)
    {
        return EglBadSurface() << "Surface cannot be EGL_NO_SURFACE.";
    }

    switch (attribute)
    {
        case EGL_GL_COLORSPACE:
        case EGL_VG_ALPHA_FORMAT:
        case EGL_VG_COLORSPACE:
        case EGL_CONFIG_ID:
        case EGL_HEIGHT:
        case EGL_HORIZONTAL_RESOLUTION:
        case EGL_LARGEST_PBUFFER:
        case EGL_MIPMAP_TEXTURE:
        case EGL_MIPMAP_LEVEL:
        case EGL_MULTISAMPLE_RESOLVE:
        case EGL_PIXEL_ASPECT_RATIO:
        case EGL_RENDER_BUFFER:
        case EGL_SWAP_BEHAVIOR:
        case EGL_TEXTURE_FORMAT:
        case EGL_TEXTURE_TARGET:
        case EGL_VERTICAL_RESOLUTION:
        case EGL_WIDTH:
            break;

        case EGL_POST_SUB_BUFFER_SUPPORTED_NV:
            if (!display->getExtensions().postSubBuffer)
            {
                return EglBadAttribute() << "EGL_POST_SUB_BUFFER_SUPPORTED_NV cannot be used "
                                            "without EGL_ANGLE_surface_orientation support.";
            }
            break;

        case EGL_FIXED_SIZE_ANGLE:
            if (!display->getExtensions().windowFixedSize)
            {
                return EglBadAttribute() << "EGL_FIXED_SIZE_ANGLE cannot be used without "
                                            "EGL_ANGLE_window_fixed_size support.";
            }
            break;

        case EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE:
            if (!display->getExtensions().flexibleSurfaceCompatibility)
            {
                return EglBadAttribute()
                       << "EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE cannot be "
                          "used without EGL_ANGLE_flexible_surface_compatibility support.";
            }
            break;

        case EGL_SURFACE_ORIENTATION_ANGLE:
            if (!display->getExtensions().surfaceOrientation)
            {
                return EglBadAttribute() << "EGL_SURFACE_ORIENTATION_ANGLE cannot be "
                                            "queried without "
                                            "EGL_ANGLE_surface_orientation support.";
            }
            break;

        case EGL_DIRECT_COMPOSITION_ANGLE:
            if (!display->getExtensions().directComposition)
            {
                return EglBadAttribute() << "EGL_DIRECT_COMPOSITION_ANGLE cannot be "
                                            "used without "
                                            "EGL_ANGLE_direct_composition support.";
            }
            break;

        case EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
            if (!display->getExtensions().robustResourceInitialization)
            {
                return EglBadAttribute() << "EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE cannot be "
                                            "used without EGL_ANGLE_robust_resource_initialization "
                                            "support.";
            }
            break;

        default:
            return EglBadAttribute() << "Invalid surface attribute.";
    }

    return NoError();
}

Error ValidateQueryContext(const Display *display,
                           const gl::Context *context,
                           EGLint attribute,
                           EGLint *value)
{
    ANGLE_TRY(ValidateDisplay(display));
    ANGLE_TRY(ValidateContext(display, context));

    switch (attribute)
    {
        case EGL_CONFIG_ID:
        case EGL_CONTEXT_CLIENT_TYPE:
        case EGL_CONTEXT_CLIENT_VERSION:
        case EGL_RENDER_BUFFER:
            break;

        case EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE:
            if (!display->getExtensions().robustResourceInitialization)
            {
                return EglBadAttribute() << "EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE cannot be "
                                            "used without EGL_ANGLE_robust_resource_initialization "
                                            "support.";
            }
            break;

        default:
            return EglBadAttribute() << "Invalid context attribute.";
    }

    return NoError();
}

Error ValidateDebugMessageControlKHR(EGLDEBUGPROCKHR callback, const AttributeMap &attribs)
{
    const ClientExtensions &clientExtensions = Display::GetClientExtensions();
    if (!clientExtensions.debug)
    {
        return EglBadAccess() << "EGL_KHR_debug extension is not available.";
    }

    for (const auto &attrib : attribs)
    {
        switch (attrib.first)
        {
            case EGL_DEBUG_MSG_CRITICAL_KHR:
            case EGL_DEBUG_MSG_ERROR_KHR:
            case EGL_DEBUG_MSG_WARN_KHR:
            case EGL_DEBUG_MSG_INFO_KHR:
                if (attrib.second != EGL_TRUE && attrib.second != EGL_FALSE)
                {
                    return EglBadAttribute() << "message controls must be EGL_TRUE or EGL_FALSE.";
                }
                break;
        }
    }

    return NoError();
}

Error ValidateQueryDebugKHR(EGLint attribute, EGLAttrib *value)
{
    const ClientExtensions &clientExtensions = Display::GetClientExtensions();
    if (!clientExtensions.debug)
    {
        return EglBadAccess() << "EGL_KHR_debug extension is not available.";
    }

    switch (attribute)
    {
        case EGL_DEBUG_MSG_CRITICAL_KHR:
        case EGL_DEBUG_MSG_ERROR_KHR:
        case EGL_DEBUG_MSG_WARN_KHR:
        case EGL_DEBUG_MSG_INFO_KHR:
        case EGL_DEBUG_CALLBACK_KHR:
            break;

        default:
            return EglBadAttribute() << "unknown attribute.";
    }

    return NoError();
}

Error ValidateLabelObjectKHR(Thread *thread,
                             const Display *display,
                             ObjectType objectType,
                             EGLObjectKHR object,
                             EGLLabelKHR label)
{
    const ClientExtensions &clientExtensions = Display::GetClientExtensions();
    if (!clientExtensions.debug)
    {
        return EglBadAccess() << "EGL_KHR_debug extension is not available.";
    }

    LabeledObject *labeledObject = nullptr;
    ANGLE_TRY(ValidateLabeledObject(thread, display, objectType, object, &labeledObject));

    return NoError();
}

}  // namespace egl
