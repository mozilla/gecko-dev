/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrVkPipelineLayout_DEFINED
#define GrVkPipelineLayout_DEFINED

#include "GrTypes.h"
#include "GrVkResource.h"
#include "vk/GrVkDefines.h"

class GrVkPipelineLayout : public GrVkResource {
public:
    GrVkPipelineLayout(VkPipelineLayout layout) : fPipelineLayout(layout) {}

    VkPipelineLayout layout() const { return fPipelineLayout; }

#ifdef SK_TRACE_VK_RESOURCES
    void dumpInfo() const override {
        SkDebugf("GrVkPipelineLayout: %d (%d refs)\n", fPipelineLayout, this->getRefCnt());
    }
#endif

private:
    GrVkPipelineLayout(const GrVkPipelineLayout&);
    GrVkPipelineLayout& operator=(const GrVkPipelineLayout&);

    void freeGPUData(const GrVkGpu* gpu) const override;

    VkPipelineLayout  fPipelineLayout;

    typedef GrVkResource INHERITED;
};

#endif
