/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsQueryFrame_h
#define nsQueryFrame_h

#include <type_traits>

#include "nscore.h"
#include "mozilla/Assertions.h"

namespace mozilla {
class ScrollContainerFrame;
}

#define NS_DECL_QUERYFRAME_TARGET(classname)      \
  static const nsQueryFrame::FrameIID kFrameIID = \
      nsQueryFrame::classname##_id;               \
  typedef classname Has_NS_DECL_QUERYFRAME_TARGET;

#define NS_DECL_QUERYFRAME void* QueryFrame(FrameIID id) const override;

#define NS_QUERYFRAME_HEAD(class)               \
  void* class ::QueryFrame(FrameIID id) const { \
    switch (id) {
#define NS_QUERYFRAME_ENTRY(class)                                    \
  case class ::kFrameIID: {                                           \
    static_assert(                                                    \
        std::is_same_v<class, class ::Has_NS_DECL_QUERYFRAME_TARGET>, \
        #class " must declare itself as a queryframe target");        \
    return const_cast<class*>(static_cast<const class*>(this));       \
  }

#define NS_QUERYFRAME_ENTRY_CONDITIONAL(class, condition)               \
  case class ::kFrameIID:                                               \
    if (condition) {                                                    \
      static_assert(                                                    \
          std::is_same_v<class, class ::Has_NS_DECL_QUERYFRAME_TARGET>, \
          #class " must declare itself as a queryframe target");        \
      return const_cast<class*>(static_cast<const class*>(this));       \
    }                                                                   \
    break;

#define NS_QUERYFRAME_TAIL_INHERITING(class) \
  default:                                   \
    break;                                   \
    }                                        \
    return class ::QueryFrame(id);           \
    }

#define NS_QUERYFRAME_TAIL_INHERITANCE_ROOT                          \
  default:                                                           \
    break;                                                           \
    }                                                                \
    MOZ_ASSERT(id != GetFrameId(),                                   \
               "A frame failed to QueryFrame to its *own type*. "    \
               "It may be missing NS_DECL_QUERYFRAME, or a "         \
               "NS_QUERYFRAME_ENTRY() line with its own type name"); \
    return nullptr;                                                  \
    }

class nsQueryFrame {
 public:
  enum FrameIID {
#define FRAME_ID(classname, ...) classname##_id,
#define ABSTRACT_FRAME_ID(classname) classname##_id,
#include "mozilla/FrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
  };

  // A strict subset of FrameIID above for frame classes that we instantiate.
  enum class ClassID : uint8_t {
#define FRAME_ID(classname, ...) classname##_id,
#define ABSTRACT_FRAME_ID(classname)
#include "mozilla/FrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
  };

  virtual void* QueryFrame(FrameIID id) const = 0;
};

class nsIFrame;

namespace detail {
template <typename Dest, typename = void>
struct FastQueryFrame {
  static constexpr bool kSupported = false;
};

// For final classes we can check the class id.
template <typename Dest>
struct FastQueryFrame<Dest, std::enable_if_t<std::is_final_v<Dest>, void>> {
  static constexpr bool kSupported = true;

  template <typename Src>
  static Dest* QueryFrame(Src* aPtr) {
    return nsQueryFrame::FrameIID(aPtr->GetClassID()) == Dest::kFrameIID
               ? static_cast<Dest*>(aPtr)
               : nullptr;
  }
};

#define IMPL_FAST_QUERYFRAME(Dest_, Check_)                        \
  template <>                                                      \
  struct FastQueryFrame<Dest_, void> {                             \
    static constexpr bool kSupported = true;                       \
    template <typename Src>                                        \
    static Dest_* QueryFrame(Src* aPtr) {                          \
      return aPtr->Check_() ? static_cast<Dest_*>(aPtr) : nullptr; \
    }                                                              \
  }

IMPL_FAST_QUERYFRAME(mozilla::ScrollContainerFrame,
                     IsScrollContainerOrSubclass);

#undef IMPL_FAST_QUERYFRAME
}

template <typename Source>
class do_QueryFrameHelper {
 public:
  explicit do_QueryFrameHelper(Source* s) : mRawPtr(s) {}

  // The return and argument types here are arbitrarily selected so no
  // corresponding member function exists.
  using MatchNullptr = void (*)(double, float);
  // Implicit constructor for nullptr, trick borrowed from already_AddRefed.
  MOZ_IMPLICIT do_QueryFrameHelper(MatchNullptr aRawPtr) : mRawPtr(nullptr) {}

  template <typename Dest>
  operator Dest*() {
    static_assert(std::is_same_v<std::remove_const_t<Dest>,
                                 typename Dest::Has_NS_DECL_QUERYFRAME_TARGET>,
                  "Dest must declare itself as a queryframe target");
    if (!mRawPtr) {
      return nullptr;
    }
    if constexpr (::detail::FastQueryFrame<Dest>::kSupported) {
      static_assert(
          std::is_base_of_v<nsIFrame, Source>,
          "We only support fast do_QueryFrame() where the source must be a "
          "derived class of nsIFrame. Consider a two-step do_QueryFrame() "
          "(once to nsIFrame, another to the target) if absolutely needed.");
      Dest* f = ::detail::FastQueryFrame<Dest>::QueryFrame(mRawPtr);
      MOZ_ASSERT(
          f == reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID)),
          "fast and slow paths should give the same result");
      return f;
    }
    if constexpr (std::is_base_of_v<nsIFrame, Source> &&
                  std::is_base_of_v<nsIFrame, Dest>) {
      // For non-final frames we can still optimize the virtual call some of the
      // time.
      if (nsQueryFrame::FrameIID(mRawPtr->GetClassID()) == Dest::kFrameIID) {
        auto* f = static_cast<Dest*>(mRawPtr);
        MOZ_ASSERT(
            f == reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID)),
            "fast and slow paths should give the same result");
        return f;
      }
    }
    return reinterpret_cast<Dest*>(mRawPtr->QueryFrame(Dest::kFrameIID));
  }

 private:
  Source* mRawPtr;
};

template <typename T>
inline do_QueryFrameHelper<T> do_QueryFrame(T* s) {
  return do_QueryFrameHelper<T>(s);
}

#endif  // nsQueryFrame_h
