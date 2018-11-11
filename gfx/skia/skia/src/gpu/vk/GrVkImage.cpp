/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrVkGpu.h"
#include "GrVkImage.h"
#include "GrVkMemory.h"
#include "GrVkUtil.h"

#define VK_CALL(GPU, X) GR_VK_CALL(GPU->vkInterface(), X)

VkPipelineStageFlags GrVkImage::LayoutToPipelineSrcStageFlags(const VkImageLayout layout) {
    if (VK_IMAGE_LAYOUT_GENERAL == layout) {
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    } else if (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL == layout ||
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == layout) {
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL == layout) {
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == layout ||
               VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL == layout) {
        return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == layout) {
        return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (VK_IMAGE_LAYOUT_PREINITIALIZED == layout) {
        return VK_PIPELINE_STAGE_HOST_BIT;
    }

    SkASSERT(VK_IMAGE_LAYOUT_UNDEFINED == layout);
    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}

VkAccessFlags GrVkImage::LayoutToSrcAccessMask(const VkImageLayout layout) {
    // Currently we assume we will never being doing any explict shader writes (this doesn't include
    // color attachment or depth/stencil writes). So we will ignore the
    // VK_MEMORY_OUTPUT_SHADER_WRITE_BIT.

    // We can only directly access the host memory if we are in preinitialized or general layout,
    // and the image is linear.
    // TODO: Add check for linear here so we are not always adding host to general, and we should
    //       only be in preinitialized if we are linear
    VkAccessFlags flags = 0;;
    if (VK_IMAGE_LAYOUT_GENERAL == layout) {
        flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_TRANSFER_WRITE_BIT |
                VK_ACCESS_TRANSFER_READ_BIT |
                VK_ACCESS_SHADER_READ_BIT |
                VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_HOST_READ_BIT;
    } else if (VK_IMAGE_LAYOUT_PREINITIALIZED == layout) {
        flags = VK_ACCESS_HOST_WRITE_BIT;
    } else if (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL == layout) {
        flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    } else if (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL == layout) {
        flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == layout) {
        flags = VK_ACCESS_TRANSFER_WRITE_BIT;
    } else if (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL == layout) {
        flags = VK_ACCESS_TRANSFER_READ_BIT;
    } else if (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == layout) {
        flags = VK_ACCESS_SHADER_READ_BIT;
    }
    return flags;
}

VkImageAspectFlags vk_format_to_aspect_flags(VkFormat format) {
    switch (format) {
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT: // fallthrough
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            SkASSERT(GrVkFormatIsSupported(format));
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

void GrVkImage::setImageLayout(const GrVkGpu* gpu, VkImageLayout newLayout,
                               VkAccessFlags dstAccessMask,
                               VkPipelineStageFlags dstStageMask,
                               bool byRegion, bool releaseFamilyQueue) {
    SkASSERT(VK_IMAGE_LAYOUT_UNDEFINED != newLayout &&
             VK_IMAGE_LAYOUT_PREINITIALIZED != newLayout);
    VkImageLayout currentLayout = this->currentLayout();

    if (releaseFamilyQueue && fInfo.fCurrentQueueFamily == fInitialQueueFamily) {
        // We never transfered the image to this queue and we are releasing it so don't do anything.
        return;
    }

    // If the old and new layout are the same and the layout is a read only layout, there is no need
    // to put in a barrier.
    if (newLayout == currentLayout &&
        (VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL == currentLayout ||
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == currentLayout ||
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL == currentLayout)) {
        return;
    }

    VkAccessFlags srcAccessMask = GrVkImage::LayoutToSrcAccessMask(currentLayout);
    VkPipelineStageFlags srcStageMask = GrVkImage::LayoutToPipelineSrcStageFlags(currentLayout);

    VkImageAspectFlags aspectFlags = vk_format_to_aspect_flags(fInfo.fFormat);

    uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    if (fInfo.fCurrentQueueFamily != VK_QUEUE_FAMILY_IGNORED &&
        gpu->queueIndex() != fInfo.fCurrentQueueFamily) {
        // The image still is owned by its original queue family and we need to transfer it into
        // ours.
        SkASSERT(!releaseFamilyQueue);
        SkASSERT(fInfo.fCurrentQueueFamily == fInitialQueueFamily);

        srcQueueFamilyIndex = fInfo.fCurrentQueueFamily;
        dstQueueFamilyIndex = gpu->queueIndex();
        fInfo.fCurrentQueueFamily = gpu->queueIndex();
    } else if (releaseFamilyQueue) {
        // We are releasing the image so we must transfer the image back to its original queue
        // family.
        SkASSERT(fInfo.fCurrentQueueFamily == gpu->queueIndex());
        srcQueueFamilyIndex = fInfo.fCurrentQueueFamily;
        dstQueueFamilyIndex = fInitialQueueFamily;
        fInfo.fCurrentQueueFamily = fInitialQueueFamily;
    }

    VkImageMemoryBarrier imageMemoryBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,          // sType
        nullptr,                                         // pNext
        srcAccessMask,                                   // outputMask
        dstAccessMask,                                   // inputMask
        currentLayout,                                   // oldLayout
        newLayout,                                       // newLayout
        srcQueueFamilyIndex,                             // srcQueueFamilyIndex
        dstQueueFamilyIndex,                             // dstQueueFamilyIndex
        fInfo.fImage,                                    // image
        { aspectFlags, 0, fInfo.fLevelCount, 0, 1 }      // subresourceRange
    };

    gpu->addImageMemoryBarrier(srcStageMask, dstStageMask, byRegion, &imageMemoryBarrier);

    this->updateImageLayout(newLayout);
}

bool GrVkImage::InitImageInfo(const GrVkGpu* gpu, const ImageDesc& imageDesc, GrVkImageInfo* info) {
    if (0 == imageDesc.fWidth || 0 == imageDesc.fHeight) {
        return false;
    }
    VkImage image = 0;
    GrVkAlloc alloc;

    bool isLinear = VK_IMAGE_TILING_LINEAR == imageDesc.fImageTiling;
    VkImageLayout initialLayout = isLinear ? VK_IMAGE_LAYOUT_PREINITIALIZED
                                           : VK_IMAGE_LAYOUT_UNDEFINED;

    // Create Image
    VkSampleCountFlagBits vkSamples;
    if (!GrSampleCountToVkSampleCount(imageDesc.fSamples, &vkSamples)) {
        return false;
    }

    SkASSERT(VK_IMAGE_TILING_OPTIMAL == imageDesc.fImageTiling ||
             VK_SAMPLE_COUNT_1_BIT == vkSamples);

    const VkImageCreateInfo imageCreateInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,         // sType
        nullptr,                                     // pNext
        0,                                           // VkImageCreateFlags
        imageDesc.fImageType,                        // VkImageType
        imageDesc.fFormat,                           // VkFormat
        { imageDesc.fWidth, imageDesc.fHeight, 1 },  // VkExtent3D
        imageDesc.fLevels,                           // mipLevels
        1,                                           // arrayLayers
        vkSamples,                                   // samples
        imageDesc.fImageTiling,                      // VkImageTiling
        imageDesc.fUsageFlags,                       // VkImageUsageFlags
        VK_SHARING_MODE_EXCLUSIVE,                   // VkSharingMode
        0,                                           // queueFamilyCount
        0,                                           // pQueueFamilyIndices
        initialLayout                                // initialLayout
    };

    GR_VK_CALL_ERRCHECK(gpu->vkInterface(), CreateImage(gpu->device(), &imageCreateInfo, nullptr,
                                                        &image));

    if (!GrVkMemory::AllocAndBindImageMemory(gpu, image, isLinear, &alloc)) {
        VK_CALL(gpu, DestroyImage(gpu->device(), image, nullptr));
        return false;
    }

    info->fImage = image;
    info->fAlloc = alloc;
    info->fImageTiling = imageDesc.fImageTiling;
    info->fImageLayout = initialLayout;
    info->fFormat = imageDesc.fFormat;
    info->fLevelCount = imageDesc.fLevels;
    info->fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    return true;
}

void GrVkImage::DestroyImageInfo(const GrVkGpu* gpu, GrVkImageInfo* info) {
    VK_CALL(gpu, DestroyImage(gpu->device(), info->fImage, nullptr));
    bool isLinear = VK_IMAGE_TILING_LINEAR == info->fImageTiling;
    GrVkMemory::FreeImageMemory(gpu, isLinear, info->fAlloc);
}

void GrVkImage::setNewResource(VkImage image, const GrVkAlloc& alloc, VkImageTiling tiling) {
    fResource = new Resource(image, alloc, tiling);
}

GrVkImage::~GrVkImage() {
    // should have been released or abandoned first
    SkASSERT(!fResource);
}

void GrVkImage::releaseImage(const GrVkGpu* gpu) {
    if (fInfo.fCurrentQueueFamily != fInitialQueueFamily) {
        this->setImageLayout(gpu, this->currentLayout(), 0, 0, false, true);
    }
    if (fResource) {
        fResource->unref(gpu);
        fResource = nullptr;
    }
}

void GrVkImage::abandonImage() {
    if (fResource) {
        fResource->unrefAndAbandon();
        fResource = nullptr;
    }
}

void GrVkImage::setResourceRelease(sk_sp<GrReleaseProcHelper> releaseHelper) {
    // Forward the release proc on to GrVkImage::Resource
    fResource->setRelease(std::move(releaseHelper));
}

void GrVkImage::Resource::freeGPUData(const GrVkGpu* gpu) const {
    SkASSERT(!fReleaseHelper);
    VK_CALL(gpu, DestroyImage(gpu->device(), fImage, nullptr));
    bool isLinear = (VK_IMAGE_TILING_LINEAR == fImageTiling);
    GrVkMemory::FreeImageMemory(gpu, isLinear, fAlloc);
}

void GrVkImage::BorrowedResource::freeGPUData(const GrVkGpu* gpu) const {
    this->invokeReleaseProc();
}

void GrVkImage::BorrowedResource::abandonGPUData() const {
    this->invokeReleaseProc();
}

