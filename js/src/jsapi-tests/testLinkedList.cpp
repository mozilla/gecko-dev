/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ScopeExit.h"

#include "ds/SlimLinkedList.h"

#include "jsapi-tests/tests.h"

using namespace js;

struct IntElement : public SlimLinkedListElement<IntElement> {
  int value;
  explicit IntElement(int value = 0) : value(value) {}
  void incr() { ++value; }
};

BEGIN_TEST(testSlimLinkedList) {
  CHECK(TestList());
  CHECK(TestMove());
  CHECK(TestExtendLists());
  return true;
}

template <size_t N>
[[nodiscard]] bool PushListValues(SlimLinkedList<IntElement>& list,
                                  const std::array<int, N>& values) {
  for (int value : values) {
    IntElement* element = new IntElement(value);
    CHECK(element);
    list.pushBack(element);
  }

  return true;
}

template <size_t N>
[[nodiscard]] bool CheckListValues(const SlimLinkedList<IntElement>& list,
                                   const std::array<int, N>& expected) {
  size_t count = 0;
  for (const IntElement* x : list) {
    CHECK(x->value == expected[count]);
    ++count;
  }
  CHECK(count == N);

  return true;
}

bool TestList() {
  SlimLinkedList<IntElement> list;
  const SlimLinkedList<IntElement>& constList = list;

  IntElement one(1), two(2), three(3);

  auto guard = mozilla::MakeScopeExit([&list]() { list.clear(); });

  // Test empty list.
  CHECK(list.isEmpty());
  CHECK(list.length() == 0);
  CHECK(!list.getFirst());
  CHECK(!list.getLast());
  CHECK(!list.popFirst());
  CHECK(!list.popLast());
  CHECK(list.begin() == list.end());
  CHECK(constList.begin() == constList.end());
  CHECK(CheckListValues(list, std::array<int, 0>{}));

  // Test pushFront.
  list.pushFront(&one);
  CHECK(!list.isEmpty());
  CHECK(list.length() == 1);
  CHECK(list.getFirst() == &one);
  CHECK(list.getLast() == &one);
  CHECK(*list.begin() == &one);
  list.pushFront(&two);
  CHECK(list.length() == 2);
  CHECK(list.getFirst() == &two);
  CHECK(list.getLast() == &one);
  CHECK(*list.begin() == &two);
  CHECK(CheckListValues(list, std::array{2, 1}));
  CHECK(list.contains(&one));
  CHECK(!list.contains(&three));

  // Test popFirst.
  IntElement* element = list.popFirst();
  CHECK(element == &two);
  CHECK(list.length() == 1);
  element = list.popFirst();
  CHECK(element == &one);
  CHECK(list.isEmpty());

  // Test pushBack.
  list.pushBack(&one);
  CHECK(!list.isEmpty());
  CHECK(list.length() == 1);
  CHECK(list.getFirst() == &one);
  CHECK(list.getLast() == &one);
  CHECK(*list.begin() == &one);
  list.pushBack(&two);
  CHECK(list.length() == 2);
  CHECK(list.getFirst() == &one);
  CHECK(list.getLast() == &two);
  CHECK(*list.begin() == &one);
  CHECK(CheckListValues(list, std::array{1, 2}));
  CHECK(list.contains(&one));
  CHECK(!list.contains(&three));

  // Test popLast.
  element = list.popLast();
  CHECK(element == &two);
  CHECK(list.length() == 1);
  element = list.popLast();
  CHECK(element == &one);
  CHECK(list.isEmpty());

  // Test remove.
  list.pushBack(&one);
  list.pushBack(&two);
  list.pushBack(&three);
  list.remove(&one);
  CHECK(CheckListValues(list, std::array{2, 3}));
  list.pushFront(&one);
  list.remove(&three);
  CHECK(CheckListValues(list, std::array{1, 2}));
  list.pushBack(&three);
  list.remove(&two);
  CHECK(CheckListValues(list, std::array{1, 3}));

  // Test clear.
  list.clear();
  CHECK(list.isEmpty());
  list.clear();
  CHECK(list.isEmpty());

  return true;
}

bool TestExtendLists() {
  SlimLinkedList<IntElement> list1;
  auto guard =
      mozilla::MakeScopeExit([&]() { list1.drain([](auto* e) { delete e; }); });

  CHECK(PushListValues(list1, std::array{0, 1, 2}));
  CHECK(CheckListValues(list1, std::array{0, 1, 2}));

  // Test extending with empty list.
  list1.append(SlimLinkedList<IntElement>());
  CHECK(CheckListValues(list1, std::array{0, 1, 2}));
  list1.prepend(SlimLinkedList<IntElement>());
  CHECK(CheckListValues(list1, std::array{0, 1, 2}));

  // Test extending empty list.
  SlimLinkedList<IntElement> list2;
  list2.append(std::move(list1));
  CHECK(list1.isEmpty());
  CHECK(CheckListValues(list2, std::array{0, 1, 2}));
  list1.prepend(std::move(list2));
  CHECK(list2.isEmpty());
  CHECK(CheckListValues(list1, std::array{0, 1, 2}));

  // Test append.
  CHECK(PushListValues(list2, std::array{3, 4, 5}));
  CHECK(CheckListValues(list2, std::array{3, 4, 5}));
  list1.append(std::move(list2));
  CHECK(CheckListValues(list1, std::array{0, 1, 2, 3, 4, 5}));
  CHECK(list2.isEmpty());

  // Test prepend.
  CHECK(PushListValues(list2, std::array{6, 7, 8}));
  CHECK(CheckListValues(list2, std::array{6, 7, 8}));
  list1.prepend(std::move(list2));
  CHECK(CheckListValues(list1, std::array{6, 7, 8, 0, 1, 2, 3, 4, 5}));
  CHECK(list2.isEmpty());

  return true;
}

bool TestMove() {
  // Test move constructor for the element.
  IntElement c1(IntElement(1));
  CHECK(c1.value == 1);

  // Test move assignment from an element not in a list.
  IntElement c2;
  c2 = IntElement(2);
  CHECK(c2.value == 2);

  SlimLinkedList<IntElement> list1;
  list1.pushBack(&c1);
  list1.pushBack(&c2);

  // Test move constructor for the list.
  SlimLinkedList<IntElement> list2(std::move(list1));
  CHECK(CheckListValues(list2, std::array{1, 2}));
  CHECK(list1.isEmpty());

  // Test move assignment for the list.
  SlimLinkedList<IntElement> list3;
  list3 = std::move(list2);
  CHECK(CheckListValues(list3, std::array{1, 2}));
  CHECK(list2.isEmpty());

  list3.clear();

  return true;
}
END_TEST(testSlimLinkedList)
