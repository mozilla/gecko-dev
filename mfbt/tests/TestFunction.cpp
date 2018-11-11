/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/Function.h"

using mozilla::function;

#define CHECK(c) \
  do { \
    bool cond = !!(c); \
    MOZ_RELEASE_ASSERT(cond, "Failed assertion: " #c); \
  } while (false)

struct ConvertibleToInt
{
  operator int() const { return 42; }
};

int increment(int arg) { return arg + 1; }

struct S {
  S() {}
  static int increment(int arg) { return arg + 1; }
  int decrement(int arg) { return arg - 1;}
  int sum(int arg1, int arg2) const { return arg1 + arg2;}
};

struct Incrementor {
  int operator()(int arg) { return arg + 1; }
};

static void
TestNonmemberFunction()
{
  function<int(int)> f = &increment;
  CHECK(f(42) == 43);
}

static void
TestStaticMemberFunction()
{
  function<int(int)> f = &S::increment;
  CHECK(f(42) == 43);
}

static void
TestFunctionObject()
{
  function<int(int)> f = Incrementor();
  CHECK(f(42) == 43);
}

static void
TestLambda()
{
  // Test non-capturing lambda
  function<int(int)> f = [](int arg){ return arg + 1; };
  CHECK(f(42) == 43);

  // Test capturing lambda
  int one = 1;
  function<int(int)> g = [one](int arg){ return arg + one; };
  CHECK(g(42) == 43);
}

static void
TestDefaultConstructionAndAssignmentLater()
{
  function<int(int)> f;  // allowed
  // Would get an assertion if we tried calling f now.
  f = &increment;
  CHECK(f(42) == 43);
}

static void
TestReassignment()
{
  function<int(int)> f = &increment;
  CHECK(f(42) == 43);
  f = [](int arg){ return arg + 2; };
  CHECK(f(42) == 44);
}

static void
TestMemberFunction()
{
  function<int(S&, int)> f = &S::decrement;
  S s;
  CHECK((f(s, 1) == 0));
}

static void
TestConstMemberFunction()
{
  function<int(const S*, int, int)> f = &S::sum;
  const S s;
  CHECK((f(&s, 1, 1) == 2));
}
int
main()
{
  TestNonmemberFunction();
  TestStaticMemberFunction();
  TestFunctionObject();
  TestLambda();
  TestDefaultConstructionAndAssignmentLater();
  TestReassignment();
  TestMemberFunction();
  TestConstMemberFunction();
  return 0;
}
