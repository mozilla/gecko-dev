/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

#include "wasm/WasmValType.h"

BEGIN_TEST(testWasmRefType_LUB) {
  using namespace js;
  using namespace wasm;

  struct TestCase {
    RefType a;
    RefType b;
    RefType lub;
  };

  // For the any hierarchy:
  //
  // s1     a1
  // │ └┐
  // s2 s3

  MutableTypeContext types = new TypeContext();
  MutableRecGroup recGroup = types->startRecGroup(4);

  TypeDef* tA1 = &recGroup->type(0);
  *tA1 = ArrayType(StorageType::I16, true);

  TypeDef* tS1 = &recGroup->type(1);
  *tS1 = StructType();

  TypeDef* tS2 = &recGroup->type(2);
  FieldTypeVector s2Fields;
  CHECK(s2Fields.append(FieldType(StorageType::I32, false)));
  *tS2 = StructType(std::move(s2Fields));
  tS2->setSuperTypeDef(tS1);

  TypeDef* tS3 = &recGroup->type(3);
  FieldTypeVector s3Fields;
  CHECK(s3Fields.append(FieldType(StorageType::I64, false)));
  *tS3 = StructType(std::move(s3Fields));
  tS3->setSuperTypeDef(tS1);

  CHECK(types->endRecGroup());

  RefType a1 = RefType::fromTypeDef(tA1, true);
  RefType s1 = RefType::fromTypeDef(tS1, true);
  RefType s2 = RefType::fromTypeDef(tS2, true);
  RefType s3 = RefType::fromTypeDef(tS3, true);

  TestCase testCases[] = {
      //
      // Tops and bottoms
      //

      // any, any -> any
      {RefType::any(), RefType::any(), RefType::any()},
      {RefType::any().asNonNullable(), RefType::any(), RefType::any()},
      {RefType::any().asNonNullable(), RefType::any().asNonNullable(),
       RefType::any().asNonNullable()},

      // none, none -> none
      {RefType::none(), RefType::none(), RefType::none()},
      {RefType::none().asNonNullable(), RefType::none(), RefType::none()},
      {RefType::none().asNonNullable(), RefType::none().asNonNullable(),
       RefType::none().asNonNullable()},

      // none, any -> any
      {RefType::none(), RefType::any(), RefType::any()},
      {RefType::none().asNonNullable(), RefType::any(), RefType::any()},
      {RefType::none(), RefType::any().asNonNullable(), RefType::any()},
      {RefType::none().asNonNullable(), RefType::any().asNonNullable(),
       RefType::any().asNonNullable()},

      // func, func -> func
      {RefType::func(), RefType::func(), RefType::func()},
      {RefType::func().asNonNullable(), RefType::func(), RefType::func()},
      {RefType::func().asNonNullable(), RefType::func().asNonNullable(),
       RefType::func().asNonNullable()},

      // nofunc, nofunc -> nofunc
      {RefType::nofunc(), RefType::nofunc(), RefType::nofunc()},
      {RefType::nofunc().asNonNullable(), RefType::nofunc(), RefType::nofunc()},
      {RefType::nofunc().asNonNullable(), RefType::nofunc().asNonNullable(),
       RefType::nofunc().asNonNullable()},

      // nofunc, func -> func
      {RefType::nofunc(), RefType::func(), RefType::func()},
      {RefType::nofunc().asNonNullable(), RefType::func(), RefType::func()},
      {RefType::nofunc(), RefType::func().asNonNullable(), RefType::func()},
      {RefType::nofunc().asNonNullable(), RefType::func().asNonNullable(),
       RefType::func().asNonNullable()},

      // extern, extern -> extern
      {RefType::extern_(), RefType::extern_(), RefType::extern_()},
      {RefType::extern_().asNonNullable(), RefType::extern_(),
       RefType::extern_()},
      {RefType::extern_().asNonNullable(), RefType::extern_().asNonNullable(),
       RefType::extern_().asNonNullable()},

      // noextern, noextern -> noextern
      {RefType::noextern(), RefType::noextern(), RefType::noextern()},
      {RefType::noextern().asNonNullable(), RefType::noextern(),
       RefType::noextern()},
      {RefType::noextern().asNonNullable(), RefType::noextern().asNonNullable(),
       RefType::noextern().asNonNullable()},

      // noextern, extern -> extern
      {RefType::noextern(), RefType::extern_(), RefType::extern_()},
      {RefType::noextern().asNonNullable(), RefType::extern_(),
       RefType::extern_()},
      {RefType::noextern(), RefType::extern_().asNonNullable(),
       RefType::extern_()},
      {RefType::noextern().asNonNullable(), RefType::extern_().asNonNullable(),
       RefType::extern_().asNonNullable()},

      // exn, exn -> exn
      {RefType::exn(), RefType::exn(), RefType::exn()},
      {RefType::exn().asNonNullable(), RefType::exn(), RefType::exn()},
      {RefType::exn().asNonNullable(), RefType::exn().asNonNullable(),
       RefType::exn().asNonNullable()},

      // noexn, noexn -> noexn
      {RefType::noexn(), RefType::noexn(), RefType::noexn()},
      {RefType::noexn().asNonNullable(), RefType::noexn(), RefType::noexn()},
      {RefType::noexn().asNonNullable(), RefType::noexn().asNonNullable(),
       RefType::noexn().asNonNullable()},

      // noexn, exn -> exn
      {RefType::noexn(), RefType::exn(), RefType::exn()},
      {RefType::noexn().asNonNullable(), RefType::exn(), RefType::exn()},
      {RefType::noexn(), RefType::exn().asNonNullable(), RefType::exn()},
      {RefType::noexn().asNonNullable(), RefType::exn().asNonNullable(),
       RefType::exn().asNonNullable()},

      //
      // concrete type, abstract types
      //

      // $a1, $a1 -> array
      {a1, a1, a1},
      {a1.asNonNullable(), a1, a1},
      {a1.asNonNullable(), a1.asNonNullable(), a1.asNonNullable()},

      // $a1, any -> any
      {a1, RefType::any(), RefType::any()},
      {a1, RefType::any().asNonNullable(), RefType::any()},
      {a1.asNonNullable(), RefType::any(), RefType::any()},
      {a1.asNonNullable(), RefType::any().asNonNullable(),
       RefType::any().asNonNullable()},

      // $a1, eq -> eq
      {a1, RefType::eq(), RefType::eq()},
      {a1.asNonNullable(), RefType::eq(), RefType::eq()},
      {a1, RefType::eq().asNonNullable(), RefType::eq()},
      {a1.asNonNullable(), RefType::eq().asNonNullable(),
       RefType::eq().asNonNullable()},

      // $a1, i31 -> eq
      {a1, RefType::i31(), RefType::eq()},
      {a1.asNonNullable(), RefType::i31(), RefType::eq()},
      {a1, RefType::i31().asNonNullable(), RefType::eq()},
      {a1.asNonNullable(), RefType::i31().asNonNullable(),
       RefType::eq().asNonNullable()},

      // $a1, struct -> eq
      {a1, RefType::struct_(), RefType::eq()},
      {a1.asNonNullable(), RefType::struct_(), RefType::eq()},
      {a1, RefType::struct_().asNonNullable(), RefType::eq()},
      {a1.asNonNullable(), RefType::struct_().asNonNullable(),
       RefType::eq().asNonNullable()},

      // $a1, array -> array
      {a1, RefType::array(), RefType::array()},
      {a1.asNonNullable(), RefType::array(), RefType::array()},
      {a1, RefType::array().asNonNullable(), RefType::array()},
      {a1.asNonNullable(), RefType::array().asNonNullable(),
       RefType::array().asNonNullable()},

      // $a1, none -> $a1
      {a1, RefType::none(), a1},
      {a1, RefType::none().asNonNullable(), a1},
      {a1.asNonNullable(), RefType::none(), a1},
      {a1.asNonNullable(), RefType::none().asNonNullable(), a1.asNonNullable()},

      //
      // concrete subtypes
      //

      // $s1, $s2 -> $s1
      {s1, s2, s1},
      {s1.asNonNullable(), s2, s1},
      {s1, s2.asNonNullable(), s1},
      {s1.asNonNullable(), s2.asNonNullable(), s1.asNonNullable()},

      //
      // concrete sibling types
      //

      // $s2, $s3 -> $s1
      {s2, s3, s1},
      {s2.asNonNullable(), s3, s1},
      {s2, s3.asNonNullable(), s1},
      {s2.asNonNullable(), s3.asNonNullable(), s1.asNonNullable()},

      //
      // unrelated concrete types
      //

      // $s1, $a1 -> eq
      {s1, a1, RefType::eq()},
      {s1.asNonNullable(), a1, RefType::eq()},
      {s1, a1.asNonNullable(), RefType::eq()},
      {s1.asNonNullable(), a1.asNonNullable(), RefType::eq().asNonNullable()},
  };

#define CHECK_LUB(a, b, expected)                                          \
  do {                                                                     \
    RefType _actual = RefType::leastUpperBound((a), (b));                  \
    if (_actual != (expected)) {                                           \
      return fail(JSAPITestString("bad LUB of ") +                         \
                      wasm::ToString((a), types).get() + " and " +         \
                      wasm::ToString((b), types).get() + ": expected " +   \
                      wasm::ToString((expected), types).get() + ", got " + \
                      wasm::ToString(_actual, types).get(),                \
                  __FILE__, __LINE__);                                     \
    }                                                                      \
  } while (false)

  for (const TestCase& testCase : testCases) {
    // LUB is commutative.
    CHECK_LUB(testCase.a, testCase.b, testCase.lub);
    CHECK_LUB(testCase.b, testCase.a, testCase.lub);
  }

  return true;
}
END_TEST(testWasmRefType_LUB)
