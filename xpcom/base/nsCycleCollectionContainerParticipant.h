/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCycleCollectionContainerParticipant_h__
#define nsCycleCollectionContainerParticipant_h__

#include <type_traits>

/*
 * To be able to implement ImplCycleCollectionIndexedContainer, we need to
 * handle const vs non-const. ImplCycleCollectionTrace requires that the the
 * value traced is non-const, and historically ImplCycleCollectionTraverse has
 * been declared to take either const or non-const values. This poses a problem
 * for containers, since it's not possible to define one
 * ImplCycleCollectionIndexedContainer for both the const and non-const case
 * with a templated parameter for the type contained by the container. The
 * reason for this is how overload resolution works when it sees non-generic
 * types with different constness. The standard solution for this is to use a
 * universal reference, but we can't do this only because suddenly our
 * ImplCycleCollectionIndexedContainer would be valid for any type. To make this
 * work we need a way to constrain the type and we need it to be a constraint on
 * the container type, not the type contained.
 *
 * This is pretty much the proposal for std::is_specialization_of, but with
 * names from the unofficial cycle collector namespace. We use this to be able
 * to do partial specialization to overload containers whose contents we wish to
 * have participate in cycle collection. `template <typename Container>
 * EnableCycleCollectionIf<Container, SomeContainer>` allows us to restrict an
 * overload to only happen for a type SomeContainer<T>, which we then can use to
 * make sure that an ImplCycleCollectionIndexedContainer overload is for a
 * particular container, const or not.
 *
 * Example:
 *
 * template <typename Container, typename Callback,
 *           EnableCycleCollectionIf<Container, nsTHashtable> = nullptr>
 * inline void ImplCycleCollectionContainer(Container&& aField,
 *                                          Callback&& aCallback) {
 *   // Implementation goes here
 * }
 */
template <typename, template <typename...> typename>
struct ImplCycleCollectionIsContainerT : std::false_type {};

template <template <typename...> typename Container, typename... Args>
struct ImplCycleCollectionIsContainerT<Container<Args...>, Container>
    : std::true_type {};

template <typename T, template <typename...> typename Container>
constexpr bool ImplCycleCollectionIsContainer = ImplCycleCollectionIsContainerT<
    std::remove_cv_t<std::remove_reference_t<T>>, Container>::value;

template <typename T, template <typename...> typename Container>
using EnableCycleCollectionIf =
    typename std::enable_if_t<ImplCycleCollectionIsContainer<T, Container>>*;

#endif  // nsCycleCollectionContainerParticipant_h__
