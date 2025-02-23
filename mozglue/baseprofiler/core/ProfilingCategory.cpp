/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfilingCategory.h"

#include "mozilla/Assertions.h"

namespace mozilla {
namespace baseprofiler {

// clang-format off

// ProfilingSubcategory_X:
// One enum for each category X, listing that category's subcategories. This
// allows the sProfilingCategoryInfo macro construction below to look up a
// per-category index for a subcategory.
#define SUBCATEGORY_ENUMS_BEGIN_CATEGORY(name, labelAsString, color) \
  enum class ProfilingSubcategory_##name : uint32_t {
#define SUBCATEGORY_ENUMS_SUBCATEGORY(category, name, labelAsString) \
    name,
#define SUBCATEGORY_ENUMS_END_CATEGORY \
  };
MOZ_PROFILING_CATEGORY_LIST(SUBCATEGORY_ENUMS_BEGIN_CATEGORY,
                            SUBCATEGORY_ENUMS_SUBCATEGORY,
                            SUBCATEGORY_ENUMS_END_CATEGORY)
#undef SUBCATEGORY_ENUMS_BEGIN_CATEGORY
#undef SUBCATEGORY_ENUMS_SUBCATEGORY
#undef SUBCATEGORY_ENUMS_END_CATEGORY

// sProfilingCategoryPairInfo:
// A list of ProfilingCategoryPairInfos with the same order as
// ProfilingCategoryPair, which can be used to map a ProfilingCategoryPair to
// its information.
#define CATEGORY_PAIR_INFO_BEGIN_CATEGORY(name, labelAsString, color)
#define CATEGORY_PAIR_INFO_SUBCATEGORY(category, name, labelAsString) \
  {ProfilingCategory::category,                                  \
   uint32_t(ProfilingSubcategory_##category::name), labelAsString},
#define CATEGORY_PAIR_INFO_END_CATEGORY
static constexpr ProfilingCategoryPairInfo sProfilingCategoryPairInfo[] = {
  MOZ_PROFILING_CATEGORY_LIST(CATEGORY_PAIR_INFO_BEGIN_CATEGORY,
                              CATEGORY_PAIR_INFO_SUBCATEGORY,
                              CATEGORY_PAIR_INFO_END_CATEGORY)
};
#undef CATEGORY_PAIR_INFO_BEGIN_CATEGORY
#undef CATEGORY_PAIR_INFO_SUBCATEGORY
#undef CATEGORY_PAIR_INFO_END_CATEGORY

// sSubcategoryNames_X:
// One array per category, listing the subcategory names of that category.
#define SUBCATEGORY_NAMES_BEGIN_CATEGORY(name, labelAsString, color) \
  static constexpr const char* sSubcategoryNames_##name[] = {
#define SUBCATEGORY_NAMES_SUBCATEGORY(supercategory, name, labelAsString) labelAsString,
#define SUBCATEGORY_NAMES_END_CATEGORY \
  };

MOZ_PROFILING_CATEGORY_LIST(SUBCATEGORY_NAMES_BEGIN_CATEGORY,
                            SUBCATEGORY_NAMES_SUBCATEGORY,
                            SUBCATEGORY_NAMES_END_CATEGORY)

#undef SUBCATEGORY_NAMES_BEGIN_CATEGORY
#undef SUBCATEGORY_NAMES_SUBCATEGORY
#undef SUBCATEGORY_NAMES_END_CATEGORY

// sProfilingCategoryInfoList:
// A list of ProfilingCategoryInfo for all categories.
#define CATEGORY_INFO_LIST_BEGIN_CATEGORY(name, labelAsString, color) \
  {labelAsString, color, Span{sSubcategoryNames_##name}},
#define CATEGORY_INFO_LIST_SUBCATEGORY(supercategory, name, labelAsString)
#define CATEGORY_INFO_LIST_END_CATEGORY

static constexpr ProfilingCategoryInfo sProfilingCategoryInfoList[] = {
  MOZ_PROFILING_CATEGORY_LIST(CATEGORY_INFO_LIST_BEGIN_CATEGORY,
                              CATEGORY_INFO_LIST_SUBCATEGORY,
                              CATEGORY_INFO_LIST_END_CATEGORY)
};

#undef CATEGORY_INFO_LIST_BEGIN_CATEGORY
#undef CATEGORY_INFO_LIST_SUBCATEGORY
#undef CATEGORY_INFO_LIST_END_CATEGORY

// clang-format on

Span<const ProfilingCategoryInfo> GetProfilingCategoryList() {
  return Span{sProfilingCategoryInfoList};
}

const ProfilingCategoryPairInfo& GetProfilingCategoryPairInfo(
    ProfilingCategoryPair aCategoryPair) {
  static_assert(
      std::size(sProfilingCategoryPairInfo) ==
          uint32_t(ProfilingCategoryPair::COUNT),
      "sProfilingCategoryPairInfo and ProfilingCategory need to have the "
      "same order and the same length");

  uint32_t categoryPairIndex = uint32_t(aCategoryPair);
  MOZ_RELEASE_ASSERT(categoryPairIndex <=
                     uint32_t(ProfilingCategoryPair::LAST));
  return sProfilingCategoryPairInfo[categoryPairIndex];
}

}  // namespace baseprofiler
}  // namespace mozilla
